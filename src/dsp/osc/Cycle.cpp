#include "Cycle.h"
#include "params/Registry.h"
#include "util/Math.h"
#include <cstring>

namespace gnarl {

bool CycleInputs::sameAs(const CycleInputs& o) const {
    auto q = [](float a, float b) { return std::fabs(a - b) < 1e-5f; };
    if (source != o.source || table != o.table || interpFrames != o.interpFrames || warpMode != o.warpMode || maxHarmonic != o.maxHarmonic || seed != o.seed) return false;
    if (!q(vaShape, o.vaShape) || !q(vaPw, o.vaPw) || !q(syncRatio, o.syncRatio) || !q(framePos, o.framePos) || !q(warp, o.warp)) return false;
    for (int i = 0; i < kNumShaper; ++i) if (!q(sh[(size_t) i], o.sh[(size_t) i])) return false;
    return true;
}

CycleBuilder::CycleBuilder() : a(kCycle), b(kCycle), re(kCycle / 2 + 1), im(kCycle / 2 + 1) {}

namespace {
inline float cubicAt(const float* x, float pos) {
    // periodic cubic interpolation, pos in samples
    pos -= std::floor(pos / kCycle) * kCycle;
    int i = (int) pos; float f = pos - (float) i;
    auto at = [&](int k) { return x[(k + kCycle) & (kCycle - 1)]; };
    float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
    float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}
inline float wrapPm1(float v) { return v - 2.f * std::floor((v + 1.f) * 0.5f); }
inline float foldPm1(float v) { float t = v * 0.25f + 0.25f; t -= std::floor(t); return 1.f - 4.f * std::fabs(t - 0.5f); }
float tri(float t) { return t < 0.25f ? 4 * t : (t < 0.75f ? 2 - 4 * t : 4 * t - 4); }
} // namespace

void CycleBuilder::rawCycle(const CycleInputs& in, float* x) {
    if (in.source == 0) {
        const float s = clampf(in.vaShape, 0, 3);
        const float pw = clampf(in.vaPw, 0.01f, 0.99f);
        const float sr = std::max(1.f, in.syncRatio);
        for (int i = 0; i < kCycle; ++i) {
            float t = (float) i / kCycle * sr; // hard sync: `sr` slave cycles per master cycle, reset at the master wrap
            t -= std::floor(t);
            float sine = std::sin(kTwoPi * t), tr = tri(t), saw = 1.f - 2.f * t, pul = t < pw ? 1.f : -1.f;
            float v;
            if (s < 1) v = lerp(sine, tr, s);
            else if (s < 2) v = lerp(tr, saw, s - 1);
            else v = lerp(saw, pul, s - 2);
            x[i] = v;
        }
        return;
    }
    const WavetableData* t = in.table;
    if (!t || t->frames < 1) { std::memset(x, 0, sizeof(float) * kCycle); return; }
    float fp = clampf(in.framePos, 0, 1) * (float) (t->frames - 1);
    int f0 = (int) fp; float fr = fp - (float) f0;
    if (!in.interpFrames) { f0 = (int) std::lround(fp); fr = 0; }
    const float* A = t->frame(f0);
    const float* B = t->frame(std::min(f0 + 1, t->frames - 1));
    for (int i = 0; i < kCycle; ++i) x[i] = A[i] + (B[i] - A[i]) * fr;
}

void CycleBuilder::build(const CycleInputs& in, float* out) {
    float* x = a.data();
    float* y = b.data();
    rawCycle(in, x);

    // ---- wavetable warp (phase / amplitude domain) ----
    const float w = clampf(in.warp, 0, 1);
    if (w > 0 && in.warpMode != WM_None) {
        switch (in.warpMode) {
            case WM_BendPlus: for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle; y[i] = cubicAt(x, (1.f - std::pow(1.f - t, 1.f + 3.f * w)) * kCycle); } std::swap(x, y); break;
            case WM_BendMinus: for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle; y[i] = cubicAt(x, std::pow(t, 1.f + 3.f * w) * kCycle); } std::swap(x, y); break;
            case WM_Sync: for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle * (1.f + 7.f * w); y[i] = cubicAt(x, (t - std::floor(t)) * kCycle); } std::swap(x, y); break;
            case WM_PWM: { float f = 1.f - 0.95f * w; for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle; y[i] = t < f ? cubicAt(x, t / f * kCycle) : 0.f; } std::swap(x, y); break; }
            case WM_Mirror: for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle; float m = t < 0.5f ? 2 * t : 2 - 2 * t; y[i] = cubicAt(x, lerp(t, m, w) * kCycle); } std::swap(x, y); break;
            case WM_PhaseDist: { float k = 0.5f - 0.49f * w; for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle; float m = t < k ? t * 0.5f / k : 0.5f + (t - k) * 0.5f / (1 - k); y[i] = cubicAt(x, m * kCycle); } std::swap(x, y); break; }
            case WM_Fold: for (int i = 0; i < kCycle; ++i) x[i] = foldPm1(x[i] * (1.f + 7.f * w)); break;
            case WM_Quantize: { float N = (float) kCycle * std::exp2(-9.f * w); for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle; y[i] = x[(int) (std::floor(t * N) / N * kCycle) & (kCycle - 1)]; } std::swap(x, y); break; }
            default: break;
        }
    }

    // ---- Shaper: time-domain stages ----
    const auto& s = in.sh;
    if (s[SH_DC] > 0) for (int i = 0; i < kCycle; ++i) x[i] += s[SH_DC];
    if (s[SH_Sym] > 0) {
        float sp = 0.5f + 0.45f * s[SH_Sym];
        for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle; float m = t < sp ? t * 0.5f / sp : 0.5f + (t - sp) * 0.5f / (1 - sp); y[i] = cubicAt(x, m * kCycle); }
        std::swap(x, y);
    }
    if (s[SH_Warp] > 0) { float g = 1.f + 3.f * s[SH_Warp]; for (int i = 0; i < kCycle; ++i) x[i] = wrapPm1(x[i] * g); }
    if (s[SH_Twist] > 0) {
        float g = 1.f + 3.f * s[SH_Twist];
        for (int i = 0; i < kCycle; ++i) { float v = x[i] * g; float m = std::floor((v + 1.f) * 0.5f); float r = v - 2.f * m; x[i] = (((int) m) & 1) ? -r : r; }
    }
    if (s[SH_Drive] > 0) { float g = 1.f + 10.f * s[SH_Drive]; for (int i = 0; i < kCycle; ++i) x[i] = clampf(x[i] * g, -1.f, 1.f); }
    if (s[SH_Window] > 0) for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle; float win = 1.f - std::fabs(2.f * t - 1.f); x[i] *= 1.f - s[SH_Window] + s[SH_Window] * win; }
    if (s[SH_Offset] > 0) { int off = (int) std::lround(s[SH_Offset] * 0.5f * kCycle); for (int i = 0; i < kCycle; ++i) y[i] = x[(i + off) & (kCycle - 1)]; std::swap(x, y); }
    if (s[SH_Wave] > 0) {
        double r = 0; for (int i = 0; i < kCycle; ++i) r += (double) x[i] * x[i];
        float amp = (float) std::sqrt(2.0 * r / kCycle);
        for (int i = 0; i < kCycle; ++i) x[i] = lerp(x[i], amp * std::sin(kTwoPi * i / kCycle), s[SH_Wave]);
    }
    if (s[SH_Bits] > 0) { float bits = 32.f - 31.f * s[SH_Bits]; float lv = std::exp2(bits - 1.f); for (int i = 0; i < kCycle; ++i) x[i] = std::round(x[i] * lv) / lv; }
    if (s[SH_Shrink] > 0) { float f = 1.f - 0.9f * s[SH_Shrink]; for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle; y[i] = t < f ? cubicAt(x, t / f * kCycle) : 0.f; } std::swap(x, y); }
    if (s[SH_SelfSync] > 0) { float r = 1.f + 7.f * s[SH_SelfSync]; for (int i = 0; i < kCycle; ++i) { float t = (float) i / kCycle * r; y[i] = cubicAt(x, (t - std::floor(t)) * kCycle); } std::swap(x, y); }
    if (s[SH_Multipoint] > 0) {
        float wdt = 0.005f + clampf(s[SH_MpWidth], 0, 1) * 0.15f;
        const float pts[3] = {0.25f, 0.5f, 0.75f};
        for (int i = 0; i < kCycle; ++i) {
            float t = (float) i / kCycle, g = 1;
            for (float p : pts) { float d = std::fabs(t - p) / wdt; if (d < 1) g *= 1.f - s[SH_Multipoint] * (0.5f + 0.5f * std::cos(kPi * d)); }
            x[i] *= g;
        }
    }
    if (s[SH_Fold] > 0) { float g = 1.f + 5.f * s[SH_Fold]; for (int i = 0; i < kCycle; ++i) x[i] = foldPm1(x[i] * g); }

    // ---- to spectrum ----
    fft.forward(x, re.data(), im.data());
    const int nb = kCycle / 2;
    const int hmax = std::clamp(in.maxHarmonic, 1, nb - 1);
    re[0] = 0; im[0] = 0; // remove DC
    for (int h = hmax + 1; h <= nb; ++h) { re[(size_t) h] = 0; im[(size_t) h] = 0; }

    // ---- Shaper: spectral stages ----
    if (s[SH_Lpf] > 0) {
        float hc = 1024.f * std::exp2(-9.5f * s[SH_Lpf]), mix = clampf(s[SH_Lpf] * 20.f, 0, 1);
        for (int h = 1; h <= hmax; ++h) { float g = 1.f / std::sqrt(1.f + std::pow(h / hc, 8.f)); g = lerp(1.f, g, mix); re[(size_t) h] *= g; im[(size_t) h] *= g; }
    }
    if (s[SH_Hpf] > 0) {
        float hc = std::exp2(8.f * s[SH_Hpf]), mix = clampf(s[SH_Hpf] * 20.f, 0, 1);
        for (int h = 1; h <= hmax; ++h) { float g = 1.f / std::sqrt(1.f + std::pow(hc / h, 4.f)); g = lerp(1.f, g, mix); re[(size_t) h] *= g; im[(size_t) h] *= g; }
    }
    if (s[SH_Spectrum] > 0) {
        float a = s[SH_Spectrum];
        for (int h = 1; h <= hmax; ++h) {
            float ph = (h & 1) ? kTwoPi * h * a * 0.11f : -kTwoPi * h * a * 0.07f; // odd and even drift in opposite directions
            float g = 0.5f + 0.5f * std::cos(ph);
            re[(size_t) h] *= g; im[(size_t) h] *= g;
        }
    }
    if (s[SH_RndAll] > 0 || s[SH_RndOdd] > 0) {
        Rng r(in.seed);
        for (int h = 1; h <= hmax; ++h) {
            float rv = r.uni();
            float g = 1.f + s[SH_RndAll] * (2.f * rv - 1.f);
            if (h & 1) g *= 1.f + s[SH_RndOdd] * (2.f * r.uni() - 1.f); else (void) r.uni();
            re[(size_t) h] *= g; im[(size_t) h] *= g;
        }
    }
    fft.inverse(re.data(), im.data(), out + 1);
    out[0] = out[kCycle];
    out[kCycle + 1] = out[1];
    out[kCycle + 2] = out[2];
    out[kCycle + 3] = out[3];
}

} // namespace gnarl
