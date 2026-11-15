#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace gnarl {

struct MidiEvent {
    int offset = 0;       // sample offset within the block
    uint8_t status = 0, d1 = 0, d2 = 0;
    int channel() const { return status & 0x0f; }
    int type() const { return status & 0xf0; }
};

struct HostInfo {
    double bpm = 120;
    double ppq = 0;          // quarter-note position at block start
    bool playing = false;
    bool offline = false;
};

class PatchStructure;
class EngineImpl;

// The complete GNARL / GNARL FX audio engine. JUCE-free.
class Engine {
public:
    explicit Engine(bool fxPlugin);
    ~Engine();
    void prepare(double sampleRate, int maxBlock);
    void reset();

    // Raw (real-unit) parameter values by registry index. Audio-thread safe if called from the audio thread before process().
    void setParam(int index, float value);
    float getParam(int index) const;

    // Publish a new structure snapshot (message thread). Engine takes ownership.
    void setStructure(std::shared_ptr<const PatchStructure> s);

    // in/out: stereo. For the synth, inL/inR may be null. sideL/sideR: sidechain or null.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n,
                 const MidiEvent* events, int numEvents, const HostInfo& host,
                 const float* sideL = nullptr, const float* sideR = nullptr);

    int latencySamples() const;
    double tailSeconds() const;
    bool isFx() const;
    EngineImpl& impl() { return *d; }

private:
    std::unique_ptr<EngineImpl> d;
};

} // namespace gnarl
