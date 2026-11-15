#include <catch2/catch_test_macros.hpp>
#include "params/Registry.h"
#include <set>
using namespace gnarl;

TEST_CASE("registry: ids unique, defaults in range, contexts refer to real params", "[registry]") {
    auto& R = Registry::get();
    std::set<std::string> ids;
    for (auto& p : R.all()) {
        INFO(p.id);
        REQUIRE(ids.insert(p.id).second);
        REQUIRE(p.min < p.max);
        REQUIRE(p.def >= p.min);
        REQUIRE(p.def <= p.max);
        REQUIRE(!p.doc.empty());
        for (auto& c : p.context) { INFO("context " << c.id); REQUIRE(R.indexOf(c.id) >= 0); }
        for (auto& r : p.contextRoutes) { INFO("route dest " << r.dest); REQUIRE(R.indexOf(r.dest) >= 0); }
        REQUIRE(std::abs(p.fromNorm(p.toNorm(p.def)) - p.def) <= 1e-3f * (p.max - p.min));
    }
}

TEST_CASE("registry: every parameter name is unique (hosts show names in automation lists)", "[registry]") {
    std::set<std::string> names;
    for (auto& p : Registry::get().all()) { INFO(p.id << " " << p.name); REQUIRE(names.insert(p.name).second); }
}
