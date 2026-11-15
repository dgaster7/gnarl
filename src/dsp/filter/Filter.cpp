#include "Filter.h"
#include <complex>
#include <cstring>

namespace gnarl {

namespace {
constexpr float kMinHz = 8.f;

float qFromReso(float reso, bool boost) {
    reso = clampf(reso, 0.f, 1.f);
    return boost ? 0.5f * std::pow(500.f, reso) : 0.5f * std::pow(50.f, reso);
}
float gFor(float hz, float rate) {
    hz = clampf(hz, kMinHz, rate * 0.49f);
    return std::tan(kPi * hz / rate);
}
SvfSection lp(float hz, float q, float rate, float gain = 1) { return {gFor(hz, rate), 1.f / q, 0, 0, gain}; }
SvfSection bp(float hz, float q, float rate, float gain = 1) { float k = 1.f / q; return {gFor(hz, rate), k, 0, k * gain, 0}; } // unity peak
SvfSection hp(float hz, float q, float rate) { float k = 1.f / q; return {gFor(hz, rate), k, 1, -k, -1}; }
SvfSection notch(float hz, float q, float rate, float depth = 1) { float k = 1.f / q; return {gFor(hz, rate), k, 1, -k * depth, 0}; }
SvfSection allpass(float hz, float q, float rate) { float k = 1.f / q; return {gFor(hz, rate), k, 1, -2 * k, 0}; }

// Male vowel formants (Peterson & Barney averages): F1, F2, F3 (Hz) and relative gains (dB).
const float kVowelF[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
const float kVowelG[5][3] = {{0, -5, -15}, {0, -8, -12}, {0, -12, -16}, {0, -3, -20}, {0, -10, -22}};
} // namespace

FilterPlan makeFilterPlan(const FilterSettings& st, float rate, float resoScale) {
    FilterPlan p;
    p.type = st.type;
    const float fc = clampf(st.cutoff, kMinHz, rate * 0.49f);
    const float q = std::max(0.5f, qFromReso(st.reso, st.boost) * resoScale);
    auto add = [&](SvfSection s) { if (p.num < FilterPlan::kMax) p.s[(size_t) p.num++] = s; };
    const float sepRatio = std::exp2(clampf(st.sep, 0, 1) * 2.f); // 1 .. 4 (0 .. 2 octaves)
    switch (st.type) {
        case FT_Off: break;
        case FT_LP12: add(lp(fc, q, rate)); break;
        case FT_LP24: add(lp(fc, 0.5412f, rate)); add(lp(fc, 1.3066f + (q - 0.5f), rate)); break;
        case FT_BP12: add(bp(fc, q, rate)); break;
        case FT_BP24: add(bp(fc, q, rate)); add(bp(fc, q, rate)); break;
        case FT_HP12: add(hp(fc, q, rate)); break;
        case FT_HP24: add(hp(fc, 0.5412f, rate)); add(hp(fc, 1.3066f + (q - 0.5f), rate)); break;
        case FT_Notch12: add(notch(fc, std::max(0.5f, q * 0.5f), rate)); break;
        case FT_Notch24: add(notch(fc, std::max(0.5f, q * 0.5f), rate)); add(notch(fc, std::max(0.5f, q * 0.5f), rate)); break;
        case FT_SepLP24: {
            float r = std::sqrt(sepRatio);
            add(lp(fc / r, q, rate)); add(lp(fc * r, q, rate));
            break;
        }
        case FT_SepLP36: add(lp(fc / sepRatio, q, rate)); add(lp(fc, q, rate)); add(lp(fc * sepRatio, q, rate)); break;
        case FT_TriPeak: {
            // Raised shelf, dominant centre resonance, two shoulder peaks, sharp dips outside the shoulders.
            const float s = std::exp2(0.15f + clampf(st.sep, 0, 1) * 1.35f);
            const float qc = std::max(1.f, q * 1.4f), qs = std::max(0.9f, q);
            const float gc = 0.5f + st.reso * 7.f;
            const float gs = clampf(st.shoulder, 0, 1) * gc * 0.75f;
            const float shelfGain = clampf(st.shelf, 0, 1) * 2.5f;
            p.numParallel = 5;
            add(lp(fc * s * 1.15f, 0.6f, rate));           // body
            add(lp(fc / (s * 1.8f), 0.7f, rate, shelfGain)); // raised shelf below
            add(bp(fc, qc, rate, gc));                       // centre peak
            add(bp(fc / s, qs, rate, gs));                   // lower shoulder
            add(bp(fc * s, qs, rate, gs));                   // upper shoulder
            const float ds = std::pow(s, 1.65f);
            add(notch(fc / ds, 7.f, rate, 0.94f));           // dips just outside the shoulders
            add(notch(fc * ds, 7.f, rate, 0.94f));
            break;
        }
        case FT_DualResoBP:
        case FT_DualResoBR: {
            float f1, f2;
            if (st.opposing) {
                // peaks move in opposite directions as cutoff moves (pivot 1 kHz)
                f1 = fc / std::sqrt(sepRatio);
                f2 = (1000.f * 1000.f / fc) * std::sqrt(sepRatio);
            } else {
                f1 = fc / std::sqrt(sepRatio);
                f2 = fc * std::sqrt(sepRatio);
            }
            if (st.type == FT_DualResoBP) {
                p.numParallel = 2;
                add(bp(f1, q, rate, 1.f)); add(bp(f2, q, rate, 1.f));
            } else {
                add(notch(f1, std::max(0.5f, q * 0.5f), rate)); add(notch(f2, std::max(0.5f, q * 0.5f), rate));
            }
            break;
        }
        case FT_Morph: {
            // continuous LP > BP > HP > Notch via SVF output mixing
            const float m = clampf(st.morph, 0, 1) * 3.f;
            const float k = 1.f / q;
            // LP (0,0,1)  BP (0,k,0)  HP (1,-k,-1)  Notch (1,-k,0)
            const float LPm[3] = {0, 0, 1}, BPm[3] = {0, k, 0}, HPm[3] = {1, -k, -1}, NTm[3] = {1, -k, 0};
            const float* a; const float* b; float t;
            if (m < 1) { a = LPm; b = BPm; t = m; } else if (m < 2) { a = BPm; b = HPm; t = m - 1; } else { a = HPm; b = NTm; t = m - 2; }
            add({gFor(fc, rate), k, lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t)});
            break;
        }
        case FT_Formant: {
            const float vpos = clampf(st.morph, 0, 1) * 4.f;
            const int i0 = std::min(3, (int) vpos);
            const float t = vpos - (float) i0;
            const float shift = std::sqrt(fc / 1000.f) * std::exp2(clampf(st.gender, -1, 1) * 0.5f);
            p.numParallel = 3;
            for (int f = 0; f < 3; ++f) {
                float hz = lerp(kVowelF[i0][f], kVowelF[i0 + 1][f], t) * shift;
                float gdb = lerp(kVowelG[i0][f], kVowelG[i0 + 1][f], t);
                float fq = clampf(hz / 90.f, 2.f, 30.f) * (0.6f + st.reso * 1.6f);
                add(bp(hz, fq, rate, dbToGain(gdb) * 1.5f));
            }
            break;
        }
        case FT_CombPlus:
        case FT_CombMinus:
            p.combDelay = clampf(rate / fc, 2.f, 1e9f);
            p.combFb = clampf(st.reso, 0, 1) * (st.boost ? 0.999f : 0.97f) * (st.type == FT_CombMinus ? -1.f : 1.f);
            p.combDamp = 0.15f;
            break;
        case FT_Ladder: {
            p.ladderG = gFor(fc, rate);
            float k = clampf(st.reso, 0, 1) * (st.boost ? 4.6f : 4.0f) * resoScale;
            p.ladderK = k;
            p.ladderPoles = std::clamp(st.poles, 1, 4);
            break;
        }
        case FT_Phaser: {
            p.phaserStages = std::clamp(st.stages, 4, 12);
            const float spread = 0.15f + clampf(st.sep, 0, 1) * 0.6f; // octaves between stages
            for (int i = 0; i < p.phaserStages; ++i) {
                float o = ((float) i - (p.phaserStages - 1) * 0.5f) * spread;
                p.phaserG[(size_t) i] = gFor(fc * std::exp2(o), rate);
            }
            p.phaserFb = clampf(st.reso, 0, 1) * 0.92f;
            break;
        }
        default: break;
    }
    return p;
}

float filterMagnitude(const FilterSettings& st, float freq, float rate) {
    using C = std::complex<double>;
    FilterPlan p = makeFilterPlan(st, rate);
    const double wd = 2 * M_PI * freq / rate;
    const C z1 = std::polar(1.0, -wd);
    const double t = std::tan(M_PI * std::min((double) freq, rate * 0.4999) / rate);
    auto sec = [&](const SvfSection& s) {
        C sn(0, t / s.g);
        C D = sn * sn + (double) s.k * sn + 1.0;
        return C(s.m0) + C(s.m1) * sn / D + C(s.m2) / D;
    };
    C H(1, 0);
    switch (p.type) {
        case FT_Off: return 1;
        case FT_CombPlus: case FT_CombMinus: {
            C zD = std::polar(1.0, -wd * p.combDelay);
            H = 1.0 / (1.0 - (double) p.combFb * zD) * (1.0 - 0.5 * std::fabs(p.combFb));
            break;
        }
        case FT_Ladder: {
            C sn(0, t / p.ladderG);
            C h1 = 1.0 / (1.0 + sn);
            C h4 = h1 * h1 * h1 * h1;
            C hp = std::pow(h1, p.ladderPoles);
            H = hp * (1.0 + 0.5 * p.ladderK) / (1.0 + (double) p.ladderK * h4 * z1);
            break;
        }
        case FT_Phaser: {
            C A(1, 0);
            for (int i = 0; i < p.phaserStages; ++i) { C sn(0, t / p.phaserG[(size_t) i]); A *= (1.0 - sn) / (1.0 + sn); }
            H = 0.5 * (1.0 + A / (1.0 - (double) p.phaserFb * A * z1));
            break;
        }
        default: {
            C par(0, 0);
            int i = 0;
            if (p.numParallel > 0) { for (; i < p.numParallel; ++i) par += sec(p.s[(size_t) i]); H = par; }
            for (; i < p.num; ++i) H *= sec(p.s[(size_t) i]);
        }
    }
    return (float) std::abs(H);
}

void FilterUnit::prepare(float rate, int maxComb) {
    sr = rate;
    int sz = 1; while (sz < maxComb + 8) sz <<= 1;
    combMask = sz - 1;
    for (auto& c : ch) { c.comb.assign((size_t) sz, 0.f); c.pre.dc.setRate(rate); c.post.dc.setRate(rate); }
    reset();
}

void FilterUnit::reset() {
    for (auto& c : ch) {
        c.ic1.fill(0); c.ic2.fill(0);
        std::memset(c.lad, 0, sizeof c.lad); c.ladOut = 0;
        std::fill(c.comb.begin(), c.comb.end(), 0.f); c.combPos = 0; c.combLp = 0;
        c.ph.fill(0); c.phFb = 0; c.pre.reset(); c.post.reset(); c.limEnv = 0;
    }
    limitGain = 1;
    rampLeft = 0;
    cur = tgt = makeFilterPlan(settings, sr);
}

void FilterUnit::setTarget(const FilterSettings& s, int n) {
    // resonance limiter: reduce Q when the filter output runs hot
    float resoScale = 1;
    if (s.limiter > 0) {
        float env = std::max(ch[0].limEnv, ch[1].limEnv);
        resoScale = 1.f / (1.f + std::max(0.f, env - 0.5f) * 6.f);
    }
    const bool structural = s.type != settings.type || s.poles != settings.poles || s.stages != settings.stages;
    settings = s;
    FilterPlan np = makeFilterPlan(s, sr, resoScale);
    if (structural || np.num != tgt.num || np.numParallel != tgt.numParallel) {
        if (s.type != cur.type) for (auto& c : ch) { c.ic1.fill(0); c.ic2.fill(0); c.ph.fill(0); c.phFb = 0; std::memset(c.lad, 0, sizeof c.lad); c.ladOut = 0; }
        cur = np; tgt = np; rampLeft = 0;
        return;
    }
    cur = tgt;           // finish any previous ramp exactly
    tgt = np;
    rampLeft = std::max(1, n);
}

float FilterUnit::processSample(ChanState& c, float x, const FilterPlan& p) {
    switch (p.type) {
        case FT_Off: return x;
        case FT_CombPlus: case FT_CombMinus: {
            float d = p.combDelay;
            float rp = (float) c.combPos - d;
            int i0 = (int) std::floor(rp);
            float f = rp - (float) i0;
            // 4-point Hermite read
            auto at = [&](int k) { return c.comb[(size_t) ((i0 + k) & combMask)]; };
            float xm1 = at(-1), x0 = at(0), x1 = at(1), x2 = at(2);
            float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            float del = ((c3 * f + c2) * f + c1) * f + x0;
            c.combLp += (del - c.combLp) * (1.f - p.combDamp);
            float y = x + p.combFb * c.combLp;
            c.comb[(size_t) (c.combPos & combMask)] = y;
            c.combPos = (c.combPos + 1) & combMask;
            return y * (1.f - 0.5f * std::fabs(p.combFb));
        }
        case FT_Ladder: {
            const float G = p.ladderG / (1.f + p.ladderG);
            float u = fastTanh(x * (1.f + 0.5f * p.ladderK) - p.ladderK * c.ladOut);
            float y = u;
            float outs[4];
            for (int i = 0; i < 4; ++i) {
                float v = (y - c.lad[i]) * G;
                float o = v + c.lad[i];
                c.lad[i] = o + v;
                y = o; outs[i] = o;
            }
            c.ladOut = outs[3];
            return outs[p.ladderPoles - 1];
        }
        case FT_Phaser: {
            float y = x + p.phaserFb * c.phFb;
            for (int i = 0; i < p.phaserStages; ++i) {
                const float G = p.phaserG[(size_t) i] / (1.f + p.phaserG[(size_t) i]);
                float v = (y - c.ph[(size_t) i]) * G;
                float l = v + c.ph[(size_t) i];
                c.ph[(size_t) i] = l + v;
                y = 2.f * l - y; // first-order all-pass
            }
            c.phFb = y;
            return 0.5f * (x + y);
        }
        default: {
            auto run = [&](int i, float in) {
                const SvfSection& s = p.s[(size_t) i];
                const float a1 = 1.f / (1.f + s.g * (s.g + s.k)), a2 = s.g * a1, a3 = s.g * a2;
                float v3 = in - c.ic2[(size_t) i];
                float v1 = a1 * c.ic1[(size_t) i] + a2 * v3;
                float v2 = c.ic2[(size_t) i] + a2 * c.ic1[(size_t) i] + a3 * v3;
                c.ic1[(size_t) i] = 2.f * v1 - c.ic1[(size_t) i];
                c.ic2[(size_t) i] = 2.f * v2 - c.ic2[(size_t) i];
                return s.m0 * in + s.m1 * v1 + s.m2 * v2;
            };
            int i = 0;
            float y = x;
            if (p.numParallel > 0) {
                float acc = 0;
                for (; i < p.numParallel; ++i) acc += run(i, x);
                y = acc;
            }
            for (; i < p.num; ++i) y = run(i, y);
            return y;
        }
    }
}

void FilterUnit::process(float* L, float* R, int n, const DrawTables& draw) {
    if (cur.type == FT_Off && tgt.type == FT_Off) return;
    float* io[2] = {L, R};
    const bool drive = settings.drive > 0;
    const float blend = driveBlend(settings.drive);
    const bool dc = distNeedsDcBlock(settings.driveType, 0);
    const float limCoef = settings.limiter == 2 ? std::exp(-1.f / (0.005f * sr)) : std::exp(-1.f / (0.15f * sr));
    for (int i = 0; i < n; ++i) {
        if (rampLeft > 0) {
            const float t = 1.f / (float) rampLeft;
            for (int k = 0; k < cur.num; ++k) {
                auto& a = cur.s[(size_t) k]; const auto& b = tgt.s[(size_t) k];
                a.g += (b.g - a.g) * t; a.k += (b.k - a.k) * t; a.m0 += (b.m0 - a.m0) * t; a.m1 += (b.m1 - a.m1) * t; a.m2 += (b.m2 - a.m2) * t;
            }
            cur.combDelay += (tgt.combDelay - cur.combDelay) * t;
            cur.combFb += (tgt.combFb - cur.combFb) * t;
            cur.ladderG += (tgt.ladderG - cur.ladderG) * t;
            cur.ladderK += (tgt.ladderK - cur.ladderK) * t;
            cur.phaserFb += (tgt.phaserFb - cur.phaserFb) * t;
            for (int k = 0; k < cur.phaserStages; ++k) cur.phaserG[(size_t) k] += (tgt.phaserG[(size_t) k] - cur.phaserG[(size_t) k]) * t;
            --rampLeft;
        }
        for (int c = 0; c < 2; ++c) {
            auto& cs = ch[(size_t) c];
            float x = io[c][i];
            if (drive && settings.drivePos == 0) {
                float s = distortSample(settings.driveType, x, settings.drive, 0, cs.pre, sr, draw, 0);
                if (dc) s = cs.pre.dc.process(s);
                x = x + (s - x) * blend;
            }
            float y = processSample(cs, x, cur);
            if (drive && settings.drivePos == 1) {
                float s = distortSample(settings.driveType, y, settings.drive, 0, cs.post, sr, draw, 0);
                if (dc) s = cs.post.dc.process(s);
                y = y + (s - y) * blend;
            }
            if (settings.limiter > 0) {
                float a = std::fabs(y);
                cs.limEnv = a > cs.limEnv ? a : a + (cs.limEnv - a) * limCoef;
            }
            if (!std::isfinite(y)) { y = 0; cs.ic1.fill(0); cs.ic2.fill(0); }
            io[c][i] = y;
        }
    }
}

} // namespace gnarl
