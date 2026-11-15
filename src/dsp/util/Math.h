#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif

namespace gnarl {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr double kPiD = 3.14159265358979323846;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float dbToGain(float db) { return std::pow(10.f, db * 0.05f); }
inline float gainToDb(float g) { return g > 1e-12f ? 20.f * std::log10(g) : -240.f; }
inline float noteToHz(float note) { return 440.f * std::exp2((note - 69.f) * (1.f / 12.f)); }
inline float semisToRatio(float s) { return std::exp2(s * (1.f / 12.f)); }

// Pade-style tanh approximation, accurate to ~1e-4 and monotone; clamps beyond |x|>4.97.
inline float fastTanh(float x) {
    x = clampf(x, -4.97f, 4.97f);
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

inline float fastSin2Pi(float phase01) { return std::sin(kTwoPi * phase01); }

// Flush denormals for the scope (FTZ + DAZ).
class ScopedFlushDenormals {
public:
    ScopedFlushDenormals() {
#if defined(__aarch64__)
        uint64_t fpcr; asm volatile("mrs %0, fpcr" : "=r"(fpcr)); old = fpcr;
        fpcr |= (1ull << 24); // FZ
        asm volatile("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__x86_64__)
        old = _mm_getcsr();
        _mm_setcsr((unsigned) old | 0x8040);
#endif
    }
    ~ScopedFlushDenormals() {
#if defined(__aarch64__)
        asm volatile("msr fpcr, %0" : : "r"(old));
#elif defined(__x86_64__)
        _mm_setcsr((unsigned) old);
#endif
    }
private:
    uint64_t old = 0;
};

// Deterministic fast PRNG (xorshift32).
struct Rng {
    uint32_t s = 0x9E3779B9u;
    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.f / 16777216.f); }       // 0..1
    float bi() { return uni() * 2.f - 1.f; }                                 // -1..1
};

// One-pole parameter smoother with exact snap.
struct Smoother {
    float cur = 0, target = 0, coef = 0;
    void setTime(float ms, float rate) { coef = ms <= 0 ? 0.f : std::exp(-1.f / (ms * 0.001f * rate)); }
    void snap(float v) { cur = target = v; }
    void set(float v) { target = v; }
    float step() {
        cur = target + (cur - target) * coef;
        if (std::fabs(cur - target) < 1e-7f * (1.f + std::fabs(target))) cur = target;
        return cur;
    }
};

// DC blocker (one-pole high-pass ~5 Hz).
struct DcBlocker {
    float x1 = 0, y1 = 0, R = 0.9995f;
    void setRate(float sr) { R = 1.f - (kTwoPi * 5.f / sr); }
    void reset() { x1 = y1 = 0; }
    float process(float x) { float y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
};

// Equal-gain balance pan: centre = unity on both sides (neutral leaves the signal unchanged).
inline void balance(float pan, float& gl, float& gr) {
    gl = pan > 0 ? 1.f - pan : 1.f;
    gr = pan < 0 ? 1.f + pan : 1.f;
}

// PolyBLEP residual for a unit step at fractional position t (0..1) within the current sample.
inline float polyBlepBefore(float t) { return t * t; }      // applied to the sample before the step
inline float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

} // namespace gnarl
