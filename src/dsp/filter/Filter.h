#pragma once
// GNARL filter unit: every filter type, stereo, zero-delay-feedback SVF sections (Simper/Cytomic),
// TPT ladder, comb and phaser. Coefficients are computed at sub-block boundaries and linearly
// interpolated per sample, so audio-rate cutoff modulation is smooth and stable.
#include "dist/Distortion.h"
#include "params/Registry.h"
#include <array>
#include <vector>

namespace gnarl {

struct FilterSettings {
    int type = FT_LP24;
    float cutoff = 1000;   // Hz, after key-track / envelope / modulation
    float reso = 0.1f;     // 0..1
    float sep = 0.3f, morph = 0, gender = 0, shoulder = 0.6f, shelf = 0.5f;
    int poles = 4, stages = 8;
    bool opposing = false, boost = false;
    float drive = 0;       // 0..1
    int driveType = DT_Tube, drivePos = 0; // 0 pre, 1 post
    int limiter = 0;       // 0 off, 1 slow, 2 fast
    float driveRemoved = 0; // audit hook: 1 = drive stage hard-removed
};

struct SvfSection { float g = 0, k = 2, m0 = 0, m1 = 0, m2 = 1; };

struct FilterPlan {
    static constexpr int kMax = 8;
    int type = FT_Off;
    int numParallel = 0; // sections [0, numParallel) are summed; the rest run in series after
    int num = 0;
    std::array<SvfSection, kMax> s{};
    // special types
    float combDelay = 100, combFb = 0, combDamp = 0;
    float ladderG = 0, ladderK = 0; int ladderPoles = 4;
    float phaserFb = 0; int phaserStages = 8; std::array<float, 12> phaserG{};
};

FilterPlan makeFilterPlan(const FilterSettings& s, float rate, float resoScale = 1.f);
// Magnitude response (linear gain) of the linear part of the filter at `freq` (Hz).
float filterMagnitude(const FilterSettings& s, float freq, float rate);

class FilterUnit {
public:
    void prepare(float rate, int maxCombSamples);
    void reset();
    // Set the settings reached at the end of the next block of n samples.
    void setTarget(const FilterSettings& s, int n);
    void process(float* L, float* R, int n, const DrawTables& draw);
    bool isOff() const { return cur.type == FT_Off && tgt.type == FT_Off; }
    float rate() const { return sr; }

private:
    float sr = 48000;
    FilterSettings settings;
    FilterPlan cur, tgt;
    int rampLeft = 0;
    std::array<FilterPlan, 1> dummy{};
    struct ChanState {
        std::array<float, FilterPlan::kMax> ic1{}, ic2{};
        float lad[4]{}; float ladOut = 0;
        std::vector<float> comb; int combPos = 0; float combLp = 0;
        std::array<float, 12> ph{}; float phFb = 0;
        DistState pre, post;
        float limEnv = 0;
    };
    std::array<ChanState, 2> ch;
    float limitGain = 1;
    int combMask = 0;
    float processSample(ChanState& c, float x, const FilterPlan& p);
};

} // namespace gnarl
