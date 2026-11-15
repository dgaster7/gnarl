#include "GnarlProcessor.h"
#include "PresetManager.h"
#include "Updater.h"
#include "ui/Editor.h"
#include "osc/Wavetable.h"
#include "params/Registry.h"

namespace gnarl {

static const char* kStateMagic = "GNRLJSON";
static float gnarlGainDb(float g) { return 20.f * std::log10(std::max(g, 1e-9f)); }

namespace {
class StructureAction : public juce::UndoableAction {
public:
    StructureAction(GnarlProcessor& p, PatchStructure b, PatchStructure a) : proc(p), before(std::move(b)), after(std::move(a)) {}
    bool perform() override { proc.setStructureDirect(after); return true; }
    bool undo() override { proc.setStructureDirect(before); return true; }
    int getSizeInUnits() override { return 8; }
private:
    GnarlProcessor& proc;
    PatchStructure before, after;
};
} // namespace

juce::AudioProcessor::BusesProperties GnarlProcessor::busesFor(bool fx) {
    if (fx)
        return juce::AudioProcessor::BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)
            .withInput("Sidechain", juce::AudioChannelSet::stereo(), false);
    return juce::AudioProcessor::BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

GnarlProcessor::GnarlProcessor(bool fxPlugin)
    : juce::AudioProcessor(busesFor(fxPlugin)), fx(fxPlugin), engine(fxPlugin),
      apvts(*this, &undoManager, "GNARL", makeLayout()) {
    auto& R = Registry::get();
    byIndex.assign((size_t) R.size(), nullptr);
    for (int i : R.forPlugin(fx ? kFx : kSynth)) {
        if (auto* p = apvts.getRawParameterValue(R[i].id)) paramLinks.emplace_back(i, p);
        byIndex[(size_t) i] = apvts.getParameter(R[i].id);
    }
    midiScratch.reserve(4096);
    structureCopy = std::make_shared<PatchStructure>();
    structureCopy->finalize();
    publishStructure();
    presetManager = std::make_unique<PresetManager>(*this);
    updaterPtr = std::make_unique<Updater>();
}

GnarlProcessor::~GnarlProcessor() { cancelPendingUpdate(); }

juce::AudioProcessorValueTreeState::ParameterLayout GnarlProcessor::makeLayout() const {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto& R = Registry::get();
    for (int i : R.forPlugin(fx ? kFx : kSynth)) {
        const auto& p = R[i];
        juce::ParameterID pid{p.id, 1};
        const auto unit = p.unit;
        auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction([unit, i](float v, int) {
            const auto& d = Registry::get()[i];
            juce::String s;
            switch (unit) {
                case Unit::Hz: s = v >= 1000 ? juce::String(v / 1000.f, 2) + " kHz" : juce::String(v, v < 100 ? 1 : 0) + " Hz"; break;
                case Unit::Gain: s = v <= 0 ? juce::String("-inf dB") : juce::String(gnarlGainDb(v), 1) + " dB"; break;
                case Unit::Db: s = juce::String(v, 1) + " dB"; break;
                case Unit::Percent: s = juce::String(v * 100.f, 0) + " %"; break;
                case Unit::Ms: s = juce::String(v, v < 10 ? 2 : 0) + " ms"; break;
                case Unit::Seconds: s = v < 1 ? juce::String(v * 1000.f, 0) + " ms" : juce::String(v, 2) + " s"; break;
                case Unit::Semitones: s = juce::String(v, 2) + " st"; break;
                case Unit::Cents: s = juce::String(v, 1) + " ct"; break;
                case Unit::Beats: s = juce::String(v, 3) + " beats"; break;
                case Unit::Degrees: s = juce::String(v, 0) + juce::String::fromUTF8("\xc2\xb0"); break;
                case Unit::Ratio: s = juce::String(v, 3) + "x"; break;
                case Unit::Bits: s = juce::String(v, 1) + " bits"; break;
                case Unit::Octaves: s = juce::String(v, 2) + " oct"; break;
                default: s = juce::String(v, 3);
            }
            (void) d;
            return s;
        }).withValueFromStringFunction([unit](const juce::String& t) {
            float v = t.getFloatValue();
            if (unit == Unit::Percent) return v / 100.f;
            if (unit == Unit::Hz && t.containsIgnoreCase("k")) return v * 1000.f;
            if (unit == Unit::Gain) return t.containsIgnoreCase("inf") ? 0.f : std::pow(10.f, v / 20.f);
            if (unit == Unit::Seconds && t.containsIgnoreCase("ms")) return v / 1000.f;
            return v;
        });
        if (p.role == Role::Choice) {
            juce::StringArray c; for (auto& s : p.choices) c.add(s);
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid, p.name, c, (int) p.defaultFor(fx)));
        } else if (p.role == Role::Toggle) {
            layout.add(std::make_unique<juce::AudioParameterBool>(pid, p.name, p.defaultFor(fx) > 0.5f));
        } else if (p.role == Role::Integer) {
            layout.add(std::make_unique<juce::AudioParameterInt>(pid, p.name, (int) p.min, (int) p.max, (int) p.defaultFor(fx)));
        } else {
            juce::NormalisableRange<float> r(p.min, p.max, p.interval, p.skew);
            layout.add(std::make_unique<juce::AudioParameterFloat>(pid, p.name, r, p.defaultFor(fx), attrs));
        }
    }
    return layout;
}

bool GnarlProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (fx) {
        if (l.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
        if (l.inputBuses.size() > 1) {
            auto sc = l.getChannelSet(true, 1);
            if (!sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono()) return false;
        }
    }
    return true;
}

void GnarlProcessor::prepareToPlay(double sr, int block) {
    for (auto& [idx, ptr] : paramLinks) engine.setParam(idx, ptr->load(std::memory_order_relaxed));
    engine.prepare(sr, block);
    lastLatency = engine.latencySamples();
    setLatencySamples(lastLatency);
}

void GnarlProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    for (auto& [idx, ptr] : paramLinks) engine.setParam(idx, ptr->load(std::memory_order_relaxed));
    midiScratch.clear();
    for (const auto m : midi) {
        auto msg = m.getMessage();
        if (msg.getRawDataSize() < 1 || msg.getRawDataSize() > 3 || midiScratch.size() == midiScratch.capacity()) continue;
        MidiEvent e; e.offset = m.samplePosition;
        auto* raw = msg.getRawData();
        e.status = raw[0]; e.d1 = msg.getRawDataSize() > 1 ? raw[1] : 0; e.d2 = msg.getRawDataSize() > 2 ? raw[2] : 0;
        midiScratch.push_back(e);
    }
    HostInfo hi;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm()) hi.bpm = *b;
            if (auto p = pos->getPpqPosition()) hi.ppq = *p;
            hi.playing = pos->getIsPlaying();
        }
    hi.offline = isNonRealtime();
    const int n = buffer.getNumSamples();
    auto mainIn = getBusBuffer(buffer, true, 0);
    auto out = getBusBuffer(buffer, false, 0);
    const float *scL = nullptr, *scR = nullptr;
    if (fx && getBusCount(true) > 1 && getBus(true, 1)->isEnabled()) {
        auto sc = getBusBuffer(buffer, true, 1);
        if (sc.getNumChannels() > 0) { scL = sc.getReadPointer(0); scR = sc.getReadPointer(sc.getNumChannels() > 1 ? 1 : 0); }
    }
    const float* inL = fx ? mainIn.getReadPointer(0) : nullptr;
    const float* inR = fx ? mainIn.getReadPointer(1) : nullptr;
    engine.process(inL, inR, out.getWritePointer(0), out.getWritePointer(1), n, midiScratch.data(), (int) midiScratch.size(), hi, scL, scR);
    const int lat = engine.latencySamples();
    if (lat != lastLatency.load()) { lastLatency = lat; triggerAsyncUpdate(); }
}

void GnarlProcessor::handleAsyncUpdate() {
    setLatencySamples(lastLatency.load());
    if (clearUndoPending.exchange(false)) undoManager.clearUndoHistory();
    // free structures replaced since the last message-thread pass (no UI code can still be reading them now)
    std::vector<std::shared_ptr<PatchStructure>> dead;
    { const juce::ScopedLock l(structureLock); dead.swap(retiredStructures); }
}

// ---------------- state ----------------
Patch GnarlProcessor::currentPatch() const {
    Patch p(fx);
    auto& R = Registry::get();
    for (auto& [idx, ptr] : paramLinks) p.values[(size_t) idx] = ptr->load();
    { const juce::ScopedLock l(structureLock); *p.structure = *structureCopy; }
    p.name = currentName.toStdString();
    (void) R;
    return p;
}

juce::String GnarlProcessor::stateToJsonString(const Patch& p, bool fxp) { return juce::String::fromUTF8(patchToJson(p, fxp).dump(1).c_str()); }

void GnarlProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto json = stateToJsonString(currentPatch(), fx);
    dest.reset();
    dest.append(kStateMagic, 8);
    auto utf8 = json.toRawUTF8();
    dest.append(utf8, std::strlen(utf8));
}

void GnarlProcessor::setStateInformation(const void* data, int size) {
    if (size >= 8 && std::memcmp(data, kStateMagic, 8) == 0) {
        juce::String json = juce::String::fromUTF8((const char*) data + 8, size - 8);
        restoreFromJson(json, false);
        return;
    }
    // legacy XML (pre-1.0 development builds)
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

bool GnarlProcessor::restoreFromJson(const juce::String& json, bool undoable) {
    std::string err;
    Json j = Json::parse(json.toStdString(), &err);
    if (!err.empty()) return false;
    Patch p(fx);
    if (!patchFromJson(j, p, &err)) return false;
    // parameters this plugin does not have (synth-only in FX etc.) are ignored by loadPatch
    loadPatch(p, undoable);
    currentName = juce::String::fromUTF8(p.name.c_str());
    return true;
}

void GnarlProcessor::loadPatch(const Patch& p, bool undoable) {
    if (undoable) undoManager.beginNewTransaction("Load patch");
    auto& R = Registry::get();
    for (auto& [idx, ptr] : paramLinks) {
        auto* param = byIndex[(size_t) idx];
        if (!param) continue;
        float v = p.values[(size_t) idx];
        param->setValueNotifyingHost(param->convertTo0to1(v));
    }
    (void) R;
    PatchStructure s = *p.structure;
    resolveUserTables(s);
    s.finalize();
    if (undoable) undoManager.perform(new StructureAction(*this, structure(), s));
    else setStructureDirect(s);
    if (!p.name.empty()) currentName = juce::String::fromUTF8(p.name.c_str());
    if (!undoable) {
        if (juce::MessageManager::existsAndIsCurrentThread()) undoManager.clearUndoHistory();
        else { clearUndoPending = true; triggerAsyncUpdate(); } // the UndoManager is message-thread only
    }
}

void GnarlProcessor::publishStructure() {
    std::shared_ptr<PatchStructure> cur;
    { const juce::ScopedLock l(structureLock); cur = structureCopy; }
    engine.setStructure(std::make_shared<PatchStructure>(*cur));
}

void GnarlProcessor::setStructureDirect(const PatchStructure& s) {
    auto next = std::make_shared<PatchStructure>(s);
    next->finalize();
    {
        const juce::ScopedLock l(structureLock);
        if (structureCopy) retiredStructures.push_back(structureCopy);
        structureCopy = next;
    }
    publishStructure();
    structureChanged.sendChangeMessage();
    triggerAsyncUpdate();
}

void GnarlProcessor::editStructure(const juce::String& name, const std::function<void(PatchStructure&)>& fn, bool undoable) {
    PatchStructure after = structure();
    fn(after);
    after.finalize();
    if (undoable) { undoManager.beginNewTransaction(name); undoManager.perform(new StructureAction(*this, structure(), after)); }
    else setStructureDirect(after);
}

void GnarlProcessor::storeAB(int slot) { abState[(size_t) (slot & 1)] = stateToJsonString(currentPatch(), fx); }
void GnarlProcessor::recallAB(int slot) {
    auto& s = abState[(size_t) (slot & 1)];
    if (s.isNotEmpty()) restoreFromJson(s, true);
    abSlot = slot & 1;
}
void GnarlProcessor::toggleAB() {
    storeAB(abSlot);
    const int other = abSlot ^ 1;
    if (abState[(size_t) other].isEmpty()) abState[(size_t) other] = abState[(size_t) abSlot];
    recallAB(other);
}

// ---------------- routing helpers ----------------
juce::RangedAudioParameter* GnarlProcessor::paramByIndex(int i) const { return (i >= 0 && i < (int) byIndex.size()) ? byIndex[(size_t) i] : nullptr; }

int GnarlProcessor::addRoute(int source, int dest, float depth) {
    int slot = -1;
    for (int k = 0; k < kNumRoutes; ++k) {
        const auto& r = structure().routes[(size_t) k];
        if (r.source == source && r.dest == dest) return k; // already routed
        if (slot < 0 && r.source == MS_None && r.dest < 0) slot = k;
    }
    if (slot < 0) return -1;
    undoManager.beginNewTransaction("Add modulation");
    PatchStructure after = structure();
    auto& r = after.routes[(size_t) slot];
    r.source = source; r.dest = dest; r.via = MS_None; r.curve = RC_Linear; r.bypass = false; r.bipolar = modSourceIsBipolar(source);
    after.finalize();
    undoManager.perform(new StructureAction(*this, structure(), after));
    setRouteDepth(slot, depth);
    return slot;
}

void GnarlProcessor::removeRoute(int k) {
    editStructure("Remove modulation", [k](PatchStructure& s) { s.routes[(size_t) k] = ModRoute(); });
    setRouteDepth(k, 0);
}

std::vector<int> GnarlProcessor::routesForParam(int p) const {
    std::vector<int> v;
    const auto& st = structure();
    for (int k = 0; k < kNumRoutes; ++k) if (st.routes[(size_t) k].dest == p && st.routes[(size_t) k].source != MS_None) v.push_back(k);
    return v;
}

float GnarlProcessor::routeDepth(int k) const {
    auto* p = apvts.getRawParameterValue("mod" + juce::String(k + 1) + "_depth");
    return p ? p->load() : 0.f;
}

void GnarlProcessor::setRouteDepth(int k, float d) {
    if (auto* p = apvts.getParameter("mod" + juce::String(k + 1) + "_depth")) p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(-1.f, 1.f, d)));
}

// ---------------- user wavetables ----------------
juce::File GnarlProcessor::appDataDir() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support").getChildFile("GNARL");
}

bool GnarlProcessor::loadUserWavetable(int osc, const juce::File& wav, juce::String* error) {
    juce::MemoryBlock mb;
    if (!wav.loadFileAsData(mb)) { if (error) *error = "cannot read file"; return false; }
    std::vector<unsigned char> bytes((const unsigned char*) mb.getData(), (const unsigned char*) mb.getData() + mb.getSize());
    std::string err;
    auto t = loadWavetableWav(bytes, wav.getFileNameWithoutExtension().toStdString(), &err);
    if (!t) { if (error) *error = juce::String(err); return false; }
    // keep a copy in the user wavetable folder so presets can find it again
    auto dir = appDataDir().getChildFile("Wavetables"); dir.createDirectory();
    auto dest = dir.getChildFile(wav.getFileName());
    if (dest != wav) wav.copyFileTo(dest);
    std::shared_ptr<const WavetableData> table = t;
    editStructure("Load wavetable", [osc, table](PatchStructure& s) { s.userTable[(size_t) osc] = table; s.userTableName[(size_t) osc] = table->name; });
    return true;
}

void GnarlProcessor::clearUserWavetable(int osc) {
    editStructure("Factory wavetable", [osc](PatchStructure& s) { s.userTable[(size_t) osc].reset(); s.userTableName[(size_t) osc].clear(); });
}

void GnarlProcessor::resolveUserTables(PatchStructure& s) {
    for (size_t k = 0; k < 3; ++k) {
        if (s.userTableName[k].empty()) { s.userTable[k].reset(); continue; }
        if (s.userTable[k] && s.userTable[k]->name == s.userTableName[k]) continue;
        juce::File f = appDataDir().getChildFile("Wavetables").getChildFile(juce::String::fromUTF8(s.userTableName[k].c_str()) + ".wav");
        if (!f.existsAsFile()) f = appDataDir().getChildFile("Content").getChildFile("Wavetables").getChildFile(juce::String::fromUTF8(s.userTableName[k].c_str()) + ".wav");
        juce::MemoryBlock mb;
        if (f.existsAsFile() && f.loadFileAsData(mb)) {
            std::vector<unsigned char> bytes((const unsigned char*) mb.getData(), (const unsigned char*) mb.getData() + mb.getSize());
            std::string err;
            s.userTable[k] = loadWavetableWav(bytes, s.userTableName[k], &err);
        } else s.userTable[k].reset(); // missing file: fall back to the factory table selected by the parameter
    }
}

juce::AudioProcessorEditor* GnarlProcessor::createEditor() { return new GnarlEditor(*this); }

} // namespace gnarl
