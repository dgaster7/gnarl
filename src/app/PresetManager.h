#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Render.h"
#include <set>

namespace gnarl {

class GnarlProcessor;

struct PresetInfo {
    juce::String name, category, author, bank; // bank: Factory, Content, User
    juce::StringArray tags;
    juce::File file;
    bool favourite = false;
    bool fx = false;
};

class PresetManager : public juce::ChangeBroadcaster {
public:
    explicit PresetManager(GnarlProcessor& p);
    void rescan();
    const juce::Array<PresetInfo>& all() const { return presets; }
    juce::Array<PresetInfo> filtered(const juce::String& search, const juce::String& category, const juce::String& bank, bool favouritesOnly) const;
    juce::StringArray categories() const;
    bool load(const PresetInfo& p);
    bool loadFile(const juce::File& f);
    bool saveUser(const juce::String& name, const juce::String& category, const juce::StringArray& tags, juce::String* error = nullptr);
    void setFavourite(const PresetInfo& p, bool fav);
    void loadInit();
    void step(int delta); // next / previous within the last filtered list
    int currentIndex() const { return current; }
    juce::File userDir() const;
    juce::File factoryDir() const;
    juce::File contentDir() const;
    static const char* extension() { return ".gnarl"; }
    void installFactoryIfNeeded(); // writes the embedded factory bank to disk when missing or outdated
    void setLastFilter(const juce::Array<PresetInfo>& f) { lastFiltered = f; }
private:
    GnarlProcessor& proc;
    juce::Array<PresetInfo> presets, lastFiltered;
    std::set<juce::String> favourites;
    int current = -1;
    void loadFavourites();
    void saveFavourites();
};

// Mutate: randomise within safe ranges, honouring section locks.
struct MutateLocks { bool osc = false, filters = false, mod = false, fx = false; };
void mutatePatch(Patch& p, float amount, const MutateLocks& locks, bool fxPlugin, uint32_t seed);

} // namespace gnarl
