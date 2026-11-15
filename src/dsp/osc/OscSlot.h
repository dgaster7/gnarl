#pragma once
#include "Cycle.h"
#include "params/Registry.h"
#include "mod/ModSources.h"
#include "util/Oversampler.h"
#include <array>
#include <memory>
#include <mutex>
#include <vector>

namespace gnarl {

struct FmOpParams { float ratio = 1; bool fixed = false; float freq = 440, level = 0, fb = 0; int wave = 0; float a = 0.001f, d = 0.5f, s = 1, r = 0.2f; };

struct OscBlockParams {
    int engine = OE_VA;
    float hzStart = 110, hzEnd = 110;       // fundamental at sub-block start/end (after pitch, glide, bend, modulation)
    int unison = 1; float detune = 20, spread = 0.5f, blend = 1; bool multi = false;
    int phaseMode = 0; float phase = 0;
    float vaShape = 2, vaPw = 0.5f, vaSync = 1;
    float hsDensity = 0.75f, hsSpread = 0.3f, hsSync = 1;
    const WavetableData* table = nullptr; float wtPos = 0; int warpMode = 0; float warp = 0; bool pwmDetune = false;
    int fmAlg = 0; std::array<FmOpParams, 4> op{};
    float gfIndex = 0.3f, gfShift = 0, gfSpread = 0, gfDetune = 0; bool gfInterp = true, gfComplex = false; int gfMode = 1;
    int haPartials = 64; float haTilt = 0, haOddEven = 0, haComb = 0; int haCombPeriod = 4; float haStretch = 0, haFormantHz = 800, haFormantGain = 0, haFormantWidth = 0.5f;
    std::array<float, kNumShaper> sh{};
};

struct ChainIn { int mode = -1; float amount = 0; float src = 0; bool srcWrapped = false; float srcFrac = 0; };

struct OscOut { float l1 = 0, r1 = 0, l2 = 0, r2 = 0; float raw = 0; };

class OscSlot {
public:
    static constexpr int kMaxUni = 9;
    void prepare(float sampleRate);
    void noteOn(const OscBlockParams& p, uint32_t seed, bool legato);
    void noteOff();
    void beginBlock(const OscBlockParams& p, int n, CycleBuilder& builder, float dt);
    OscOut tick(int i, const ChainIn& ci);
    bool wrapped = false; float wrapFrac = 0;   // fundamental phase of unison voice 0 wrapped during the last tick
    float lastRaw() const { return raw; }

private:
    float sr = 48000;
    OscBlockParams P;
    int n = 16;
    int uni = 1;
    std::array<float, kMaxUni> detRatio{}, panL{}, panR{}, uniGain{};
    std::array<uint8_t, kMaxUni> bus{};
    std::array<double, kMaxUni> fph{};   // fundamental phase 0..1
    std::array<double, kMaxUni> slave{}; // cycle-osc slave phase (sync)
    std::array<float, kMaxUni> prevOut{};
    // windowed-sinc BLEP: every engine output is delayed by 16 samples so step corrections can be applied on both sides
    struct BlepRing { std::array<float, 64> corr{}; std::array<float, 32> hist{}; int pos = 0; };
    std::array<BlepRing, kMaxUni> bl{};
    inline void blepEvent(int u, float jump, float frac);
    inline float blepOut(int u, float v);
    const float* sawTab = nullptr; // band-limited saw for the HyperSaw (chosen per block)
    std::shared_ptr<struct SawTables> saws;
    float incStart = 0, incEnd = 0;     // fundamental phase increment
    float raw = 0;
    // cycle tables
    alignas(16) std::array<float, CycleBuilder::kPadded> tabCur{}, tabPrev{};
    CycleInputs inputs; bool haveTable = false; int xfade = 0; static constexpr int kXfade = 64;
    int rebuildCounter = 0; int bandIndex = -1000;
    // hypersaw
    std::array<std::array<double, 9>, kMaxUni> hs{};
    // fm
    struct FmVoice { std::array<double, 4> ph{}; std::array<float, 4> out{}; IirDown2 dec, dec2; };
    std::array<FmVoice, kMaxUni> fm;
    std::array<Envelope, 4> opEnv; std::array<float, 4> opEnvStart{}, opEnvEnd{}, opLvlStart{}, opLvlEnd{}, opFbStart{}, opFbEnd{};
    // grain
    struct Grain { double rp = 0; float age = -1; float len = 1; double inc = 0; };
    struct GStream { double ph = 0; std::array<Grain, 3> g{}; int next = 0; };
    std::array<std::array<GStream, 3>, kMaxUni> gs{};
    std::array<double, kMaxUni> subPh{};
    // harmonic
    static constexpr int kPartials = 128;
    struct HVoice { std::array<float, kPartials> c{}, s{}, cw{}, sw{}; };
    std::array<HVoice, kMaxUni> hv{};
    std::array<float, kPartials> hAmpS{}, hAmpE{};
    int hCount = 0;

    void setupUnison();
    void rebuildIfNeeded(CycleBuilder& b, bool force);
    float cycleSample(float phase) const;
    void harmonicSetup(float hz);
};

} // namespace gnarl
