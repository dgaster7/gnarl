#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "util/FFT.h"
#include <random>
using namespace gnarl;
TEST_CASE("RealFFT forward/inverse round trip and scaling", "[util]") {
    RealFFT f(11);
    const int n = 2048;
    std::vector<float> x(n), y(n), re(n / 2 + 1), im(n / 2 + 1);
    std::mt19937 r(3); std::uniform_real_distribution<float> u(-1, 1);
    for (auto& v : x) v = u(r);
    f.forward(x.data(), re.data(), im.data());
    // DC bin equals sum
    double s = 0; for (float v : x) s += v;
    REQUIRE(re[0] == Catch::Approx(s).margin(1e-2));
    f.inverse(re.data(), im.data(), y.data());
    double err = 0; for (int i = 0; i < n; ++i) err = std::max(err, (double) std::fabs(x[i] - y[i]));
    REQUIRE(err < 1e-5);
    // single cosine at bin 5 has magnitude n/2
    for (int i = 0; i < n; ++i) x[i] = std::cos(2 * M_PI * 5 * i / n);
    f.forward(x.data(), re.data(), im.data());
    REQUIRE(re[5] == Catch::Approx(n / 2).margin(1e-2));
}
