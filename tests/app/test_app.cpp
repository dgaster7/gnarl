// Check 3 (UI binding audit) and processor-level contract tests: state, presets, undo, UI = host = DSP values.
#include <catch2/catch_test_macros.hpp>
#include "GnarlProcessor.h"
#include "PresetManager.h"
#include "Updater.h"
#include "ui/Editor.h"
#include "params/Registry.h"
#include <map>
#include <random>

using namespace gnarl;

static void pump(int ms = 50) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

TEST_CASE("Check 3: every UI control is bound to exactly one parameter and every parameter is reachable", "[ui]") {
    for (bool fx : {false, true}) {
        GnarlProcessor proc(fx);
        std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
        auto* editor = dynamic_cast<GnarlEditor*>(ed.get());
        REQUIRE(editor != nullptr);
        std::vector<juce::Component*> bound;
        GnarlEditor::collectBound(*editor, bound);
        std::map<std::string, int> count;
        for (auto* c : bound) count[c->getProperties()[ui::kParamProperty].toString().toStdString()]++;
        auto& R = Registry::get();
        int missing = 0, dupes = 0, unknown = 0;
        std::string report;
        for (int i : R.forPlugin(fx ? kFx : kSynth)) {
            int n = count.count(R[i].id) ? count[R[i].id] : 0;
            if (n == 0) { ++missing; report += " missing:" + R[i].id; }
            if (n > 1) { ++dupes; report += " duplicated:" + R[i].id; }
        }
        for (auto& [id, n] : count) if (R.indexOf(id) < 0) { ++unknown; report += " unknown:" + id; }
        INFO((fx ? "GNARL FX" : "GNARL") << " bound controls " << bound.size() << report);
        REQUIRE(missing == 0);
        REQUIRE(dupes == 0);
        REQUIRE(unknown == 0);
        REQUIRE(bound.size() == R.forPlugin(fx ? kFx : kSynth).size());
    }
}

TEST_CASE("UI value = host value = DSP value after automation, preset load, undo and resize", "[ui]") {
    GnarlProcessor proc(false);
    proc.setPlayConfigDetails(0, 2, 48000, 256);
    proc.prepareToPlay(48000, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* editor = dynamic_cast<GnarlEditor*>(ed.get());
    auto& R = Registry::get();
    std::mt19937 rng(7);
    auto check = [&](const char* when) {
        juce::AudioBuffer<float> buf(2, 256); juce::MidiBuffer midi;
        proc.processBlock(buf, midi);
        pump(30);
        std::vector<juce::Component*> bound;
        GnarlEditor::collectBound(*editor, bound);
        int bad = 0; std::string first;
        for (auto* c : bound) {
            auto id = c->getProperties()[ui::kParamProperty].toString();
            int i = R.indexOf(id.toStdString());
            auto* p = proc.paramByIndex(i);
            float host = p->convertFrom0to1(p->getValue());
            float dsp = proc.getEngine().getParam(i);
            float ui = host;
            if (auto* k = dynamic_cast<ui::ModKnob*>(c)) ui = (float) k->slider().getValue();
            else if (auto* cb = dynamic_cast<ui::ParamCombo*>(c)) ui = (float) (cb->box.getSelectedItemIndex()) + R[i].min;
            else if (auto* t = dynamic_cast<ui::ParamToggle*>(c)) ui = t->button.getToggleState() ? 1.f : 0.f;
            float tol = 1e-4f * (R[i].max - R[i].min) + 1e-6f;
            if (std::fabs(ui - host) > tol || std::fabs(dsp - host) > tol) { if (!bad++) first = id.toStdString() + " ui=" + std::to_string(ui) + " host=" + std::to_string(host) + " dsp=" + std::to_string(dsp); }
        }
        INFO(when << ": " << bad << " mismatches, first " << first);
        REQUIRE(bad == 0);
    };
    // automation from the host
    for (int i : R.forPlugin(kSynth)) if (auto* p = proc.paramByIndex(i)) p->setValueNotifyingHost(std::uniform_real_distribution<float>(0, 1)(rng));
    check("after host automation");
    // preset load
    Patch pt(false); pt.set("f1_cutoff", 321.f); pt.set("osca_engine", 3); pt.set("fx_dist1_drive", 0.77f);
    proc.loadPatch(pt, true);
    check("after preset load");
    REQUIRE(std::fabs(proc.getEngine().getParam(R.indexOf("f1_cutoff")) - 321.f) < 0.5f);
    proc.undo().undo();
    check("after undo");
    editor->setScale(1.5f); pump(30);
    check("after resize to 150%");
    editor->setScale(0.75f); pump(30);
    check("after resize to 75%");
}

TEST_CASE("processor state round trip keeps every parameter and the patch structure", "[state]") {
    for (bool fx : {false, true}) {
        GnarlProcessor a(fx), b(fx);
        auto& R = Registry::get();
        std::mt19937 rng(3);
        for (int i : R.forPlugin(fx ? kFx : kSynth)) if (auto* p = a.paramByIndex(i)) p->setValueNotifyingHost(std::uniform_real_distribution<float>(0, 1)(rng));
        int k = a.addRoute(MS_Lfo2, R.indexOf("f1_cutoff"), 0.6f);
        REQUIRE(k >= 0);
        a.editStructure("t", [](PatchStructure& s) { s.fxSlots[3] = 5; s.macroNames[2] = "Tear"; s.seq[0].value[7] = 0.25f; s.lfo[1].a = {{0, 0, 0}, {0.3f, 1, 0.5f}, {1, -1, 0}}; });
        juce::MemoryBlock mb; a.getStateInformation(mb);
        b.setStateInformation(mb.getData(), (int) mb.getSize());
        int diffs = 0;
        // JUCE stores denormalised floats and converts through the skewed range, so a reload can move the
        // normalised value by one or two float ulps; 2e-6 normalised is identical for every practical purpose.
        std::string firstDiffs;
        for (int i : R.forPlugin(fx ? kFx : kSynth)) if (R[i].isDiscrete() ? (std::lround(R[i].fromNorm(a.paramByIndex(i)->getValue()) * (R[i].role == Role::Toggle ? 1.f : 1.f)) != std::lround(R[i].role == Role::Toggle ? (a.paramByIndex(i)->getValue() >= 0.5f ? 1.f : 0.f) : R[i].fromNorm(a.paramByIndex(i)->getValue())) || (R[i].role == Role::Toggle ? (a.paramByIndex(i)->getValue() >= 0.5f) != (b.paramByIndex(i)->getValue() >= 0.5f) : std::lround(R[i].fromNorm(a.paramByIndex(i)->getValue())) != std::lround(R[i].fromNorm(b.paramByIndex(i)->getValue())))) : std::fabs(a.paramByIndex(i)->getValue() - b.paramByIndex(i)->getValue()) > 2e-6f) { if (++diffs < 6) firstDiffs += R[i].id + " " + std::to_string(a.paramByIndex(i)->getValue()) + "/" + std::to_string(b.paramByIndex(i)->getValue()) + "; "; }
        INFO(firstDiffs);
        REQUIRE(diffs == 0);
        REQUIRE(b.structure().routes[(size_t) k].source == MS_Lfo2);
        REQUIRE(b.structure().fxSlots[3] == 5);
        REQUIRE(b.structure().macroNames[2] == "Tear");
        REQUIRE(b.structure().seq[0].value[7] == 0.25f);
        REQUIRE(b.structure().lfo[1].a.size() == 3);
        REQUIRE(std::fabs(b.routeDepth(k) - 0.6f) < 1e-6f);
    }
}

TEST_CASE("user preset save/load round trip; User bank is a separate folder", "[presets]") {
    GnarlProcessor proc(false);
    proc.state().getParameter("f1_reso")->setValueNotifyingHost(0.42f);
    juce::String err;
    const juce::String name = "zz-test-" + juce::String(juce::Random::getSystemRandom().nextInt(100000));
    REQUIRE(proc.presets().saveUser(name, "Test", {"a", "b"}, &err));
    auto f = proc.presets().userDir().getChildFile(name + ".gnarl");
    REQUIRE(f.existsAsFile());
    REQUIRE(f.getFullPathName().contains("/GNARL/User/"));
    proc.state().getParameter("f1_reso")->setValueNotifyingHost(0.9f);
    REQUIRE(proc.presets().loadFile(f));
    REQUIRE(std::fabs(proc.state().getParameter("f1_reso")->getValue() - 0.42f) < 1e-5f);
    f.deleteFile();
}

TEST_CASE("Mutate respects section locks", "[presets]") {
    Patch p(false);
    Patch before = p;
    MutateLocks locks; locks.filters = true; locks.fx = true;
    mutatePatch(p, 1.0f, locks, false, 1234);
    auto& R = Registry::get();
    int changedOsc = 0, changedLocked = 0;
    for (int i = 0; i < R.size(); ++i) {
        if (p.values[(size_t) i] == before.values[(size_t) i]) continue;
        auto& id = R[i].id;
        if (id.rfind("f1_", 0) == 0 || id.rfind("f2_", 0) == 0 || id.rfind("fx_", 0) == 0) ++changedLocked;
        if (id.rfind("osc", 0) == 0) ++changedOsc;
    }
    REQUIRE(changedLocked == 0);
    REQUIRE(changedOsc > 10);
    REQUIRE(p.values[(size_t) R.indexOf("master_level")] == before.values[(size_t) R.indexOf("master_level")]);
}

TEST_CASE("updater primitives: signature, manifest, sha256, version compare", "[updater]") {
    juce::MemoryBlock msg("hello", 5);
    REQUIRE_FALSE(Updater::verifySignature(msg, juce::String::repeatedString("00", 64), kUpdatePublicKey));
    Updater::Manifest m;
    REQUIRE(Updater::parseManifest(R"({"version":"1.2.3","pluginVersion":"1.2.3","contentVersion":4,"releaseNotes":"x","assets":[{"name":"GNARL-macOS.zip","url":"https://e/x.zip","sha256":"AB","size":3}]})", m));
    REQUIRE(m.version == "1.2.3"); REQUIRE(m.contentVersion == 4); REQUIRE(m.find("GNARL-macOS.zip") != nullptr); REQUIRE(m.find("GNARL-macOS.zip")->sha256 == "ab");
    REQUIRE(VersionNumber::parse("1.0.9") < VersionNumber::parse("1.0.10"));
    REQUIRE(VersionNumber::parse("v2.0.0").str() == "2.0.0");
    auto tmp = juce::File::createTempFile("sha");
    tmp.replaceWithText("abc");
    REQUIRE(Updater::sha256Hex(tmp) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    tmp.deleteFile();
}

TEST_CASE("saved-state fixtures from every release load", "[fixtures]") {
    auto root = juce::File(GNARL_SOURCE_DIR).getChildFile("tests/fixtures");
    int loaded = 0;
    for (const auto& e : juce::RangedDirectoryIterator(root, true, "*.state")) {
        auto f = e.getFile();
        const bool fx = f.getFileName().containsIgnoreCase("fx");
        GnarlProcessor proc(fx);
        juce::MemoryBlock mb; REQUIRE(f.loadFileAsData(mb));
        proc.setStateInformation(mb.getData(), (int) mb.getSize());
        // The fixture's companion .json holds the expected parameter values; every one must survive.
        auto expect = f.withFileExtension(".expect.json");
        if (expect.existsAsFile()) {
            auto v = juce::JSON::parse(expect);
            int bad = 0;
            if (auto* o = v.getDynamicObject())
                for (auto& prop : o->getProperties()) {
                    auto* p = proc.state().getParameter(prop.name.toString());
                    if (!p) continue;
                    float got = p->convertFrom0to1(p->getValue());
                    if (std::fabs(got - (float) (double) prop.value) > 1e-4f * (std::fabs((float) (double) prop.value) + 1.f)) ++bad;
                }
            INFO(f.getFileName() << " mismatches " << bad);
            REQUIRE(bad == 0);
        }
        ++loaded;
    }
    INFO("fixtures loaded: " << loaded);
    REQUIRE(loaded >= 6); // at least the 1.0.0 fixtures
}

// Hidden: writes this release's saved-state fixtures. Run once per release:
//   GNARL_WRITE_FIXTURES=tests/fixtures/<version> gnarl_app_tests "[.make-fixtures]"
TEST_CASE("write saved-state fixtures for this release", "[.make-fixtures]") {
    auto* dir = std::getenv("GNARL_WRITE_FIXTURES");
    REQUIRE(dir != nullptr);
    juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile(dir));
    out.createDirectory();
    auto& R = Registry::get();
    for (bool fx : {false, true}) {
        for (int seed = 0; seed < 3; ++seed) {
            GnarlProcessor proc(fx);
            std::mt19937 rng((unsigned) (seed * 977 + (fx ? 5 : 1)));
            if (seed > 0)
                for (int i : R.forPlugin(fx ? kFx : kSynth)) {
                    if (R[i].id == "master_level") continue;
                    if (std::uniform_real_distribution<float>(0, 1)(rng) < 0.4f) proc.paramByIndex(i)->setValueNotifyingHost(std::uniform_real_distribution<float>(0, 1)(rng));
                }
            if (seed == 2) {
                proc.addRoute(MS_Lfo1, R.indexOf("f1_cutoff"), 0.4f);
                proc.addRoute(MS_Macro3, R.indexOf("fx_dist1_drive"), 0.5f);
                proc.editStructure("fixture", [](PatchStructure& s) { s.fxSlots[0] = 0; s.fxSlots[1] = 3; s.macroNames[2] = "Tear"; });
            }
            juce::MemoryBlock mb; proc.getStateInformation(mb);
            auto base = out.getChildFile(juce::String(fx ? "fx" : "synth") + "-" + juce::String(seed));
            base.withFileExtension(".state").replaceWithData(mb.getData(), mb.getSize());
            juce::DynamicObject::Ptr exp = new juce::DynamicObject();
            for (int i : R.forPlugin(fx ? kFx : kSynth)) { auto* p = proc.paramByIndex(i); exp->setProperty(juce::String(R[i].id), p->convertFrom0to1(p->getValue())); }
            base.withFileExtension(".expect.json").replaceWithText(juce::JSON::toString(juce::var(exp.get())));
        }
    }
}
