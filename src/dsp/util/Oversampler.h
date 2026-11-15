#pragma once
// Oversampling: polyphase IIR half-band (minimum latency, used inside voices) and
// linear-phase FIR half-band (integer latency, used by FX modules so dry/wet stay aligned).
#include <array>
#include <vector>

namespace gnarl {

// ---- polyphase IIR half-band (2-path all-pass), coefficients designed at runtime (elliptic) ----
struct HalfbandCoefs {
    std::vector<float> c;
    static HalfbandCoefs design(int numCoefs, double transitionBw);
};

class IirUp2 {
public:
    void init(const HalfbandCoefs& k) { c = k.c; x.assign(c.size() + 2, 0); y.assign(c.size() + 2, 0); }
    void reset() { std::fill(x.begin(), x.end(), 0.f); std::fill(y.begin(), y.end(), 0.f); }
    inline void process(float in, float& out0, float& out1) {
        float s0 = in, s1 = in;
        const size_t n = c.size();
        size_t i = 0;
        for (; i + 1 < n; i += 2) {
            float t0 = (s0 - y[i]) * c[i] + x[i];
            float t1 = (s1 - y[i + 1]) * c[i + 1] + x[i + 1];
            x[i] = s0; x[i + 1] = s1; y[i] = t0; y[i + 1] = t1;
            s0 = t0; s1 = t1;
        }
        if (i < n) { float t0 = (s0 - y[i]) * c[i] + x[i]; x[i] = s0; y[i] = t0; s0 = t0; }
        out0 = s0; out1 = s1;
    }
private:
    std::vector<float> c, x, y;
};

class IirDown2 {
public:
    void init(const HalfbandCoefs& k) { c = k.c; x.assign(c.size() + 2, 0); y.assign(c.size() + 2, 0); }
    void reset() { std::fill(x.begin(), x.end(), 0.f); std::fill(y.begin(), y.end(), 0.f); }
    inline float process(float in0, float in1) {
        float s0 = in1, s1 = in0;
        const size_t n = c.size();
        size_t i = 0;
        for (; i + 1 < n; i += 2) {
            float t0 = (s0 - y[i]) * c[i] + x[i];
            float t1 = (s1 - y[i + 1]) * c[i + 1] + x[i + 1];
            x[i] = s0; x[i + 1] = s1; y[i] = t0; y[i + 1] = t1;
            s0 = t0; s1 = t1;
        }
        if (i < n) { float t0 = (s0 - y[i]) * c[i] + x[i]; x[i] = s0; y[i] = t0; s0 = t0; }
        return 0.5f * (s0 + s1);
    }
private:
    std::vector<float> c, x, y;
};

// ---- linear-phase FIR half-band ----
class FirHalfband {
public:
    void init(int K, double beta); // length 4K+3
    int centre() const { return (int) (taps.size() / 2); }
    std::vector<float> taps;       // full symmetric impulse response, gain 1 at DC
};

// Multi-stage oversampler for `channels` channels, factor 1/2/4/8.
class Oversampler {
public:
    enum class Kind { Iir, LinearPhase };
    void prepare(int channels, int maxBlock, int maxFactorLog2, Kind kind);
    void setFactorLog2(int f);      // 0..maxFactorLog2; resets state if it changes
    int factorLog2() const { return fl2; }
    int factor() const { return 1 << fl2; }
    void reset();
    // Upsample n base-rate samples per channel into internal buffers; returns high-rate channel pointer.
    void up(const float* const* in, int n);
    float* data(int ch) { return buf[(size_t) ch][(size_t) fl2].data(); }
    // Downsample the internal (processed) high-rate buffer into out.
    void down(float* const* out, int n);
    // Latency in base-rate samples (integer for LinearPhase, 0 reported for Iir).
    int latency() const { return kind == Kind::LinearPhase ? linLatency : 0; }

private:
    Kind kind = Kind::Iir;
    int chans = 0, maxN = 0, maxF = 0, fl2 = 0, linLatency = 0;
    // buf[ch][stage]: stage 0 = highest rate working buffer
    std::vector<std::array<std::vector<float>, 4>> buf;
    // IIR
    std::vector<std::array<IirUp2, 3>> iu;
    std::vector<std::array<IirDown2, 3>> id;
    // FIR states per channel per stage
    std::array<FirHalfband, 3> fir;
    struct FirState { std::vector<float> hist; int pos = 0; };
    std::vector<std::array<FirState, 3>> fu, fd;
    std::vector<std::vector<float>> pad; int padLen = 0; std::vector<int> padPos;
    void firUp(int ch, int stage, const float* in, float* out, int n);
    void firDown(int ch, int stage, const float* in, float* out, int n);
    void computeLinLatency();
};

} // namespace gnarl
