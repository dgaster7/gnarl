#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/PatchJson.h"
#include "engine/Render.h"

namespace gnarl {

class PresetManager;
class Updater;

class GnarlProcessor : public juce::AudioProcessor, private juce::AsyncUpdater {
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
    double getTailLengthSeconds() const override { return fx ? engine.tailSeconds() : engine.tailSeconds() + 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isFxPlugin() const { return fx; }
    juce::AudioProcessorValueTreeState& state() { return apvts; }
    juce::UndoManager& undo() { return undoManager; }
    Engine& getEngine() { return engine; }

    // ---- patch structure (message thread) ----
    const PatchStructure& structure() const { return *structureCopy; }
    // Edit the structure as one undoable step; publishes to the audio engine.
    void editStructure(const juce::String& name, const std::function<void(PatchStructure&)>& fn, bool undoable = true);
    void setStructureDirect(const PatchStructure& s); // used by undo
    juce::ChangeBroadcaster structureChanged;

    // ---- whole patches ----
    Patch currentPatch() const;
    void loadPatch(const Patch& p, bool undoable = true);
    juce::String patchName() const { return currentName; }
    void setPatchName(const juce::String& n) { currentName = n; }
    // A/B compare
    void storeAB(int slot);
    void recallAB(int slot);
    int currentAB() const { return abSlot; }
    void toggleAB();

    // ---- routing helpers for the UI ----
    int addRoute(int source, int destParam, float depth);
    void removeRoute(int k);
    std::vector<int> routesForParam(int paramIndex) const;
    float routeDepth(int k) const;
    void setRouteDepth(int k, float d);
    juce::RangedAudioParameter* paramByIndex(int registryIndex) const;

    // ---- user wavetables ----
    bool loadUserWavetable(int osc, const juce::File& wav, juce::String* error = nullptr);
    void clearUserWavetable(int osc);
    static juce::File appDataDir();

    PresetManager& presets() { return *presetManager; }
    Updater& updater() { return *updaterPtr; }

    static juce::String stateToJsonString(const Patch& p, bool fx);
    bool restoreFromJson(const juce::String& json, bool undoable);

private:
    static BusesProperties busesFor(bool fx);
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() const;
    void handleAsyncUpdate() override;
    void publishStructure();
    void resolveUserTables(PatchStructure& s);

    const bool fx;
    Engine engine;
    juce::UndoManager undoManager{30000, 100};
    juce::AudioProcessorValueTreeState apvts;
    std::vector<std::pair<int, std::atomic<float>*>> paramLinks; // registry index -> raw value
    std::vector<juce::RangedAudioParameter*> byIndex;              // registry index -> host parameter (nullptr if not in this plugin)
    std::vector<MidiEvent> midiScratch;
    std::atomic<int> lastLatency{0};
    std::shared_ptr<PatchStructure> structureCopy;
    juce::String currentName{"Init"};
    std::array<juce::String, 2> abState; int abSlot = 0;
    std::unique_ptr<PresetManager> presetManager;
    std::unique_ptr<Updater> updaterPtr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GnarlProcessor)
};

} // namespace gnarl
