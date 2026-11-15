// CPU benchmark: factory presets at 48 kHz / 256, 4x oversampling (the default), one held note.
#include <catch2/catch_test_macros.hpp>
#include "engine/PatchJson.h"
#include "engine/Render.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
using namespace gnarl;
namespace fs = std::filesystem;

TEST_CASE("CPU benchmark of factory presets", "[cpu]") {
    std::vector<std::pair<double, std::string>> res;
    for (auto& e : fs::recursive_directory_iterator(fs::path(GNARL_SOURCE_DIR) / "content" / "presets")) {
        if (e.path().extension() != ".gnarl") continue;
        std::ifstream in(e.path()); std::stringstream ss; ss << in.rdbuf();
        std::string err; Json j = Json::parse(ss.str(), &err);
        Patch p; patchFromJson(j, p, &err);
        const bool fx = j["plugin"].str() == "GNARL FX";
        p.set("oversampling", 2);
        RenderSpec s; s.fx = fx; s.input = TestInput::Saw110; s.seconds = 4; s.blockSize = 256; s.offline = false;
        s.notes = {{0.0, 4.0, 36, 100, 64}};
        auto r = render(p, s);
        res.push_back({r.cpuSeconds / s.seconds * 100.0, (fx ? "FX: " : "") + p.name});
    }
    std::sort(res.begin(), res.end());
    double sum = 0; for (auto& r : res) sum += r.first;
    const double median = res[res.size() / 2].first, mean = sum / (double) res.size();
    std::ostringstream md;
    md << "# CPU benchmark\n\nOne held note, 48 kHz, 256-sample blocks, 4x oversampling, single core, this machine. Percent of one core.\n\n";
    md << "Presets: " << res.size() << ". Median " << median << "%, mean " << mean << "%, max " << res.back().first << "% (" << res.back().second << ").\n\n";
    int over25 = 0; for (auto& r : res) over25 += r.first > 25;
    md << "Presets over 25%: " << over25 << "\n\n| Preset | CPU % |\n|---|---|\n";
    for (auto it = res.rbegin(); it != res.rend(); ++it) md << "| " << it->second << " | " << it->first << " |\n";
    std::ofstream(std::string(GNARL_BUILD_DIR) + "/cpu_benchmark.md") << md.str();
    std::printf("CPU: median %.2f%%, mean %.2f%%, max %.2f%% (%s), over 25%%: %d\n", median, mean, res.back().first, res.back().second.c_str(), over25);
    REQUIRE(median < 10.0);
}
