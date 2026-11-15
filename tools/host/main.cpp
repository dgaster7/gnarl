// gnarl_host: headless plugin host.
//   gnarl_host effect <plugin.vst3|.component> <report.md>   host-level effect test (Check 2): every parameter, set through
//                                                            the host parameter API of the BUILT binary, must change the audio
//   gnarl_host info <plugin>                                 prints name, version, parameter count
//   gnarl_host recall <plugin> <fixture.state> <expect.json> loads a saved state and checks every parameter value
//   gnarl_host render <plugin> <out.wav>                      renders a note (smoke test of an installed copy)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/PatchJson.h"
#include "params/Registry.h"
#include <cstdio>

using namespace gnarl;

static std::unique_ptr<juce::AudioPluginInstance> load(const juce::String& path, double sr, int block) {
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager(fm);
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* f : fm.getFormats()) {
        if (!f->fileMightContainThisPluginType(path)) continue;
        f->findAllTypesForFile(types, path);
        if (!types.isEmpty()) break;
    }
    if (types.isEmpty()) { std::fprintf(stderr, "no plugin found in %s\n", path.toRawUTF8()); return nullptr; }
    // Forward test hooks only now, after scanning: the scan may create a throwaway instance which must not act on them.
    for (auto* v : {"GNARL_UPDATER_SELFTEST", "GNARL_UPDATER_SELFTEST_ROLLBACK", "GNARL_UPDATE_MANIFEST_URL"})
        if (auto* x = std::getenv((juce::String("HOST_") + v).toRawUTF8())) setenv(v, x, 1);
    juce::String err;
    auto inst = fm.createPluginInstance(*types[0], sr, block, err);
    if (!inst) std::fprintf(stderr, "cannot instantiate: %s\n", err.toRawUTF8());
    return inst;
}

enum class Pattern { Single, Phrase, Bend };
static std::vector<float> renderBlocks(juce::AudioPluginInstance& p, bool fx, int blocks, int block, bool note, Pattern pat = Pattern::Single) {
    const int ins = p.getTotalNumInputChannels(), outs = p.getTotalNumOutputChannels();
    juce::AudioBuffer<float> buf(std::max(ins, outs), block);
    std::vector<float> out;
    double phase = 0, ph2 = 0.3;
    juce::Random rng(5);
    for (int b = 0; b < blocks; ++b) {
        buf.clear();
        if (fx)
            for (int i = 0; i < block; ++i) {
                // stereo input (two detuned saws) on the main bus, a drum-like pulse on the sidechain bus
                float l = (float) (0.3 * (2.0 * (phase - std::floor(phase)) - 1.0)), r = (float) (0.3 * (2.0 * (ph2 - std::floor(ph2)) - 1.0));
                phase += 110.0 / 48000.0; ph2 += 111.3 / 48000.0;
                if (ins > 0) buf.setSample(0, i, l);
                if (ins > 1) buf.setSample(1, i, r);
                const int t = b * block + i;
                float sc = (float) ((rng.nextFloat() * 2 - 1) * std::exp(-(t % 12000) / 900.0)) * 0.5f;
                for (int c = 2; c < ins; ++c) buf.setSample(c, i, sc);
            }
        juce::MidiBuffer midi;
        if (note) {
            if (pat == Pattern::Phrase) {
                if (b == 2) midi.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8) 100), 0);
                if (b == 14) midi.addEvent(juce::MidiMessage::noteOn(1, 55, (juce::uint8) 100), 0);
                if (b == 20) midi.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
                if (b == 28) midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
                if (b == 40) { midi.addEvent(juce::MidiMessage::noteOff(1, 55), 0); midi.addEvent(juce::MidiMessage::noteOff(1, 60), 1); }
            } else {
                if (b == 2) midi.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8) 100), 0);
                if (b == 4 && pat == Pattern::Bend) midi.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0);
                if (b == 36) midi.addEvent(juce::MidiMessage::noteOff(1, 48), 0); // release stage is tested too
            }
        }
        p.processBlock(buf, midi);
        for (int i = 0; i < block; ++i) out.push_back(buf.getSample(0, i) + 0.5f * buf.getSample(outs > 1 ? 1 : 0, i)); // L + R/2: a fingerprint, not a level
    }
    return out;
}

static double diffDb(const std::vector<float>& a, const std::vector<float>& b) {
    double s = 0; for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) { double d = (double) a[i] - b[i]; s += d * d; }
    return 10 * std::log10(s / (double) std::max<size_t>(1, a.size()) + 1e-60);
}

// The plugin's own state blob wrapped the way JUCE's VST3 host expects it (XML "VST3PluginState" with a
// base64 "IComponent" child), so the hosted binary receives exactly our state through IComponent::setState.
static juce::MemoryBlock stateFor(const Patch& p, bool fx) {
    juce::MemoryBlock raw; raw.append("GNRLJSON", 8);
    auto s = patchToJson(p, fx).dump(0);
    raw.append(s.data(), s.size());
    juce::XmlElement x("VST3PluginState");
    x.createNewChildElement("IComponent")->addTextElement(raw.toBase64Encoding());
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary(x, mb);
    return mb;
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::fprintf(stderr, "usage: gnarl_host effect|info|recall|render ...\n"); return 2; }
    const juce::String cmd = argv[1], path = argv[2];
    const int block = 512;
    auto plugin = load(path, 48000, block);
    if (!plugin) return 1;
    const bool fx = plugin->getName().contains("FX");
    plugin->enableAllBuses();
    plugin->prepareToPlay(48000, block);

    if (cmd == "info") {
        std::printf("%s %s, %d parameters, latency %d\n", plugin->getName().toRawUTF8(), plugin->getPluginDescription().version.toRawUTF8(), plugin->getParameters().size(), plugin->getLatencySamples());
        return 0;
    }
    if (cmd == "render" && argc >= 4) {
        auto out = renderBlocks(*plugin, fx, 200, block, true);
        double pk = 0; for (float v : out) pk = std::max(pk, (double) std::fabs(v));
        std::printf("rendered %zu samples, audible: %s\n", out.size(), pk > 1e-4 ? "yes" : "NO");
        return pk > 1e-4 ? 0 : 1;
    }
    // map host parameters by name (JUCE exposes the plugin's parameter names unchanged)
    // Map registry parameters to host parameters by order (JUCE exposes them in the plugin's order),
    // confirming each by name; this stays exact even when two parameters share a display name.
    std::map<juce::String, juce::AudioProcessorParameter*> byName;
    auto& R = Registry::get();
    {
        auto hosted = plugin->getParameters();
        int j = 0;
        for (int i : R.forPlugin(plugin->getName().contains("FX") ? kFx : kSynth)) {
            const auto nm = juce::String::fromUTF8(R[i].name.c_str());
            int k = j;
            while (k < hosted.size() && hosted[k]->getName(256) != nm) ++k;
            if (k < hosted.size()) { byName[juce::String(R[i].id)] = hosted[k]; j = k + 1; }
            else if (j < hosted.size() && R[i].id.find("wt_warp") != std::string::npos) byName[juce::String(R[i].id)] = hosted[j++]; // renamed in 1.0.2 ("Warp" -> "WT Warp")
        }
    }
    auto hostParam = [&](int i) -> juce::AudioProcessorParameter* { auto it = byName.find(juce::String(R[i].id)); return it == byName.end() ? nullptr : it->second; };

    if (cmd == "recall" && argc >= 5) {
        juce::MemoryBlock raw; juce::File(argv[3]).loadFileAsData(raw);
        // fixtures hold the plugin's own state; wrap it the way the VST3 host hands state to the plugin
        juce::XmlElement x("VST3PluginState");
        x.createNewChildElement("IComponent")->addTextElement(raw.toBase64Encoding());
        juce::MemoryBlock mb; juce::AudioProcessor::copyXmlToBinary(x, mb);
        plugin->setStateInformation(mb.getData(), (int) mb.getSize());
        auto v = juce::JSON::parse(juce::File(argv[4]));
        int bad = 0, checked = 0;
        if (auto* o = v.getDynamicObject())
            for (auto& prop : o->getProperties()) {
                int i = R.indexOf(prop.name.toString().toStdString());
                if (i < 0) continue;
                auto* hpp = hostParam(i);
                if (!hpp) continue;
                float expectNorm = R[i].toNorm((float) (double) prop.value);
                float got = hpp->getValue();
                ++checked;
                if (std::fabs(got - expectNorm) > (R[i].isDiscrete() ? 0.5f / std::max(1.f, R[i].max - R[i].min) : 2e-4f)) { if (bad++ < 5) std::printf("  mismatch %s: %f vs %f\n", R[i].id.c_str(), got, expectNorm); }
            }
        std::printf("recall: %d parameters checked, %d mismatches (%s %s)\n", checked, bad, plugin->getName().toRawUTF8(), plugin->getPluginDescription().version.toRawUTF8());
        return bad == 0 && checked > 0 ? 0 : 1;
    }

    if (cmd == "effect" && argc >= 4) {
        int pass = 0, fail = 0;
        std::string md = "# Host-level effect test (Check 2)\n\nEvery parameter set through the host parameter API of the built plugin binary (`" + path.toStdString() + "`), "
                         "with the audit context loaded as plugin state; min/default/max/mid renders must differ by more than -90 dB.\n\n| Parameter | Result | Max difference |\n|---|---|---|\n";
        const char* only = std::getenv("GNARL_HOST_FILTER");
        for (int i : R.forPlugin(fx ? kFx : kSynth)) {
            const auto& d = R[i];
            if (only && d.id.find(only) == std::string::npos) continue;
            auto* hp = hostParam(i);
            if (!hp) { md += "| `" + d.id + "` | FAIL | not exposed to the host |\n"; ++fail; continue; }
            // context (same as the Check 1 audit): parameter values, test routes and module placement
            Patch p(false); // synth defaults as the baseline (filter 1 on), like the Check 1 audit
            if (fx) { p.set("f1_type", FT_LP24); p.set("f1_cutoff", 6000); }
            p.set("master_level", 0.7f);
            if (fx && d.id.rfind("env1_", 0) == 0) { p.addRoute(MS_Env1, "f1_cutoff", 0.7f); p.set("f1_cutoff", 300); } // env 1 is not the amp env in GNARL FX
            const Pattern pat = (d.id == "voices" || d.id == "legato" || d.id.rfind("glide", 0) == 0) ? Pattern::Phrase : (d.id == "bend_range" ? Pattern::Bend : Pattern::Single);
            plugin->setNonRealtime(d.id == "render_os");
            for (auto& c : d.context) p.set(c.id, c.value);
            for (auto& r : d.contextRoutes) {
                if (r.source.rfind("#route", 0) == 0) { int k = std::stoi(r.source.substr(6)) - 1; auto& rt = p.structure->routes[(size_t) k]; rt.source = modSourceFromId(r.source.substr(r.source.find(':') + 1)); rt.dest = R.indexOf(r.dest); }
                else p.addRoute(modSourceFromId(r.source), r.dest, r.depth);
            }
            if (d.id.rfind("fx_", 0) == 0 && d.id.rfind("fx_clip", 0) != 0) {
                auto m = d.id.substr(3, d.id.find('_', 3) - 3);
                auto& ids = fxModuleIds();
                for (size_t k = 0; k < ids.size(); ++k) if (ids[k] == m) p.structure->placeModule((int) k);
            }
            std::vector<std::vector<float>> outs;
            std::vector<float> norms = {0.f, d.toNorm(d.defaultFor(fx)), 1.f, 0.5f};
            if (d.role == Role::Choice || d.role == Role::Integer) for (float v = d.min; v <= d.max; v += 1) norms.push_back(d.toNorm(v));
            auto st = stateFor(p, fx);
            plugin->setStateInformation(st.getData(), (int) st.getSize());
            for (float nv : norms) {
                plugin->reset();
                hp->setValueNotifyingHost(nv);
                outs.push_back(renderBlocks(*plugin, fx || d.plugins == kFx, 60, block, true, pat));
            }
            if (std::getenv("GNARL_HOST_DEBUG")) {
                juce::MemoryBlock mb; plugin->getStateInformation(mb);
                auto js = juce::String::fromUTF8((const char*) mb.getData() + 8, (int) mb.getSize() - 8);
                auto v = juce::JSON::parse(js);
                std::printf("  %s host '%s' value %f; state %s=%s osca_sh_drive=%s\n", d.id.c_str(), hp->getName(256).toRawUTF8(), hp->getValue(), d.id.c_str(),
                            v["params"][juce::Identifier(d.id)].toString().toRawUTF8(), v["params"]["osca_sh_drive"].toString().toRawUTF8());
            }
            plugin->setNonRealtime(false);
            double best = -600;
            for (size_t a = 0; a < outs.size(); ++a) for (size_t b = a + 1; b < outs.size(); ++b) best = std::max(best, diffDb(outs[a], outs[b]));
            bool ok = best > -90;
            ok ? ++pass : ++fail;
            char line[256]; std::snprintf(line, sizeof line, "| `%s` | %s | %.1f dB |\n", d.id.c_str(), ok ? "PASS" : "FAIL", best);
            md += line;
            if (!ok) std::printf("  FAIL %s (%.1f dB)\n", d.id.c_str(), best);
        }
        md = md.substr(0, md.find("\n\n") + 2) + std::to_string(pass) + " pass, " + std::to_string(fail) + " fail.\n\n" + md.substr(md.find("\n\n") + 2);
        juce::File(argv[3]).replaceWithText(md);
        std::printf("host effect test (%s): %d pass, %d fail\n", plugin->getName().toRawUTF8(), pass, fail);
        return fail == 0 ? 0 : 1;
    }
    std::fprintf(stderr, "unknown command\n");
    return 2;
}
