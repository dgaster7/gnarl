// gnarl_presetgen: designs the factory banks in code, calibrates each preset's level by measurement,
// runs the section-11 gates and writes JSON presets + build/preset_report.md.
//   gnarl_presetgen <content-dir> <report.md> [batch]
#include "engine/PatchJson.h"
#include "osc/Wavetable.h"
#include "engine/PresetGates.h"
#include "analysis/Analysis.h"
#include "params/Registry.h"
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

using namespace gnarl;
namespace fs = std::filesystem;

namespace {

int mod(const char* id) { auto& ids = fxModuleIds(); for (size_t i = 0; i < ids.size(); ++i) if (ids[i] == id) return (int) i; return -1; }

struct D {
    Patch p;
    std::mt19937 rng;
    bool fx;
    int macrosUsed = 0;
    D(bool f, uint32_t seed) : p(f), rng(seed), fx(f) { p.structure->fxSlots.fill(-1); }
    float r(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }
    int ri(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }
    template <typename T> T pick(std::initializer_list<T> l) { auto it = l.begin(); std::advance(it, ri(0, (int) l.size() - 1)); return *it; }
    void set(const std::string& id, float v) { p.set(id, v); }
    void route(int src, const std::string& dest, float depth, int curve = RC_Linear) { p.addRoute(src, dest, depth, curve); }
    void macro(int k, const std::string& name, std::initializer_list<std::pair<std::string, float>> dests) {
        p.structure->macroNames[(size_t) k] = name;
        for (auto& [d, depth] : dests) p.addRoute(MS_Macro1 + k, d, depth);
    }
    void fxOn(const char* m) { int i = mod(m); p.structure->placeModule(i); set(std::string("fx_") + m + "_on", 1); }
    void div(const std::string& lfo, const char* name) { set(lfo + "_sync", 1); auto& d = syncDivisions(); for (size_t i = 0; i < d.size(); ++i) if (std::string(d[i].name) == name) set(lfo + "_div", (float) i); }
    void lfoShape(int k, std::vector<ShapePoint> a) { p.structure->lfo[(size_t) k].a = std::move(a); }
};

std::vector<ShapePoint> sawDown() { return {{0, 1, 0}, {1, -1, 0}}; }
std::vector<ShapePoint> squareShape() { return {{0, 1, 0}, {0.5f, 1, 0}, {0.501f, -1, 0}, {1, -1, 0}}; }
std::vector<ShapePoint> wompShape() { return {{0, -1, 0}, {0.15f, 1, -0.6f}, {0.5f, 0.2f, 0.4f}, {0.75f, 0.8f, -0.3f}, {1, -1, 0.6f}}; }
std::vector<ShapePoint> stepsShape(D& d, int n) { std::vector<ShapePoint> v; for (int i = 0; i < n; ++i) { float y = d.r(-1, 1); v.push_back({(float) i / n, y, 0}); v.push_back({(float) (i + 1) / n - 0.001f, y, 0}); } v.back().x = 1; return v; }

// Targets that are always audible, used to fill unused macros and to repair a macro that measured inactive.
struct Fallback { const char* name; std::function<void(D&, int src)> apply; };
std::vector<Fallback> fallbacks(bool fx) {
    std::vector<Fallback> v = {
        {"Grit", [](D& d, int s) { d.fxOn("dist2"); if (d.p.get("fx_dist2_drive") < 0.05f) d.set("fx_dist2_drive", 0.05f); d.route(s, "fx_dist2_drive", 0.4f); }},
        {"Width", [](D& d, int s) { d.fxOn("chorus"); d.set("fx_chorus_mix", 0.f); d.route(s, "fx_chorus_mix", 0.5f); }},
        {"Space", [](D& d, int s) { d.fxOn("reverb"); d.set("fx_reverb_mix", 0.f); d.set("fx_reverb_decay", 1.4f); d.set("fx_reverb_duck", 0.5f); d.route(s, "fx_reverb_mix", 0.35f); }},
        {"Bits", [](D& d, int s) { d.fxOn("crush"); d.set("fx_crush_mix", 0.f); d.set("fx_crush_bits", 6); d.route(s, "fx_crush_mix", 0.6f); }},
        {"Tone", [](D& d, int s) { d.fxOn("eq"); d.route(s, "fx_eq_g4", -0.4f); }},
        {"Chaos", [](D& d, int s) { d.fxOn("fshift"); d.set("fx_fshift_hz", 7); d.set("fx_fshift_mix", 0.f); d.route(s, "fx_fshift_mix", 0.4f); }},
        {"Phase", [](D& d, int s) { d.fxOn("phaser"); d.set("fx_phaser_mix", 0.f); d.route(s, "fx_phaser_mix", 0.6f); }},
        {"Echo", [](D& d, int s) { d.fxOn("delay"); d.set("fx_delay_sync", 1); d.set("fx_delay_div", 12); d.set("fx_delay_mix", 0.f); d.set("fx_delay_fb", 0.3f); d.route(s, "fx_delay_mix", 0.3f); }},
    };
    if (!fx) v.insert(v.begin() + 1, {"Sub", [](D& d, int s) { d.route(s, "sub_level", 0.35f); }});
    return v;
}
void clearRoutesFrom(D& d, int src) { for (int k = 0; k < kNumRoutes; ++k) { auto& r = d.p.structure->routes[(size_t) k]; if (r.source == src) { r = ModRoute(); d.p.set("mod" + std::to_string(k + 1) + "_depth", 0); } } }
bool nameUsed(D& d, const std::string& n) { for (auto& m : d.p.structure->macroNames) if (m == n) return true; return false; }
void repairMacro(D& d, int k, int attempt) {
    clearRoutesFrom(d, MS_Macro1 + k);
    auto fb = fallbacks(d.fx);
    for (size_t i = 0; i < fb.size(); ++i) {
        auto& f = fb[(i + (size_t) attempt + (size_t) k) % fb.size()];
        if (nameUsed(d, f.name)) continue;
        d.p.structure->macroNames[(size_t) k] = f.name;
        f.apply(d, MS_Macro1 + k);
        return;
    }
}
void repairWheel(D& d, int attempt) {
    clearRoutesFrom(d, MS_ModWheel);
    auto fb = fallbacks(d.fx);
    fb[(size_t) (attempt + 3) % fb.size()].apply(d, MS_ModWheel);
}

// Common finishing: sub, standard macros not set by the recipe, mod wheel, FX safety.
void finish(D& d, const std::string& category) {
    // every preset must have all 8 macros; fill any unused with sensible defaults
    const auto& names = d.p.structure->macroNames;
    auto unused = [&](int k) { return names[(size_t) k].rfind("Macro ", 0) == 0; };
    int fbi = 0;
    auto fb = fallbacks(d.fx);
    for (int k = 0; k < 8; ++k) {
        if (!unused(k)) continue;
        while (fbi < (int) fb.size() && nameUsed(d, fb[(size_t) fbi].name)) ++fbi;
        auto& f = fb[(size_t) (fbi++ % (int) fb.size())];
        d.p.structure->macroNames[(size_t) k] = f.name;
        f.apply(d, MS_Macro1 + k);
    }
    // mod wheel: always does something musical
    bool wheel = false;
    for (auto& r : d.p.structure->routes) if (r.source == MS_ModWheel) wheel = true;
    if (!wheel) { d.route(MS_ModWheel, d.fx ? "f1_cutoff" : "lfo2_rate", 0.4f); if (!d.fx) { d.route(MS_Lfo2, "f1_cutoff", 0.0f); d.route(MS_ModWheel, "osca_sh_spectrum", 0.6f); } }
    // keep the low end mono and the final stage on
    d.set("fx_clip_on", 1); d.set("fx_clip_mode", 0); d.set("fx_clip_ceiling", -0.3f);
    if (!d.fx && d.p.structure->slotOf(mod("width")) < 0) { d.fxOn("width"); d.set("fx_width_mono_on", 1); d.set("fx_width_mono_hz", 120); }
    else if (!d.fx) { d.set("fx_width_mono_on", 1); d.set("fx_width_mono_hz", 120); }
    (void) category;
}

// ------------------------------------------------------------------ synth recipes
void neuro(D& d, int i) {
    // detuned saws -> opposing notch/band-pass on a synced LFO -> distortion -> phaser -> distortion -> OTT, clean sub
    int eng = i % 3;
    if (eng == 0) { d.set("osca_engine", OE_HyperSaw); d.set("osca_hs_density", d.r(0.4f, 0.9f)); d.set("osca_hs_spread", d.r(0.2f, 0.5f)); }
    else if (eng == 1) { d.set("osca_engine", OE_VA); d.set("osca_unison", (float) d.pick({3, 5, 7})); d.set("osca_detune", d.r(10, 30)); d.set("osca_spread", 0.6f); }
    else { d.set("osca_engine", OE_Wavetable); d.set("osca_wt_table", (float) d.pick({1, 33, 37, 46})); d.set("osca_wt_pos", d.r(0.2f, 0.8f)); d.set("osca_unison", 3); d.set("osca_detune", 15); }
    d.set("osca_level", 0.7f);
    if (i % 4 == 1) { d.set("oscb_level", 0.45f); d.set("oscb_engine", OE_VA); d.set("oscb_va_shape", 2); d.set("oscb_octave", -1); d.set("oscb_fine", d.r(-12, 12)); }
    d.set("sub_level", d.r(0.45f, 0.65f)); d.set("sub_wave", 0);
    const int ft = d.pick({(int) FT_DualResoBR, (int) FT_Notch24, (int) FT_DualResoBP, (int) FT_Phaser});
    d.set("f1_type", (float) ft); d.set("f1_cutoff", d.r(500, 1600)); d.set("f1_reso", d.r(0.45f, 0.75f)); d.set("f1_sep", d.r(0.3f, 0.7f)); d.set("f1_opposing", (float) (i % 2));
    d.set("f2_type", FT_LP24); d.set("f2_cutoff", d.r(4000, 9000)); d.set("f2_reso", 0.15f);
    d.div("lfo1", d.pick({"1/4", "1/8", "1/8T", "1/16", "1/4T"}));
    d.set("lfo1_mode", 1);
    if (i % 3 == 2) d.lfoShape(0, wompShape());
    d.route(MS_Lfo1, "f1_cutoff", d.r(0.25f, 0.45f));
    d.route(MS_Lfo1, "f1_sep", d.r(-0.4f, 0.4f));
    d.fxOn("dist1"); d.set("fx_dist1_type", (float) d.pick({(int) DT_Tube, (int) DT_Fold, (int) DT_Diode, (int) DT_Rectify, (int) DT_SineFold})); d.set("fx_dist1_drive", d.r(0.35f, 0.6f)); d.set("fx_dist1_tone", d.r(6000, 14000));
    d.fxOn("phaser"); d.set("fx_phaser_rate", d.r(0.1f, 0.6f)); d.set("fx_phaser_depth", 0.6f); d.set("fx_phaser_fb", d.r(0.2f, 0.6f)); d.set("fx_phaser_mix", d.r(0.4f, 0.8f));
    d.fxOn("dist2"); d.set("fx_dist2_type", (float) d.pick({(int) DT_Soft, (int) DT_Hard, (int) DT_Fuzz, (int) DT_Valve})); d.set("fx_dist2_drive", d.r(0.2f, 0.45f));
    d.fxOn("mbcomp"); d.set("fx_mbcomp_depth", d.r(0.3f, 0.6f));
    if (i % 5 == 3) { d.fxOn("comb"); d.set("fx_comb_key", 1); d.set("fx_comb_fb", 0.6f); d.set("fx_comb_mix", 0.3f); }
    if (i % 5 == 4) { d.fxOn("disperse"); d.set("fx_disperse_amount", 0.4f); d.set("fx_disperse_freq", 400); }
    d.macro(0, "Squelch", {{"f1_reso", 0.3f}});
    d.macro(1, "Tear", {{"fx_dist1_drive", 0.35f}, {"fx_dist2_drive", 0.25f}});
    d.macro(2, "Notch", {{"f1_cutoff", 0.25f}});
    d.macro(4, "Width", {{"osca_spread", 0.5f}, {"fx_phaser_mix", 0.3f}});
    d.macro(5, "Sub", {{"sub_level", 0.3f}});
    d.macro(6, "Rate", {{"f1_sep", 0.4f}});
    d.route(MS_ModWheel, "fx_phaser_depth", 0.4f);
    d.route(MS_Velocity, "f1_reso", 0.15f);
}

void squelch(D& d, int i) {
    // resonant ladder / separation LP, fast decay env, glide, velocity -> env depth
    d.set("osca_engine", (float) d.pick({(int) OE_VA, (int) OE_VA, (int) OE_Wavetable, (int) OE_Harmonic}));
    d.set("osca_va_shape", d.pick({2.f, 3.f, 2.5f})); d.set("osca_va_pw", d.r(0.2f, 0.5f));
    d.set("osca_wt_table", (float) d.pick({1, 2, 34, 40})); d.set("osca_wt_pos", d.r(0, 1));
    d.set("osca_ha_partials", 96); d.set("osca_ha_tilt", d.r(-0.2f, 0.3f));
    d.set("osca_level", 0.75f);
    d.set("f1_type", (float) d.pick({(int) FT_Ladder, (int) FT_SepLP24, (int) FT_SepLP36, (int) FT_LP24}));
    d.set("f1_cutoff", d.r(180, 600)); d.set("f1_reso", d.r(0.55f, 0.8f)); d.set("f1_sep", d.r(0.2f, 0.6f)); d.set("f1_drive", d.r(0.1f, 0.4f));
    d.set("f1_env", d.r(0.35f, 0.6f)); d.set("f1_env_src", 1);
    d.set("env2_a", 0.001f); d.set("env2_d", d.r(0.08f, 0.3f)); d.set("env2_s", d.r(0.0f, 0.2f)); d.set("env2_r", 0.1f);
    d.set("voices", 1); d.set("legato", 1); d.set("glide_time", d.r(40, 140)); d.set("glide_legato", 1);
    d.set("sub_level", d.r(0.3f, 0.5f));
    d.route(MS_Velocity, "f1_env", 0.25f);
    if (i % 4 == 0) { // stretch squelch: harmonic stretch on an envelope
        d.set("osca_engine", OE_Harmonic); d.route(MS_Env3, "osca_ha_stretch", d.r(0.3f, 0.7f));
        d.set("env3_d", d.r(0.1f, 0.4f)); d.set("env3_s", 0);
    }
    if (i % 4 == 1) { // mod delay time on a fast LFO into a band-pass
        d.fxOn("moddelay"); d.set("fx_moddelay_time", d.r(2, 9)); d.set("fx_moddelay_rate", d.r(40, 160)); d.set("fx_moddelay_depth", d.r(0.2f, 0.5f));
        d.set("fx_moddelay_fb", d.r(0.4f, 0.75f)); d.set("fx_moddelay_bp", d.r(700, 2500)); d.set("fx_moddelay_mix", 0.45f);
    }
    if (i % 4 == 2) { d.set("f1_type", FT_TriPeak); d.set("f1_shoulder", 0.7f); }
    d.fxOn("dist1"); d.set("fx_dist1_type", (float) d.pick({(int) DT_Tube, (int) DT_Soft, (int) DT_Valve})); d.set("fx_dist1_drive", d.r(0.2f, 0.4f));
    if (i % 3 == 0) { d.fxOn("chorus"); d.set("fx_chorus_mix", 0.25f); }
    d.macro(0, "Squelch", {{"f1_reso", 0.2f}, {"f1_env", 0.25f}});
    d.macro(1, "Decay", {{"env2_d", 0.35f}});
    d.macro(2, "Liquid", {{"f1_cutoff", 0.2f}});
    d.macro(3, "Glide", {{"glide_time", 0.5f}, {"osca_sh_wave", 0.0f}});
    d.macro(3, "Glide", {{"osca_sh_spectrum", 0.5f}});
    d.route(MS_ModWheel, "f1_cutoff", 0.25f);
}

void talking(D& d, int i) {
    // Formant engine with Index and Formant Shift on separate LFOs, Spread on a macro, vowel filter morph
    d.set("osca_engine", OE_GrainFormant); d.set("osca_wt_table", (float) d.pick({4, 7, 8, 35, 44, 61}));
    d.set("osca_gf_mode", (float) (i % 3 != 0)); d.set("osca_gf_complex", (float) (i % 2)); d.set("osca_gf_shift", d.r(-12, 12));
    d.set("osca_level", 0.8f);
    d.div("lfo1", d.pick({"1/2", "1 bar", "1/4D", "1/4"})); d.route(MS_Lfo1, "osca_gf_index", d.r(0.3f, 0.5f));
    d.set("lfo2_rate", d.r(0.2f, 1.5f)); d.route(MS_Lfo2, "osca_gf_shift", d.r(0.1f, 0.25f));
    d.set("f1_type", FT_Formant); d.set("f1_cutoff", d.r(700, 1500)); d.set("f1_reso", d.r(0.3f, 0.6f)); d.set("f1_morph", d.r(0.1f, 0.9f));
    d.div("lfo3", d.pick({"1/8", "1/4", "1/8D", "1/4T"})); d.route(MS_Lfo3, "f1_morph", d.r(0.25f, 0.45f));
    d.set("sub_level", d.r(0.25f, 0.45f));
    if (i % 2 == 0) { d.fxOn("vowel"); d.set("fx_vowel_gain", d.r(6, 12)); d.route(MS_Lfo3, "fx_vowel_morph", 0.4f); d.set("fx_vowel_mix", 0.6f); }
    d.fxOn("dist1"); d.set("fx_dist1_drive", d.r(0.15f, 0.35f)); d.set("fx_dist1_type", (float) d.pick({(int) DT_Tube, (int) DT_Fold}));
    if (i % 3 == 1) { d.fxOn("reso"); d.set("fx_reso_key", 1); d.set("fx_reso_mix", 0.25f); d.set("fx_reso_decay", 0.3f); }
    d.macro(0, "Spread", {{"osca_gf_spread", 0.6f}});
    d.macro(1, "Tear", {{"fx_dist1_drive", 0.35f}});
    d.macro(2, "Vowel", {{"f1_morph", 0.4f}});
    d.macro(3, "Formant", {{"osca_gf_shift", 0.15f}});
    d.macro(6, "Speed", {{"lfo2_rate", 0.4f}});
    d.macro(7, "Detune", {{"osca_gf_detune", 0.6f}});
    d.route(MS_ModWheel, "lfo2_rate", 0.4f);
}

void fmGrowl(D& d, int i) {
    // 2-3 operators, envelope on FM amount, notch after, distortion after that
    d.set("osca_engine", OE_FM); d.set("osca_fm_alg", (float) d.pick({0, 1, 2, 3}));
    d.set("osca_fm_op1_ratio", 1); d.set("osca_fm_op2_ratio", d.pick({1.f, 2.f, 0.5f, 3.f})); d.set("osca_fm_op3_ratio", d.pick({1.f, 2.f, 4.f, 1.5f}));
    d.set("osca_fm_op2_level", d.r(0.2f, 0.5f)); d.set("osca_fm_op3_level", i % 2 ? d.r(0.1f, 0.35f) : 0.f); d.set("osca_fm_op2_fb", d.r(0, 0.4f));
    d.set("osca_fm_op2_wave", (float) d.pick({0, 0, 2, 5}));
    d.set("osca_level", 0.75f);
    d.set("env2_a", 0.005f); d.set("env2_d", d.r(0.15f, 0.6f)); d.set("env2_s", d.r(0.1f, 0.4f));
    d.route(MS_Env2, "osca_fm_op2_level", d.r(0.25f, 0.45f));
    d.set("f1_type", (float) d.pick({(int) FT_Notch24, (int) FT_DualResoBR, (int) FT_Notch12})); d.set("f1_cutoff", d.r(400, 1500)); d.set("f1_reso", 0.5f);
    d.div("lfo1", d.pick({"1/8", "1/16", "1/8T", "1/4"})); d.route(MS_Lfo1, "f1_cutoff", d.r(0.2f, 0.4f));
    d.fxOn("dist1"); d.set("fx_dist1_type", (float) d.pick({(int) DT_Hard, (int) DT_Tube, (int) DT_Fold, (int) DT_Diode})); d.set("fx_dist1_drive", d.r(0.3f, 0.55f));
    d.fxOn("mbcomp"); d.set("fx_mbcomp_depth", 0.4f);
    d.set("sub_level", d.r(0.35f, 0.55f));
    if (i % 4 == 2) { d.fxOn("fshift"); d.set("fx_fshift_hz", d.r(-30, 30)); d.set("fx_fshift_mix", 0.3f); }
    d.macro(0, "Index", {{"osca_fm_op2_level", 0.3f}});
    d.macro(1, "Tear", {{"fx_dist1_drive", 0.35f}});
    d.macro(2, "Notch", {{"f1_cutoff", 0.3f}});
    d.macro(3, "Feedback", {{"osca_fm_op2_fb", 0.6f}});
    d.macro(6, "Ratio", {{"osca_fm_op2_ratio", 0.15f}});
    d.route(MS_ModWheel, "osca_fm_op3_level", 0.4f);
}

void triPeak(D& d, int i) {
    if (i % 3 == 0) { // octave-down grit: main through Tri-Peak, second osc saw one octave down
        d.set("osca_engine", OE_VA); d.set("osca_va_shape", 2.6f); d.set("oscb_engine", OE_VA); d.set("oscb_va_shape", 2); d.set("oscb_octave", -1); d.set("oscb_level", 0.5f);
        d.set("oscb_balance", 1); d.set("filter_routing", 2); d.set("f2_type", FT_LP24); d.set("f2_cutoff", 900);
    } else {
        d.set("osca_engine", (float) d.pick({(int) OE_Wavetable, (int) OE_HyperSaw, (int) OE_VA}));
        d.set("osca_wt_table", (float) d.pick({1, 3, 17, 20})); d.set("osca_wt_pos", d.r(0, 1));
    }
    d.set("osca_level", 0.75f);
    const int ft = i % 4 == 3 ? (int) d.pick({(int) FT_DualResoBP, (int) FT_DualResoBR}) : (int) FT_TriPeak;
    d.set("f1_type", (float) ft); d.set("f1_cutoff", d.r(400, 1400)); d.set("f1_reso", d.r(0.55f, 0.85f)); d.set("f1_sep", d.r(0.2f, 0.6f));
    d.set("f1_shoulder", d.r(0.4f, 0.9f)); d.set("f1_shelf", d.r(0.2f, 0.8f));
    // peak sweep: separation on an LFO and cutoff on an envelope
    d.set("lfo1_rate", d.r(0.1f, 0.8f)); d.route(MS_Lfo1, "f1_sep", d.r(0.25f, 0.45f));
    d.set("env2_d", d.r(0.3f, 1.2f)); d.set("env2_s", 0.2f); d.set("f1_env", d.r(0.2f, 0.4f));
    d.set("sub_level", 0.4f);
    d.fxOn("dist1"); d.set("fx_dist1_drive", d.r(0.2f, 0.45f)); d.set("fx_dist1_type", (float) d.pick({(int) DT_Tube, (int) DT_Valve, (int) DT_Soft}));
    if (i % 2) { d.fxOn("reverb"); d.set("fx_reverb_mix", 0.12f); d.set("fx_reverb_decay", 1.2f); d.set("fx_reverb_duck", 0.6f); }
    d.macro(0, "Peaks", {{"f1_reso", 0.2f}, {"f1_shoulder", 0.3f}});
    d.macro(1, "Tear", {{"fx_dist1_drive", 0.3f}});
    d.macro(2, "Spread", {{"f1_sep", 0.4f}});
    d.macro(3, "Shelf", {{"f1_shelf", 0.5f}});
    d.macro(6, "Sweep", {{"lfo1_rate", 0.4f}});
    d.route(MS_ModWheel, "f1_cutoff", 0.3f);
}

void sineComp(D& d, int i) {
    // sine + a little noise -> soft clip -> moving notch -> clip -> Vowel EQ -> clip
    d.set("osca_engine", OE_VA); d.set("osca_va_shape", 0); d.set("osca_level", 0.8f);
    d.set("noise_level", d.r(0.03f, 0.1f)); d.set("noise_type", (float) d.pick({0, 1}));
    d.set("sat_type", DT_Soft); d.set("sat_drive", 0.0f);
    d.set("f1_type", FT_Off);
    d.fxOn("dist1"); d.set("fx_dist1_type", DT_Soft); d.set("fx_dist1_drive", d.r(0.4f, 0.7f));
    d.fxOn("eq"); d.set("fx_eq_g2", -18); d.set("fx_eq_q2", 6); d.set("fx_eq_f2", d.r(500, 1500)); // the moving notch
    d.div("lfo1", d.pick({"1/4", "1/8", "1/2"})); d.route(MS_Lfo1, "fx_eq_f2", d.r(0.25f, 0.4f));
    d.fxOn("dist2"); d.set("fx_dist2_type", (float) d.pick({(int) DT_Hard, (int) DT_Soft})); d.set("fx_dist2_drive", d.r(0.3f, 0.6f));
    d.fxOn("vowel"); d.set("fx_vowel_gain", d.r(8, 14)); d.set("fx_vowel_morph", d.r(0, 1)); d.route(MS_Lfo1, "fx_vowel_morph", 0.3f);
    d.fxOn("comp"); d.set("fx_comp_thresh", -18); d.set("fx_comp_ratio", 4); d.set("fx_comp_clip", 1);
    // keep the slots in recipe order: dist1, eq(notch), dist2, vowel, comp
    d.set("sub_level", 0.5f); d.set("sub_direct", 1);
    d.macro(0, "Pressure", {{"fx_dist1_drive", 0.25f}});
    d.macro(1, "Clip", {{"fx_dist2_drive", 0.3f}});
    d.macro(2, "Vowel", {{"fx_vowel_morph", 0.5f}});
    d.macro(3, "Notch", {{"fx_eq_f2", 0.3f}});
    d.macro(7, "Noise", {{"noise_level", 0.25f}});
    d.route(MS_ModWheel, "fx_eq_g2", 0.4f);
    (void) i;
}

void glitch(D& d, int i) {
    // rhythm from the patch: step sequencers and drawn LFOs at different synced rates
    d.set("osca_engine", (float) d.pick({(int) OE_Wavetable, (int) OE_VA, (int) OE_FM}));
    d.set("osca_wt_table", (float) d.pick({0, 15, 30, 50, 56})); d.set("osca_fm_op2_level", 0.4f);
    d.set("osca_level", 0.75f);
    d.set("f1_type", (float) d.pick({(int) FT_LP24, (int) FT_BP12, (int) FT_Morph})); d.set("f1_cutoff", d.r(400, 1500)); d.set("f1_reso", d.r(0.3f, 0.6f));
    d.set("seq1_div", 14); d.set("seq1_steps", (float) d.pick({8, 12, 16})); d.route(MS_Seq1, "f1_cutoff", d.r(0.25f, 0.4f));
    for (int s = 0; s < 32; ++s) { d.p.structure->seq[0].value[(size_t) s] = d.r(-1, 1); d.p.structure->seq[0].slide[(size_t) s] = (uint8_t) (d.r(0, 1) < 0.25f); }
    d.set("seq2_div", 11); d.fxOn("crush"); d.set("fx_crush_mix", 0.0f); d.route(MS_Seq2, "fx_crush_mix", 0.6f);
    for (int s = 0; s < 32; ++s) d.p.structure->seq[1].value[(size_t) s] = d.r(0, 1) < 0.3f ? 1.f : -1.f;
    d.lfoShape(0, stepsShape(d, d.pick({4, 8}))); d.div("lfo1", d.pick({"1/2", "1 bar", "1/4D"}));
    d.route(MS_Lfo1, "osca_wt_pos", 0.4f); d.route(MS_Lfo1, "osca_fm_op2_level", 0.3f);
    d.fxOn("dist1"); d.set("fx_dist1_drive", 0.3f);
    if (i % 3 == 0) { d.fxOn("ring"); d.set("fx_ring_key", 1); d.set("fx_ring_mix", 0.0f); d.route(MS_Rnd1, "fx_ring_mix", 0.5f); d.set("rnd1_sync", 1); d.set("rnd1_div", 14); }
    if (i % 3 == 1) { d.fxOn("delay"); d.set("fx_delay_sync", 1); d.set("fx_delay_div", 12); d.set("fx_delay_mix", 0.2f); d.set("fx_delay_fb", 0.35f); }
    d.set("sub_level", 0.35f);
    d.macro(0, "Grid", {{"f1_reso", 0.25f}});
    d.macro(1, "Crush", {{"fx_crush_bits", -0.4f}});
    d.macro(2, "Motion", {{"seq1_slide", 0.5f}});
    d.macro(6, "Position", {{"osca_wt_pos", 0.3f}, {"osca_fm_op2_level", 0.3f}});
    d.route(MS_ModWheel, "fx_dist1_drive", 0.3f);
}

void dnb(D& d, int i) {
    // reese / womp bass: one oscillator through both filters in series, drawn envelope timed in beats
    d.set("osca_engine", OE_VA); d.set("osca_va_shape", 2); d.set("osca_unison", 2); d.set("osca_detune", d.r(12, 30)); d.set("osca_level", 0.7f);
    if (i % 2) { d.set("oscb_engine", OE_VA); d.set("oscb_va_shape", 2); d.set("oscb_fine", d.r(8, 20)); d.set("oscb_level", 0.5f); }
    d.set("f1_type", FT_LP24); d.set("f1_cutoff", d.r(250, 700)); d.set("f1_reso", d.r(0.2f, 0.5f));
    d.set("f2_type", (float) d.pick({(int) FT_LP12, (int) FT_BP12, (int) FT_Morph})); d.set("f2_cutoff", d.r(800, 2500)); d.set("f2_reso", 0.3f);
    if (i % 3 == 0) { // womp: drawn envelope in beats on cutoff
        d.set("env2_mode", 1); d.set("env2_beats", 1); d.set("env2_len_b", d.pick({1.f, 2.f})); d.p.structure->env[1].pts = {{0, 0, 0}, {0.1f, 1, -0.5f}, {0.45f, 0.3f, 0.5f}, {0.6f, 0.9f, -0.4f}, {1, 0, 0.4f}};
        d.set("f1_env", 0.45f); d.set("f2_env", 0.3f); d.set("f2_env_src", 2); d.set("env3_d", 0.5f); d.set("env3_s", 0.3f);
    } else { d.set("env2_d", d.r(0.2f, 0.6f)); d.set("env2_s", 0.3f); d.set("f1_env", d.r(0.2f, 0.4f)); }
    d.fxOn("dist1"); d.set("fx_dist1_type", (float) d.pick({(int) DT_Tube, (int) DT_Soft, (int) DT_Fuzz})); d.set("fx_dist1_drive", d.r(0.25f, 0.5f));
    d.fxOn("mbcomp"); d.set("fx_mbcomp_depth", d.r(0.3f, 0.6f));
    d.set("sub_level", d.r(0.5f, 0.7f));
    d.div("lfo1", d.pick({"1/4", "1/8", "1/2"}));
    d.route(MS_Lfo1, "f2_cutoff", 0.25f);
    d.macro(0, "Womp", {{"f1_env", 0.3f}});
    d.macro(1, "Tear", {{"fx_dist1_drive", 0.3f}});
    d.macro(2, "Reese", {{"osca_detune", 0.4f}});
    d.macro(4, "Wobble", {{"f2_reso", 0.4f}, {"f2_cutoff", 0.15f}});
    d.macro(5, "Sub", {{"sub_level", 0.25f}});
    d.route(MS_ModWheel, "f2_cutoff", 0.3f);
}

void subClean(D& d, int i) {
    const int e = i % 5;
    if (e == 0) { d.set("osca_engine", OE_VA); d.set("osca_va_shape", 0.f); }
    else if (e == 1) { d.set("osca_engine", OE_VA); d.set("osca_va_shape", 1.f); d.set("osca_sh_drive", 0.15f); }
    else if (e == 2) { d.set("osca_engine", OE_Harmonic); d.set("osca_ha_partials", 4); d.set("osca_ha_tilt", -0.6f); }
    else if (e == 3) { d.set("osca_engine", OE_FM); d.set("osca_fm_op2_level", 0.12f); d.set("osca_fm_op2_ratio", 1); }
    else { d.set("osca_engine", OE_VA); d.set("osca_va_shape", 2.2f); d.set("f1_type", FT_Ladder); }
    d.set("osca_level", 0.8f);
    if (e != 4) d.set("f1_type", (float) (e % 2 ? FT_LP24 : FT_LP12));
    d.set("f1_cutoff", 120.f + 90.f * (float) e + d.r(0, 60)); d.set("f1_reso", 0.05f + 0.08f * (float) e);
    d.set("sub_level", d.r(0.3f, 0.5f)); d.set("sub_wave", (float) d.pick({0, 1}));
    d.set("env1_a", 0.003f); d.set("env1_r", 0.12f);
    d.macro(0, "Body", {{"f1_cutoff", 0.25f}});
    d.macro(1, "Warmth", {{"osca_sh_drive", 0.3f}});
    d.macro(2, "Tone", {{"osca_va_shape", 0.25f}});
    d.macro(5, "Sub", {{"sub_level", 0.3f}});
    d.route(MS_ModWheel, "f1_cutoff", 0.2f);
    (void) i;
}

void chip(D& d, int i) {
    d.set("osca_engine", OE_VA); d.set("osca_va_shape", 3); d.set("osca_va_pw", d.pick({0.125f, 0.25f, 0.5f})); d.set("osca_level", 0.7f);
    d.set("f1_type", FT_Off);
    d.fxOn("crush"); d.set("fx_crush_bits", d.r(4, 8)); d.set("fx_crush_rate", d.r(6000, 16000));
    d.set("seq1_div", 15); d.route(MS_Seq1, "osca_fine", 0.5f, RC_Oct1); // arpeggio by octave steps
    for (int s = 0; s < 32; ++s) d.p.structure->seq[0].value[(size_t) s] = (s % 4 == 0) ? 0.f : ((s % 4 == 2) ? 1.f : 0.5833f);
    d.route(MS_Lfo1, "osca_va_pw", 0.2f); d.set("lfo1_rate", d.r(2, 6));
    d.set("sub_level", 0.3f);
    d.macro(0, "Bits", {{"fx_crush_bits", -0.4f}});
    d.macro(1, "Rate", {{"fx_crush_rate", -0.3f}});
    d.macro(2, "Width", {{"osca_va_pw", 0.3f}});
    d.route(MS_ModWheel, "lfo1_rate", 0.4f);
    (void) i;
}

void experimental(D& d, int i) {
    // chain FM: oscillator one octave down FMs the next, level near zero, envelope on the link amount
    d.set("osca_engine", OE_VA); d.set("osca_va_shape", 0); d.set("osca_octave", -1); d.set("osca_level", 0.0f);
    d.set("oscb_engine", (float) d.pick({(int) OE_VA, (int) OE_Wavetable})); d.set("oscb_level", 0.8f); d.set("oscb_wt_table", 9);
    d.set("link_ab_mode", (float) d.pick({(int) CM_FM, (int) CM_PM, (int) CM_Ring, (int) CM_Sync})); d.set("link_ab_amount", 0.15f);
    d.set("env2_d", d.r(0.2f, 0.8f)); d.set("env2_s", 0.1f); d.route(MS_Env2, "link_ab_amount", d.r(0.3f, 0.5f));
    d.set("f1_type", (float) d.pick({(int) FT_CombPlus, (int) FT_CombMinus, (int) FT_Phaser, (int) FT_Morph})); d.set("f1_cutoff", d.r(200, 900)); d.set("f1_reso", 0.6f); d.set("f1_keytrack", 1);
    if (i % 2) { d.fxOn("spectral"); d.set("fx_spectral_thresh", -48); d.set("fx_spectral_contrast", 0.4f); d.set("fx_spectral_mix", 0.6f); }
    else { d.fxOn("fshift"); d.set("fx_fshift_hz", d.r(-80, 80)); d.set("fx_fshift_fb", 0.3f); d.set("fx_fshift_mix", 0.5f); }
    d.fxOn("reso"); d.set("fx_reso_key", 1); d.set("fx_reso_mix", 0.3f); d.set("fx_reso_fifth", 1);
    d.set("sub_level", 0.3f);
    d.macro(0, "Link", {{"link_ab_amount", 0.4f}});
    d.macro(1, "Comb", {{"f1_reso", 0.3f}});
    d.macro(2, "Shift", {{"fx_fshift_hz", 0.1f}, {"fx_spectral_contrast", 0.4f}});
    d.macro(3, "Ring", {{"fx_reso_mix", 0.4f}});
    d.route(MS_ModWheel, "f1_cutoff", 0.3f);
}

// ------------------------------------------------------------------ FX recipes (GNARL FX, run over bounced audio)
void fxRecipe(D& d, int cat, int i) {
    d.set("master_level", 0.8f);
    switch (cat) {
        case 0: // neuro chain
            d.set("f1_type", (float) d.pick({(int) FT_DualResoBR, (int) FT_Notch24})); d.set("f1_cutoff", d.r(500, 1500)); d.set("f1_reso", 0.55f);
            d.div("lfo1", d.pick({"1/8", "1/16", "1/4T"})); d.route(MS_Lfo1, "f1_cutoff", 0.35f);
            d.fxOn("phaser"); d.set("fx_phaser_mix", 0.6f);
            d.fxOn("dist1"); d.set("fx_dist1_drive", d.r(0.25f, 0.45f)); d.set("fx_dist1_type", (float) d.pick({(int) DT_Tube, (int) DT_Fold, (int) DT_Rectify, (int) DT_SineFold}));
            d.fxOn("dist2"); d.set("fx_dist2_drive", 0.2f); d.set("fx_dist2_type", (float) d.pick({(int) DT_Soft, (int) DT_Hard}));
            d.fxOn("mbcomp"); d.set("fx_mbcomp_depth", 0.5f);
            if (i % 2) { d.fxOn("comb"); d.set("fx_comb_freq", d.r(80, 300)); d.set("fx_comb_mix", 0.25f); }
            d.macro(0, "Notch", {{"f1_cutoff", 0.2f}}); d.macro(1, "Tear", {{"fx_dist1_drive", 0.12f}}); d.macro(2, "Phase", {{"fx_phaser_depth", 0.4f}});
            d.fxOn("comp"); d.set("fx_comp_thresh", -14); d.set("fx_comp_ratio", 6); d.set("fx_comp_attack", 2); d.set("fx_comp_clip", 1);
            break;
        case 1: // crossover sweep (multiband distortion with one hot band, crossover / converge on an LFO)
            d.fxOn("mbd"); d.set("fx_mbd_b2_drive", d.r(0.6f, 0.9f)); d.set("fx_mbd_b2_type", (float) d.pick({(int) DT_Fold, (int) DT_Hard, (int) DT_Tube}));
            d.set("lfo1_rate", d.r(0.2f, 2.f));
            if (i % 2) d.route(MS_Lfo1, "fx_mbd_converge", 0.6f); else d.route(MS_Lfo1, "fx_mbd_xlo", 0.4f);
            d.macro(0, "Hot Band", {{"fx_mbd_b2_drive", 0.3f}}); d.macro(1, "Converge", {{"fx_mbd_converge", 0.5f}}); d.macro(2, "Crossover", {{"fx_mbd_xhi", 0.3f}});
            break;
        case 2: // formant
            d.set("f1_type", FT_Formant); d.set("f1_cutoff", d.r(600, 1600)); d.set("f1_reso", d.r(0.3f, 0.7f)); d.set("f1_gender", d.r(-0.6f, 0.6f)); d.set("f1_morph", d.r(0, 1));
            d.set("lfo1_rate", d.r(0.3f, 4.f)); d.lfoShape(0, i % 2 ? wompShape() : stepsShape(d, 4)); d.route(MS_Lfo1, "f1_morph", d.r(0.3f, 0.5f));
            d.fxOn("vowel"); d.set("fx_vowel_gain", d.r(6, 14)); d.set("fx_vowel_shift", d.r(-6, 6)); d.route(MS_Lfo1, "fx_vowel_morph", 0.3f); d.set("fx_vowel_mix", d.r(0.3f, 0.7f));
            d.fxOn("dist1"); d.set("fx_dist1_drive", d.r(0.2f, 0.45f)); d.set("fx_dist1_type", (float) d.pick({(int) DT_Tube, (int) DT_Fold, (int) DT_Diode, (int) DT_Valve}));
            if (i % 3 == 0) { d.fxOn("moddelay"); d.set("fx_moddelay_rate", d.r(20, 90)); d.set("fx_moddelay_depth", 0.3f); d.set("fx_moddelay_mix", 0.35f); }
            if (i % 3 == 1) { d.fxOn("flanger"); d.set("fx_flanger_mix", 0.4f); }
            d.macro(0, "Vowel", {{"f1_morph", 0.4f}}); d.macro(1, "Gender", {{"f1_gender", 0.6f}}); d.macro(2, "Tear", {{"fx_dist1_drive", 0.3f}});
            break;
        case 3: // rhythmic gating / stutter from the input
            d.set("f1_type", FT_LP24); d.set("f1_cutoff", 900); d.set("f1_reso", 0.4f);
            d.set("seq1_div", 14); d.route(MS_Seq1, "f1_cutoff", 0.4f);
            for (int s = 0; s < 32; ++s) d.p.structure->seq[0].value[(size_t) s] = d.r(-1, 1);
            d.fxOn("crush"); d.set("fx_crush_mix", 0.0f); d.route(MS_Seq2, "fx_crush_mix", 0.5f); d.set("seq2_div", 11);
            d.fxOn("delay"); d.set("fx_delay_sync", 1); d.set("fx_delay_div", 12); d.set("fx_delay_mix", 0.25f);
            d.macro(0, "Gate", {{"f1_cutoff", 0.25f}}); d.macro(1, "Crush", {{"fx_crush_bits", -0.4f}}); d.macro(2, "Echo", {{"fx_delay_mix", 0.4f}});
            break;
        case 4: // space / resonant
            d.fxOn("reso"); d.set("fx_reso_freq", d.r(55, 330)); d.set("fx_reso_mix", d.r(0.25f, 0.5f)); d.set("fx_reso_fifth", (float) (i % 2)); d.set("fx_reso_bright", d.r(0.2f, 0.9f)); d.set("fx_reso_decay", d.r(0.2f, 1.5f));
            if (i % 3 == 0) { d.fxOn("disperse"); d.set("fx_disperse_amount", d.r(0.3f, 0.7f)); d.set("fx_disperse_freq", d.r(200, 1500)); }
            if (i % 3 == 1) { d.fxOn("comb"); d.set("fx_comb_freq", d.r(60, 400)); d.set("fx_comb_fb", d.r(-0.8f, 0.8f)); d.set("fx_comb_mix", 0.4f); }
            if (i % 3 == 2) { d.fxOn("fshift"); d.set("fx_fshift_hz", d.r(-40, 40)); d.set("fx_fshift_mix", 0.3f); }
            d.fxOn("reverb"); d.set("fx_reverb_mix", d.r(0.15f, 0.35f)); d.set("fx_reverb_duck", 0.5f); d.set("fx_reverb_size", d.r(0.2f, 0.9f));
            d.set("lfo1_rate", d.r(0.1f, 2.f)); d.route(MS_Lfo1, "fx_reso_freq", d.r(0.1f, 0.3f));
            d.macro(0, "Ring", {{"fx_reso_decay", 0.4f}}); d.macro(1, "Smear", {{"fx_disperse_amount", 0.4f}}); d.macro(2, "Space", {{"fx_reverb_mix", 0.35f}});
            break;
        default: // lo-fi
            d.fxOn("crush"); d.set("fx_crush_bits", d.r(5, 9)); d.set("fx_crush_rate", d.r(5000, 12000));
            d.fxOn("dist1"); d.set("fx_dist1_type", DT_Downsample); d.set("fx_dist1_drive", 0.2f);
            d.fxOn("chorus"); d.set("fx_chorus_mix", 0.3f);
            d.set("lfo1_rate", 0.5f); d.route(MS_Lfo1, "fx_crush_rate", 0.2f);
            d.macro(0, "Bits", {{"fx_crush_bits", -0.4f}}); d.macro(1, "Rate", {{"fx_dist1_drive", 0.4f}}); d.macro(2, "Wobble", {{"fx_chorus_depth", 0.4f}});
    }
    d.macro(3, "Width", {{"fx_width_amount", 0.3f}}); d.fxOn("width"); d.set("fx_width_mono_on", 1); d.set("fx_width_mono_hz", 170);
    d.macro(4, "Follow", {{"f1_reso", cat == 0 ? 0.15f : 0.3f}}); d.route(MS_Follower, "f1_cutoff", 0.15f);
    d.macro(5, "Drive", {{"fxp_in_gain", 0.06f}});
    d.macro(6, "Speed", {{"lfo1_rate", 0.4f}});
    d.macro(7, "Tilt", {{"fx_eq_g4", cat == 0 ? -0.15f : 0.1f}}); d.fxOn("eq");
    d.route(MS_ModWheel, "lfo1_rate", 0.3f);
    if (d.p.get("f1_type") == FT_Off) { d.set("f1_type", FT_LP12); d.set("f1_cutoff", 18000); d.route(MS_ModWheel, "f1_cutoff", -0.3f); }
}

struct Cat { const char* name; int count; bool needsSub; std::function<void(D&, int)> fn; };

const char* kAdj[] = {"Black", "Liquid", "Rusted", "Tectonic", "Feral", "Hollow", "Molten", "Acid", "Velvet", "Iron", "Static", "Venom", "Lunar", "Cryo", "Gnarled",
                      "Fractal", "Swamp", "Neon", "Rubber", "Glass", "Wet", "Bent", "Toxic", "Grim", "Elastic", "Ghost", "Vapor", "Hydra", "Dusty", "Slime"};
const char* kNoun[] = {"Reese", "Throat", "Gremlin", "Engine", "Tongue", "Spine", "Coil", "Mutter", "Gator", "Sludge", "Wire", "Beast", "Vortex", "Mouth", "Crawler",
                       "Signal", "Furnace", "Tendril", "Chirp", "Drone", "Larynx", "Gear", "Hive", "Pulse", "Grinder", "Wraith", "Tide", "Mantis", "Ratchet", "Organ"};

std::string nameFor(int global, int) { int a = global % 30, b = (global / 30 + 7 * a) % 30; return std::string(kAdj[a]) + " " + kNoun[b]; } // unique for < 900 presets

} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--export-wavetables") {
        // write the 64 factory tables as Serum-compatible .wav files (content package)
        fs::create_directories(argv[2]);
        for (auto& t : factoryTables()) {
            auto bytes = saveWavetableWav(*t);
            std::ofstream(fs::path(argv[2]) / ("GNARL " + t->name + ".wav"), std::ios::binary).write((const char*) bytes.data(), (std::streamsize) bytes.size());
        }
        std::printf("exported %zu wavetables\n", factoryTables().size());
        return 0;
    }
    if (argc < 3) { std::fprintf(stderr, "usage: gnarl_presetgen <content-dir> <report.md>\n"); return 2; }
    const fs::path out = argv[1];
    const std::string reportPath = argv[2];
    std::vector<Cat> synthCats = {
        {"Neuro-Reese", 25, true, neuro}, {"Squelch-Liquid", 25, false, squelch}, {"Talking-Formant", 20, false, talking}, {"FM Growl", 20, false, fmGrowl},
        {"Tri-Peak-Separation", 15, false, triPeak}, {"Sine-Compression", 10, true, sineComp}, {"Glitch-Rhythmic", 15, false, glitch}, {"DnB", 15, true, dnb},
        {"Sub-Clean", 5, true, subClean}, {"Chip-Lofi", 5, false, chip}, {"Experimental", 5, false, experimental}};
    const char* fxCats[] = {"Neuro Chain", "Crossover", "Formant", "Rhythmic", "Space-Resonant", "Lo-fi"};
    const int fxCounts[] = {8, 6, 6, 8, 6, 6};

    struct Job { bool fx; std::string cat; int idx; int global; bool needsSub; std::function<void(D&, int)> fn; int fxCat; };
    std::vector<Job> jobs;
    int global = 0;
    for (auto& c : synthCats) for (int i = 0; i < c.count; ++i) jobs.push_back({false, c.name, i, global++, c.needsSub, c.fn, -1});
    for (int c = 0; c < 6; ++c) for (int i = 0; i < fxCounts[c]; ++i) jobs.push_back({true, fxCats[c], i, global++, false, nullptr, c});

    struct Out { std::string name, cat; bool fx; GateResult g; std::string file; int attempts; };
    std::vector<Out> results(jobs.size());
    std::atomic<int> next{0};
    std::mutex logLock, fpLock;
    std::vector<std::pair<bool, std::vector<float>>> accepted; std::vector<std::string> acceptedNames;
    auto worker = [&] {
        for (int j; (j = next++) < (int) jobs.size();) {
            const auto& job = jobs[(size_t) j];
            Out o; o.cat = job.cat; o.fx = job.fx;
            GateResult g; int attempt = 0;
            Patch best;
            for (attempt = 0; attempt < 6; ++attempt) {
                D d(job.fx, (uint32_t) (job.global * 1009 + attempt * 7919 + 17));
                if (job.fx) fxRecipe(d, job.fxCat, job.idx); else job.fn(d, job.idx);
                if (job.needsSub) { d.set("sub_level", std::max(d.p.get("sub_level"), 0.6f)); d.set("sub_direct", 1); }
                if (job.fx) d.set("fxp_in_gain", 4.f);
                finish(d, job.cat);
                d.p.name = nameFor(job.global, attempt);
                d.p.category = job.cat; d.p.author = "GNARL Factory";
                d.p.tags = {job.cat};
                // calibrate the level by measurement: loudness target, then the peak limit
                GateSpec spec; spec.fx = job.fx; spec.needsSub = job.needsSub; spec.targetLufs = job.fx ? -16 : -15;
                for (int it = 0; it < 3; ++it) {
                    auto c2 = renderNoteForGates(d.p, job.fx, 36, 2.0, 2.0, true);
                    auto c1 = renderNoteForGates(d.p, job.fx, 24, 2.0, 2.0, true);
                    std::vector<float> l(c2.L.begin() + 2400, c2.L.end()), r(c2.R.begin() + 2400, c2.R.end());
                    double lufs = analysis::loudnessLufs(l, r, 48000);
                    double pk = analysis::toDb(std::max({analysis::peak(c2.L), analysis::peak(c2.R), analysis::peak(c1.L), analysis::peak(c1.R)}));
                    double gain = std::pow(10.0, (spec.targetLufs - lufs) / 20.0);
                    gain = std::min(gain, std::pow(10.0, (-1.0 - pk) / 20.0));
                    float m = (float) std::clamp(d.p.get("master_level") * gain, 0.02, 1.0);
                    d.p.set("master_level", m);
                }
                g = evaluatePreset(d.p, spec);
                for (int fix = 0; fix < 3 && !g.pass; ++fix) {
                    bool changed = false;
                    for (int k = 0; k < 8; ++k) if (g.macroDiffDb.size() == 8 && g.macroDiffDb[(size_t) k] < -60) { repairMacro(d, k, fix); changed = true; }
                    if (g.modWheelDiffDb < -60) { repairWheel(d, fix); changed = true; }
                    if (!changed) break;
                    g = evaluatePreset(d.p, spec);
                }
                best = d.p;
                if (g.pass) {
                    // near-duplicate of an already accepted preset? then design another variation
                    std::lock_guard<std::mutex> l(fpLock);
                    bool dup = false;
                    for (size_t a = 0; a < accepted.size(); ++a)
                        if (accepted[a].first == job.fx && analysis::fingerprintDistance(accepted[a].second, g.fingerprint) < 1.0) { dup = true; break; }
                    if (!dup || attempt == 5) { accepted.push_back({job.fx, g.fingerprint}); acceptedNames.push_back(best.name); break; }
                }
            }
            o.g = g; o.name = best.name; o.attempts = attempt + 1;
            // write
            fs::path dir = out / (job.fx ? "FX" : "Synth") / job.cat;
            fs::create_directories(dir);
            fs::path f = dir / (best.name + ".gnarl");
            std::ofstream(f) << patchToJson(best, job.fx).dump(1);
            o.file = f.string();
            {
                std::lock_guard<std::mutex> l(logLock);
                std::printf("%-5s %-22s %-22s %s lufs %.1f pk %.1f  %s\n", job.fx ? "FX" : "SYN", job.cat.c_str(), best.name.c_str(), g.pass ? "PASS" : "FAIL", g.lufs, g.peakPreClipDb, g.failures.empty() ? "" : g.failures[0].c_str());
                std::fflush(stdout);
            }
            results[(size_t) j] = o;
        }
    };
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < std::max(1u, std::thread::hardware_concurrency()); ++t) pool.emplace_back(worker);
    for (auto& t : pool) t.join();

    // duplicate check: fingerprint distance between every pair within a plugin
    for (size_t a = 0; a < results.size(); ++a)
        for (size_t b = a + 1; b < results.size(); ++b) {
            if (results[a].fx != results[b].fx) continue;
            double dist = analysis::fingerprintDistance(results[a].g.fingerprint, results[b].g.fingerprint);
            if (dist < 1.0) { results[b].g.pass = false; results[b].g.failures.push_back("nearly identical to " + results[a].name + " (fingerprint distance " + std::to_string(dist).substr(0, 4) + " dB)"); }
        }
    int fails = 0;
    for (auto& r : results) fails += !r.g.pass;
    std::ostringstream md;
    md << "# GNARL factory preset report (section 11 gates)\n\nGenerated by `gnarl_presetgen`. " << results.size() << " presets, " << (results.size() - fails) << " pass, " << fails << " fail.\n\n";
    md << "These presets were designed by measurement, not by ear: every level is calibrated from rendered loudness and peaks, and every gate below is a measurement.\n\n";
    md << "Gates: C1 and C2 notes plus a 2-bar legato glide phrase at 140 BPM; mod wheel at 0 and 127; each macro at 0 and 1. Fail if silent; tail longer than 10 s after note-off; "
          "pre-clipper peak above -0.3 dBFS in the default state; a macro/mod-wheel extreme above +3 dBFS into the safety clipper; DC above -60 dB; no fundamental where the category needs a sub; "
          "content below 120 Hz with L/R correlation under 0.9; a macro or the mod wheel changing nothing (difference below -60 dBFS); loudness more than 2 LU from the bank target "
          "(synth -15 LUFS, FX -16 LUFS on the standard input); spectrum-over-time fingerprint within 1.0 dB mean distance of another preset.\n\n";
    md << "| Preset | Plugin | Category | Result | LUFS | Pre-clip peak | Worst extreme | Low corr | Tail s | Mod wheel | Weakest macro | Movement | CPU % | Notes |\n|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    for (auto& r : results) {
        char line[512];
        std::string notes; for (auto& f : r.g.failures) notes += f + "; ";
        std::snprintf(line, sizeof line, "| %s | %s | %s | %s | %.1f | %.1f | %.1f | %.2f | %.1f | %.0f dB | %.0f dB | %.2f | %.1f | %s |\n", r.name.c_str(), r.fx ? "FX" : "Synth", r.cat.c_str(),
                      r.g.pass ? "PASS" : "FAIL", r.g.lufs, r.g.peakPreClipDb, r.g.worstVariantPeakDb, r.g.lowCorr, r.g.tailSeconds, r.g.modWheelDiffDb, r.g.minMacroDiffDb, r.g.movement, r.g.cpuPercent, notes.c_str());
        md << line;
    }
    // the 20 most extreme by movement x macro range
    std::vector<size_t> order(results.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        auto score = [&](const Out& o) { double mac = 0; for (double v : o.g.macroDiffDb) mac += std::max(v + 60, 0.0); return o.g.movement * 10 + mac / 8 + (o.g.worstVariantPeakDb + 10); };
        return score(results[a]) > score(results[b]);
    });
    md << "\n## 20 presets to audition first (most extreme measurements: movement over time, macro range and drive)\n\n";
    for (int i = 0; i < 20 && i < (int) order.size(); ++i) { auto& r = results[order[(size_t) i]]; md << i + 1 << ". " << r.name << " (" << (r.fx ? "GNARL FX" : "GNARL") << ", " << r.cat << ")\n"; }
    std::ofstream(reportPath) << md.str();
    std::printf("\n%zu presets, %d fail. Report: %s\n", results.size(), fails, reportPath.c_str());
    return fails ? 1 : 0;
}
