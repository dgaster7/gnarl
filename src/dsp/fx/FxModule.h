#pragma once
#include "dist/Distortion.h"
#include "engine/Audit.h"
#include "engine/PatchStructure.h"
#include "util/Math.h"
#include "util/Oversampler.h"
#include <memory>
#include <vector>

namespace gnarl {

struct FxContext {
    float sr = 48000;
    double bpm = 120, ppq = 0; bool playing = false;
    float lastNoteHz = 261.63f;
    const float* scL = nullptr; const float* scR = nullptr; // sidechain (FX plugin), may be null
    const PatchStructure* st = nullptr;
    DrawTables draw;
    int osLog2 = 2;
    bool fxPlugin = false;
};

// Values of the module's parameters at the start (e0) and end (e1) of the block.
struct FxParams {
    const float* e0; const float* e1;
    float at0(int i) const { return e0[i]; }
    float at1(int i) const { return e1[i]; }
};

class FxModule {
public:
    virtual ~FxModule() = default;
    void init(int onParam, int mixParam) { onIx = onParam; mixIx = mixParam; }
    virtual void prepare(float sampleRate, int maxBlock, int maxOsLog2);
    virtual void reset() {}
    virtual int latency() const { return 0; }
    virtual double tailSeconds() const { return 0; }
    virtual void processWet(float* L, float* R, int n, const FxParams& p, FxContext& ctx) = 0;
    // Called by the chain: wet = processWet(copy of input), out = dry (latency-aligned) * (1-mix) + wet * mix.
    void run(float* L, float* R, int n, const FxParams& p, FxContext& ctx);
    bool isOn(const float* e) const { return e[onIx] > 0.5f; }
    int onIx = -1, mixIx = -1;
protected:
    float sr = 48000;
    int maxN = 2048;
    std::vector<float> wetL, wetR;
    std::vector<float> dlyL, dlyR; int dlyPos = 0, dlyMask = 0;
    int lastLatency = 0;
};

// Shared helper: oversampled nonlinear processing with integer latency (linear-phase FIR).
struct OsStage {
    Oversampler os;
    void prepare(int maxBlock, int maxOsLog2) { os.prepare(2, maxBlock, maxOsLog2, Oversampler::Kind::LinearPhase); }
    void setFactor(int l2) { os.setFactorLog2(l2); }
    int latency() const { return os.latency(); }
};

class FxChain {
public:
    explicit FxChain(bool fxPlugin);
    ~FxChain();
    void prepare(float sampleRate, int maxBlock);
    void reset();
    void process(float* L, float* R, int n, const float* e0, const float* e1, FxContext& ctx);
    int latency(const float* e, const PatchStructure* st) const;
    double tail(const float* e, const PatchStructure* st) const;
    FxModule* module(int i) { return mods[(size_t) i].get(); }
    int numModules() const { return (int) mods.size(); }
private:
    std::vector<std::unique_ptr<FxModule>> mods;
    bool fxp;
};

// Final safety clipper / limiter (always after the slots).
class FinalStage {
public:
    void prepare(float sampleRate, int maxBlock);
    void reset();
    void process(float* L, float* R, int n, bool on, int mode, float ceilingDb0, float ceilingDb1, float driveDb0, float driveDb1);
private:
    float sr = 48000, gr = 1, env = 0;
};

} // namespace gnarl
