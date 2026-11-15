#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"

namespace gnarl {

class GnarlProcessor : public juce::AudioProcessor {
public:
    explicit GnarlProcessor(bool fxPlugin);
    ~GnarlProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return fx ? "GNARL FX" : "GNARL"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return engine.tailSeconds(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isFxPlugin() const { return fx; }
    juce::AudioProcessorValueTreeState& state() { return apvts; }
    Engine& getEngine() { return engine; }

private:
    static BusesProperties busesFor(bool fx);
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() const;
    const bool fx;
    Engine engine;
    juce::AudioProcessorValueTreeState apvts;
    std::vector<std::pair<int, std::atomic<float>*>> paramLinks; // registry index -> raw value
    std::vector<MidiEvent> midiScratch;
    juce::AudioBuffer<float> scratch;
    int lastLatency = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GnarlProcessor)
};

} // namespace gnarl
