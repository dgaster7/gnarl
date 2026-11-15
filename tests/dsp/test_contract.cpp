// Direct tests of parameter-contract clauses that need a dedicated measurement.
#include <catch2/catch_test_macros.hpp>
#include "analysis/Analysis.h"
#include "engine/Render.h"
#include "params/Registry.h"
using namespace gnarl;
namespace A = gnarl::analysis;

static int moduleIndex(const char* id) { auto& v = fxModuleIds(); for (size_t i = 0; i < v.size(); ++i) if (v[i] == id) return (int) i; return -1; }

TEST_CASE("contract: 'mono below X Hz' really is mono below X; width at 1 is unchanged", "[contract]") {
    for (float x : {80.f, 150.f, 400.f}) {
        Patch p(true);
        p.set("fx_width_on", 1); p.set("fx_width_mono_on", 1); p.set("fx_width_mono_hz", x);
        p.structure->placeModule(moduleIndex("width"));
        RenderSpec s; s.fx = true; s.input = TestInput::Noise; s.seconds = 3; s.inputGain = 0.3f;
        auto r = render(p, s);
        // side energy below X relative to mid energy below X
        std::vector<float> m(r.L.size()), sd(r.L.size());
        for (size_t i = 0; i < m.size(); ++i) { m[i] = 0.5f * (r.L[i] + r.R[i]); sd[i] = 0.5f * (r.L[i] - r.R[i]); }
        double mid = A::bandEnergyDb(m, 48000, 20, x, 9600), side = A::bandEnergyDb(sd, 48000, 20, x, 9600);
        INFO("X " << x << " Hz: side below X is " << side - mid << " dB under mid");
        REQUIRE(side - mid < -20);
        // above 2X the stereo image is preserved
        double midHi = A::bandEnergyDb(m, 48000, 3 * x, 16000, 9600), sideHi = A::bandEnergyDb(sd, 48000, 3 * x, 16000, 9600);
        REQUIRE(std::fabs(sideHi - midHi) < 1.0);
    }
    // neutral width, mono off: bit-identical
    Patch p(true); p.set("fx_width_on", 1); p.set("master_level", 1); p.set("fx_clip_on", 0); p.structure->placeModule(moduleIndex("width"));
    RenderSpec s; s.fx = true; s.input = TestInput::Noise; s.seconds = 0.5; s.inputGain = 0.3f;
    auto r = render(p, s);
    std::vector<float> L(r.L.size()), R(r.L.size()); generateInput(TestInput::Noise, L, R, 48000, s.seed, s.inputGain);
    REQUIRE(A::maxAbsDiff(r.L, L) == 0.0);
}

TEST_CASE("contract: dry/wet of latency modules is latency-aligned (no comb filtering at 50%)", "[contract]") {
    for (const char* m : {"dist1", "mbd", "spectral", "tamer"}) {
        Patch p(true);
        const std::string id = m;
        p.set("fx_" + id + "_on", 1); p.set("fx_" + id + "_mix", 0.5f); p.set("master_level", 1); p.set("fx_clip_on", 0);
        if (id == "dist1") p.set("fx_dist1_drive", 0); // linear: wet == delayed dry
        p.structure->placeModule(moduleIndex(m));
        RenderSpec s; s.fx = true; s.input = TestInput::Noise; s.seconds = 1.0; s.inputGain = 0.3f;
        auto r = render(p, s);
        std::vector<float> L(r.L.size()), R(r.L.size()); generateInput(TestInput::Noise, L, R, 48000, s.seed, s.inputGain);
        // with no processing active the wet path is the same delayed signal: 50% mix must equal the dry delayed by the reported latency
        INFO(m << " latency " << r.latency);
        REQUIRE(r.latency >= 0);
        REQUIRE(A::maxAbsDiff(r.L, L, r.latency, (size_t) r.latency + 10) < 1e-6);
    }
}
