#pragma once
#include "util/FFT.h"
#include "util/Math.h"
#include <array>
#include <vector>

namespace gnarl {

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process(float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void set(double B0, double B1, double B2, double A0, double A1, double A2) { b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0); }
    void peak(double f, double q, double db, double sr) { double A = std::pow(10, db / 40), w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q); set(1 + al * A, -2 * std::cos(w), 1 - al * A, 1 + al / A, -2 * std::cos(w), 1 - al / A); }
    void lowShelf(double f, double db, double sr) { double A = std::pow(10, db / 40), w = 2 * M_PI * f / sr, c = std::cos(w), al = std::sin(w) / 2 * std::sqrt(2.0), sA = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA), (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA); }
    void highShelf(double f, double db, double sr) { double A = std::pow(10, db / 40), w = 2 * M_PI * f / sr, c = std::cos(w), al = std::sin(w) / 2 * std::sqrt(2.0), sA = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA), (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA); }
    void lp(double f, double q, double sr) { double w = 2 * M_PI * std::min(f, sr * 0.49) / sr, c = std::cos(w), al = std::sin(w) / (2 * q); set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void hp(double f, double q, double sr) { double w = 2 * M_PI * std::min(f, sr * 0.49) / sr, c = std::cos(w), al = std::sin(w) / (2 * q); set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void bp(double f, double q, double sr) { double w = 2 * M_PI * std::min(f, sr * 0.49) / sr, c = std::cos(w), al = std::sin(w) / (2 * q); set(al, 0, -al, 1 + al, -2 * c, 1 - al); }
    void allpass(double f, double q, double sr) { double w = 2 * M_PI * std::min(f, sr * 0.49) / sr, c = std::cos(w), al = std::sin(w) / (2 * q); set(1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al); }
};

// TPT state-variable filter (smoothly modulatable)
struct Svf {
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    // mode: 0 lp, 1 bp (unity peak), 2 hp, 3 notch
    inline float process(float x, float g, float k, int mode) {
        float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        switch (mode) { case 0: return v2; case 1: return k * v1; case 2: return x - k * v1 - v2; default: return x - k * v1; }
    }
    static float gOf(float hz, float sr) { return std::tan(kPi * clampf(hz, 5.f, sr * 0.49f) / sr); }
};

// 4th-order Linkwitz-Riley style low-pass (two cascaded Butterworth SVF sections)
struct Lp4 {
    Svf a, b;
    void reset() { a.reset(); b.reset(); }
    inline float process(float x, float g) { return b.process(a.process(x, g, 1.41421356f, 0), g, 1.41421356f, 0); }
};
struct Hp4 {
    Svf a, b;
    void reset() { a.reset(); b.reset(); }
    inline float process(float x, float g) { return b.process(a.process(x, g, 1.41421356f, 2), g, 1.41421356f, 2); }
};

// Fractional delay line with cubic interpolation
struct FracDelay {
    std::vector<float> buf; int mask = 0, pos = 0;
    void prepare(int maxSamples) { int s = 1; while (s < maxSamples + 4) s <<= 1; buf.assign((size_t) s, 0.f); mask = s - 1; pos = 0; }
    void reset() { std::fill(buf.begin(), buf.end(), 0.f); pos = 0; }
    inline void push(float x) { buf[(size_t) pos] = x; pos = (pos + 1) & mask; }
    inline float read(float delay) const { // delay in samples (>= 1)
        float rp = (float) pos - delay;
        int i = (int) std::floor(rp); float f = rp - (float) i;
        auto at = [&](int k) { return buf[(size_t) ((i + k) & mask)]; };
        float xm1 = at(-1), x0 = at(0), x1 = at(1), x2 = at(2);
        float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    inline float readInt(int delay) const { return buf[(size_t) ((pos - delay) & mask)]; }
};

// Integer delay for latency alignment
struct IntDelay {
    std::vector<float> buf; int mask = 0, pos = 0;
    void prepare(int maxSamples) { int s = 1; while (s < maxSamples + 2) s <<= 1; buf.assign((size_t) s, 0.f); mask = s - 1; pos = 0; }
    void reset() { std::fill(buf.begin(), buf.end(), 0.f); pos = 0; }
    inline float process(float x, int d) { buf[(size_t) pos] = x; float y = buf[(size_t) ((pos - d) & mask)]; pos = (pos + 1) & mask; return y; }
};

// Hilbert transformer (Olli Niemitalo's 2x4 all-pass design): outputs a (real) and b (90 deg) .
struct Hilbert {
    struct AP { float a2; float x1 = 0, x2 = 0, y1 = 0, y2 = 0; inline float p(float x) { float y = a2 * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
    std::array<AP, 4> A{{{0.6923878f * 0.6923878f}, {0.9360654322959f * 0.9360654322959f}, {0.9882295226860f * 0.9882295226860f}, {0.9987488452737f * 0.9987488452737f}}};
    std::array<AP, 4> B{{{0.4021921162426f * 0.4021921162426f}, {0.8561710882420f * 0.8561710882420f}, {0.9722909545651f * 0.9722909545651f}, {0.9952884791278f * 0.9952884791278f}}};
    float bDelay = 0;
    void reset() { for (auto& x : A) x.x1 = x.x2 = x.y1 = x.y2 = 0; for (auto& x : B) x.x1 = x.x2 = x.y1 = x.y2 = 0; bDelay = 0; }
    inline void process(float x, float& re, float& im) {
        float a = x, b = x;
        for (auto& s : A) a = s.p(a);
        for (auto& s : B) b = s.p(b);
        re = a; im = bDelay; bDelay = b;
    }
};

struct EnvFollow {
    float env = 0, ca = 0, cr = 0;
    void set(float atkMs, float relMs, float sr) { ca = std::exp(-1.f / (std::max(atkMs, 0.01f) * 0.001f * sr)); cr = std::exp(-1.f / (std::max(relMs, 0.1f) * 0.001f * sr)); }
    inline float process(float x) { x = std::fabs(x); env = x > env ? x + (env - x) * ca : x + (env - x) * cr; return env; }
    void reset() { env = 0; }
};

// STFT engine (Hann analysis + synthesis, 75% overlap): latency = N samples.
class Stft {
public:
    static constexpr int kOrder = 10, N = 1 << kOrder, Hop = N / 4;
    void prepare() {
        fft.init(kOrder);
        win.resize(N); for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos(kTwoPi * i / N);
        in.assign(N, 0); out.assign(2 * N, 0); frame.assign(N, 0); re.assign(N / 2 + 1, 0); im.assign(N / 2 + 1, 0);
        reset();
    }
    void reset() { std::fill(in.begin(), in.end(), 0.f); std::fill(out.begin(), out.end(), 0.f); inPos = 0; outPos = 0; hopCount = 0; }
    // process one sample; callback(re, im) is called once per hop to modify the spectrum
    template <typename F> inline float process(float x, F&& cb) {
        in[(size_t) inPos] = x; inPos = (inPos + 1) & (N - 1);
        float y = out[(size_t) outPos]; out[(size_t) outPos] = 0; outPos = (outPos + 1) % (2 * N);
        if (++hopCount >= Hop) {
            hopCount = 0;
            for (int i = 0; i < N; ++i) frame[(size_t) i] = in[(size_t) ((inPos + i) & (N - 1))] * win[(size_t) i];
            fft.forward(frame.data(), re.data(), im.data());
            cb(re.data(), im.data());
            fft.inverse(re.data(), im.data(), frame.data());
            // synthesis window and overlap-add; Hann^2 at 75% overlap sums to 1.5
            const float g = 1.f / 1.5f;
            for (int i = 0; i < N; ++i) out[(size_t) ((outPos + i) % (2 * N))] += frame[(size_t) i] * win[(size_t) i] * g;
        }
        return y;
    }
    static int latency() { return N - 1; }
private:
    RealFFT fft;
    std::vector<float> win, in, out, frame, re, im;
    int inPos = 0, outPos = 0, hopCount = 0;
};

} // namespace gnarl
