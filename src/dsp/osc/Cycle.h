#pragma once
// Single-cycle builder: raw cycle (VA shape or wavetable frames) -> warp -> Shaper (18 stages)
// -> re-band-limit (FFT, zero harmonics above the limit, inverse FFT). Realtime-safe: all scratch is
// allocated in the constructor; build() never allocates.
#include "Wavetable.h"
#include "util/FFT.h"
#include <array>

namespace gnarl {

constexpr int kNumShaper = 19;
enum ShaperStage { SH_DC, SH_Sym, SH_Warp, SH_Twist, SH_Drive, SH_Window, SH_Offset, SH_Wave, SH_Bits, SH_Shrink,
                   SH_SelfSync, SH_Multipoint, SH_MpWidth, SH_Lpf, SH_Hpf, SH_Fold, SH_Spectrum, SH_RndAll, SH_RndOdd };

struct CycleInputs {
    int source = 0;                 // 0 = virtual analog shape, 1 = wavetable frame
    float vaShape = 2, vaPw = 0.5f, syncRatio = 1; // syncRatio: hard sync done inside the cycle (exactly band-limited)
    const WavetableData* table = nullptr;
    float framePos = 0;             // 0..1
    bool interpFrames = true;
    int warpMode = 0; float warp = 0;
    std::array<float, kNumShaper> sh{};
    int maxHarmonic = 1023;
    uint32_t seed = 12345;
    bool sameAs(const CycleInputs& o) const;
};

class CycleBuilder {
public:
    CycleBuilder();
    // out must hold kCycle + 4 floats; out[1..kCycle] is the cycle, out[0] and out[kCycle+1..+3] are wrap padding.
    void build(const CycleInputs& in, float* out);
    static constexpr int kPadded = kCycle + 4;
private:
    RealFFT fft{11};
    std::vector<float> a, b, re, im;
    void rawCycle(const CycleInputs& in, float* x);
};

// Hermite read from a padded table at phase 0..1
inline float readCycle(const float* t, float phase) {
    float pos = phase * (float) kCycle;
    int i = (int) pos;
    float f = pos - (float) i;
    i = i & (kCycle - 1);
    const float* p = t + i; // p[0] = x[i-1], p[1] = x[i], p[2] = x[i+1], p[3] = x[i+2]
    float xm1 = p[0], x0 = p[1], x1 = p[2], x2 = p[3];
    float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

} // namespace gnarl
