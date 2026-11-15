// Section 11 preset gates over the shipped factory bank (content/presets). Writes build/preset_report.md.
#include <catch2/catch_test_macros.hpp>
#include "analysis/Analysis.h"
#include "engine/PatchJson.h"
#include "engine/PresetGates.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
using namespace gnarl;
namespace fs = std::filesystem;

TEST_CASE("factory presets pass the section 11 gates", "[presets]") {
    const fs::path root = fs::path(GNARL_SOURCE_DIR) / "content" / "presets";
    std::vector<fs::path> files;
    for (auto& e : fs::recursive_directory_iterator(root)) if (e.path().extension() == ".gnarl") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() >= 200);
    struct Row { std::string name, cat; bool fx; GateResult g; };
    std::vector<Row> rows(files.size());
    std::atomic<size_t> next{0};
    auto work = [&] {
        for (size_t i; (i = next++) < files.size();) {
            std::ifstream in(files[i]); std::stringstream ss; ss << in.rdbuf();
            std::string err; Json j = Json::parse(ss.str(), &err);
            Patch p; patchFromJson(j, p, &err);
            Row r; r.name = p.name; r.cat = p.category; r.fx = j["plugin"].str() == "GNARL FX";
            GateSpec spec; spec.fx = r.fx; spec.targetLufs = r.fx ? -16 : -15;
            spec.needsSub = !r.fx && (r.cat == "Neuro-Reese" || r.cat == "Sine-Compression" || r.cat == "DnB" || r.cat == "Sub-Clean");
            r.g = evaluatePreset(p, spec);
            rows[i] = r;
        }
    };
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < std::max(1u, std::thread::hardware_concurrency()); ++t) pool.emplace_back(work);
    for (auto& t : pool) t.join();
    for (size_t a = 0; a < rows.size(); ++a)
        for (size_t b = a + 1; b < rows.size(); ++b)
            if (rows[a].fx == rows[b].fx && analysis::fingerprintDistance(rows[a].g.fingerprint, rows[b].g.fingerprint) < 1.0) { rows[b].g.pass = false; rows[b].g.failures.push_back("nearly identical to " + rows[a].name); }
    // category counts (section 7)
    std::map<std::string, int> counts; int fxCount = 0;
    for (auto& r : rows) { if (r.fx) ++fxCount; else counts[r.cat]++; }
    const std::map<std::string, int> minimum = {{"Neuro-Reese", 25}, {"Squelch-Liquid", 25}, {"Talking-Formant", 20}, {"FM Growl", 20}, {"Tri-Peak-Separation", 15},
                                                 {"Sine-Compression", 10}, {"Glitch-Rhythmic", 15}, {"DnB", 15}, {"Sub-Clean", 5}, {"Chip-Lofi", 5}, {"Experimental", 5}};
    for (auto& [c, n] : minimum) { INFO(c); REQUIRE(counts[c] >= n); }
    REQUIRE(fxCount >= 40);
    int fails = 0; std::ostringstream md;
    for (auto& r : rows) fails += !r.g.pass;
    md << "# Factory preset gates (section 11)\n\n" << rows.size() << " presets checked by `gnarl_tests \"[presets]\"`: " << (rows.size() - fails) << " pass, " << fails << " fail.\n\n";
    md << "| Preset | Plugin | Category | Result | LUFS | Pre-clip peak | Low corr | Mod wheel | Weakest macro | CPU % | Notes |\n|---|---|---|---|---|---|---|---|---|---|---|\n";
    for (auto& r : rows) {
        std::string notes; for (auto& f : r.g.failures) notes += f + "; ";
        char line[400];
        std::snprintf(line, sizeof line, "| %s | %s | %s | %s | %.1f | %.1f | %.2f | %.0f dB | %.0f dB | %.1f | %s |\n", r.name.c_str(), r.fx ? "FX" : "Synth", r.cat.c_str(), r.g.pass ? "PASS" : "FAIL", r.g.lufs, r.g.peakPreClipDb, r.g.lowCorr, r.g.modWheelDiffDb, r.g.minMacroDiffDb, r.g.cpuPercent, notes.c_str());
        md << line;
    }
    std::ofstream(std::string(GNARL_BUILD_DIR) + "/preset_report.md") << md.str();
    for (auto& r : rows) if (!r.g.pass) std::printf("  FAIL %s: %s\n", r.name.c_str(), r.g.failures.empty() ? "" : r.g.failures[0].c_str());
    REQUIRE(fails == 0);
}
