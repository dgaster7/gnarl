// Check 1: automated parameter audit, generated from the registry so nothing can be forgotten.
// Writes build/parameter_audit.md. Every test is real rendering through the full engine.
#include <catch2/catch_test_macros.hpp>
#include "analysis/Analysis.h"
#include "engine/Audit.h"
#include "engine/PatchJson.h"
#include "engine/Render.h"
#include "params/Registry.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

using namespace gnarl;
namespace A = gnarl::analysis;

namespace {

constexpr double kEffectThresholdDb = -90;   // a parameter must change the output RMS-difference by more than this
constexpr double kZeroThreshold = 1e-6;      // -120 dBFS max abs difference
constexpr double kWetLeakThresholdDb = -100; // residual with wet input silenced at 100% mix

bool isFxModuleParam(const std::string& id) { return id.rfind("fx_", 0) == 0 && id.rfind("fx_clip", 0) != 0; }
std::string moduleOf(const std::string& id) { auto e = id.find('_', 3); return id.substr(3, e - 3); }
int moduleIndex(const std::string& m) {
    auto& ids = fxModuleIds();
    for (size_t i = 0; i < ids.size(); ++i) if (ids[i] == m) return (int) i;
    return -1;
}

struct Case {
    Patch patch;
    RenderSpec spec;
};

Case makeCase(int idx) {
    auto& R = Registry::get();
    const auto& p = R[idx];
    Case c;
    c.spec.sampleRate = 48000;
    c.spec.blockSize = 128;
    c.spec.seconds = 1.0;
    c.spec.fx = (p.plugins == kFx);
    c.spec.input = TestInput::Chord;
    c.spec.notes = {{0.05, 0.85, 48, 100, 64}};
    if (p.id == "voices" || p.id == "legato" || p.id.rfind("glide", 0) == 0)
        c.spec.notes = {{0.05, 0.5, 48, 100, 64}, {0.3, 0.8, 55, 100, 64}, {0.6, 0.9, 60, 100, 64}};
    if (p.id == "bend_range") c.spec.ccs = {{0.1, -1, 16383}};
    if (p.id.find("_r") != std::string::npos && p.id.rfind("env", 0) == 0) c.spec.notes = {{0.05, 0.4, 48, 100, 64}};
    if (p.id == "fx_vocoder_bands" || p.id.rfind("fx_vocoder", 0) == 0 || p.id == "ef_sidechain" || p.id == "fx_reso_sc") {
        c.spec.fx = true; c.spec.sidechain = TestInput::Drums;
    }
    if (p.id == "fxp_env_trigger") { c.spec.fx = true; c.spec.input = TestInput::Drums; c.spec.notes = {{0.4, 0.6, 48, 100, 64}}; }
    for (auto& v : p.context) c.patch.set(v.id, v.value);
    for (auto& r : p.contextRoutes) {
        if (r.source.rfind("#route", 0) == 0) {
            auto colon = r.source.find(':');
            int k = std::stoi(r.source.substr(6, colon - 6)) - 1;
            auto& rt = c.patch.structure->routes[(size_t) k];
            rt.source = modSourceFromId(r.source.substr(colon + 1));
            rt.dest = R.indexOf(r.dest);
            c.patch.values[(size_t) idx] = 0.7f; // depth param itself; the test then varies it
        } else {
            c.patch.addRoute(modSourceFromId(r.source), r.dest, r.depth);
        }
    }
    if (isFxModuleParam(p.id)) {
        int m = moduleIndex(moduleOf(p.id));
        if (m >= 0) c.patch.structure->placeModule(m);
    }
    return c;
}

RenderResult run(const Case& c) { return render(c.patch, c.spec); }

struct Row {
    std::string id, role;
    std::string effect = "-", zero = "-", wet = "-", automation = "-", roundtrip = "-", modEffect = "-", modZero = "-";
    std::string notes;
    bool failed = false;
};

std::string fmt(const char* f, double v) { char b[64]; std::snprintf(b, sizeof b, f, v); return b; }

double diffDb(const RenderResult& a, const RenderResult& b) {
    int lag = a.latency - b.latency;
    double d = std::max(A::diffRms(a.L, b.L, lag), A::diffRms(a.R, b.R, lag));
    return A::toDb(d);
}
double maxDiff(const RenderResult& a, const RenderResult& b) {
    int lag = a.latency - b.latency;
    return std::max(A::maxAbsDiff(a.L, b.L, lag), A::maxAbsDiff(a.R, b.R, lag));
}

bool zeroRole(Role r) { return r == Role::Mix || r == Role::Amount || r == Role::Drive || r == Role::Depth || r == Role::Send || r == Role::Level || r == Role::Neutral; }

Row auditParam(int idx) {
    auto& R = Registry::get();
    const auto& p = R[idx];
    Row row; row.id = p.id;
    const char* roles[] = {"Continuous", "Mix", "Amount", "Drive", "Depth", "Send", "Level", "Neutral", "Choice", "Toggle", "Integer"};
    row.role = roles[(int) p.role];
    audit::clear();
    audit::active = true;

    // ---------- Effect test ----------
    {
        Case c = makeCase(idx);
        std::vector<float> vals = {p.min, p.def, p.max, p.fromNorm(0.5f)};
        if (p.role == Role::Choice || p.role == Role::Integer) for (float v = p.min; v <= p.max; v += 1) vals.push_back(v);
        std::vector<RenderResult> rs;
        for (float v : vals) { Case cc = c; cc.patch.values[(size_t) idx] = v; rs.push_back(run(cc)); }
        double best = -600; bool finite = true;
        for (auto& r : rs) finite &= r.finite;
        for (size_t i = 0; i < rs.size(); ++i) for (size_t j = i + 1; j < rs.size(); ++j) best = std::max(best, diffDb(rs[i], rs[j]));
        // every distinct choice must be distinguishable from at least one other value
        bool allChoicesDistinct = true;
        if (p.role == Role::Choice) {
            for (size_t i = 4; i < rs.size(); ++i) {
                double bi = -600;
                for (size_t j = 4; j < rs.size(); ++j) if (i != j) bi = std::max(bi, diffDb(rs[i], rs[j]));
                if (bi < kEffectThresholdDb) { allChoicesDistinct = false; row.notes += "choice " + std::to_string((int) vals[i]) + " indistinct; "; }
            }
        }
        bool ok = finite && best > kEffectThresholdDb && allChoicesDistinct;
        row.effect = std::string(ok ? "PASS " : "FAIL ") + fmt("%.1f dB", best);
        if (!finite) row.notes += "non-finite output; ";
        row.failed |= !ok;
    }
    // ---------- Zero test ----------
    if (zeroRole(p.role)) {
        Case c = makeCase(idx);
        c.patch.values[(size_t) idx] = p.zeroValue();
        RenderResult atZero = run(c);
        // Reference 1: the stage owned by this parameter hard-removed (audit hook inside the DSP).
        for (int& h : audit::hookHits) h = 0;
        audit::removed[idx] = 1;
        RenderResult remHook = run(c);
        int hits = audit::hookHits[idx];
        audit::removed[idx] = 0;
        double d = hits > 0 ? maxDiff(atZero, remHook) : 1e9;
        // Reference 2 (FX modules / routes): the whole module or route removed from the patch.
        bool structuralAvail = false;
        Case removed = c;
        if (isFxModuleParam(p.id)) {
            int m = moduleIndex(moduleOf(p.id));
            for (auto& s : removed.patch.structure->fxSlots) if (s == m) s = -1;
            structuralAvail = true;
        }
        if (p.id.rfind("mod", 0) == 0 && p.id.find("_depth") != std::string::npos) {
            int k = std::stoi(p.id.substr(3)) - 1;
            removed.patch.structure->routes[(size_t) k].bypass = true;
            structuralAvail = true;
        }
        if (structuralAvail) d = std::min(d, maxDiff(atZero, run(removed)));
        bool hookOk = structuralAvail || hits > 0;
        bool ok = d <= kZeroThreshold && hookOk;
        if (p.id == "master_level") { d = std::max(A::peak(atZero.L), A::peak(atZero.R)); ok = d == 0.0; }
        row.zero = std::string(ok ? "PASS " : "FAIL ") + fmt("%.1f dBFS", A::toDb(d));
        if (!hookOk) row.notes += "no removal hook reached; ";
        row.failed |= !ok;
    }
    // ---------- Full-wet test ----------
    if (p.role == Role::Mix) {
        Case c = makeCase(idx);
        c.patch.values[(size_t) idx] = p.max;
        audit::zeroWetParam = idx;
        RenderResult r = run(c);
        audit::zeroWetParam = -1;
        double lvl = A::toDb(std::max(A::rms(r.L), A::rms(r.R)));
        bool ok = lvl < kWetLeakThresholdDb;
        row.wet = std::string(ok ? "PASS " : "FAIL ") + fmt("%.1f dBFS", lvl);
        row.failed |= !ok;
    }
    // ---------- Automation (click) test ----------
    if (!p.isDiscrete() && p.smoothMs > 0) {
        Case c = makeCase(idx);
        c.spec.blockSize = 32;
        const double t0 = 0.3, t1 = 0.35;
        Case cmin = c, cmax = c;
        cmin.patch.values[(size_t) idx] = p.min;
        cmax.patch.values[(size_t) idx] = p.max;
        RenderResult rmin = run(cmin), rmax = run(cmax);
        Case sw = cmin;
        sw.spec.automation = [idx, &p, t0, t1](Engine& e, double t) {
            double f = std::clamp((t - t0) / (t1 - t0), 0.0, 1.0);
            e.setParam(idx, p.fromNorm((float) f));
        };
        RenderResult rs = run(sw);
        size_t a = (size_t) ((t0 - 0.01) * 48000), b = (size_t) ((t1 + 0.03) * 48000);
        double staticHf = std::max({A::maxHfBurstDb(rmin.L, 48000, a, b), A::maxHfBurstDb(rmax.L, 48000, a, b),
                                    A::maxHfBurstDb(rmin.R, 48000, a, b), A::maxHfBurstDb(rmax.R, 48000, a, b)});
        double sweepHf = std::max(A::maxHfBurstDb(rs.L, 48000, a, b), A::maxHfBurstDb(rs.R, 48000, a, b));
        double excess = sweepHf - staticHf;
        bool ok = rs.finite && (excess < 10 || sweepHf < -70);
        row.automation = std::string(ok ? "PASS " : "FAIL ") + fmt("%+.1f dB", excess);
        row.failed |= !ok;
    }
    // ---------- Round trip ----------
    {
        std::mt19937 rng((unsigned) idx * 7919u + 1);
        Patch pt;
        float v = p.fromNorm(std::uniform_real_distribution<float>(0, 1)(rng));
        pt.values[(size_t) idx] = v;
        bool ok = true;
        for (int fxp = 0; fxp < 2; ++fxp) {
            if (!(p.plugins & (fxp ? kFx : kSynth))) continue;
            std::string text = patchToJson(pt, fxp).dump();
            Patch back; std::string err;
            ok &= patchFromJson(Json::parse(text, &err), back, &err) && err.empty();
            ok &= back.values[(size_t) idx] == v;
        }
        row.roundtrip = ok ? "PASS" : "FAIL";
        row.failed |= !ok;
    }
    // ---------- As a modulation destination ----------
    if (p.modulatable && !p.isDiscrete()) {
        Case c = makeCase(idx);
        const bool bip = p.role == Role::Depth || p.role == Role::Neutral;
        c.patch.values[(size_t) idx] = bip ? p.neutral : p.min;
        int k = c.patch.addRoute(MS_Macro8, p.id, 1.0f);
        Case lo = c, hi = c;
        lo.patch.set("macro8", 0); hi.patch.set("macro8", 1);
        RenderResult a = run(lo), b = run(hi);
        double d = diffDb(a, b);
        bool ok = k >= 0 && d > kEffectThresholdDb && a.finite && b.finite;
        row.modEffect = std::string(ok ? "PASS " : "FAIL ") + fmt("%.1f dB", d);
        row.failed |= !ok;
        // depth 0 and bypass must equal no route at all
        Case depth0 = hi; depth0.patch.set("mod" + std::to_string(k + 1) + "_depth", 0);
        Case byp = hi; byp.patch.structure->routes[(size_t) k].bypass = true;
        Case none = hi; none.patch.structure->routes[(size_t) k] = ModRoute();
        RenderResult r0 = run(depth0), rb = run(byp), rn = run(none);
        double z = std::max(maxDiff(r0, rn), maxDiff(rb, rn));
        bool zok = z <= kZeroThreshold;
        row.modZero = std::string(zok ? "PASS " : "FAIL ") + fmt("%.1f dBFS", A::toDb(z));
        row.failed |= !zok;
    }
    audit::clear();
    return row;
}

} // namespace

TEST_CASE("Check 1: parameter audit (every registered parameter)", "[audit]") {
    auto& R = Registry::get();
    const int n = R.size();
    std::vector<Row> rows((size_t) n);
    std::atomic<int> next{0};
    unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    const char* only = std::getenv("GNARL_AUDIT_FILTER");
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < threads; ++t)
        pool.emplace_back([&] {
            for (int i; (i = next++) < n;) {
                if (only && R[i].id.find(only) == std::string::npos) { rows[(size_t) i].id = ""; continue; }
                rows[(size_t) i] = auditParam(i);
            }
        });
    for (auto& t : pool) t.join();

    int fails = 0, total = 0;
    std::ostringstream md;
    for (auto& r : rows) { if (r.id.empty()) continue; ++total; fails += r.failed; }
    md << "# GNARL parameter audit (Check 1)\n\n";
    md << "Generated by `gnarl_tests \"[audit]\"`. " << total << " parameters, " << (total - fails) << " pass, " << fails << " fail.\n\n";
    md << "Thresholds: effect > " << kEffectThresholdDb << " dB RMS difference between min/default/max/mid (all choices must be distinct); "
       << "zero test max abs difference vs. stage removed <= -120 dBFS (latency-aligned); full-wet leakage with the wet input silenced < "
       << kWetLeakThresholdDb << " dBFS; automation sweep min->max in 50 ms adds < 10 dB of >8 kHz burst energy over the static extremes "
       << "(or stays below -70 dB); round trip exact through preset JSON; as a modulation destination a full-depth Macro route must change the output, "
       << "and depth 0 / bypass must equal no route bit-for-bit.\n\n";
    md << "| Parameter | Role | Effect | Zero | Full wet | Automation | Round trip | Mod effect | Mod zero | Notes |\n|---|---|---|---|---|---|---|---|---|---|\n";
    for (auto& r : rows) {
        if (r.id.empty()) continue;
        md << "| `" << r.id << "` | " << r.role << " | " << r.effect << " | " << r.zero << " | " << r.wet << " | " << r.automation << " | "
           << r.roundtrip << " | " << r.modEffect << " | " << r.modZero << " | " << r.notes << " |\n";
    }
    std::ofstream(std::string(GNARL_BUILD_DIR) + "/parameter_audit.md") << md.str();
    std::printf("parameter audit: %d/%d pass, report at %s/parameter_audit.md\n", total - fails, total, GNARL_BUILD_DIR);
    for (auto& r : rows) if (r.failed) std::printf("  FAIL %s  effect=%s zero=%s wet=%s auto=%s rt=%s mod=%s modzero=%s %s\n", r.id.c_str(), r.effect.c_str(), r.zero.c_str(), r.wet.c_str(), r.automation.c_str(), r.roundtrip.c_str(), r.modEffect.c_str(), r.modZero.c_str(), r.notes.c_str());
    REQUIRE(fails == 0);
}

TEST_CASE("Check 1: range test (random values, many rates and block sizes)", "[audit][range]") {
    auto& R = Registry::get();
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        for (int fx = 0; fx < 2; ++fx) {
            std::mt19937 rng((unsigned) sr + (unsigned) fx);
            Patch pt;
            for (int i : R.forPlugin(fx ? kFx : kSynth)) {
                auto& p = R[i];
                if (p.id == "master_level") continue;
                pt.values[(size_t) i] = p.fromNorm(std::uniform_real_distribution<float>(0, 1)(rng));
            }
            pt.set("fx_clip_on", 1);
            for (int m = 0; m < (int) fxModuleIds().size(); ++m) pt.structure->placeModule(m);
            for (int k = 0; k < 64; ++k) {
                int src = std::uniform_int_distribution<int>(0, MS_Count - 1)(rng);
                auto mods = R.forPlugin(fx ? kFx : kSynth);
                int dst = mods[(size_t) std::uniform_int_distribution<int>(0, (int) mods.size() - 1)(rng)];
                if (!R[dst].modulatable || R[dst].isDiscrete()) continue;
                pt.addRoute(src, R[dst].id, std::uniform_real_distribution<float>(-1, 1)(rng));
            }
            RenderSpec s;
            s.sampleRate = sr; s.fx = fx; s.input = TestInput::Noise; s.randomBlockSizes = true; s.blockSize = 2048;
            s.seconds = 10000.0 * 140 / sr; // ~10,000 blocks at the mean random block size
            s.seconds = std::min(s.seconds, 12.0);
            for (int k = 0; k < 40; ++k) s.notes.push_back({k * 0.3, k * 0.3 + 0.25, 30 + (k * 7) % 60, 1 + (k * 37) % 127, 64});
            auto r = render(pt, s);
            INFO("sr " << sr << " fx " << fx);
            REQUIRE(r.finite);
            double pk = std::max(A::peak(r.L), A::peak(r.R));
            REQUIRE(pk <= 1.0001); // bounded by the final clipper (ceiling <= 0 dBFS)
            double audioSec = s.seconds;
            REQUIRE(r.cpuSeconds < audioSec * 4); // no denormal stalls (all modules on, random values)
        }
    }
}
