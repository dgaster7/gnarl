#pragma once
// Section 11 preset quality gates (shared by the preset generator and the [presets] test).
#include "Render.h"
#include <string>
#include <vector>

namespace gnarl {

struct GateResult {
    bool pass = true;
    std::vector<std::string> failures;
    double lufs = -100, peakPreClipDb = -200, worstVariantPeakDb = -200, dcDb = -200, tailSeconds = 0, lowCorr = 1, fundamentalDb = -200;
    double modWheelDiffDb = -600, minMacroDiffDb = 0, movement = 0;
    std::vector<double> macroDiffDb;
    std::vector<float> fingerprint;
    double cpuPercent = 0; // of one core at 48 kHz, measured on the C2 render
};

struct GateSpec {
    bool fx = false;
    bool needsSub = false;
    double targetLufs = -14;
    double lufsTolerance = 2;
};

GateResult evaluatePreset(const Patch& p, const GateSpec& spec);
// Renders used by the gates (exposed for calibration).
RenderResult renderNoteForGates(const Patch& p, bool fx, int note, double seconds, double noteOff, bool clipperOff = false);

} // namespace gnarl
