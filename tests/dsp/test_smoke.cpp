#include <catch2/catch_test_macros.hpp>
#include "analysis/Analysis.h"
#include "engine/Render.h"
#include "osc/Wavetable.h"
using namespace gnarl;
namespace A = gnarl::analysis;

TEST_CASE("smoke: init patch makes a note; FX init is transparent", "[smoke]") {
    REQUIRE(numFactoryTables() == 64);
    Patch p;
    RenderSpec s; s.seconds = 1.0; s.notes = {{0.05, 0.6, 48, 100, 64}};
    auto r = render(p, s);
    REQUIRE(r.finite);
    double rms = A::rms(r.L, 4800, 28800);
    INFO("init rms " << A::toDb(rms) << " dBFS, cpu " << r.cpuSeconds);
    REQUIRE(A::toDb(rms) > -30);
    REQUIRE(A::rms(r.L, 0, 2000) == 0.0);           // silence before the note
    REQUIRE(A::toDb(A::rms(r.L, 40000)) < -90);      // silence after release
    // FX plugin with defaults: output = input * master level, through the clipper's linear region
    RenderSpec f; f.fx = true; f.input = TestInput::Chord; f.inputGain = 0.3f; f.seconds = 0.5;
    Patch fp(true); fp.set("master_level", 1.0f);
    auto fr = render(fp, f);
    std::vector<float> inL(fr.L.size()), inR(fr.L.size());
    generateInput(TestInput::Chord, inL, inR, 48000, f.seed, 0.3f);
    INFO("fx passthrough max diff " << A::maxAbsDiff(fr.L, inL));
    REQUIRE(A::maxAbsDiff(fr.L, inL) < 1e-6);
}
