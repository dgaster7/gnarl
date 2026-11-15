#pragma once
// Headless offline render harness (used by tests, gnarl_render, preset gates).
#include "Engine.h"
#include "PatchStructure.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gnarl {

struct Patch {
    std::vector<float> values;                    // raw value per registry index
    std::shared_ptr<PatchStructure> structure;
    std::string name, category, author;
    std::vector<std::string> tags;
    Patch();                                      // registry defaults + default structure
    void set(const std::string& id, float v);
    float get(const std::string& id) const;
    int addRoute(int source, const std::string& destId, float depth, int curve = 0, int via = -1, bool bipolar = true); // returns route index
};

enum class TestInput { None, Saw110, Sine220, Noise, Chord, Drums, Sweep };

struct NoteEvent { double on = 0, off = 0.5; int note = 48; int velocity = 100; int releaseVelocity = 64; };
struct CcEvent { double time = 0; int cc = 1; int value = 0; }; // cc -1 = pitch bend (value 0..16383), -2 = channel aftertouch

struct RenderSpec {
    double sampleRate = 48000;
    int blockSize = 256;
    bool randomBlockSizes = false;
    double seconds = 1.0;
    bool fx = false;
    TestInput input = TestInput::None;
    float inputGain = 0.5f;
    TestInput sidechain = TestInput::None;
    double bpm = 140;
    bool offline = true;
    std::vector<NoteEvent> notes;
    std::vector<CcEvent> ccs;
    // Called before each block with the time (seconds) at block start; may change parameters.
    std::function<void(Engine&, double t)> automation;
    unsigned seed = 1;
};

struct RenderResult {
    std::vector<float> L, R;
    double sampleRate = 48000;
    int latency = 0;
    double cpuSeconds = 0;
    bool finite = true;
};

RenderResult render(const Patch& patch, const RenderSpec& spec);
void generateInput(TestInput type, std::vector<float>& L, std::vector<float>& R, double sr, unsigned seed, float gain);
void applyPatch(Engine& e, const Patch& p);

bool writeWav(const std::string& path, const RenderResult& r);

} // namespace gnarl
