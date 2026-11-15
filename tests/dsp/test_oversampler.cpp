#include <catch2/catch_test_macros.hpp>
#include "util/Oversampler.h"
#include "analysis/Analysis.h"
#include <cmath>
using namespace gnarl;

static std::vector<float> roundTrip(Oversampler::Kind kind, int fl2, double f, double sr, int n, std::vector<float>* top = nullptr) {
    Oversampler os; os.prepare(1, 256, 3, kind); os.setFactorLog2(fl2);
    std::vector<float> in((size_t) n), out((size_t) n);
    for (int i = 0; i < n; ++i) in[(size_t) i] = (float) std::sin(2 * M_PI * f * i / sr);
    for (int p = 0; p < n; p += 256) {
        const float* ip[1] = {in.data() + p}; float* op[1] = {out.data() + p};
        os.up(ip, 256);
        if (top) top->insert(top->end(), os.data(0), os.data(0) + (256 << fl2));
        os.down(op, 256);
    }
    return out;
}

TEST_CASE("oversampler: passband gain, linear-phase latency, image rejection", "[util][os]") {
    const double sr = 48000;
    for (int fl2 = 1; fl2 <= 3; ++fl2) {
        // Linear phase: output == input delayed by latency (within -80 dB), for 1 kHz and 15 kHz.
        Oversampler probe; probe.prepare(1, 256, 3, Oversampler::Kind::LinearPhase); probe.setFactorLog2(fl2);
        int lat = probe.latency();
        for (double f : {1000.0, 15000.0}) {
            auto out = roundTrip(Oversampler::Kind::LinearPhase, fl2, f, sr, 16384);
            std::vector<float> ref(out.size());
            for (size_t i = 0; i < ref.size(); ++i) ref[i] = (float) std::sin(2 * M_PI * f * ((double) i - lat) / sr);
            double err = analysis::diffRms(out, ref, 0, 4096);
            INFO("fir fl2 " << fl2 << " f " << f << " lat " << lat << " err dB " << analysis::toDb(err));
            REQUIRE(analysis::toDb(err) < (f > 10000 ? -50 : -80));
        }
        // IIR: passband amplitude within 0.1 dB at 1 kHz and 18 kHz
        for (double f : {1000.0, 18000.0}) {
            auto out = roundTrip(Oversampler::Kind::Iir, fl2, f, sr, 16384);
            double g = analysis::rms(out, 4096) * std::sqrt(2.0);
            INFO("iir fl2 " << fl2 << " f " << f << " gain dB " << analysis::toDb(g));
            REQUIRE(std::fabs(analysis::toDb(g)) < 0.1);
        }
        // Image rejection at the top rate: a 10 kHz tone must not create an image at 38 kHz (2x stage)
        std::vector<float> top;
        roundTrip(Oversampler::Kind::Iir, fl2, 10000, sr, 16384, &top);
        double hiSr = sr * (1 << fl2);
        double img = analysis::bandEnergyDb(top, hiSr, 36000, 40000, 8192 << fl2);
        double sig = analysis::bandEnergyDb(top, hiSr, 9000, 11000, 8192 << fl2);
        INFO("iir image rejection fl2 " << fl2 << ": " << sig - img << " dB");
        REQUIRE(sig - img > 90);
    }
}
