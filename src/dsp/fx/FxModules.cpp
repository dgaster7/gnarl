#include "FxHelpers.h"
#include "FxModule.h"
#include "engine/ParamIx.h"
#include <cstring>

namespace gnarl {

// =============================== base ===============================
void FxModule::prepare(float sampleRate, int maxBlock, int) {
    sr = sampleRate; maxN = maxBlock;
    wetL.assign((size_t) maxBlock, 0); wetR.assign((size_t) maxBlock, 0);
    int s = 1; while (s < 8192) s <<= 1;
    dlyL.assign((size_t) s, 0); dlyR.assign((size_t) s, 0); dlyMask = s - 1; dlyPos = 0;
}

void FxModule::run(float* L, float* R, int n, const FxParams& p, FxContext& ctx) {
    const bool zw = audit::zeroWet(mixIx);
    if (zw) { std::memset(wetL.data(), 0, sizeof(float) * (size_t) n); std::memset(wetR.data(), 0, sizeof(float) * (size_t) n); }
    else { std::memcpy(wetL.data(), L, sizeof(float) * (size_t) n); std::memcpy(wetR.data(), R, sizeof(float) * (size_t) n); }
    processWet(wetL.data(), wetR.data(), n, p, ctx);
    const int lat = latency();
    if (lat != lastLatency) { std::fill(dlyL.begin(), dlyL.end(), 0.f); std::fill(dlyR.begin(), dlyR.end(), 0.f); lastLatency = lat; }
    float m0 = p.e0[mixIx], m1 = p.e1[mixIx];
    if (audit::isRemoved(mixIx)) m0 = m1 = 0;
    for (int i = 0; i < n; ++i) {
        dlyL[(size_t) dlyPos] = L[i]; dlyR[(size_t) dlyPos] = R[i];
        const float dl = dlyL[(size_t) ((dlyPos - lat) & dlyMask)], dr = dlyR[(size_t) ((dlyPos - lat) & dlyMask)];
        dlyPos = (dlyPos + 1) & dlyMask;
        const float m = m0 + (m1 - m0) * (float) i / (float) n;
        if (m >= 1.f) { L[i] = wetL[(size_t) i]; R[i] = wetR[(size_t) i]; }
        else if (m <= 0.f) { L[i] = dl; R[i] = dr; }
        else { L[i] = dl + (wetL[(size_t) i] - dl) * m; R[i] = dr + (wetR[(size_t) i] - dr) * m; }
    }
}

namespace {

inline float lerpP(const FxParams& p, int ix, int i, int n) { return p.e0[ix] + (p.e1[ix] - p.e0[ix]) * (float) i / (float) n; }
inline int choiceOf(const FxParams& p, int ix) { return (int) std::lround(p.e1[ix]); }
inline bool on(const FxParams& p, int ix) { return p.e1[ix] > 0.5f; }

// Applies a per-sample nonlinear function at the oversampled rate with integer (linear-phase) latency.
// When `active` is false the signal is only delayed by the same latency, so the module stays bit-exact transparent.
struct OsBlock {
    Oversampler os; IntDelay dl[2]; int l2 = 2;
    void prepare(int maxBlock) { os.prepare(2, maxBlock, 3, Oversampler::Kind::LinearPhase); os.setFactorLog2(2); dl[0].prepare(128); dl[1].prepare(128); }
    void reset() { os.reset(); dl[0].reset(); dl[1].reset(); }
    void setL2(int v) { l2 = std::clamp(v, 1, 3); if (os.factorLog2() != l2) os.setFactorLog2(l2); }
};

// ------------------------------- Distortion -------------------------------
class DistFx : public FxModule {
public:
    int type, drive, bias, tone, out, morph;
    explicit DistFx(const std::string& m) {
        type = ix("fx_" + m + "_type"); drive = ix("fx_" + m + "_drive"); bias = ix("fx_" + m + "_bias"); tone = ix("fx_" + m + "_tone");
        out = ix("fx_" + m + "_out"); morph = ix("fx_" + m + "_morph");
    }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); ob.prepare(mb); for (auto& d : dc) d.setRate(s * 8); reset(); }
    void reset() override { ob.reset(); for (auto& d : dc) d.reset(); for (auto& s : st) s.reset(); for (auto& t : tl) t.reset(); }
    int latency() const override { return ob.os.latency(); }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext& ctx) override {
        ob.setL2(ctx.osLog2);
        float d0 = p.e0[drive], d1 = p.e1[drive];
        if (audit::isRemoved(drive)) d0 = d1 = 0;
        float b0 = p.e0[bias], b1 = p.e1[bias];
        if (audit::isRemoved(bias)) b0 = b1 = 0;
        const int ty = choiceOf(p, type);
        float* io[2] = {L, R};
        const int lat = ob.os.latency();
        if (d0 <= 0 && d1 <= 0 && b0 == 0 && b1 == 0) {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] = ob.dl[c].process(io[c][i], lat);
        } else {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) ob.dl[c].process(io[c][i], lat); // keep the bypass delay primed
            ob.os.up(io, n);
            const int N = n << ob.os.factorLog2();
            const float rate = sr * (float) (1 << ob.os.factorLog2());
            const bool dcb = distNeedsDcBlock(ty, std::max(std::fabs(b0), std::fabs(b1)));
            for (int c = 0; c < 2; ++c) {
                float* u = ob.os.data(c);
                for (int i = 0; i < N; ++i) {
                    const float t = (float) i / (float) N;
                    const float d = d0 + (d1 - d0) * t, bs = b0 + (b1 - b0) * t;
                    const float mo = p.e0[morph] + (p.e1[morph] - p.e0[morph]) * t;
                    float s = distortSample(ty, u[i], d, bs, st[c], rate, ctx.draw, mo);
                    if (dcb) s = dc[c].process(s);
                    const float bl = std::max(driveBlend(d), bs != 0 ? 1.f : 0.f);
                    u[i] = u[i] + (s - u[i]) * bl;
                }
            }
            ob.os.down(io, n);
        }
        // post tone (low-pass) and output gain — both neutral at their defaults
        const bool toneOn = !(p.e0[tone] >= 19999.f && p.e1[tone] >= 19999.f) && !audit::isRemoved(tone);
        if (toneOn) {
            float g0 = Svf::gOf(p.e0[tone], sr), g1 = Svf::gOf(p.e1[tone], sr);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] = tl[c].process(io[c][i], g0 + (g1 - g0) * (float) i / (float) n, 1.41421356f, 0);
        }
        if ((p.e0[out] != 0 || p.e1[out] != 0) && !audit::isRemoved(out)) {
            float a = dbToGain(p.e0[out]), b = dbToGain(p.e1[out]);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] *= a + (b - a) * (float) i / (float) n;
        }
    }
private:
    OsBlock ob; DcBlocker dc[2]; DistState st[2]; Svf tl[2];
};

// ------------------------------- Multiband distortion -------------------------------
class MbDistFx : public FxModule {
public:
    int xlo, xhi, conv;
    struct BandIx { int type, drive, mix, dyn, fb; } bi[3];
    MbDistFx() {
        xlo = ix("fx_mbd_xlo"); xhi = ix("fx_mbd_xhi"); conv = ix("fx_mbd_converge");
        for (int b = 0; b < 3; ++b) { std::string p = "fx_mbd_b" + std::to_string(b + 1) + "_"; bi[b] = {ix(p + "type"), ix(p + "drive"), ix(p + "mix"), ix(p + "dyn"), ix(p + "fb")}; }
    }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); ob.prepare(mb); reset(); }
    void reset() override { ob.reset(); for (auto& c : ch) { c = Chan(); } }
    int latency() const override { return ob.os.latency(); }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext& ctx) override {
        ob.setL2(ctx.osLog2);
        float* io[2] = {L, R};
        const int lat = ob.os.latency();
        bool active = false;
        float drv[3][2], dyn[3][2], fb[3][2];
        for (int b = 0; b < 3; ++b) {
            drv[b][0] = p.e0[bi[b].drive]; drv[b][1] = p.e1[bi[b].drive]; if (audit::isRemoved(bi[b].drive)) drv[b][0] = drv[b][1] = 0;
            dyn[b][0] = p.e0[bi[b].dyn]; dyn[b][1] = p.e1[bi[b].dyn]; if (audit::isRemoved(bi[b].dyn)) dyn[b][0] = dyn[b][1] = 0;
            fb[b][0] = p.e0[bi[b].fb]; fb[b][1] = p.e1[bi[b].fb]; if (audit::isRemoved(bi[b].fb)) fb[b][0] = fb[b][1] = 0;
            active |= drv[b][0] > 0 || drv[b][1] > 0 || dyn[b][0] != 0 || dyn[b][1] != 0 || fb[b][0] > 0 || fb[b][1] > 0;
        }
        if (!active) { for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] = ob.dl[c].process(io[c][i], lat); return; }
        for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) ob.dl[c].process(io[c][i], lat);
        ob.os.up(io, n);
        const int N = n << ob.os.factorLog2();
        const float rate = sr * (float) (1 << ob.os.factorLog2());
        auto xover = [&](int e, float& lo, float& hi) {
            float a = (e ? p.e1 : p.e0)[xlo], b = (e ? p.e1 : p.e0)[xhi], cv = (e ? p.e1 : p.e0)[conv];
            if (audit::isRemoved(conv)) cv = 0;
            if (b < a * 1.05f) b = a * 1.05f;
            float c = std::sqrt(a * b), r = std::sqrt(b / a);
            float r2 = std::pow(r, clampf(1.f - cv, 0.02f, 2.f));
            lo = clampf(c / r2, 20, rate * 0.45f); hi = clampf(c * r2, lo * 1.01f, rate * 0.45f);
        };
        float lo0, hi0, lo1, hi1; xover(0, lo0, hi0); xover(1, lo1, hi1);
        const float gl0 = Svf::gOf(lo0, rate), gl1 = Svf::gOf(lo1, rate), gh0 = Svf::gOf(hi0, rate), gh1 = Svf::gOf(hi1, rate);
        float envA = 0, envR = 0;
        { float a = 5.f, r = 80.f; envA = std::exp(-1.f / (a * 0.001f * rate)); envR = std::exp(-1.f / (r * 0.001f * rate)); }
        for (int c = 0; c < 2; ++c) {
            float* u = ob.os.data(c);
            auto& C = ch[c];
            for (int i = 0; i < N; ++i) {
                const float t = (float) i / (float) N;
                const float x = u[i];
                float low = C.lp1.process(x, gl0 + (gl1 - gl0) * t);
                float rest = x - low;
                float mid = C.lp2.process(rest, gh0 + (gh1 - gh0) * t);
                float high = rest - mid;
                float bands[3] = {low, mid, high};
                float sum = 0;
                for (int b = 0; b < 3; ++b) {
                    const float d = drv[b][0] + (drv[b][1] - drv[b][0]) * t;
                    const float dy = dyn[b][0] + (dyn[b][1] - dyn[b][0]) * t;
                    const float f = fb[b][0] + (fb[b][1] - fb[b][0]) * t;
                    float m = p.e0[bi[b].mix] + (p.e1[bi[b].mix] - p.e0[bi[b].mix]) * t;
                    if (audit::isRemoved(bi[b].mix)) m = 0;
                    float clean = bands[b];
                    if (d <= 0 && dy == 0 && f <= 0) { sum += clean; continue; }
                    float in = audit::zeroWet(bi[b].mix) ? 0.f : clean;
                    if (f > 0) in += f * C.prev[b];
                    float y = in;
                    if (dy != 0) {
                        float a = std::fabs(in);
                        C.env[b] = a > C.env[b] ? a + (C.env[b] - a) * envA : a + (C.env[b] - a) * envR;
                        float e = C.env[b], gn;
                        if (dy > 0) gn = clampf(std::pow(std::max(e, 1e-4f) / 0.25f, -dy * 0.6f), 0.25f, 4.f);
                        else { float th = 0.15f * -dy; float r = clampf(e / th, 0, 1); gn = 1.f + (r * r - 1.f) * std::min(1.f, -dy * 4.f); }
                        y *= gn;
                    }
                    if (d > 0) {
                        float s = distortSample(choiceOf(p, bi[b].type), y, d, 0, C.ds[b], rate, ctx.draw, 0);
                        y = y + (s - y) * driveBlend(d);
                    }
                    C.prev[b] = y;
                    sum += clean + (y - clean) * m;
                }
                u[i] = sum;
            }
        }
        ob.os.down(io, n);
    }
private:
    OsBlock ob;
    struct Chan { Lp4 lp1, lp2; float prev[3] = {}; float env[3] = {}; DistState ds[3]; };
    Chan ch[2];
};

// ------------------------------- Phaser -------------------------------
class PhaserFx : public FxModule {
public:
    int rate, depth, fb, centre, stages, stereo;
    PhaserFx() { rate = ix("fx_phaser_rate"); depth = ix("fx_phaser_depth"); fb = ix("fx_phaser_fb"); centre = ix("fx_phaser_centre"); stages = ix("fx_phaser_stages"); stereo = ix("fx_phaser_stereo"); }
    void reset() override { for (auto& c : st) { std::fill(std::begin(c.s), std::end(c.s), 0.f); c.fbv = 0; } ph = 0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float* io[2] = {L, R};
        const int ns = std::clamp(choiceOf(p, stages), 2, 12);
        const double inc = p.e1[rate] / sr;
        const double ph0 = ph;
        for (int c = 0; c < 2; ++c) {
            auto& S = st[c];
            double lph = ph0 + (c ? p.e1[stereo] / 360.0 : 0.0);
            for (int i = 0; i < n; ++i) {
                const float dep = lerpP(p, depth, i, n), f = lerpP(p, fb, i, n), cen = lerpP(p, centre, i, n);
                lph += inc;
                const float lfo = std::sin(kTwoPi * (float) (lph - std::floor(lph)));
                const float hz = cen * std::exp2(lfo * dep * 2.f);
                const float g = Svf::gOf(hz, sr), G = g / (1 + g);
                float y = io[c][i] + f * S.fbv;
                for (int k = 0; k < ns; ++k) { float v = (y - S.s[k]) * G; float l = v + S.s[k]; S.s[k] = l + v; y = 2 * l - y; }
                S.fbv = y;
                io[c][i] = 0.5f * (io[c][i] + y);
            }
        }
        ph = ph0 + inc * n; ph -= std::floor(ph);
    }
private:
    struct C { float s[12] = {}; float fbv = 0; } st[2];
    double ph = 0;
};

// ------------------------------- Flanger -------------------------------
class FlangerFx : public FxModule {
public:
    int rate, depth, delay, fb, stereo;
    FlangerFx() { rate = ix("fx_flanger_rate"); depth = ix("fx_flanger_depth"); delay = ix("fx_flanger_delay"); fb = ix("fx_flanger_fb"); stereo = ix("fx_flanger_stereo"); }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); for (auto& d : dl) d.prepare((int) (s * 0.05f)); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); fbv[0] = fbv[1] = 0; ph = 0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float* io[2] = {L, R};
        const double inc = p.e1[rate] / sr;
        for (int c = 0; c < 2; ++c) {
            double lph = ph + (c ? p.e1[stereo] / 360.0 : 0.0);
            for (int i = 0; i < n; ++i) {
                lph += inc;
                const float lfo = 0.5f + 0.5f * std::sin(kTwoPi * (float) (lph - std::floor(lph)));
                const float base = lerpP(p, delay, i, n) * 0.001f * sr;
                const float d = std::max(1.f, base * (1.f + lerpP(p, depth, i, n) * (lfo * 2.f - 1.f) * 0.9f));
                const float x = io[c][i];
                dl[c].push(x + lerpP(p, fb, i, n) * fbv[c]);
                const float y = dl[c].read(d + 1.f);
                fbv[c] = y;
                io[c][i] = 0.5f * (x + y);
            }
        }
        ph += inc * n; ph -= std::floor(ph);
    }
    double tailSeconds() const override { return 1.0; }
private:
    FracDelay dl[2]; float fbv[2] = {}; double ph = 0;
};

// ------------------------------- Chorus -------------------------------
class ChorusFx : public FxModule {
public:
    int rate, depth, delay, width, mono;
    ChorusFx() { rate = ix("fx_chorus_rate"); depth = ix("fx_chorus_depth"); delay = ix("fx_chorus_delay"); width = ix("fx_chorus_width"); mono = ix("fx_chorus_mono"); }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); for (auto& d : dl) d.prepare((int) (s * 0.1f)); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); for (auto& h : hp) h.reset(); ph = 0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        const double inc = p.e1[rate] / sr;
        for (int i = 0; i < n; ++i) {
            ph += inc; ph -= std::floor(ph);
            const float base = lerpP(p, delay, i, n) * 0.001f * sr, dep = lerpP(p, depth, i, n) * 0.004f * sr;
            const float x = 0.5f * (L[i] + R[i]);
            dl[0].push(L[i]); dl[1].push(R[i]);
            float v[3];
            for (int k = 0; k < 3; ++k) {
                float lfo = std::sin(kTwoPi * (float) (ph + k / 3.0));
                v[k] = dl[k % 2].read(std::max(2.f, base + dep * lfo));
            }
            float wl = (v[0] + 0.7f * v[2]) * 0.6f, wr = (v[1] + 0.7f * v[2]) * 0.6f;
            // width of the wet signal, then mono below the crossover (wet side high-passed)
            const float w = lerpP(p, width, i, n);
            float m = 0.5f * (wl + wr), s = 0.5f * (wl - wr) * w;
            s = hp[0].process(s, Svf::gOf(lerpP(p, mono, i, n), sr));
            L[i] = m + s; R[i] = m - s;
            (void) x;
        }
    }
private:
    FracDelay dl[2]; Hp4 hp[1]; double ph = 0;
};

// ------------------------------- Comb -------------------------------
class CombFx : public FxModule {
public:
    int freq, fb, damp, key;
    CombFx() { freq = ix("fx_comb_freq"); fb = ix("fx_comb_fb"); damp = ix("fx_comb_damp"); key = ix("fx_comb_key"); }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); for (auto& d : dl) d.prepare((int) (s / 18.f)); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); lp[0] = lp[1] = 0; }
    double tailSeconds() const override { return 2.0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext& ctx) override {
        float* io[2] = {L, R};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i) {
                float hz = on(p, key) ? ctx.lastNoteHz : lerpP(p, freq, i, n);
                float d = clampf(sr / std::max(hz, 20.f), 2.f, (float) dl[c].mask - 4);
                float f = lerpP(p, fb, i, n), dm = lerpP(p, damp, i, n);
                if (audit::isRemoved(damp)) dm = 0;
                float y = dl[c].read(d);
                lp[c] = dm > 0 ? lp[c] + (y - lp[c]) * (1.f - dm * 0.9f) : y;
                float out = io[c][i] + f * lp[c];
                dl[c].push(out);
                io[c][i] = out * (1.f - 0.5f * std::fabs(f));
            }
    }
private:
    FracDelay dl[2]; float lp[2] = {};
};

// ------------------------------- Vowel EQ -------------------------------
class VowelFx : public FxModule {
public:
    int morph, shift, q, gain;
    VowelFx() { morph = ix("fx_vowel_morph"); shift = ix("fx_vowel_shift"); q = ix("fx_vowel_q"); gain = ix("fx_vowel_gain"); }
    void reset() override { for (auto& c : bq) for (auto& b : c) b.reset(); }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float g = p.e1[gain];
        if (audit::isRemoved(gain)) g = 0;
        if (g == 0 && p.e0[gain] == 0) return;
        static const float F[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
        float v = clampf(p.e1[morph], 0, 1) * 4; int i0 = std::min(3, (int) v); float t = v - (float) i0;
        float sh = std::exp2((audit::isRemoved(shift) ? 0.f : p.e1[shift]) / 12.f);
        for (int k = 0; k < 3; ++k) {
            float hz = lerp(F[i0][k], F[i0 + 1][k], t) * sh;
            for (int c = 0; c < 2; ++c) bq[c][k].peak(hz, p.e1[q], g * (k == 0 ? 1.f : 0.85f), sr);
        }
        float* io[2] = {L, R};
        for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) { float x = io[c][i]; for (auto& b : bq[c]) x = b.process(x); io[c][i] = x; }
    }
private:
    Biquad bq[2][3];
};

// ------------------------------- 4-band EQ -------------------------------
class EqFx : public FxModule {
public:
    int f[4], g[4], q2, q3;
    EqFx() { for (int i = 0; i < 4; ++i) { f[i] = ix("fx_eq_f" + std::to_string(i + 1)); g[i] = ix("fx_eq_g" + std::to_string(i + 1)); } q2 = ix("fx_eq_q2"); q3 = ix("fx_eq_q3"); }
    void reset() override { for (auto& c : bq) for (auto& b : c) b.reset(); }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float* io[2] = {L, R};
        for (int k = 0; k < 4; ++k) {
            float gain = p.e1[g[k]];
            if (audit::isRemoved(g[k])) gain = 0;
            if (gain == 0 && p.e0[g[k]] == 0) continue;
            for (int c = 0; c < 2; ++c) {
                auto& b = bq[c][k];
                if (k == 0) b.lowShelf(p.e1[f[k]], gain, sr);
                else if (k == 3) b.highShelf(p.e1[f[k]], gain, sr);
                else b.peak(p.e1[f[k]], p.e1[k == 1 ? q2 : q3], gain, sr);
                for (int i = 0; i < n; ++i) io[c][i] = b.process(io[c][i]);
            }
        }
    }
private:
    Biquad bq[2][4];
};

// ------------------------------- Frequency shifter -------------------------------
class FreqShiftFx : public FxModule {
public:
    int hz, fb, stereo;
    FreqShiftFx() { hz = ix("fx_fshift_hz"); fb = ix("fx_fshift_fb"); stereo = ix("fx_fshift_stereo"); }
    void reset() override { for (auto& h : hil) h.reset(); ph[0] = ph[1] = 0; fbv[0] = fbv[1] = 0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float* io[2] = {L, R};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i) {
                float s = lerpP(p, hz, i, n);
                float stv = audit::isRemoved(stereo) ? 0.f : lerpP(p, stereo, i, n);
                if (c == 1) s *= 1.f - 2.f * stv;
                float f = audit::isRemoved(fb) ? 0.f : lerpP(p, fb, i, n);
                float re, im;
                hil[c].process(io[c][i] + f * fbv[c], re, im);
                ph[c] += s / sr; ph[c] -= std::floor(ph[c]);
                float a = kTwoPi * (float) ph[c];
                float y = re * std::cos(a) - im * std::sin(a);
                fbv[c] = y;
                io[c][i] = y;
            }
    }
private:
    Hilbert hil[2]; double ph[2] = {}; float fbv[2] = {};
};

// ------------------------------- Ring mod -------------------------------
class RingFx : public FxModule {
public:
    int freq, wave, key;
    RingFx() { freq = ix("fx_ring_freq"); wave = ix("fx_ring_wave"); key = ix("fx_ring_key"); }
    void reset() override { ph = 0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext& ctx) override {
        const int w = choiceOf(p, wave);
        for (int i = 0; i < n; ++i) {
            float f = lerpP(p, freq, i, n);
            if (on(p, key)) f = ctx.lastNoteHz * f / 200.f;
            ph += f / sr; ph -= std::floor(ph);
            float x = (float) ph, m;
            if (w == 0) m = std::sin(kTwoPi * x);
            else if (w == 1) m = x < 0.25f ? 4 * x : (x < 0.75f ? 2 - 4 * x : 4 * x - 4);
            else m = std::tanh(8.f * std::sin(kTwoPi * x)); // soft square (limited aliasing)
            L[i] *= m; R[i] *= m;
        }
    }
private:
    double ph = 0;
};

// ------------------------------- Bitcrusher -------------------------------
class CrushFx : public FxModule {
public:
    int bits, rate, dither;
    CrushFx() { bits = ix("fx_crush_bits"); rate = ix("fx_crush_rate"); dither = ix("fx_crush_dither"); }
    void reset() override { ph = 1; hold[0] = hold[1] = 0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        const bool dth = on(p, dither);
        for (int i = 0; i < n; ++i) {
            float target = std::min(lerpP(p, rate, i, n), sr);
            ph += target / sr;
            if (ph >= 1) {
                ph -= std::floor(ph);
                float lv = std::exp2(lerpP(p, bits, i, n) - 1.f);
                float in[2] = {L[i], R[i]};
                for (int c = 0; c < 2; ++c) {
                    float x = in[c];
                    if (dth) x += (rng.uni() - rng.uni()) / lv;
                    hold[c] = std::round(clampf(x, -1, 1) * lv) / lv;
                }
            }
            L[i] = hold[0]; R[i] = hold[1];
        }
    }
private:
    double ph = 1; float hold[2] = {}; Rng rng{99};
};

// ------------------------------- Multiband comp (OTT style) -------------------------------
class MbCompFx : public FxModule {
public:
    int depth, time, up, down, xlo, xhi, out;
    MbCompFx() { depth = ix("fx_mbcomp_depth"); time = ix("fx_mbcomp_time"); up = ix("fx_mbcomp_up"); down = ix("fx_mbcomp_down"); xlo = ix("fx_mbcomp_xlo"); xhi = ix("fx_mbcomp_xhi"); out = ix("fx_mbcomp_out"); }
    void reset() override { for (auto& c : ch) c = Chan(); for (auto& e : env) e = 0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float dp0 = p.e0[depth], dp1 = p.e1[depth];
        if (audit::isRemoved(depth)) dp0 = dp1 = 0;
        float* io[2] = {L, R};
        if (dp0 > 0 || dp1 > 0) {
            const float ts = p.e1[time];
            const float atk[3] = {20.f * ts, 10.f * ts, 4.f * ts}, rel[3] = {250.f * ts, 150.f * ts, 80.f * ts};
            float ca[3], cr[3];
            for (int b = 0; b < 3; ++b) { ca[b] = std::exp(-1.f / (atk[b] * 0.001f * sr)); cr[b] = std::exp(-1.f / (rel[b] * 0.001f * sr)); }
            const float gl = Svf::gOf(p.e1[xlo], sr), gh = Svf::gOf(std::max(p.e1[xhi], p.e1[xlo] * 1.1f), sr);
            const float u = audit::isRemoved(up) ? 0.f : p.e1[up], dn = audit::isRemoved(down) ? 0.f : p.e1[down];
            for (int i = 0; i < n; ++i) {
                const float dpt = dp0 + (dp1 - dp0) * (float) i / (float) n;
                float band[2][3];
                for (int c = 0; c < 2; ++c) {
                    float x = io[c][i];
                    float lo = ch[c].a.process(x, gl), rest = x - lo, mid = ch[c].b.process(rest, gh), hi = rest - mid;
                    band[c][0] = lo; band[c][1] = mid; band[c][2] = hi;
                }
                for (int b = 0; b < 3; ++b) {
                    float lvl = std::max(std::fabs(band[0][b]), std::fabs(band[1][b]));
                    env[b] = lvl > env[b] ? lvl + (env[b] - lvl) * ca[b] : lvl + (env[b] - lvl) * cr[b];
                    float edb = gainToDb(std::max(env[b], 1e-6f));
                    float gdb = 0;
                    if (edb > -24.f) gdb -= (edb + 24.f) * (1.f - 1.f / (1.f + dn * 9.f)); // downward, ratio up to 10:1
                    if (edb < -42.f && edb > -90.f) gdb += std::min((-42.f - edb) * u * 0.75f, 24.f); // upward, up to +24 dB
                    float g = dbToGain(gdb * dpt);
                    band[0][b] *= g; band[1][b] *= g;
                }
                for (int c = 0; c < 2; ++c) io[c][i] = band[c][0] + band[c][1] + band[c][2];
            }
        }
        if ((p.e0[out] != 0 || p.e1[out] != 0) && !audit::isRemoved(out)) {
            float a = dbToGain(p.e0[out]), b = dbToGain(p.e1[out]);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] *= a + (b - a) * (float) i / (float) n;
        }
    }
private:
    struct Chan { Lp4 a, b; } ch[2];
    float env[3] = {};
};

// ------------------------------- Compressor -------------------------------
class CompFx : public FxModule {
public:
    int thresh, ratio, attack, release, makeup, clip;
    CompFx() { thresh = ix("fx_comp_thresh"); ratio = ix("fx_comp_ratio"); attack = ix("fx_comp_attack"); release = ix("fx_comp_release"); makeup = ix("fx_comp_makeup"); clip = ix("fx_comp_clip"); }
    void reset() override { env = 0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float r1 = p.e1[ratio];
        if (audit::isRemoved(ratio)) r1 = 1;
        const float ca = std::exp(-1.f / (p.e1[attack] * 0.001f * sr)), cr = std::exp(-1.f / (p.e1[release] * 0.001f * sr));
        const bool doMake = (p.e0[makeup] != 0 || p.e1[makeup] != 0) && !audit::isRemoved(makeup);
        const bool doClip = on(p, clip);
        for (int i = 0; i < n; ++i) {
            float g = 1;
            if (r1 > 1 || p.e0[ratio] > 1) {
                float lvl = std::max(std::fabs(L[i]), std::fabs(R[i]));
                env = lvl > env ? lvl + (env - lvl) * ca : lvl + (env - lvl) * cr;
                float edb = gainToDb(std::max(env, 1e-6f));
                float th = lerpP(p, thresh, i, n), rt = audit::isRemoved(ratio) ? 1.f : lerpP(p, ratio, i, n);
                float over = edb - th;
                // 6 dB soft knee
                float gr = 0;
                if (over > 3) gr = over * (1.f - 1.f / rt);
                else if (over > -3) gr = (over + 3) * (over + 3) / 12.f * (1.f - 1.f / rt);
                g = dbToGain(-gr);
            }
            if (doMake) g *= dbToGain(lerpP(p, makeup, i, n));
            float l = L[i] * g, r = R[i] * g;
            if (doClip) { l = fastTanh(l); r = fastTanh(r); }
            L[i] = l; R[i] = r;
        }
    }
private:
    float env = 0;
};

// ------------------------------- Delay -------------------------------
class DelayFx : public FxModule {
public:
    int time, sync, div, fb, lp, hp, pp;
    DelayFx() { time = ix("fx_delay_time"); sync = ix("fx_delay_sync"); div = ix("fx_delay_div"); fb = ix("fx_delay_fb"); lp = ix("fx_delay_lp"); hp = ix("fx_delay_hp"); pp = ix("fx_delay_pp"); }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); for (auto& d : dl) d.prepare((int) (s * 4.1f)); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); for (auto& f : flp) f.reset(); for (auto& f : fhp) f.reset(); smoothT = -1; }
    double tailSeconds() const override { return 8.0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext& ctx) override {
        float target = p.e1[time] * 0.001f * sr;
        if (on(p, sync)) target = (float) (syncDivisions()[(size_t) std::clamp(choiceOf(p, div), 0, (int) syncDivisions().size() - 1)].beats * 60.0 / ctx.bpm) * sr;
        target = clampf(target, 1.f, sr * 4.f);
        if (smoothT < 0) smoothT = target;
        const float sc = std::exp(-1.f / (0.05f * sr));
        const bool ping = on(p, pp);
        const float glp = Svf::gOf(p.e1[lp], sr), ghp = Svf::gOf(p.e1[hp], sr);
        for (int i = 0; i < n; ++i) {
            smoothT = target + (smoothT - target) * sc;
            const float f = audit::isRemoved(fb) ? 0.f : lerpP(p, fb, i, n);
            float yl = dl[0].read(smoothT), yr = dl[1].read(smoothT);
            float wl = fhp[0].process(flp[0].process(yl, glp, 1.41421356f, 0), ghp, 1.41421356f, 2);
            float wr = fhp[1].process(flp[1].process(yr, glp, 1.41421356f, 0), ghp, 1.41421356f, 2);
            if (ping) { dl[0].push(0.5f * (L[i] + R[i]) + f * wr); dl[1].push(f * wl); }
            else { dl[0].push(L[i] + f * wl); dl[1].push(R[i] + f * wr); }
            L[i] = yl; R[i] = yr;
        }
    }
private:
    FracDelay dl[2]; Svf flp[2], fhp[2]; float smoothT = -1;
};

// ------------------------------- Reverb (8-line FDN) -------------------------------
class ReverbFx : public FxModule {
public:
    int size, decay, pre, damp, duck, width;
    ReverbFx() { size = ix("fx_reverb_size"); decay = ix("fx_reverb_decay"); pre = ix("fx_reverb_pre"); damp = ix("fx_reverb_damp"); duck = ix("fx_reverb_duck"); width = ix("fx_reverb_width"); }
    void prepare(float s, int mb, int l2) override {
        FxModule::prepare(s, mb, l2);
        for (auto& d : lines) d.prepare((int) (s * 0.25f));
        preL.prepare((int) (s * 0.3f)); preR.prepare((int) (s * 0.3f));
        reset();
    }
    void reset() override { for (auto& d : lines) d.reset(); preL.reset(); preR.reset(); for (auto& l : lp) l = 0; env = 0; }
    double tailSeconds() const override { return lastDecay * 1.5 + 0.3; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        static const float base[8] = {0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0533f, 0.0599f, 0.0677f, 0.0731f};
        const float sz = 0.3f + p.e1[size] * 1.7f;
        lastDecay = p.e1[decay];
        float dl[8], gn[8];
        for (int k = 0; k < 8; ++k) { dl[k] = base[k] * sz * sr; gn[k] = std::pow(10.f, -3.f * (dl[k] / sr) / std::max(p.e1[decay], 0.05f)); }
        const float dmp = audit::isRemoved(damp) ? 0.f : p.e1[damp];
        const float dk = std::exp(-kTwoPi * (2000.f + (1.f - dmp) * 16000.f) / sr);
        const float preS = std::max(1.f, p.e1[pre] * 0.001f * sr);
        const float dk2 = audit::isRemoved(duck) ? 0.f : p.e1[duck];
        const float ea = std::exp(-1.f / (0.01f * sr)), er = std::exp(-1.f / (0.25f * sr));
        for (int i = 0; i < n; ++i) {
            const float inL = L[i], inR = R[i];
            float lvl = std::max(std::fabs(inL), std::fabs(inR));
            env = lvl > env ? lvl + (env - lvl) * ea : lvl + (env - lvl) * er;
            preL.push(inL); preR.push(inR);
            const float xl = preL.read(preS), xr = preR.read(preS);
            float o[8];
            for (int k = 0; k < 8; ++k) {
                float v = lines[k].read(dl[k]) * gn[k];
                lp[k] = v + (lp[k] - v) * dk * 0.f + (dmp > 0 ? (lp[k] - v) * dk : 0.f);
                o[k] = dmp > 0 ? lp[k] : v;
            }
            // Householder mix
            float sum = 0; for (float v : o) sum += v; sum *= 0.25f;
            for (int k = 0; k < 8; ++k) lines[k].push(o[k] - sum + ((k & 1) ? xr : xl) * 0.35f);
            float wl = (o[0] + o[2] + o[4] + o[6]) * 0.35f, wr = (o[1] + o[3] + o[5] + o[7]) * 0.35f;
            const float w = lerpP(p, width, i, n);
            float m = 0.5f * (wl + wr), s = 0.5f * (wl - wr) * w;
            float duckG = dk2 > 0 ? 1.f / (1.f + dk2 * 8.f * env) : 1.f;
            L[i] = (m + s) * duckG; R[i] = (m - s) * duckG;
        }
    }
private:
    FracDelay lines[8]; FracDelay preL, preR; float lp[8] = {}; float env = 0; double lastDecay = 2;
};

// ------------------------------- Mod delay -------------------------------
class ModDelayFx : public FxModule {
public:
    int time, rate, depth, fb, bp, q;
    ModDelayFx() { time = ix("fx_moddelay_time"); rate = ix("fx_moddelay_rate"); depth = ix("fx_moddelay_depth"); fb = ix("fx_moddelay_fb"); bp = ix("fx_moddelay_bp"); q = ix("fx_moddelay_q"); }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); for (auto& d : dl) d.prepare((int) (s * 0.11f)); reset(); }
    void reset() override { for (auto& d : dl) d.reset(); for (auto& f : bpf) f.reset(); ph = 0; fbv[0] = fbv[1] = 0; }
    double tailSeconds() const override { return 2.0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float* io[2] = {L, R};
        const double inc = p.e1[rate] / sr;
        const double ph0 = ph;
        for (int c = 0; c < 2; ++c) {
            double lph = ph0 + c * 0.25;
            for (int i = 0; i < n; ++i) {
                lph += inc;
                const float lfo = std::sin(kTwoPi * (float) (lph - std::floor(lph)));
                const float dep = audit::isRemoved(depth) ? 0.f : lerpP(p, depth, i, n);
                const float ms = lerpP(p, time, i, n) * (1.f + dep * 0.9f * lfo);
                const float d = clampf(ms * 0.001f * sr, 1.f, sr * 0.1f);
                const float y = dl[c].read(d);
                const float g = Svf::gOf(lerpP(p, bp, i, n), sr), k = 1.f / std::max(lerpP(p, q, i, n), 0.1f);
                const float fbs = bpf[c].process(y, g, k, 1);
                dl[c].push(io[c][i] + lerpP(p, fb, i, n) * fbs);
                io[c][i] = y;
            }
        }
        ph = ph0 + inc * n; ph -= std::floor(ph);
    }
private:
    FracDelay dl[2]; Svf bpf[2]; double ph = 0; float fbv[2] = {};
};

// ------------------------------- Resonator -------------------------------
class ResonatorFx : public FxModule {
public:
    int freq, key, fifth, decay, bright, sc;
    ResonatorFx() { freq = ix("fx_reso_freq"); key = ix("fx_reso_key"); fifth = ix("fx_reso_fifth"); decay = ix("fx_reso_decay"); bright = ix("fx_reso_bright"); sc = ix("fx_reso_sc"); }
    void reset() override { for (auto& c : st) for (auto& r : c) r.y1 = r.y2 = 0; }
    double tailSeconds() const override { return lastDecay * 1.2; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext& ctx) override {
        const float f0 = on(p, key) ? ctx.lastNoteHz : p.e1[freq];
        const int banks = on(p, fifth) ? 2 : 1;
        lastDecay = p.e1[decay];
        const float r = std::exp(-6.9078f / (std::max(p.e1[decay], 0.01f) * sr));
        float coef[16], gain[16]; int cnt = 0;
        for (int b = 0; b < banks; ++b)
            for (int k = 1; k <= 8; ++k) {
                float hz = f0 * (b ? 1.5f : 1.f) * (float) k;
                if (hz > sr * 0.45f) break;
                coef[cnt] = 2.f * r * std::cos(kTwoPi * hz / sr);
                gain[cnt] = std::pow(0.15f + 0.85f * p.e1[bright], (float) (k - 1)) * (1.f - r) * 4.f;
                ++cnt;
            }
        const bool useSc = ctx.fxPlugin && on(p, sc) && ctx.scL;
        float* io[2] = {L, R};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i) {
                const float x = useSc ? (c ? ctx.scR[i] : ctx.scL[i]) : io[c][i];
                float y = 0;
                for (int k = 0; k < cnt; ++k) {
                    auto& s = st[c][k];
                    float v = coef[k] * s.y1 - r * r * s.y2 + x * gain[k];
                    s.y2 = s.y1; s.y1 = v; y += v;
                }
                io[c][i] = y;
            }
    }
private:
    struct Res { float y1 = 0, y2 = 0; };
    Res st[2][16]; double lastDecay = 0.5;
};

// ------------------------------- Disperser -------------------------------
class DisperserFx : public FxModule {
public:
    int freq, amount, pinch, spread;
    DisperserFx() { freq = ix("fx_disperse_freq"); amount = ix("fx_disperse_amount"); pinch = ix("fx_disperse_pinch"); spread = ix("fx_disperse_spread"); }
    void reset() override { for (auto& c : ap) for (auto& b : c) b.reset(); }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float a = p.e1[amount];
        if (audit::isRemoved(amount)) a = 0;
        if (a <= 0) return;
        const float stagesF = a * 32.f;
        const int full = (int) stagesF; const float frac = stagesF - (float) full;
        const int total = std::min(32, full + (frac > 0 ? 1 : 0));
        const float spr = audit::isRemoved(spread) ? 0.f : p.e1[spread];
        for (int k = 0; k < total; ++k) {
            float o = total > 1 ? ((float) k / (total - 1) * 2.f - 1.f) * spr * 2.f : 0.f;
            for (int c = 0; c < 2; ++c) ap[c][k].allpass(clampf(p.e1[freq] * std::exp2(o), 20, sr * 0.45f), p.e1[pinch], sr);
        }
        float* io[2] = {L, R};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i) {
                float x = io[c][i];
                for (int k = 0; k < full; ++k) x = ap[c][k].process(x);
                if (frac > 0 && full < 32) { float y = ap[c][full].process(x); x = x + (y - x) * frac; }
                io[c][i] = x;
            }
    }
private:
    Biquad ap[2][32];
};

// ------------------------------- Spectral shaper / gate -------------------------------
class SpectralFx : public FxModule {
public:
    int thresh, tilt, contrast, smoothP;
    SpectralFx() { thresh = ix("fx_spectral_thresh"); tilt = ix("fx_spectral_tilt"); contrast = ix("fx_spectral_contrast"); smoothP = ix("fx_spectral_smooth"); }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); for (auto& x : stft) x.prepare(); for (auto& d : dl) d.prepare(Stft::N + 8); gate.assign((size_t) Stft::N / 2 + 1, 1.f); reset(); }
    void reset() override { for (auto& x : stft) x.reset(); for (auto& d : dl) d.reset(); std::fill(gate.begin(), gate.end(), 1.f); }
    int latency() const override { return Stft::N; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float th = p.e1[thresh], tl = p.e1[tilt], ct = p.e1[contrast];
        if (audit::isRemoved(thresh)) th = -100; if (audit::isRemoved(tilt)) tl = 0; if (audit::isRemoved(contrast)) ct = 0;
        const bool neutral = th <= -100 && tl == 0 && ct == 0 && p.e0[thresh] <= -100 && p.e0[tilt] == 0 && p.e0[contrast] == 0;
        const float thLin = dbToGain(th) * (float) Stft::N * 0.25f;
        const float hopSec = (float) Stft::Hop / sr;
        const float sm = std::exp(-hopSec / std::max(p.e1[smoothP] * 0.001f, 1e-4f));
        float* io[2] = {L, R};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i) {
                float x = io[c][i];
                float d = dl[c].process(x, Stft::N);
                float y = stft[c].process(x, [&](float* re, float* im) {
                    const int nb = Stft::N / 2;
                    float envAcc = 0;
                    for (int k = 1; k <= nb; ++k) {
                        float mag = std::sqrt(re[k] * re[k] + im[k] * im[k]);
                        float g = 1;
                        if (th > -100) { float target = mag > thLin ? 1.f : 0.f; float& gs = gate[(size_t) k]; gs = target + (gs - target) * sm; g *= gs; }
                        if (tl != 0) { float hz = (float) k * sr / Stft::N; g *= dbToGain(tl * std::log2(std::max(hz, 20.f) / 1000.f)); }
                        if (ct > 0) {
                            envAcc = envAcc * 0.9f + mag * 0.1f;
                            float rel = mag / (envAcc + 1e-9f);
                            g *= std::pow(std::max(rel, 1e-3f), ct * 1.5f);
                        }
                        re[k] *= g; im[k] *= g;
                    }
                    re[0] = 0; im[0] = 0;
                });
                io[c][i] = neutral ? d : y;
            }
    }
private:
    Stft stft[2]; IntDelay dl[2]; std::vector<float> gate;
};

// ------------------------------- Resonance tamer -------------------------------
class TamerFx : public FxModule {
public:
    int depth, sens, speed, lo, hi;
    TamerFx() { depth = ix("fx_tamer_depth"); sens = ix("fx_tamer_sens"); speed = ix("fx_tamer_speed"); lo = ix("fx_tamer_lo"); hi = ix("fx_tamer_hi"); }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); for (auto& x : stft) x.prepare(); for (auto& d : dl) d.prepare(Stft::N + 8); for (auto& g : gains) g.assign((size_t) Stft::N / 2 + 1, 1.f); mags.assign((size_t) Stft::N / 2 + 1, 0.f); reset(); }
    void reset() override { for (auto& x : stft) x.reset(); for (auto& d : dl) d.reset(); for (auto& g : gains) std::fill(g.begin(), g.end(), 1.f); }
    int latency() const override { return Stft::N; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        float dp = p.e1[depth];
        if (audit::isRemoved(depth)) dp = 0;
        const bool neutral = dp <= 0 && p.e0[depth] <= 0;
        const float thr = 1.5f + (1.f - p.e1[sens]) * 6.f; // peak/envelope ratio above which we cut
        const float hopSec = (float) Stft::Hop / sr;
        const float sm = std::exp(-hopSec / std::max(p.e1[speed] * 0.001f, 1e-4f));
        const float flo = p.e1[lo], fhi = std::max(p.e1[hi], p.e1[lo]);
        float* io[2] = {L, R};
        for (int c = 0; c < 2; ++c) {
            auto& G = gains[(size_t) c];
            for (int i = 0; i < n; ++i) {
                float x = io[c][i];
                float d = dl[c].process(x, Stft::N);
                float y = stft[c].process(x, [&](float* re, float* im) {
                    const int nb = Stft::N / 2;
                    for (int k = 0; k <= nb; ++k) mags[(size_t) k] = std::sqrt(re[k] * re[k] + im[k] * im[k]);
                    for (int k = 1; k < nb; ++k) {
                        float hz = (float) k * sr / Stft::N;
                        float target = 1;
                        if (hz >= flo && hz <= fhi) {
                            int w = std::max(2, k / 6); // ~1/3 octave smoothing window
                            float acc = 0; int cnt = 0;
                            for (int j = std::max(1, k - w); j <= std::min(nb, k + w); ++j) { acc += mags[(size_t) j]; ++cnt; }
                            float env = acc / (float) cnt + 1e-9f;
                            float ratio = mags[(size_t) k] / env;
                            if (ratio > thr) target = std::pow(thr / ratio, dp);
                        }
                        float& g = G[(size_t) k];
                        g = target + (g - target) * sm;
                        re[k] *= g; im[k] *= g;
                    }
                });
                io[c][i] = neutral ? d : y;
            }
        }
    }
private:
    Stft stft[2]; IntDelay dl[2]; std::vector<float> gains[2]; std::vector<float> mags;
};

// ------------------------------- 3-band split container -------------------------------
class SplitFx : public FxModule {
public:
    int xlo, xhi;
    struct BandIx { int gain, fltOn, fltType, fltCut, fltRes, phOn, phRate, phDepth, phFb, distOn, distType, distDrive, combOn, combFreq, combFb, crushOn, crushBits; } bi[3];
    SplitFx() {
        xlo = ix("fx_split_xlo"); xhi = ix("fx_split_xhi");
        for (int b = 0; b < 3; ++b) {
            std::string p = "fx_split_b" + std::to_string(b + 1) + "_";
            bi[b] = {ix(p + "gain"), ix(p + "flt_on"), ix(p + "flt_type"), ix(p + "flt_cut"), ix(p + "flt_res"), ix(p + "ph_on"), ix(p + "ph_rate"), ix(p + "ph_depth"), ix(p + "ph_fb"),
                     ix(p + "dist_on"), ix(p + "dist_type"), ix(p + "dist_drive"), ix(p + "comb_on"), ix(p + "comb_freq"), ix(p + "comb_fb"), ix(p + "crush_on"), ix(p + "crush_bits")};
        }
    }
    void prepare(float s, int mb, int l2) override { FxModule::prepare(s, mb, l2); for (auto& c : ch) for (auto& b : c.band) b.comb.prepare((int) (s / 18.f)); reset(); }
    void reset() override { for (auto& c : ch) { c.a.reset(); c.b.reset(); for (auto& b : c.band) { b.flt.reset(); std::fill(std::begin(b.ph), std::end(b.ph), 0.f); b.phFb = 0; b.ds.reset(); b.comb.reset(); } } for (auto& p : phPh) p = 0; }
    double tailSeconds() const override { return 1.0; }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext& ctx) override {
        bool any = false;
        for (int b = 0; b < 3; ++b) {
            const auto& B = bi[b];
            any |= p.e0[B.gain] != 1 || p.e1[B.gain] != 1 || on(p, B.fltOn) || on(p, B.phOn) || (on(p, B.distOn) && p.e1[B.distDrive] > 0) || on(p, B.combOn) || on(p, B.crushOn);
        }
        if (!any) return;
        const float gl = Svf::gOf(p.e1[xlo], sr), gh = Svf::gOf(std::max(p.e1[xhi], p.e1[xlo] * 1.1f), sr);
        const auto& order = ctx.st->splitOrder;
        float* io[2] = {L, R};
        for (int c = 0; c < 2; ++c) {
            auto& C = ch[c];
            for (int i = 0; i < n; ++i) {
                const float x = io[c][i];
                float low = C.a.process(x, gl), rest = x - low, mid = C.b.process(rest, gh), high = rest - mid;
                float bands[3] = {low, mid, high};
                float sum = 0;
                for (int b = 0; b < 3; ++b) {
                    const auto& B = bi[b]; auto& S = C.band[b];
                    float y = bands[b];
                    for (int m : order[(size_t) b]) {
                        switch (m) {
                            case 0: if (on(p, B.fltOn)) { float g = Svf::gOf(lerpP(p, B.fltCut, i, n), sr), k = 2.f - 1.9f * lerpP(p, B.fltRes, i, n); y = S.flt.process(y, g, k, choiceOf(p, B.fltType)); } break;
                            case 1: if (on(p, B.phOn)) {
                                double& ph = phPh[(size_t) (b * 2 + c)];
                                ph += lerpP(p, B.phRate, i, n) / sr; ph -= std::floor(ph);
                                float hz = 600.f * std::exp2(std::sin(kTwoPi * (float) ph) * lerpP(p, B.phDepth, i, n) * 2.f);
                                float g = Svf::gOf(hz, sr), G = g / (1 + g);
                                float v = y + lerpP(p, B.phFb, i, n) * S.phFb;
                                for (int k = 0; k < 4; ++k) { float w = (v - S.ph[k]) * G; float l = w + S.ph[k]; S.ph[k] = l + w; v = 2 * l - v; }
                                S.phFb = v; y = 0.5f * (y + v);
                            } break;
                            case 2: if (on(p, B.distOn)) { float d = audit::isRemoved(B.distDrive) ? 0.f : lerpP(p, B.distDrive, i, n); if (d > 0) { float s = distortSample(choiceOf(p, B.distType), y, d, 0, S.ds, sr, ctx.draw, 0); y = y + (s - y) * driveBlend(d); } } break;
                            case 3: if (on(p, B.combOn)) { float dly = clampf(sr / lerpP(p, B.combFreq, i, n), 2.f, (float) S.comb.mask - 4); float o = y + lerpP(p, B.combFb, i, n) * S.comb.read(dly); S.comb.push(o); y = o * (1.f - 0.5f * std::fabs(lerpP(p, B.combFb, i, n))); } break;
                            case 4: if (on(p, B.crushOn)) { float lv = std::exp2(lerpP(p, B.crushBits, i, n) - 1.f); y = std::round(clampf(y, -1, 1) * lv) / lv; } break;
                            default: break;
                        }
                    }
                    float g = audit::isRemoved(B.gain) ? 1.f : lerpP(p, B.gain, i, n);
                    sum += y * g;
                }
                io[c][i] = sum;
            }
        }
    }
private:
    struct Band { Svf flt; float ph[4] = {}; float phFb = 0; DistState ds; FracDelay comb; };
    struct Chan { Lp4 a, b; Band band[3]; } ch[2];
    std::array<double, 6> phPh{};
};

// ------------------------------- Vocoder (GNARL FX) -------------------------------
class VocoderFx : public FxModule {
public:
    int bands, attack, release, shift, hf;
    VocoderFx() { bands = ix("fx_vocoder_bands"); attack = ix("fx_vocoder_attack"); release = ix("fx_vocoder_release"); shift = ix("fx_vocoder_shift"); hf = ix("fx_vocoder_hf"); }
    void reset() override { for (auto& b : car) for (auto& f : b) f.reset(); for (auto& f : mod) f.reset(); for (auto& e : env) e.reset(); hpM.reset(); }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext& ctx) override {
        const int nb = std::clamp(choiceOf(p, bands), 8, 32);
        const float sh = std::exp2((audit::isRemoved(shift) ? 0.f : p.e1[shift]) / 12.f);
        const float q = 4.f + nb * 0.35f, k = 1.f / q;
        float gM[32], gC[32];
        for (int b = 0; b < nb; ++b) {
            float hz = 80.f * std::pow(100.f, (float) b / (nb - 1)); // 80 Hz .. 8 kHz
            gM[b] = Svf::gOf(hz, sr); gC[b] = Svf::gOf(clampf(hz * sh, 20, sr * 0.45f), sr);
            env[b].set(p.e1[attack], p.e1[release], sr);
        }
        const float hfAmt = audit::isRemoved(hf) ? 0.f : p.e1[hf];
        const float ghp = Svf::gOf(8000.f, sr);
        for (int i = 0; i < n; ++i) {
            const float m = ctx.scL ? 0.5f * (ctx.scL[i] + ctx.scR[i]) : 0.f;
            float outL = 0, outR = 0;
            for (int b = 0; b < nb; ++b) {
                float e = env[b].process(mod[b].process(m, gM[b], k, 1));
                outL += car[0][b].process(L[i], gC[b], k, 1) * e;
                outR += car[1][b].process(R[i], gC[b], k, 1) * e;
            }
            const float gain = 6.f * std::sqrt((float) nb / 16.f);
            float hfp = hfAmt > 0 ? hpM.process(m, ghp, 1.41421356f, 2) * hfAmt : 0.f;
            L[i] = outL * gain + hfp; R[i] = outR * gain + hfp;
        }
    }
private:
    Svf car[2][32], mod[32], hpM; EnvFollow env[32];
};

// ------------------------------- Width -------------------------------
class WidthFx : public FxModule {
public:
    int amount, monoOn, monoHz;
    WidthFx() { amount = ix("fx_width_amount"); monoOn = ix("fx_width_mono_on"); monoHz = ix("fx_width_mono_hz"); }
    void reset() override { hp.reset(); }
    void processWet(float* L, float* R, int n, const FxParams& p, FxContext&) override {
        const bool mono = on(p, monoOn) && !audit::isRemoved(monoOn);
        const bool w = (p.e0[amount] != 1 || p.e1[amount] != 1) && !audit::isRemoved(amount);
        if (!mono && !w) return;
        for (int i = 0; i < n; ++i) {
            float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]);
            if (w) s *= lerpP(p, amount, i, n);
            if (mono) s = hp.process(s, Svf::gOf(lerpP(p, monoHz, i, n), sr)); // side high-passed: truly mono below the crossover
            L[i] = m + s; R[i] = m - s;
        }
    }
private:
    Hp4 hp;
};

} // namespace

// =============================== chain ===============================
FxChain::FxChain(bool fxPlugin) : fxp(fxPlugin) {
    const auto& ids = fxModuleIds();
    for (const auto& id : ids) {
        std::unique_ptr<FxModule> m;
        if (id == "dist1" || id == "dist2") m = std::make_unique<DistFx>(id);
        else if (id == "mbd") m = std::make_unique<MbDistFx>();
        else if (id == "phaser") m = std::make_unique<PhaserFx>();
        else if (id == "flanger") m = std::make_unique<FlangerFx>();
        else if (id == "chorus") m = std::make_unique<ChorusFx>();
        else if (id == "comb") m = std::make_unique<CombFx>();
        else if (id == "vowel") m = std::make_unique<VowelFx>();
        else if (id == "eq") m = std::make_unique<EqFx>();
        else if (id == "fshift") m = std::make_unique<FreqShiftFx>();
        else if (id == "ring") m = std::make_unique<RingFx>();
        else if (id == "crush") m = std::make_unique<CrushFx>();
        else if (id == "mbcomp") m = std::make_unique<MbCompFx>();
        else if (id == "comp") m = std::make_unique<CompFx>();
        else if (id == "delay") m = std::make_unique<DelayFx>();
        else if (id == "reverb") m = std::make_unique<ReverbFx>();
        else if (id == "moddelay") m = std::make_unique<ModDelayFx>();
        else if (id == "reso") m = std::make_unique<ResonatorFx>();
        else if (id == "disperse") m = std::make_unique<DisperserFx>();
        else if (id == "spectral") m = std::make_unique<SpectralFx>();
        else if (id == "tamer") m = std::make_unique<TamerFx>();
        else if (id == "split") m = std::make_unique<SplitFx>();
        else if (id == "vocoder") m = std::make_unique<VocoderFx>();
        else if (id == "width") m = std::make_unique<WidthFx>();
        m->init(ix("fx_" + id + "_on"), ix("fx_" + id + "_mix"));
        mods.push_back(std::move(m));
    }
}
FxChain::~FxChain() = default;

void FxChain::prepare(float sampleRate, int maxBlock) { for (auto& m : mods) m->prepare(sampleRate, maxBlock, 3); }
void FxChain::reset() { for (auto& m : mods) m->reset(); }

void FxChain::process(float* L, float* R, int n, const float* e0, const float* e1, FxContext& ctx) {
    FxParams p{e0, e1};
    static const int vocoderIndex = 22;
    for (int s = 0; s < kNumFxSlots; ++s) {
        const int m = ctx.st->fxSlots[(size_t) s];
        if (m < 0 || m >= (int) mods.size()) continue;
        if (m == vocoderIndex && !fxp) continue; // the vocoder needs the GNARL FX sidechain
        auto& mod = *mods[(size_t) m];
        if (!mod.isOn(e1)) continue;
        mod.run(L, R, n, p, ctx);
    }
}

int FxChain::latency(const float* e, const PatchStructure* st) const {
    if (!st) return 0;
    int lat = 0;
    for (int s = 0; s < kNumFxSlots; ++s) {
        const int m = st->fxSlots[(size_t) s];
        if (m < 0 || m >= (int) mods.size() || !mods[(size_t) m]->isOn(e)) continue;
        lat += mods[(size_t) m]->latency();
    }
    return lat;
}

double FxChain::tail(const float* e, const PatchStructure* st) const {
    if (!st) return 0;
    double t = 0;
    for (int s = 0; s < kNumFxSlots; ++s) {
        const int m = st->fxSlots[(size_t) s];
        if (m < 0 || m >= (int) mods.size() || !mods[(size_t) m]->isOn(e)) continue;
        t += mods[(size_t) m]->tailSeconds();
    }
    return t;
}

// =============================== final stage ===============================
void FinalStage::prepare(float s, int) { sr = s; reset(); }
void FinalStage::reset() { gr = 1; env = 0; }
void FinalStage::process(float* L, float* R, int n, bool onFlag, int mode, float c0, float c1, float d0, float d1) {
    if (!onFlag) {
        // catastrophe guard only: non-finite -> 0, hard bound at +12 dBFS
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(L[i])) L[i] = 0; if (!std::isfinite(R[i])) R[i] = 0;
            L[i] = clampf(L[i], -4.f, 4.f); R[i] = clampf(R[i], -4.f, 4.f);
        }
        return;
    }
    const float rel = std::exp(-1.f / (0.05f * sr));
    for (int i = 0; i < n; ++i) {
        const float t = (float) i / (float) n;
        const float ceil = dbToGain(c0 + (c1 - c0) * t);
        const float drv = (d0 == 0 && d1 == 0) ? 1.f : dbToGain(d0 + (d1 - d0) * t);
        float l = std::isfinite(L[i]) ? L[i] * drv : 0.f, r = std::isfinite(R[i]) ? R[i] * drv : 0.f;
        switch (mode) {
            default:
            case 0: { // soft clip: linear below half the ceiling, tanh knee above
                auto sc = [&](float x) { float a = std::fabs(x), k = 0.5f * ceil; if (a <= k) return x; float y = k + (ceil - k) * fastTanh((a - k) / (ceil - k)); return x < 0 ? -y : y; };
                l = sc(l); r = sc(r); break;
            }
            case 1: l = clampf(l, -ceil, ceil); r = clampf(r, -ceil, ceil); break;
            case 2: { // fast peak limiter + clip
                float pk = std::max(std::fabs(l), std::fabs(r));
                float target = pk > ceil ? ceil / pk : 1.f;
                gr = target < gr ? target : target + (gr - target) * rel;
                l = clampf(l * gr, -ceil, ceil); r = clampf(r * gr, -ceil, ceil); break;
            }
        }
        L[i] = l; R[i] = r;
    }
}

} // namespace gnarl
