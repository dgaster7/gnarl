#include "PresetManager.h"
#include "GnarlProcessor.h"
#include "params/Registry.h"
#include <set>
#if __has_include("GnarlFactoryData.h")
#include "GnarlFactoryData.h"
#define GNARL_HAS_FACTORY 1
#endif

namespace gnarl {

PresetManager::PresetManager(GnarlProcessor& p) : proc(p) {
    installFactoryIfNeeded();
    loadFavourites();
    rescan();
}

juce::File PresetManager::userDir() const { auto d = GnarlProcessor::appDataDir().getChildFile("User").getChildFile(proc.isFxPlugin() ? "FX" : "Synth"); d.createDirectory(); return d; }
juce::File PresetManager::factoryDir() const { return GnarlProcessor::appDataDir().getChildFile("Factory"); }
juce::File PresetManager::contentDir() const { return GnarlProcessor::appDataDir().getChildFile("Content"); }

void PresetManager::installFactoryIfNeeded() {
#ifdef GNARL_HAS_FACTORY
    auto dir = factoryDir();
    auto stamp = dir.getChildFile(".version");
    const juce::String ver = juce::String(GNARL_VERSION_STRING) + "/" + juce::String(GNARL_CONTENT_VERSION);
    if (stamp.existsAsFile() && stamp.loadFileAsString() == ver) return;
    dir.deleteRecursively();
    dir.createDirectory();
    for (int i = 0; i < GnarlFactoryData::namedResourceListSize; ++i) {
        int size = 0;
        const char* data = GnarlFactoryData::getNamedResource(GnarlFactoryData::namedResourceList[i], size);
        juce::String orig = GnarlFactoryData::getNamedResourceOriginalFilename(GnarlFactoryData::namedResourceList[i]);
        if (!orig.endsWith(PresetManager::extension()) && !orig.endsWith(".wav")) continue;
        // presets are stored as <Plugin>__<Category>__<Name>.gnarl to stay flat inside binary data
        juce::String rel = orig.replace("__", "/");
        auto f = dir.getChildFile(rel);
        f.getParentDirectory().createDirectory();
        f.replaceWithData(data, (size_t) size);
    }
    stamp.replaceWithText(ver);
#endif
}

void PresetManager::loadFavourites() {
    favourites.clear();
    auto f = GnarlProcessor::appDataDir().getChildFile("favourites.json");
    if (auto v = juce::JSON::parse(f); v.isArray()) for (auto& x : *v.getArray()) favourites.insert(x.toString());
}
void PresetManager::saveFavourites() {
    juce::Array<juce::var> a; for (auto& s : favourites) a.add(s);
    auto f = GnarlProcessor::appDataDir().getChildFile("favourites.json");
    f.getParentDirectory().createDirectory();
    f.replaceWithText(juce::JSON::toString(juce::var(a)));
}

void PresetManager::rescan() {
    presets.clear();
    const juce::String plug = proc.isFxPlugin() ? "FX" : "Synth";
    auto scan = [&](const juce::File& root, const juce::String& bank) {
        if (!root.isDirectory()) return;
        for (const auto& e : juce::RangedDirectoryIterator(root, true, juce::String("*") + extension())) {
            auto f = e.getFile();
            std::string err;
            Json j = Json::parse(f.loadFileAsString().toStdString(), &err);
            if (!err.empty() || !j.isObject()) continue;
            const bool isFx = j["plugin"].str() == "GNARL FX";
            if (isFx != proc.isFxPlugin()) continue;
            PresetInfo pi;
            pi.name = juce::String::fromUTF8(j["name"].str().c_str());
            if (pi.name.isEmpty()) pi.name = f.getFileNameWithoutExtension();
            pi.category = juce::String::fromUTF8(j["category"].str().c_str());
            pi.author = juce::String::fromUTF8(j["author"].str().c_str());
            for (auto& t : j["tags"].elements()) pi.tags.add(juce::String::fromUTF8(t.str().c_str()));
            pi.bank = bank; pi.file = f; pi.fx = isFx;
            pi.favourite = favourites.count(f.getFullPathName()) > 0;
            presets.add(pi);
        }
    };
    scan(contentDir().getChildFile("Presets").getChildFile(plug), "Content");
    // downloaded content supersedes the factory copy built into the plugin (same category + name)
    std::set<juce::String> inContent;
    for (auto& p : presets) inContent.insert(p.category + "/" + p.name);
    const int before = presets.size();
    scan(factoryDir().getChildFile(plug), "Factory");
    for (int i = presets.size(); --i >= before;) if (inContent.count(presets[i].category + "/" + presets[i].name)) presets.remove(i);
    scan(userDir(), "User");
    std::sort(presets.begin(), presets.end(), [](const PresetInfo& a, const PresetInfo& b) {
        if (a.bank != b.bank) return a.bank < b.bank;
        if (a.category != b.category) return a.category < b.category;
        return a.name.compareNatural(b.name) < 0;
    });
    lastFiltered = presets;
    sendChangeMessage();
}

juce::Array<PresetInfo> PresetManager::filtered(const juce::String& search, const juce::String& category, const juce::String& bank, bool fav) const {
    juce::Array<PresetInfo> out;
    for (auto& p : presets) {
        if (fav && !p.favourite) continue;
        if (category.isNotEmpty() && category != "All" && p.category != category) continue;
        if (bank.isNotEmpty() && bank != "All" && p.bank != bank) continue;
        if (search.isNotEmpty()) {
            bool hit = p.name.containsIgnoreCase(search) || p.category.containsIgnoreCase(search);
            for (auto& t : p.tags) hit |= t.containsIgnoreCase(search);
            if (!hit) continue;
        }
        out.add(p);
    }
    return out;
}

juce::StringArray PresetManager::categories() const {
    juce::StringArray c; c.add("All");
    for (auto& p : presets) if (p.category.isNotEmpty()) c.addIfNotAlreadyThere(p.category);
    return c;
}

bool PresetManager::loadFile(const juce::File& f) {
    if (!proc.restoreFromJson(f.loadFileAsString(), true)) return false;
    for (int i = 0; i < lastFiltered.size(); ++i) if (lastFiltered[i].file == f) current = i;
    return true;
}

bool PresetManager::load(const PresetInfo& p) { return loadFile(p.file); }

void PresetManager::step(int delta) {
    if (lastFiltered.isEmpty()) return;
    current = ((current < 0 ? 0 : current + delta) % lastFiltered.size() + lastFiltered.size()) % lastFiltered.size();
    loadFile(lastFiltered[current].file);
}

bool PresetManager::saveUser(const juce::String& name, const juce::String& category, const juce::StringArray& tags, juce::String* error) {
    auto clean = juce::File::createLegalFileName(name.trim());
    if (clean.isEmpty()) { if (error) *error = "empty name"; return false; }
    Patch p = proc.currentPatch();
    p.name = name.toStdString(); p.category = category.toStdString(); p.author = "User";
    p.tags.clear(); for (auto& t : tags) p.tags.push_back(t.toStdString());
    auto f = userDir().getChildFile(clean + extension());
    if (!f.replaceWithText(juce::String::fromUTF8(patchToJson(p, proc.isFxPlugin()).dump(1).c_str()))) { if (error) *error = "cannot write file"; return false; }
    proc.setPatchName(name);
    rescan();
    return true;
}

void PresetManager::setFavourite(const PresetInfo& p, bool fav) {
    if (fav) favourites.insert(p.file.getFullPathName()); else favourites.erase(p.file.getFullPathName());
    saveFavourites();
    for (auto& x : presets) if (x.file == p.file) x.favourite = fav;
    sendChangeMessage();
}

void PresetManager::loadInit() {
    Patch p(proc.isFxPlugin());
    p.name = "Init";
    proc.loadPatch(p, true);
    proc.setPatchName("Init");
    current = -1;
}

// ---------------- Mutate ----------------
void mutatePatch(Patch& p, float amount, const MutateLocks& locks, bool fxPlugin, uint32_t seed) {
    auto& R = Registry::get();
    juce::Random rng((juce::int64) seed);
    amount = juce::jlimit(0.f, 1.f, amount);
    static const std::set<std::string> never = {"master_level", "voices", "bend_range", "oversampling", "render_os", "fx_clip_on", "fx_clip_mode",
                                                "fx_clip_ceiling", "fx_clip_drive", "fxp_in_gain", "legato", "glide_legato", "sub_direct", "ef_sidechain", "fxp_env_trigger"};
    auto sectionLocked = [&](const std::string& id) {
        auto starts = [&](const char* s) { return id.rfind(s, 0) == 0; };
        if (starts("osc") || starts("link_") || starts("sub_") || starts("noise_")) return locks.osc;
        if (starts("f1_") || starts("f2_") || starts("filter_") || starts("sat_") || id == "link_cutoff" || id == "link_reso" || id == "link_pan") return locks.filters;
        if (starts("env") || starts("lfo") || starts("seq") || starts("rnd") || starts("fn") || starts("mod") || starts("macro") || starts("ef_") || starts("tr_") || starts("glide")) return locks.mod;
        if (starts("fx_")) return locks.fx;
        return false;
    };
    for (int i : R.forPlugin(fxPlugin ? kFx : kSynth)) {
        const auto& d = R[i];
        if (never.count(d.id) || sectionLocked(d.id)) continue;
        if (d.id.rfind("fx_", 0) == 0 && d.id.size() > 3 && d.id.substr(d.id.size() - 3) == "_on") continue; // module on/off stays as designed
        float& v = p.values[(size_t) i];
        if (d.role == Role::Choice || d.role == Role::Toggle || d.role == Role::Integer) {
            if (rng.nextFloat() < amount * 0.15f) {
                int n = (int) (d.max - d.min) + 1;
                float nv = d.min + (float) rng.nextInt(n);
                if (d.id == "f1_type" && nv == 0) nv = FT_LP24; // keep filter 1 on
                if (d.id.find("unison") != std::string::npos) nv = std::min(nv, 5.f);
                v = nv;
            }
            continue;
        }
        float n = d.toNorm(v);
        n = juce::jlimit(0.f, 1.f, n + (rng.nextFloat() * 2.f - 1.f) * amount * 0.35f);
        float nv = d.fromNorm(n);
        // safe ranges: never silence the main oscillator, avoid runaway feedback and extreme resonance
        if (d.role == Role::Level && d.id == "osca_level") nv = std::max(nv, 0.3f);
        if (d.id.find("_fb") != std::string::npos) nv = juce::jlimit(d.min, d.max, nv * 0.85f);
        if (d.id.find("reso") != std::string::npos && d.unit == Unit::Percent) nv = std::min(nv, 0.92f);
        if (d.id.find("_a") == d.id.size() - 2 && d.unit == Unit::Seconds) nv = std::min(nv, 2.f);
        if (d.id.find("_r") == d.id.size() - 2 && d.unit == Unit::Seconds) nv = std::min(nv, 4.f);
        v = nv;
    }
    if (!locks.mod) {
        // re-aim a few routes at random continuous destinations
        auto mods = R.forPlugin(fxPlugin ? kFx : kSynth);
        for (int k = 0; k < kNumRoutes; ++k) {
            auto& r = p.structure->routes[(size_t) k];
            if (r.source == MS_None || rng.nextFloat() > amount * 0.2f) continue;
            for (int tries = 0; tries < 20; ++tries) {
                int d = mods[(size_t) rng.nextInt((int) mods.size())];
                const auto& pd = R[d];
                if (!pd.modulatable || pd.isDiscrete() || never.count(pd.id) || sectionLocked(pd.id)) continue;
                r.dest = d; break;
            }
        }
        for (auto& l : p.structure->lfo)
            if (rng.nextFloat() < amount * 0.3f)
                for (size_t i = 1; i + 1 < l.a.size(); ++i) l.a[i].y = juce::jlimit(-1.f, 1.f, l.a[i].y + (rng.nextFloat() * 2 - 1) * amount * 0.5f);
        for (auto& s : p.structure->seq)
            if (rng.nextFloat() < amount * 0.4f)
                for (auto& v : s.value) if (rng.nextFloat() < amount) v = juce::jlimit(-1.f, 1.f, v + (rng.nextFloat() * 2 - 1) * amount);
    }
    p.structure->finalize();
}

} // namespace gnarl
