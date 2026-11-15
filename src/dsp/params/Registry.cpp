#include "Registry.h"
#include <cassert>
#include <stdexcept>

namespace gnarl {

static float skewForCentre(float lo, float hi, float centre) {
    return std::log(0.5f) / std::log((centre - lo) / (hi - lo));
}

const std::vector<SyncDivision>& syncDivisions() {
    static const std::vector<SyncDivision> d = {
        {"8 bars", 32}, {"4 bars", 16}, {"2 bars", 8}, {"1 bar", 4}, {"1/2D", 3}, {"1/2", 2}, {"1/2T", 4.0 / 3},
        {"1/4D", 1.5}, {"1/4", 1}, {"1/4T", 2.0 / 3}, {"1/8D", 0.75}, {"1/8", 0.5}, {"1/8T", 1.0 / 3},
        {"1/16D", 0.375}, {"1/16", 0.25}, {"1/16T", 1.0 / 6}, {"1/32D", 0.1875}, {"1/32", 0.125},
        {"1/32T", 1.0 / 12}, {"1/64", 0.0625}};
    return d;
}

namespace choices {
const std::vector<std::string>& oscEngines() {
    static const std::vector<std::string> v = {"Analog", "HyperSaw", "Wavetable", "FM", "Grain/Formant", "Harmonic"};
    return v;
}
const std::vector<std::string>& filterTypes() {
    static const std::vector<std::string> v = {"Off", "LP 12", "LP 24", "BP 12", "BP 24", "HP 12", "HP 24", "Notch 12", "Notch 24",
                                               "Ladder", "Sep LP 24", "Sep LP 36", "Tri-Peak", "Dual-Reso BP", "Dual-Reso BR", "Morph",
                                               "Comb +", "Comb -", "Formant", "Phaser"};
    return v;
}
const std::vector<std::string>& distTypes() {
    static const std::vector<std::string> v = {"Soft Clip", "Hard Clip", "Tube", "Valve", "Diode", "Wavefolder", "Sine Fold",
                                               "Rectify", "Fuzz", "Bit Reduce", "Downsample", "Draw"};
    return v;
}
const std::vector<std::string>& warpModes() {
    static const std::vector<std::string> v = {"None", "Bend +", "Bend -", "Sync", "PWM Squeeze", "Mirror", "Phase Dist", "Fold", "Quantize"};
    return v;
}
const std::vector<std::string>& chainModes() {
    static const std::vector<std::string> v = {"Add", "Ring", "Sync", "PM", "FM", "AM"};
    return v;
}
const std::vector<std::string>& fmWaves() {
    static const std::vector<std::string> v = {"Sine", "Half Sine", "Abs Sine", "Quarter Sine", "Triangle", "Soft Square", "Soft Saw"};
    return v;
}
} // namespace choices

const std::vector<std::string>& fxModuleIds() {
    static const std::vector<std::string> v = {"dist1", "dist2", "mbd", "phaser", "flanger", "chorus", "comb", "vowel", "eq",
                                               "fshift", "ring", "crush", "mbcomp", "comp", "delay", "reverb", "moddelay",
                                               "reso", "disperse", "spectral", "tamer", "split", "vocoder", "width"};
    return v;
}
const std::vector<std::string>& fxModuleNames() {
    static const std::vector<std::string> v = {"Distortion A", "Distortion B", "Multiband Dist", "Phaser", "Flanger", "Chorus",
                                               "Comb", "Vowel EQ", "EQ", "Freq Shifter", "Ring Mod", "Bitcrusher",
                                               "Multiband Comp", "Compressor", "Delay", "Reverb", "Mod Delay", "Resonator",
                                               "Disperser", "Spectral Shaper", "Resonance Tamer", "3-Band Split", "Vocoder", "Width"};
    return v;
}

namespace {
struct B {
    std::vector<ParamDef>& out;
    std::string section;
    Scope scope = Scope::Voice;
    uint8_t plugins = kBoth;

    ParamDef& add(const std::string& id, const std::string& name, float mn, float mx, float df, Unit u, Role r) {
        ParamDef p;
        p.id = id; p.name = name; p.section = section; p.min = mn; p.max = mx; p.def = df; p.unit = u; p.role = r;
        p.scope = scope; p.plugins = plugins;
        if (r == Role::Choice || r == Role::Toggle || r == Role::Integer) { p.interval = 1; p.smoothMs = 0; p.modulatable = false; }
        out.push_back(p);
        return out.back();
    }
    ParamDef& cont(const std::string& id, const std::string& name, float mn, float mx, float df, Unit u, Role r = Role::Continuous) {
        return add(id, name, mn, mx, df, u, r);
    }
    ParamDef& freq(const std::string& id, const std::string& name, float mn, float mx, float df, float centre) {
        auto& p = add(id, name, mn, mx, df, Unit::Hz, Role::Continuous);
        p.skew = skewForCentre(mn, mx, centre);
        return p;
    }
    ParamDef& time(const std::string& id, const std::string& name, float mn, float mx, float df, float centre, Unit u = Unit::Seconds) {
        auto& p = add(id, name, mn, mx, df, u, Role::Continuous);
        p.skew = skewForCentre(mn, mx, centre);
        return p;
    }
    ParamDef& choice(const std::string& id, const std::string& name, const std::vector<std::string>& c, int df) {
        auto& p = add(id, name, 0, (float) c.size() - 1, (float) df, Unit::None, Role::Choice);
        p.choices = c;
        return p;
    }
    ParamDef& toggle(const std::string& id, const std::string& name, bool df) {
        auto& p = add(id, name, 0, 1, df ? 1.f : 0.f, Unit::None, Role::Toggle);
        p.choices = {"Off", "On"};
        return p;
    }
    ParamDef& integer(const std::string& id, const std::string& name, int mn, int mx, int df) {
        return add(id, name, (float) mn, (float) mx, (float) df, Unit::None, Role::Integer);
    }
};

std::vector<std::string> divNames() {
    std::vector<std::string> v;
    for (auto& d : syncDivisions()) v.push_back(d.name);
    return v;
}

ParamDef& ctx(ParamDef& p, std::vector<ContextValue> c, std::vector<ContextRoute> r = {}) {
    for (auto& x : c) p.context.push_back(x);
    for (auto& x : r) p.contextRoutes.push_back(x);
    return p;
}
ParamDef& doc(ParamDef& p, const std::string& d) { p.doc = d; return p; }

void buildGlobal(std::vector<ParamDef>& v) {
    B b{v, "Global", Scope::Global, kBoth};
    doc(b.cont("master_level", "Master Level", 0, 1, 0.7f, Unit::Gain, Role::Level), "Output gain; 0 is true silence.");
    b.plugins = kSynth;
    doc(b.integer("voices", "Voices", 1, 8, 1), "1 = monophonic; up to 8-voice polyphony.");
    doc(ctx(b.toggle("legato", "Legato", true), {{"voices", 1}}), "Mono only: overlapping notes do not retrigger envelopes.");
    doc(ctx(b.time("glide_time", "Glide", 0, 2000, 0, 200, Unit::Ms), {{"voices", 1}}), "Portamento time; 0 = no glide.");
    b.out.back().role = Role::Amount;
    doc(ctx(b.cont("glide_curve", "Glide Curve", -1, 1, 0, Unit::None), {{"voices", 1}, {"glide_time", 300}}), "Shape of the glide trajectory (-1 fast start, +1 slow start).");
    doc(ctx(b.toggle("glide_legato", "Glide Legato Only", true), {{"voices", 1}, {"glide_time", 300}}), "Glide only on overlapping notes.");
    doc(b.cont("bend_range", "Bend Range", 0, 24, 2, Unit::Semitones), "Pitch-bend range in semitones.");
    b.out.back().interval = 1; b.out.back().modulatable = false;
    b.plugins = kBoth;
    doc(b.choice("oversampling", "Oversampling", {"1x", "2x", "4x", "8x"}, 2), "Oversampling for voice filters/drive and FX nonlinear stages.");
    doc(b.choice("render_os", "Offline Oversampling", {"Same", "2x Higher", "4x Higher"}, 1), "Extra oversampling during offline bounce.");
    b.plugins = kSynth;
    doc(b.choice("post_drive_type", "Post Drive Type", choices::distTypes(), 0), "Type of the optional mono drive after the voice mix.");
    ctx(b.out.back(), {{"post_drive", 0.6f}});
    doc(b.cont("post_drive", "Post Drive", 0, 1, 0, Unit::Percent, Role::Drive), "Mono drive after the voice mix; 0 = bypassed.");
    b.plugins = kBoth;
    b.section = "Filters";
    doc(ctx(b.choice("filter_routing", "Filter Routing", {"Serial", "Parallel", "Split"}, 0), {{"f2_type", FT_HP12}, {"f2_cutoff", 800}}), "Serial (F1 > sat > F2), Parallel, or Split (bus 1 > F1, bus 2 > F2).");
    doc(ctx(b.choice("sat_type", "Saturation Type", choices::distTypes(), 0), {{"sat_drive", 0.6f}}), "Saturation between filters (serial only).");
    doc(b.cont("sat_drive", "Saturation Drive", 0, 1, 0, Unit::Percent, Role::Drive), "Drive of the between-filter saturation; 0 = bypassed.");
    doc(ctx(b.cont("sat_mix", "Saturation Mix", 0, 1, 1, Unit::Percent, Role::Mix), {{"sat_drive", 0.7f}}), "Dry/wet of the saturation slot.");
    doc(ctx(b.toggle("filter_link", "Link Filters", false), {{"f2_type", FT_LP24}, {"f2_cutoff", 300}, {"filter_routing", 1}}), "Filter 2 follows Filter 1 (type, cutoff, reso, sep) with offsets.");
    doc(ctx(b.cont("link_cutoff", "Link Cutoff Offset", -48, 48, 0, Unit::Semitones, Role::Neutral), {{"filter_link", 1}, {"filter_routing", 1}, {"f1_reso", 0.6f}}), "Filter 2 cutoff offset when linked.");
    doc(ctx(b.cont("link_reso", "Link Reso Offset", -1, 1, 0, Unit::None, Role::Neutral), {{"filter_link", 1}, {"filter_routing", 1}, {"f1_reso", 0.5f}, {"f1_cutoff", 700}}), "Filter 2 resonance offset when linked.");
    doc(ctx(b.cont("link_pan", "Link Pan Offset", -1, 1, 0, Unit::None, Role::Neutral), {{"filter_link", 1}, {"filter_routing", 1}}), "Filter 2 pan offset when linked (Filter 1 pans the opposite way).");
}

void buildOsc(std::vector<ParamDef>& v, int k) {
    const std::string o = std::string("osc") + char('a' + k) + "_";
    const std::string N = std::string("Osc ") + char('A' + k) + " ";
    B b{v, std::string("Osc ") + char('A' + k), Scope::Voice, kSynth};
    // Make this oscillator audible and the others silent in every context.
    std::vector<ContextValue> solo = {{"osca_level", k == 0 ? 0.8f : 0.f}, {"oscb_level", k == 1 ? 0.8f : 0.f}, {"oscc_level", k == 2 ? 0.8f : 0.f}, {"f1_type", 0}};
    auto C = [&](ParamDef& p, std::vector<ContextValue> extra = {}, std::vector<ContextRoute> r = {}) -> ParamDef& {
        auto all = solo; for (auto& e : extra) all.push_back(e);
        return ctx(p, all, r);
    };
    C(doc(b.choice(o + "engine", N + "Engine", choices::oscEngines(), 0), "Oscillator engine."));
    C(doc(b.integer(o + "octave", N + "Octave", -4, 4, 0), "Octave transpose ±4."));
    C(doc(b.integer(o + "semi", N + "Semi", -12, 12, 0), "Semitone transpose."));
    C(doc(b.cont(o + "fine", N + "Fine", -100, 100, 0, Unit::Cents, Role::Neutral), "Fine tune in cents."));
    C(doc(b.cont(o + "phase", N + "Phase", 0, 360, 0, Unit::Degrees), "Start phase (key-sync mode)."), {{o + "phase_mode", 0}});
    b.out.back().modulatable = false; b.out.back().smoothMs = 0;
    C(doc(b.choice(o + "phase_mode", N + "Phase Mode", {"Key Sync", "Free Run", "Random"}, 0), "Phase reset on note-on, free-running, or random start."), {{o + "phase", 90}});
    C(doc(b.toggle(o + "invert", N + "Invert", false), "Invert output polarity."), {{"oscb_level", 0.8f}, {"osca_level", 0.8f}, {"oscc_level", 0}});
    if (k == 1) { b.out.back().context.push_back({"osca_level", 0.8f}); }
    C(doc(b.toggle(o + "fixed", N + "Fixed Freq", false), "Ignore the keyboard and play the Fixed Freq."), {{o + "fixed_hz", 440}});
    C(doc(b.freq(o + "fixed_hz", N + "Fixed Freq Hz", 1, 20000, 110, 440), "Frequency used when Fixed Freq is on."), {{o + "fixed", 1}});
    C(doc(b.cont(o + "level", N + "Level", 0, 1, k == 0 ? 0.8f : 0.f, Unit::Gain, Role::Level), "Oscillator level; 0 = silent."));
    b.out.back().context.clear(); C(b.out.back(), {{o + "level", 0.8f}}); // level test toggles itself
    C(doc(b.cont(o + "pan", N + "Pan", -1, 1, 0, Unit::None, Role::Neutral), "Stereo position."));
    C(doc(b.cont(o + "balance", N + "Filter Balance", -1, 1, -1, Unit::None, Role::Continuous), "-1 = all to Filter 1, +1 = all to Filter 2."), {{"filter_routing", 2}, {"f1_type", FT_LP24}, {"f1_cutoff", 300}, {"f2_type", FT_HP24}, {"f2_cutoff", 3000}});
    C(doc(b.cont(o + "fx_send", N + "Direct FX Send", 0, 1, 0, Unit::Percent, Role::Send), "Extra signal sent straight to the FX chain, bypassing filters."), {{"f1_type", FT_LP24}, {"f1_cutoff", 200}});
    C(doc(b.integer(o + "unison", N + "Unison", 1, 9, 1), "Unison voices."), {{o + "detune", 30}});
    C(doc(b.cont(o + "detune", N + "Unison Detune", 0, 100, 20, Unit::Cents, Role::Amount), "Unison spread in cents."), {{o + "unison", 5}});
    C(doc(b.cont(o + "spread", N + "Unison Stereo", 0, 1, 0.5f, Unit::Percent, Role::Amount), "Unison stereo spread."), {{o + "unison", 5}, {o + "detune", 30}});
    C(doc(b.cont(o + "blend", N + "Unison Blend", 0, 1, 1, Unit::Percent, Role::Continuous), "Level of the outer unison voices against the centre voice."), {{o + "unison", 5}, {o + "detune", 30}});
    C(doc(b.toggle(o + "multi", N + "Multi Mode", false), "Eight detuned copies, four to each filter bus."), {{o + "detune", 30}, {"filter_routing", 2}});
    // Virtual analog
    std::vector<ContextValue> va = {{o + "engine", OE_VA}};
    C(doc(b.cont(o + "va_shape", N + "Shape", 0, 3, 2, Unit::None), "Morph sine > triangle > saw > pulse."), va);
    C(doc(b.cont(o + "va_pw", N + "Pulse Width", 0.01f, 0.99f, 0.5f, Unit::Percent), "Pulse width (shape at pulse)."), {{o + "engine", OE_VA}, {o + "va_shape", 3}});
    C(doc(b.cont(o + "va_sync", N + "Sync", 1, 16, 1, Unit::Ratio, Role::Amount), "Hard sync ratio; 1 = no sync."), va);
    b.out.back().neutral = 1;
    // HyperSaw
    std::vector<ContextValue> hs = {{o + "engine", OE_HyperSaw}};
    C(doc(b.cont(o + "hs_density", N + "HyperSaw Density", 0, 1, 0.75f, Unit::Percent), "Number/level of saws (1 to 9)."), {{o + "engine", OE_HyperSaw}, {o + "hs_spread", 0.4f}});
    C(doc(b.cont(o + "hs_spread", N + "HyperSaw Spread", 0, 1, 0.3f, Unit::Percent, Role::Amount), "Detune between the saws."), hs);
    C(doc(b.cont(o + "hs_sync", N + "HyperSaw Sync", 1, 16, 1, Unit::Ratio, Role::Amount), "Hard sync ratio of all saws; 1 = none."), hs);
    b.out.back().neutral = 1;
    // Wavetable (also the Grain/Formant source)
    std::vector<ContextValue> wt = {{o + "engine", OE_Wavetable}};
    C(doc(b.integer(o + "wt_table", N + "Table", 0, 63, 0), "Factory wavetable index (user tables override via the browser)."), {{o + "engine", OE_Wavetable}, {o + "wt_pos", 0.5f}});
    C(doc(b.cont(o + "wt_pos", N + "WT Position", 0, 1, 0, Unit::Percent), "Frame position, smoothly interpolated."), {{o + "engine", OE_Wavetable}, {o + "wt_table", 1}});
    C(doc(b.choice(o + "wt_warp_mode", N + "Warp Mode", choices::warpModes(), 0), "Phase/shape warp applied to the frame."), {{o + "engine", OE_Wavetable}, {o + "wt_warp", 0.6f}});
    C(doc(b.cont(o + "wt_warp", N + "Warp", 0, 1, 0, Unit::Percent, Role::Amount), "Warp amount; 0 = none."), {{o + "engine", OE_Wavetable}, {o + "wt_warp_mode", WM_BendPlus}});
    C(doc(b.toggle(o + "wt_pwm_detune", N + "PWM Auto Detune", false), "PWM Squeeze keeps perceived pitch by detuning the fundamental."), {{o + "engine", OE_Wavetable}, {o + "wt_warp_mode", WM_PWM}, {o + "wt_warp", 0.7f}});
    // FM
    std::vector<ContextValue> fm = {{o + "engine", OE_FM}};
    C(doc(b.integer(o + "fm_alg", N + "FM Algorithm", 0, 7, 0), "Operator routing algorithm (8)."), {{o + "engine", OE_FM}, {o + "fm_op2_level", 0.6f}, {o + "fm_op3_level", 0.6f}, {o + "fm_op4_level", 0.5f}});
    for (int op = 1; op <= 4; ++op) {
        const std::string p = o + "fm_op" + std::to_string(op) + "_";
        const std::string n = N + "Op" + std::to_string(op) + " ";
        std::vector<ContextValue> base = {{o + "engine", OE_FM}, {o + "fm_alg", 0}};
        if (op != 1) base.push_back({p + "level", 0.6f});
        if (op == 1) base.push_back({o + "fm_op2_level", 0.0f});
        C(doc(b.cont(p + "ratio", n + "Ratio", 0.125f, 32, op == 1 ? 1.f : 2.f, Unit::Ratio), "Frequency ratio to the note."), base);
        b.out.back().skew = skewForCentre(0.125f, 32, 2);
        { auto bb = base; bb.push_back({p + "freq", 300}); C(doc(b.toggle(p + "fixed", n + "Fixed", false), "Operator ignores the keyboard."), bb); }
        { auto bb = base; bb.push_back({p + "fixed", 1}); C(doc(b.freq(p + "freq", n + "Fixed Hz", 1, 10000, 440, 440), "Operator frequency when Fixed."), bb); }
        { auto bb = base; if (op == 1) bb.back().value = 0; C(doc(b.cont(p + "level", n + "Level", 0, 1, op == 1 ? 1.f : 0.f, Unit::Gain, Role::Level), op == 1 ? "Carrier level." : "Modulator index."), bb); }
        if (op != 1) b.out.back().role = Role::Amount;
        C(doc(b.cont(p + "fb", n + "Feedback", 0, 1, 0, Unit::Percent, Role::Amount), "Operator self-feedback."), base);
        { auto bb = base; bb.push_back({p + "fb", 0.4f}); C(doc(b.choice(p + "wave", n + "Wave", choices::fmWaves(), 0), "Operator waveform."), bb); }
        C(doc(b.time(p + "a", n + "Attack", 0, 10, 0.001f, 0.5f), "Operator envelope attack."), base);
        { auto bb = base; bb.push_back({p + "s", 0.2f}); C(doc(b.time(p + "d", n + "Decay", 0, 10, 0.5f, 0.5f), "Operator envelope decay."), bb); }
        { auto bb = base; bb.push_back({p + "d", 0.05f}); C(doc(b.cont(p + "s", n + "Sustain", 0, 1, 1, Unit::Percent), "Operator envelope sustain."), bb); }
        C(doc(b.time(p + "r", n + "Release", 0, 10, 0.2f, 0.5f), "Operator envelope release."), base);
    }
    // Grain / formant
    std::vector<ContextValue> gf = {{o + "engine", OE_GrainFormant}, {o + "wt_table", 8}};
    C(doc(b.cont(o + "gf_index", N + "GF Index", 0, 1, 0.3f, Unit::Percent), "Frame of the grain table."), gf);
    C(doc(b.cont(o + "gf_shift", N + "Formant Shift", -64, 64, 0, Unit::Semitones, Role::Continuous), "Formant shift in semitones."), gf);
    C(doc(b.cont(o + "gf_spread", N + "Formant Spread", 0, 1, 0, Unit::Percent, Role::Amount), "Second grain stream spread (sub/sync-like)."), gf);
    C(doc(b.cont(o + "gf_detune", N + "Local Detune", 0, 1, 0, Unit::Percent, Role::Amount), "Two detuned streams an octave apart."), gf);
    { auto g = gf; g.push_back({o + "gf_shift", 17}); C(doc(b.toggle(o + "gf_interp", N + "GF Interpolation", true), "Interpolate between grain frames."), g); }
    C(doc(b.toggle(o + "gf_complex", N + "GF Complex", false), "Simple: the fundamental stays audible; Complex: pure grains."), gf);
    { auto g = gf; g.push_back({o + "gf_shift", 12}); C(doc(b.choice(o + "gf_mode", N + "GF Mode", {"Grain", "Formant"}, 1), "Formant mode holds formants at fixed pitch across the keyboard."), g); }
    // Harmonic
    std::vector<ContextValue> ha = {{o + "engine", OE_Harmonic}};
    C(doc(b.integer(o + "ha_partials", N + "Partials", 1, 128, 64), "Number of partials."), ha);
    C(doc(b.cont(o + "ha_tilt", N + "Tilt", -1, 1, 0, Unit::None), "Spectral tilt (0 = saw-like 1/n)."), ha);
    C(doc(b.cont(o + "ha_oddeven", N + "Odd/Even", -1, 1, 0, Unit::None, Role::Neutral), "-1 odd only, +1 even (and fundamental) only."), ha);
    C(doc(b.cont(o + "ha_comb", N + "Comb", 0, 1, 0, Unit::Percent, Role::Amount), "Depth of spectral comb notches."), ha);
    C(doc(b.integer(o + "ha_comb_period", N + "Comb Period", 2, 16, 4), "Partials between comb notches."), {{o + "engine", OE_Harmonic}, {o + "ha_comb", 1}});
    C(doc(b.cont(o + "ha_stretch", N + "Stretch", -1, 1, 0, Unit::None, Role::Neutral), "Pull partials away from harmonic positions."), ha);
    C(doc(b.freq(o + "ha_formant_hz", N + "Formant Freq", 100, 8000, 800, 1000), "Centre of the spectral formant."), {{o + "engine", OE_Harmonic}, {o + "ha_formant_gain", 1}});
    C(doc(b.cont(o + "ha_formant_gain", N + "Formant Amount", 0, 1, 0, Unit::Percent, Role::Amount), "Strength of the spectral formant."), ha);
    C(doc(b.cont(o + "ha_formant_width", N + "Formant Width", 0.05f, 2, 0.5f, Unit::Octaves), "Width of the formant in octaves."), {{o + "engine", OE_Harmonic}, {o + "ha_formant_gain", 1}});
    // Shaper
    b.section = std::string("Osc ") + char('A' + k) + " Shaper";
    struct S { const char* id; const char* name; const char* d; };
    const S shapers[] = {
        {"dc", "DC", "DC adjust of the cycle (removed again after band-limit? no: re-centred)"},
        {"sym", "Symmetry", "Stretch one half-cycle against the other."},
        {"warp", "Warp", "Wrap the wave past a threshold."},
        {"twist", "Twist", "Wrap and invert past a threshold."},
        {"drive", "Drive", "Boost and clip."},
        {"window", "Window", "Triangular window over the cycle."},
        {"offset", "Offset", "Rotate the cycle (phase offset)."},
        {"wave", "Wave", "Morph toward a sine."},
        {"bits", "Bits", "Bit reduction from 32 bits down to 1 bit."},
        {"shrink", "Shrink", "Compress the cycle and zero-fill."},
        {"selfsync", "SelfSync", "Stretch and repeat (sync-like)."},
        {"multipoint", "Multipoint", "Zero the wave at three points."},
        {"mp_width", "Multipoint Width", "Width of the multipoint zero regions."},
        {"lpf", "LPF", "Low-pass the cycle spectrum."},
        {"hpf", "HPF", "High-pass the cycle spectrum."},
        {"fold", "Fold", "Wavefold the cycle."},
        {"spectrum", "Spectrum", "Harmonic levels drift in alternating directions."},
        {"rnd_all", "Random All", "Randomise all harmonic levels."},
        {"rnd_odd", "Random Odd", "Randomise odd harmonic levels."},
    };
    for (auto& s : shapers) {
        std::string id = o + "sh_" + s.id;
        auto& p = b.cont(id, N + s.name, 0, 1, 0, Unit::Percent, Role::Amount);
        p.smoothMs = 10;
        doc(p, s.d);
        if (std::string(s.id) == "dc") p.doc = "DC adjust: shifts the cycle up/down before Shaper stages (re-band-limited; output DC-blocked).";
        if (std::string(s.id) == "mp_width") { p.role = Role::Continuous; p.def = 0.1f; C(p, {{o + "sh_multipoint", 1}}); }
        else C(p);
    }
}

void buildChain(std::vector<ParamDef>& v) {
    B b{v, "Osc Chain", Scope::Voice, kSynth};
    const char* links[] = {"ab", "bc", "ca"};
    const char* names[] = {"A>B", "B>C", "C>A"};
    for (int i = 0; i < 3; ++i) {
        std::string id = std::string("link_") + links[i];
        // Both oscillators sound; source at -1 oct.
        std::vector<ContextValue> c = {{"f1_type", 0}};
        const char src = links[i][0], dst = links[i][1];
        for (char x : {'a', 'b', 'c'}) c.push_back({std::string("osc") + x + "_level", x == dst ? 0.8f : 0.f});
        c.push_back({std::string("osc") + src + "_octave", -1});
        auto cm = c; cm.push_back({id + "_amount", 0.6f});
        doc(ctx(b.choice(id + "_mode", std::string("Link ") + names[i] + " Mode", choices::chainModes(), 4), cm), "How the source oscillator modulates the next one.");
        doc(ctx(b.cont(id + "_amount", std::string("Link ") + names[i] + " Amount", 0, 1, 0, Unit::Percent, Role::Amount), c), "Amount of the link; 0 = none.");
    }
}

void buildSubNoise(std::vector<ParamDef>& v) {
    B b{v, "Sub", Scope::Voice, kSynth};
    std::vector<ContextValue> c = {{"sub_level", 0.8f}, {"osca_level", 0}};
    doc(ctx(b.choice("sub_wave", "Sub Wave", {"Sine", "Triangle", "Soft Square"}, 0), c), "Sub waveform.");
    doc(ctx(b.choice("sub_octave", "Sub Octave", {"-1", "-2"}, 0), c), "Sub octave below the note.");
    doc(ctx(b.cont("sub_level", "Sub Level", 0, 1, 0, Unit::Gain, Role::Level), {{"osca_level", 0}}), "Sub level; 0 = silent.");
    doc(ctx(b.toggle("sub_direct", "Sub Bypass Filters+FX", true), {{"sub_level", 0.8f}, {"osca_level", 0}, {"f1_type", FT_HP24}, {"f1_cutoff", 400}}), "On: sub skips filters and FX (clean, mono).");
    b.section = "Noise";
    std::vector<ContextValue> n = {{"noise_level", 0.6f}, {"osca_level", 0}, {"f1_type", 0}};
    doc(ctx(b.choice("noise_type", "Noise Type", {"White", "Pink", "Crackle", "Metallic", "Digital"}, 0), n), "Noise colour/texture.");
    doc(ctx(b.cont("noise_level", "Noise Level", 0, 1, 0, Unit::Gain, Role::Level), {{"osca_level", 0}, {"f1_type", 0}}), "Noise level; 0 = silent.");
    { auto nn = n; nn[0] = {"noise_level", 0.6f}; nn.push_back({"noise_type", 4}); doc(ctx(b.toggle("noise_track", "Noise Pitch Track", false), nn), "Textured noise follows the played pitch."); }
    doc(ctx(b.cont("noise_balance", "Noise Filter Balance", -1, 1, -1, Unit::None), {{"noise_level", 0.6f}, {"osca_level", 0}, {"filter_routing", 2}, {"f1_type", FT_LP24}, {"f1_cutoff", 300}, {"f2_type", FT_HP24}, {"f2_cutoff", 3000}}), "-1 = Filter 1, +1 = Filter 2.");
}

void buildFilter(std::vector<ParamDef>& v, int k) {
    const std::string f = "f" + std::to_string(k + 1) + "_";
    const std::string N = "F" + std::to_string(k + 1) + " ";
    B b{v, "Filter " + std::to_string(k + 1), Scope::Voice, kBoth};
    // Route the test signal only through this filter.
    std::vector<ContextValue> solo;
    if (k == 0) solo = {{"f2_type", 0}, {"filter_routing", 0}};
    else solo = {{"f1_type", 0}, {"filter_routing", 0}};
    auto C = [&](ParamDef& p, int type, std::vector<ContextValue> extra = {}) -> ParamDef& {
        auto all = solo;
        if (type >= 0) all.push_back({f + "type", (float) type});
        for (auto& e : extra) all.push_back(e);
        return ctx(p, all);
    };
    C(doc(b.choice(f + "type", N + "Type", choices::filterTypes(), k == 0 ? FT_LP24 : FT_Off), "Filter model."), -1, {{f + "cutoff", 800}, {f + "reso", 0.5f}});
    if (k == 1) b.out.back().def = FT_Off;
    C(doc(b.freq(f + "cutoff", N + "Cutoff", 20, 20000, k == 0 ? 6000.f : 1000.f, 1000), "Cutoff / centre frequency."), FT_LP24, {{f + "reso", 0.3f}});
    C(doc(b.cont(f + "reso", N + "Resonance", 0, 1, 0.1f, Unit::Percent), "Resonance / emphasis."), FT_LP24, {{f + "cutoff", 1000}});
    C(doc(b.cont(f + "sep", N + "Separation", 0, 1, 0.3f, Unit::Percent), "Spacing of the resonant peaks (multi-peak types)."), FT_TriPeak, {{f + "cutoff", 1000}, {f + "reso", 0.7f}});
    C(doc(b.cont(f + "morph", N + "Morph / Vowel", 0, 1, 0, Unit::Percent), "Morph LP>BP>HP>Notch; vowel A-E-I-O-U for Formant."), FT_Morph, {{f + "cutoff", 1000}, {f + "reso", 0.5f}});
    C(doc(b.cont(f + "gender", N + "Gender", -1, 1, 0, Unit::None, Role::Neutral), "Formant gender shift."), FT_Formant, {{f + "morph", 0.3f}});
    C(doc(b.cont(f + "shoulder", N + "Shoulder Level", 0, 1, 0.6f, Unit::Percent), "Tri-Peak shoulder peak level."), FT_TriPeak, {{f + "cutoff", 1000}, {f + "reso", 0.7f}});
    C(doc(b.cont(f + "shelf", N + "Shelf Boost", 0, 1, 0.5f, Unit::Percent), "Tri-Peak raised shelf below the peaks."), FT_TriPeak, {{f + "cutoff", 1500}, {f + "reso", 0.7f}});
    C(doc(b.integer(f + "poles", N + "Ladder Poles", 1, 4, 4), "Ladder slope 6-24 dB/oct."), FT_Ladder, {{f + "cutoff", 600}});
    C(doc(b.integer(f + "stages", N + "Phaser Stages", 4, 12, 8), "Phaser-filter all-pass stages."), FT_Phaser, {{f + "cutoff", 800}, {f + "reso", 0.6f}});
    C(doc(b.toggle(f + "opposing", N + "Opposing", false), "Dual-Reso peaks move in opposite directions with cutoff."), FT_DualResoBP, {{f + "cutoff", 2000}, {f + "reso", 0.7f}, {f + "sep", 0.5f}});
    C(doc(b.cont(f + "keytrack", N + "Key Track", 0, 1, 0, Unit::Percent, Role::Amount), "Cutoff follows the played note (100% = 1 oct/oct)."), FT_LP24, {{f + "cutoff", 500}, {f + "reso", 0.4f}});
    C(doc(b.cont(f + "drive", N + "Drive", 0, 1, 0, Unit::Percent, Role::Drive), "Filter drive; 0 = none."), FT_LP24, {{f + "cutoff", 2000}});
    C(doc(b.choice(f + "drive_type", N + "Drive Type", choices::distTypes(), 2), "Filter drive curve."), FT_LP24, {{f + "drive", 0.7f}});
    C(doc(b.choice(f + "drive_pos", N + "Drive Position", {"Pre", "Post"}, 0), "Drive before or after the filter."), FT_LP24, {{f + "drive", 0.8f}, {f + "reso", 0.8f}, {f + "cutoff", 400}});
    C(doc(b.cont(f + "env", N + "Env Amount", -1, 1, 0, Unit::None, Role::Depth), "Bipolar envelope amount on cutoff (±8 octaves)."), FT_LP24, {{f + "cutoff", 400}, {"env2_d", 0.2f}, {"env2_s", 0}});
    C(doc(b.choice(f + "env_src", N + "Env Source", {"Env 1", "Env 2", "Env 3", "Env 4"}, 1 + k), "Which envelope the Env Amount uses."), FT_LP24, {{f + "cutoff", 400}, {f + "env", 0.6f}, {"env2_d", 0.05f}, {"env2_s", 0}, {"env3_d", 1.0f}});
    C(doc(b.toggle(f + "boost", N + "Reso Boost", false), "Extends the resonance range."), FT_LP24, {{f + "reso", 0.8f}, {f + "cutoff", 600}});
    C(doc(b.choice(f + "limiter", N + "Reso Limiter", {"Off", "Slow", "Fast"}, 0), "Tames runaway resonance by level."), FT_LP24, {{f + "reso", 1}, {f + "boost", 1}, {f + "cutoff", 300}, {f + "drive", 0.6f}});
    C(doc(b.cont(f + "pan", N + "Pan", -1, 1, 0, Unit::None, Role::Neutral), "Filter output pan."), FT_LP24);
    C(doc(b.cont(f + "level", N + "Level", 0, 1, 1, Unit::Gain, Role::Level), "Filter output level; 0 = silent."), FT_LP24);
}

void buildEnvelopes(std::vector<ParamDef>& v) {
    for (int k = 1; k <= 4; ++k) {
        const std::string e = "env" + std::to_string(k) + "_";
        const std::string N = "Env " + std::to_string(k) + " ";
        B b{v, "Envelope " + std::to_string(k), Scope::Voice, kBoth};
        // Env 1 = amp envelope (always audible). Others: route to F1 cutoff.
        std::vector<ContextValue> c;
        std::vector<ContextRoute> r;
        if (k > 1) { c = {{"f1_type", FT_LP24}, {"f1_cutoff", 200}, {"f1_reso", 0.3f}}; r = {{"env" + std::to_string(k), "f1_cutoff", 0.7f}}; }
        auto C = [&](ParamDef& p, std::vector<ContextValue> extra = {}) -> ParamDef& {
            auto all = c; for (auto& x : extra) all.push_back(x);
            return ctx(p, all, r);
        };
        C(doc(b.time(e + "a", N + "Attack", 0, 20, k == 1 ? 0.002f : 0.001f, 0.5f), "Attack time (seconds mode)."), {{e + "s", 0.5f}});
        C(doc(b.time(e + "d", N + "Decay", 0, 20, k == 1 ? 0.3f : 0.4f, 0.5f), "Decay time (seconds mode)."), {{e + "s", 0.2f}});
        C(doc(b.cont(e + "s", N + "Sustain", 0, 1, k == 1 ? 1.f : 0.f, Unit::Percent), "Sustain level."), {{e + "d", 0.05f}});
        C(doc(b.time(e + "r", N + "Release", 0, 20, k == 1 ? 0.08f : 0.3f, 0.5f), "Release time (seconds mode)."), {{e + "s", 1}});
        C(doc(b.cont(e + "ac", N + "Attack Curve", -1, 1, 0, Unit::None), "Attack curvature."), {{e + "a", 0.3f}, {e + "s", 1}});
        C(doc(b.cont(e + "dc", N + "Decay Curve", -1, 1, -0.5f, Unit::None), "Decay curvature."), {{e + "d", 0.3f}, {e + "s", 0}});
        C(doc(b.cont(e + "rc", N + "Release Curve", -1, 1, -0.5f, Unit::None), "Release curvature."), {{e + "r", 0.3f}, {e + "s", 1}});
        C(doc(b.toggle(e + "beats", N + "Tempo Sync", false), "Stage times in beats instead of seconds."), {{e + "d_b", 0.25f}, {e + "s", 0}, {e + "d", 2}});
        C(doc(b.time(e + "a_b", N + "Attack (beats)", 0, 16, 0, 1, Unit::Beats), "Attack in beats (tempo mode)."), {{e + "beats", 1}, {e + "s", 0.5f}});
        C(doc(b.time(e + "d_b", N + "Decay (beats)", 0, 16, 0.5f, 1, Unit::Beats), "Decay in beats (tempo mode)."), {{e + "beats", 1}, {e + "s", 0.2f}});
        C(doc(b.time(e + "r_b", N + "Release (beats)", 0, 16, 0.25f, 1, Unit::Beats), "Release in beats (tempo mode)."), {{e + "beats", 1}, {e + "s", 1}});
        C(doc(b.choice(e + "mode", N + "Mode", {"ADSR", "Drawn"}, 0), "ADSR or drawn multi-segment shape."), {{e + "s", 1}});
        C(doc(b.time(e + "len", N + "Drawn Length", 0.01f, 20, 1, 1), "Length of the drawn envelope (seconds)."), {{e + "mode", 1}});
        C(doc(b.time(e + "len_b", N + "Drawn Length (beats)", 0.0625f, 32, 2, 2, Unit::Beats), "Length of the drawn envelope (tempo mode)."), {{e + "mode", 1}, {e + "beats", 1}});
    }
}

void buildLfos(std::vector<ParamDef>& v) {
    for (int k = 1; k <= 6; ++k) {
        const std::string l = "lfo" + std::to_string(k) + "_";
        const std::string N = "LFO " + std::to_string(k) + " ";
        B b{v, "LFO " + std::to_string(k), k <= 4 ? Scope::Global : Scope::Voice, kBoth};
        std::vector<ContextValue> c = {{"f1_type", FT_LP24}, {"f1_cutoff", 800}, {"f1_reso", 0.4f}};
        std::vector<ContextRoute> r = {{"lfo" + std::to_string(k), "f1_cutoff", 0.6f}};
        auto C = [&](ParamDef& p, std::vector<ContextValue> extra = {}) -> ParamDef& {
            auto all = c; for (auto& x : extra) all.push_back(x);
            return ctx(p, all, r);
        };
        C(doc(b.freq(l + "rate", N + "Rate", 0.01f, 2000, 2, 4), "Free rate (up to audio range)."));
        C(doc(b.toggle(l + "sync", N + "Tempo Sync", false), "Rate from tempo division."), {{l + "div", 14}});
        C(doc(b.choice(l + "div", N + "Division", divNames(), 8), "Tempo division incl. dotted/triplet."), {{l + "sync", 1}});
        C(doc(b.cont(l + "phase", N + "Phase", 0, 360, 0, Unit::Degrees), "Start phase."), {{l + "rate", 1}});
        C(doc(b.cont(l + "offset", N + "Offset", -1, 1, 0, Unit::None, Role::Neutral), "Output offset."));
        C(doc(b.time(l + "delay", N + "Delay", 0, 10, 0, 1), "Delay before the LFO starts."), {{l + "rate", 5}});
        C(doc(b.time(l + "fade", N + "Fade In", 0, 10, 0, 1), "Fade-in time after the delay."), {{l + "rate", 5}});
        C(doc(b.cont(l + "smooth", N + "Smooth", 0, 1, 0, Unit::Percent, Role::Amount), "Smooths the shape (slew)."), {{l + "rate", 3}, {l + "morph", 1}});
        C(doc(b.choice(l + "mode", N + "Trigger Mode", {"Retrigger", "Free", "One Shot"}, k <= 4 ? 1 : 0), "Retrigger on note, free-run, or one-shot."), {{l + "rate", 1.3f}});
        C(doc(b.cont(l + "morph", N + "Shape Morph", 0, 1, 0, Unit::Percent), "Morph shape A > shape B."));
        C(doc(b.time(l + "morph_time", N + "Morph Time", 0, 30, 0, 2), "Auto-morph from Morph to B over this time after note-on; 0 = manual."), {{l + "rate", 4}});
        b.out.back().role = Role::Amount;
    }
}

void buildSeqRandMacros(std::vector<ParamDef>& v) {
    for (int k = 1; k <= 2; ++k) {
        const std::string s = "seq" + std::to_string(k) + "_";
        const std::string N = "Seq " + std::to_string(k) + " ";
        B b{v, "Step Seq " + std::to_string(k), Scope::Global, kBoth};
        std::vector<ContextValue> c = {{"f1_type", FT_LP24}, {"f1_cutoff", 500}, {"f1_reso", 0.3f}};
        std::vector<ContextRoute> r = {{"seq" + std::to_string(k), "f1_cutoff", 0.8f}};
        doc(ctx(b.choice(s + "div", N + "Rate", divNames(), 14), c, r), "Step length.");
        doc(ctx(b.integer(s + "steps", N + "Steps", 1, 32, 16), c, r), "Sequence length.");
        doc(ctx(b.cont(s + "slide", N + "Slide Time", 0, 1, 0.5f, Unit::Percent), c, r), "Glide time for steps with slide on (fraction of a step).");
        doc(ctx(b.choice(s + "mode", N + "Mode", {"Retrigger", "Free"}, 1), c, r), "Restart on note or follow the transport.");
    }
    for (int k = 1; k <= 4; ++k) {
        const std::string s = "rnd" + std::to_string(k) + "_";
        const std::string N = "Random " + std::to_string(k) + " ";
        B b{v, "Random " + std::to_string(k), Scope::Global, kBoth};
        std::vector<ContextValue> c = {{"f1_type", FT_LP24}, {"f1_cutoff", 600}, {"f1_reso", 0.3f}};
        std::vector<ContextRoute> r = {{"rnd" + std::to_string(k), "f1_cutoff", 0.8f}};
        doc(ctx(b.choice(s + "type", N + "Type", {"Sample & Hold", "Smooth", "Chaos", "Per Note"}, 0), c, r), "Random generator type.");
        doc(ctx(b.freq(s + "rate", N + "Rate", 0.01f, 100, 4, 4), c, r), "Rate.");
        doc(ctx(b.toggle(s + "sync", N + "Tempo Sync", false), c, r), "Tempo-synced rate.");
        doc(ctx(b.choice(s + "div", N + "Division", divNames(), 11), {{"f1_type", FT_LP24}, {"f1_cutoff", 600}, {s + "sync", 1}}, r), "Tempo division.");
        doc(ctx(b.toggle(s + "bipolar", N + "Bipolar", true), c, r), "Bipolar (-1..1) or unipolar (0..1).");
    }
    {
        B b{v, "Macros", Scope::Global, kBoth};
        for (int k = 1; k <= 8; ++k) {
            auto& p = b.cont("macro" + std::to_string(k), "Macro " + std::to_string(k), 0, 1, 0, Unit::Percent);
            ctx(p, {{"f1_type", FT_LP24}, {"f1_cutoff", 400}}, {{"macro" + std::to_string(k), "f1_cutoff", 0.8f}});
            p.doc = "Macro knob (assign by dragging onto any control).";
        }
    }
    {
        B b{v, "Functions", Scope::Global, kBoth};
        for (int k = 1; k <= 4; ++k) {
            const std::string s = "fn" + std::to_string(k) + "_";
            std::vector<ContextValue> c = {{"f1_type", FT_LP24}, {"f1_cutoff", 400}};
            std::vector<ContextRoute> r = {{"fn" + std::to_string(k), "f1_cutoff", 0.8f}};
            doc(ctx(b.choice(s + "op", "Function " + std::to_string(k) + " Op", {"Add", "Multiply", "Min", "Max", "Lag"}, 0), c, r), "Combines inputs A and B into a new source.");
            doc(ctx(b.time(s + "lag", "Function " + std::to_string(k) + " Lag", 0, 2000, 0, 100, Unit::Ms), c, r), "Lag (slew) applied to the function output.");
            b.out.back().role = Role::Amount;
        }
    }
    {
        B b{v, "Follower", Scope::Global, kBoth};
        std::vector<ContextValue> c = {{"f1_type", FT_LP24}, {"f1_cutoff", 400}};
        std::vector<ContextRoute> r = {{"follower", "f1_cutoff", 0.8f}};
        doc(ctx(b.time("ef_attack", "Follower Attack", 0.1f, 500, 5, 20, Unit::Ms), c, r), "Envelope follower attack.");
        doc(ctx(b.time("ef_release", "Follower Release", 1, 2000, 80, 100, Unit::Ms), c, r), "Envelope follower release.");
        doc(ctx(b.cont("ef_gain", "Follower Gain", -24, 24, 0, Unit::Db), c, r), "Follower input gain.");
        doc(ctx(b.cont("tr_sens", "Transient Sensitivity", 0, 1, 0.5f, Unit::Percent), c, {{"transient", "f1_cutoff", 0.8f}}), "Transient detector sensitivity.");
        doc(ctx(b.time("tr_decay", "Transient Decay", 1, 1000, 60, 60, Unit::Ms), c, {{"transient", "f1_cutoff", 0.8f}}), "Transient source decay.");
        b.plugins = kFx;
        doc(ctx(b.toggle("ef_sidechain", "Follower Sidechain", false), c, r), "Follower/transient listen to the sidechain input.");
        doc(ctx(b.choice("fxp_env_trigger", "Envelope Trigger", {"MIDI", "Transient", "Both"}, 2), {{"f1_type", FT_LP24}, {"f1_cutoff", 300}}, {{"env2", "f1_cutoff", 0.8f}}), "What retriggers the envelopes in GNARL FX.");
        doc(b.cont("fxp_in_gain", "Input Gain", -24, 24, 0, Unit::Db, Role::Neutral), "Input gain (0 dB = unchanged).");
    }
}

void buildRoutes(std::vector<ParamDef>& v) {
    B b{v, "Mod Routes", Scope::Global, kBoth};
    for (int k = 1; k <= 64; ++k) {
        auto& p = b.cont("mod" + std::to_string(k) + "_depth", "Route " + std::to_string(k) + " Depth", -1, 1, 0, Unit::None, Role::Depth);
        p.smoothMs = 10;
        p.doc = "Depth of modulation route " + std::to_string(k) + " (source/destination set in the matrix).";
        p.contextRoutes.push_back({"#route" + std::to_string(k) + ":lfo1", "f1_cutoff", 0});
        ctx(p, {{"f1_type", FT_LP24}, {"f1_cutoff", 600}, {"lfo1_rate", 3}});
    }
}

void buildFx(std::vector<ParamDef>& v) {
    B b{v, "FX", Scope::Global, kBoth};
    auto mod = [&](const std::string& m, const std::string& name) {
        b.section = "FX " + name;
        doc(b.toggle("fx_" + m + "_on", name + " On", false), "Module on/off; off = bit-transparent.");
        doc(b.cont("fx_" + m + "_mix", name + " Mix", 0, 1, 1, Unit::Percent, Role::Mix), "Dry/wet: 0% = dry exactly, 100% = wet only.");
    };
    // Context: module on + placed in a slot is handled by the audit (it places every module it tests).
    auto C = [&](ParamDef& p, const std::string& m, std::vector<ContextValue> extra = {}) -> ParamDef& {
        std::vector<ContextValue> c = {{"fx_" + m + "_on", 1}};
        for (auto& e : extra) c.push_back(e);
        return ctx(p, c);
    };
    auto onCtx = [&](const std::string& m, std::vector<ContextValue> extra) {
        // apply context to the on/mix params just added
        auto& on = v[v.size() - 2]; auto& mix = v[v.size() - 1];
        ctx(on, extra); auto e2 = extra; e2.push_back({"fx_" + m + "_on", 1}); ctx(mix, e2);
    };
    for (int d = 1; d <= 2; ++d) {
        std::string m = "dist" + std::to_string(d);
        mod(m, d == 1 ? "Distortion A" : "Distortion B");
        onCtx(m, {{"fx_" + m + "_drive", 0.6f}});
        C(doc(b.choice("fx_" + m + "_type", (d == 1 ? "Dist A" : "Dist B") + std::string(" Type"), choices::distTypes(), 0), "Distortion curve."), m, {{"fx_" + m + "_drive", 0.6f}});
        C(doc(b.cont("fx_" + m + "_drive", (d == 1 ? "Dist A" : "Dist B") + std::string(" Drive"), 0, 1, 0.3f, Unit::Percent, Role::Drive), "Drive; 0 = none."), m);
        C(doc(b.cont("fx_" + m + "_bias", (d == 1 ? "Dist A" : "Dist B") + std::string(" Bias"), -1, 1, 0, Unit::None, Role::Neutral), "Asymmetry bias into the shaper (DC-blocked)."), m, {{"fx_" + m + "_drive", 0.5f}});
        C(doc(b.freq("fx_" + m + "_tone", (d == 1 ? "Dist A" : "Dist B") + std::string(" Tone"), 200, 20000, 20000, 3000), "Post low-pass; max = open."), m, {{"fx_" + m + "_drive", 0.6f}});
        b.out.back().role = Role::Neutral; b.out.back().neutral = 20000;
        C(doc(b.cont("fx_" + m + "_out", (d == 1 ? "Dist A" : "Dist B") + std::string(" Output"), -24, 12, 0, Unit::Db, Role::Neutral), "Output gain of the wet signal."), m, {{"fx_" + m + "_drive", 0.6f}});
        C(doc(b.cont("fx_" + m + "_morph", (d == 1 ? "Dist A" : "Dist B") + std::string(" Draw Morph"), 0, 1, 0, Unit::Percent), "Draw type: morph curve 1 > curve 2."), m, {{"fx_" + m + "_type", DT_Draw}, {"fx_" + m + "_drive", 0.6f}});
    }
    // Multiband distortion
    {
        std::string m = "mbd"; mod(m, "Multiband Dist");
        onCtx(m, {{"fx_mbd_b2_drive", 0.8f}});
        C(doc(b.freq("fx_mbd_xlo", "MB Dist Low X-over", 40, 2000, 250, 300), "Low/mid crossover."), m, {{"fx_mbd_b1_drive", 0.9f}});
        C(doc(b.freq("fx_mbd_xhi", "MB Dist High X-over", 400, 12000, 2500, 2500), "Mid/high crossover."), m, {{"fx_mbd_b3_drive", 0.9f}});
        C(doc(b.cont("fx_mbd_converge", "MB Dist Converge", -1, 1, 0, Unit::None, Role::Neutral), "Narrow (+) or widen (-) the mid band around its centre."), m, {{"fx_mbd_b2_drive", 0.9f}});
        const char* bn[] = {"Low", "Mid", "High"};
        for (int i = 1; i <= 3; ++i) {
            std::string p = "fx_mbd_b" + std::to_string(i) + "_";
            std::string n = std::string("MB Dist ") + bn[i - 1] + " ";
            C(doc(b.choice(p + "type", n + "Type", choices::distTypes(), 0), "Band distortion type."), m, {{p + "drive", 0.7f}});
            C(doc(b.cont(p + "drive", n + "Drive", 0, 1, 0, Unit::Percent, Role::Drive), "Band drive; 0 = clean."), m);
            C(doc(b.cont(p + "mix", n + "Mix", 0, 1, 1, Unit::Percent, Role::Mix), "Band dry/wet."), m, {{p + "drive", 0.8f}});
            C(doc(b.cont(p + "dyn", n + "Dynamics", -1, 1, 0, Unit::None, Role::Depth), "-1 gate ... +1 compress."), m, {{p + "drive", 0.4f}});
            C(doc(b.cont(p + "fb", n + "Feedback", 0, 0.95f, 0, Unit::Percent, Role::Amount), "Band feedback."), m, {{p + "drive", 0.5f}});
        }
    }
    {
        std::string m = "phaser"; mod(m, "Phaser"); onCtx(m, {});
        C(doc(b.freq("fx_phaser_rate", "Phaser Rate", 0.01f, 20, 0.5f, 1), "Sweep rate."), m, {{"fx_phaser_depth", 0.8f}});
        C(doc(b.cont("fx_phaser_depth", "Phaser Depth", 0, 1, 0.5f, Unit::Percent), "Sweep depth (0 = static notches)."), m);
        C(doc(b.cont("fx_phaser_fb", "Phaser Feedback", -0.95f, 0.95f, 0.3f, Unit::Percent), "Feedback."), m);
        C(doc(b.freq("fx_phaser_centre", "Phaser Centre", 50, 10000, 800, 1000), "Centre frequency."), m);
        C(doc(b.integer("fx_phaser_stages", "Phaser Stages", 2, 12, 6), "All-pass stages."), m);
        C(doc(b.cont("fx_phaser_stereo", "Phaser Stereo", 0, 180, 90, Unit::Degrees), "L/R LFO phase offset."), m, {{"fx_phaser_depth", 0.8f}});
    }
    {
        std::string m = "flanger"; mod(m, "Flanger"); onCtx(m, {});
        C(doc(b.freq("fx_flanger_rate", "Flanger Rate", 0.01f, 20, 0.3f, 1), "Sweep rate."), m, {{"fx_flanger_depth", 0.8f}});
        C(doc(b.cont("fx_flanger_depth", "Flanger Depth", 0, 1, 0.5f, Unit::Percent), "Sweep depth."), m);
        C(doc(b.time("fx_flanger_delay", "Flanger Delay", 0.1f, 20, 2, 3, Unit::Ms), "Base delay."), m);
        C(doc(b.cont("fx_flanger_fb", "Flanger Feedback", -0.98f, 0.98f, 0.5f, Unit::Percent), "Feedback."), m);
        C(doc(b.cont("fx_flanger_stereo", "Flanger Stereo", 0, 180, 90, Unit::Degrees), "L/R phase offset."), m, {{"fx_flanger_depth", 0.8f}});
    }
    {
        std::string m = "chorus"; mod(m, "Chorus"); onCtx(m, {});
        C(doc(b.freq("fx_chorus_rate", "Chorus Rate", 0.01f, 10, 0.6f, 1), "Rate."), m);
        C(doc(b.cont("fx_chorus_depth", "Chorus Depth", 0, 1, 0.5f, Unit::Percent), "Depth."), m);
        C(doc(b.time("fx_chorus_delay", "Chorus Delay", 1, 40, 12, 12, Unit::Ms), "Base delay."), m);
        C(doc(b.cont("fx_chorus_width", "Chorus Width", 0, 1, 1, Unit::Percent), "Stereo width of the wet signal."), m);
        C(doc(b.freq("fx_chorus_mono", "Chorus Mono Below", 20, 500, 150, 150), "Wet is mono below this frequency (dry low end untouched)."), m);
    }
    {
        std::string m = "comb"; mod(m, "Comb"); onCtx(m, {});
        C(doc(b.freq("fx_comb_freq", "Comb Freq", 20, 5000, 200, 300), "Comb frequency."), m);
        C(doc(b.cont("fx_comb_fb", "Comb Feedback", -0.99f, 0.99f, 0.7f, Unit::Percent), "Feedback (+/- polarity)."), m);
        C(doc(b.cont("fx_comb_damp", "Comb Damping", 0, 1, 0.2f, Unit::Percent, Role::Amount), "High damping in the loop."), m);
        C(doc(b.toggle("fx_comb_key", "Comb Key Track", false), "Frequency follows the last played note."), m);
        b.out.back().plugins = kBoth;
    }
    {
        std::string m = "vowel"; mod(m, "Vowel EQ"); onCtx(m, {});
        C(doc(b.cont("fx_vowel_morph", "Vowel Morph", 0, 1, 0, Unit::Percent), "Vowel A>E>I>O>U (moves all 3 peaks)."), m);
        C(doc(b.cont("fx_vowel_shift", "Vowel Shift", -12, 12, 0, Unit::Semitones, Role::Neutral), "Shift the peaks."), m);
        C(doc(b.cont("fx_vowel_q", "Vowel Q", 0.5f, 20, 6, Unit::None), "Peak sharpness."), m);
        C(doc(b.cont("fx_vowel_gain", "Vowel Gain", 0, 24, 12, Unit::Db, Role::Amount), "Peak boost; 0 dB = flat."), m);
    }
    {
        std::string m = "eq"; mod(m, "EQ"); onCtx(m, {{"fx_eq_g1", 6}});
        const char* bn[] = {"Low Shelf", "Peak 1", "Peak 2", "High Shelf"};
        const float fd[] = {120, 600, 3000, 8000};
        for (int i = 1; i <= 4; ++i) {
            std::string p = "fx_eq_"; std::string n = std::string("EQ ") + bn[i - 1] + " ";
            C(doc(b.freq(p + "f" + std::to_string(i), n + "Freq", 20, 20000, fd[i - 1], 1000), "Band frequency."), m, {{p + "g" + std::to_string(i), 9}});
            C(doc(b.cont(p + "g" + std::to_string(i), n + "Gain", -24, 24, 0, Unit::Db, Role::Neutral), "Band gain; 0 dB = flat."), m);
            if (i == 2 || i == 3) C(doc(b.cont(p + "q" + std::to_string(i), n + "Q", 0.1f, 18, 1, Unit::None), "Band Q."), m, {{p + "g" + std::to_string(i), 9}});
        }
    }
    {
        std::string m = "fshift"; mod(m, "Freq Shifter"); onCtx(m, {{"fx_fshift_hz", 50}});
        C(doc(b.cont("fx_fshift_hz", "Shift", -2000, 2000, 0, Unit::Hz), "Frequency shift in Hz."), m);
        C(doc(b.cont("fx_fshift_fb", "Shift Feedback", 0, 0.95f, 0, Unit::Percent, Role::Amount), "Feedback."), m, {{"fx_fshift_hz", 30}});
        C(doc(b.cont("fx_fshift_stereo", "Shift Stereo", -1, 1, 0, Unit::None, Role::Neutral), "Opposite shift on the right channel (fraction)."), m, {{"fx_fshift_hz", 60}});
    }
    {
        std::string m = "ring"; mod(m, "Ring Mod"); onCtx(m, {});
        C(doc(b.freq("fx_ring_freq", "Ring Freq", 1, 5000, 200, 200), "Modulator frequency."), m);
        C(doc(b.choice("fx_ring_wave", "Ring Wave", {"Sine", "Triangle", "Square"}, 0), "Modulator waveform."), m);
        C(doc(b.toggle("fx_ring_key", "Ring Key Track", false), "Frequency follows the last note (freq becomes ratio×note)."), m);
    }
    {
        std::string m = "crush"; mod(m, "Bitcrusher"); onCtx(m, {});
        C(doc(b.cont("fx_crush_bits", "Crush Bits", 1, 24, 8, Unit::Bits), "Bit depth."), m);
        C(doc(b.freq("fx_crush_rate", "Crush Rate", 100, 48000, 12000, 6000), "Sample-rate reduction target."), m);
        C(doc(b.toggle("fx_crush_dither", "Crush Dither", false), "TPDF dither before quantising."), m, {{"fx_crush_bits", 4}});
    }
    {
        std::string m = "mbcomp"; mod(m, "Multiband Comp"); onCtx(m, {});
        C(doc(b.cont("fx_mbcomp_depth", "MB Comp Depth", 0, 1, 0.6f, Unit::Percent, Role::Amount), "Overall amount; 0 = unity."), m);
        C(doc(b.cont("fx_mbcomp_time", "MB Comp Time", 0.1f, 10, 1, Unit::Ratio), "Attack/release time scale."), m);
        C(doc(b.cont("fx_mbcomp_up", "MB Comp Upward", 0, 1, 0.6f, Unit::Percent, Role::Amount), "Upward compression amount."), m);
        C(doc(b.cont("fx_mbcomp_down", "MB Comp Downward", 0, 1, 0.8f, Unit::Percent, Role::Amount), "Downward compression amount."), m);
        C(doc(b.freq("fx_mbcomp_xlo", "MB Comp Low X-over", 40, 1000, 120, 150), "Low/mid crossover."), m);
        C(doc(b.freq("fx_mbcomp_xhi", "MB Comp High X-over", 1000, 12000, 2500, 2500), "Mid/high crossover."), m);
        C(doc(b.cont("fx_mbcomp_out", "MB Comp Output", -24, 24, 0, Unit::Db, Role::Neutral), "Output gain."), m);
    }
    {
        std::string m = "comp"; mod(m, "Compressor"); onCtx(m, {{"fx_comp_thresh", -30}});
        C(doc(b.cont("fx_comp_thresh", "Comp Threshold", -60, 0, -18, Unit::Db), "Threshold."), m, {{"fx_comp_ratio", 8}});
        C(doc(b.cont("fx_comp_ratio", "Comp Ratio", 1, 20, 4, Unit::Ratio, Role::Amount), "Ratio; 1 = none."), m, {{"fx_comp_thresh", -30}});
        b.out.back().neutral = 1;
        C(doc(b.time("fx_comp_attack", "Comp Attack", 0.1f, 200, 5, 10, Unit::Ms), "Attack."), m, {{"fx_comp_thresh", -30}, {"fx_comp_ratio", 10}});
        C(doc(b.time("fx_comp_release", "Comp Release", 5, 2000, 100, 100, Unit::Ms), "Release."), m, {{"fx_comp_thresh", -30}, {"fx_comp_ratio", 10}});
        C(doc(b.cont("fx_comp_makeup", "Comp Makeup", -12, 24, 0, Unit::Db, Role::Neutral), "Makeup gain."), m);
        C(doc(b.toggle("fx_comp_clip", "Comp Soft Clip", false), "Soft clip after makeup."), m, {{"fx_comp_makeup", 18}});
    }
    {
        std::string m = "delay"; mod(m, "Delay"); onCtx(m, {});
        C(doc(b.time("fx_delay_time", "Delay Time", 1, 2000, 250, 250, Unit::Ms), "Delay time (free)."), m);
        C(doc(b.toggle("fx_delay_sync", "Delay Sync", false), "Tempo-synced time."), m);
        C(doc(b.choice("fx_delay_div", "Delay Division", divNames(), 11), "Tempo division."), m, {{"fx_delay_sync", 1}});
        C(doc(b.cont("fx_delay_fb", "Delay Feedback", 0, 0.98f, 0.4f, Unit::Percent, Role::Amount), "Feedback."), m);
        C(doc(b.freq("fx_delay_lp", "Delay High Cut", 200, 20000, 12000, 4000), "Low-pass in the loop."), m, {{"fx_delay_fb", 0.6f}});
        C(doc(b.freq("fx_delay_hp", "Delay Low Cut", 20, 2000, 80, 200), "High-pass in the loop."), m, {{"fx_delay_fb", 0.6f}});
        C(doc(b.toggle("fx_delay_pp", "Delay Ping-Pong", false), "Ping-pong between channels."), m);
    }
    {
        std::string m = "reverb"; mod(m, "Reverb"); onCtx(m, {}); v.back().def = 0.3f;
        C(doc(b.cont("fx_reverb_size", "Reverb Size", 0, 1, 0.5f, Unit::Percent), "Room size."), m);
        C(doc(b.time("fx_reverb_decay", "Reverb Decay", 0.1f, 20, 2, 2), "Decay time (RT60)."), m);
        C(doc(b.time("fx_reverb_pre", "Reverb Pre-delay", 0, 250, 10, 30, Unit::Ms), "Pre-delay."), m);
        C(doc(b.cont("fx_reverb_damp", "Reverb Damping", 0, 1, 0.4f, Unit::Percent, Role::Amount), "High damping."), m);
        C(doc(b.cont("fx_reverb_duck", "Reverb Ducking", 0, 1, 0, Unit::Percent, Role::Amount), "Duck the wet signal while input is loud."), m);
        C(doc(b.cont("fx_reverb_width", "Reverb Width", 0, 1, 1, Unit::Percent), "Wet stereo width."), m);
    }
    {
        std::string m = "moddelay"; mod(m, "Mod Delay"); onCtx(m, {});
        C(doc(b.time("fx_moddelay_time", "Mod Delay Time", 0.1f, 50, 5, 5, Unit::Ms), "Delay time; click-free under fast modulation."), m);
        C(doc(b.freq("fx_moddelay_rate", "Mod Delay LFO Rate", 0.01f, 2000, 3, 10), "Internal modulation rate."), m, {{"fx_moddelay_depth", 0.6f}});
        C(doc(b.cont("fx_moddelay_depth", "Mod Delay LFO Depth", 0, 1, 0, Unit::Percent, Role::Amount), "Internal modulation depth."), m);
        C(doc(b.cont("fx_moddelay_fb", "Mod Delay Feedback", -0.98f, 0.98f, 0.6f, Unit::Percent), "Feedback."), m);
        C(doc(b.freq("fx_moddelay_bp", "Mod Delay BP Freq", 50, 12000, 1500, 1500), "Band-pass in the loop."), m, {{"fx_moddelay_fb", 0.8f}});
        C(doc(b.cont("fx_moddelay_q", "Mod Delay BP Q", 0.1f, 10, 0.7f, Unit::None), "Band-pass Q."), m, {{"fx_moddelay_fb", 0.8f}});
    }
    {
        std::string m = "reso"; mod(m, "Resonator"); onCtx(m, {});
        C(doc(b.freq("fx_reso_freq", "Resonator Freq", 20, 2000, 110, 200), "Root frequency."), m);
        C(doc(b.toggle("fx_reso_key", "Resonator Key Track", false), "Root follows the last note."), m);
        C(doc(b.toggle("fx_reso_fifth", "Resonator Fifth", false), "Adds a resonator a fifth above."), m);
        C(doc(b.time("fx_reso_decay", "Resonator Decay", 0.01f, 10, 0.5f, 0.5f), "Decay time."), m);
        C(doc(b.cont("fx_reso_bright", "Resonator Brightness", 0, 1, 0.5f, Unit::Percent), "Upper partial level."), m);
        b.plugins = kFx;
        C(doc(b.toggle("fx_reso_sc", "Resonator Sidechain Exciter", false), "Excite the resonators from the sidechain input instead of the main input."), m);
        b.plugins = kBoth;
    }
    {
        std::string m = "disperse"; mod(m, "Disperser"); onCtx(m, {{"fx_disperse_amount", 0.6f}});
        C(doc(b.freq("fx_disperse_freq", "Disperser Freq", 50, 10000, 600, 600), "Centre of the phase smear."), m, {{"fx_disperse_amount", 0.6f}});
        C(doc(b.cont("fx_disperse_amount", "Disperser Amount", 0, 1, 0.5f, Unit::Percent, Role::Amount), "Number of all-pass stages (0 = none)."), m);
        C(doc(b.cont("fx_disperse_pinch", "Disperser Pinch", 0.1f, 10, 1, Unit::None), "All-pass Q (narrow/wide smear)."), m, {{"fx_disperse_amount", 0.6f}});
        C(doc(b.cont("fx_disperse_spread", "Disperser Spread", 0, 1, 0, Unit::Percent, Role::Amount), "Spread stage frequencies around the centre."), m, {{"fx_disperse_amount", 0.6f}});
    }
    {
        std::string m = "spectral"; mod(m, "Spectral Shaper"); onCtx(m, {{"fx_spectral_thresh", -30}});
        C(doc(b.cont("fx_spectral_thresh", "Spectral Gate Threshold", -100, 0, -100, Unit::Db), "Per-bin gate threshold; -100 = open."), m);
        b.out.back().role = Role::Amount;
        C(doc(b.cont("fx_spectral_tilt", "Spectral Tilt", -6, 6, 0, Unit::Db, Role::Neutral), "Spectral tilt in dB/octave around 1 kHz."), m);
        C(doc(b.cont("fx_spectral_contrast", "Spectral Contrast", 0, 1, 0, Unit::Percent, Role::Amount), "Exaggerate peaks against the spectral envelope."), m);
        C(doc(b.time("fx_spectral_smooth", "Spectral Smooth", 0, 500, 20, 50, Unit::Ms), "Gate smoothing time."), m, {{"fx_spectral_thresh", -35}});
    }
    {
        std::string m = "tamer"; mod(m, "Resonance Tamer"); onCtx(m, {{"fx_tamer_depth", 1}});
        C(doc(b.cont("fx_tamer_depth", "Tamer Depth", 0, 1, 0.5f, Unit::Percent, Role::Amount), "Max suppression; 0 = none."), m, {{"fx_tamer_sens", 1}});
        C(doc(b.cont("fx_tamer_sens", "Tamer Sensitivity", 0, 1, 0.5f, Unit::Percent), "How peaky a band must be to be suppressed."), m, {{"fx_tamer_depth", 1}});
        C(doc(b.time("fx_tamer_speed", "Tamer Speed", 1, 500, 30, 30, Unit::Ms), "Response time."), m, {{"fx_tamer_depth", 1}, {"fx_tamer_sens", 1}});
        C(doc(b.freq("fx_tamer_lo", "Tamer Low Limit", 20, 20000, 200, 1000), "Lowest band processed."), m, {{"fx_tamer_depth", 1}, {"fx_tamer_sens", 1}});
        C(doc(b.freq("fx_tamer_hi", "Tamer High Limit", 20, 20000, 16000, 1000), "Highest band processed."), m, {{"fx_tamer_depth", 1}, {"fx_tamer_sens", 1}, {"fx_tamer_lo", 20}});
    }
    {
        std::string m = "split"; mod(m, "3-Band Split"); onCtx(m, {{"fx_split_b2_dist_on", 1}, {"fx_split_b2_dist_drive", 0.8f}});
        C(doc(b.freq("fx_split_xlo", "Split Low X-over", 40, 1000, 150, 150), "Low/mid crossover."), m, {{"fx_split_b1_gain", 0}});
        C(doc(b.freq("fx_split_xhi", "Split High X-over", 800, 12000, 2500, 2500), "Mid/high crossover."), m, {{"fx_split_b3_gain", 0}});
        const char* bn[] = {"Low", "Mid", "High"};
        for (int i = 1; i <= 3; ++i) {
            std::string p = "fx_split_b" + std::to_string(i) + "_";
            std::string n = std::string("Split ") + bn[i - 1] + " ";
            C(doc(b.cont(p + "gain", n + "Level", 0, 2, 1, Unit::Gain, Role::Level), "Band level; 0 = silent."), m);
            // mini chain modules
            C(doc(b.toggle(p + "flt_on", n + "Filter On", false), "Band filter."), m);
            C(doc(b.choice(p + "flt_type", n + "Filter Type", {"LP", "BP", "HP", "Notch"}, 1), "Band filter type."), m, {{p + "flt_on", 1}});
            C(doc(b.freq(p + "flt_cut", n + "Filter Cutoff", 20, 20000, i == 1 ? 100.f : (i == 2 ? 600.f : 3000.f), 1000), "Band filter cutoff."), m, {{p + "flt_on", 1}});
            C(doc(b.cont(p + "flt_res", n + "Filter Reso", 0, 1, 0.3f, Unit::Percent), "Band filter resonance."), m, {{p + "flt_on", 1}});
            C(doc(b.toggle(p + "ph_on", n + "Phaser On", false), "Band phaser."), m);
            C(doc(b.freq(p + "ph_rate", n + "Phaser Rate", 0.01f, 20, 0.5f, 1), "Band phaser rate."), m, {{p + "ph_on", 1}});
            C(doc(b.cont(p + "ph_depth", n + "Phaser Depth", 0, 1, 0.6f, Unit::Percent), "Band phaser depth."), m, {{p + "ph_on", 1}});
            C(doc(b.cont(p + "ph_fb", n + "Phaser Feedback", -0.95f, 0.95f, 0.4f, Unit::Percent), "Band phaser feedback."), m, {{p + "ph_on", 1}});
            C(doc(b.toggle(p + "dist_on", n + "Dist On", false), "Band distortion."), m, {{p + "dist_drive", 0.7f}});
            C(doc(b.choice(p + "dist_type", n + "Dist Type", choices::distTypes(), 0), "Band distortion type."), m, {{p + "dist_on", 1}, {p + "dist_drive", 0.7f}});
            C(doc(b.cont(p + "dist_drive", n + "Dist Drive", 0, 1, 0.5f, Unit::Percent, Role::Drive), "Band drive."), m, {{p + "dist_on", 1}});
            C(doc(b.toggle(p + "comb_on", n + "Comb On", false), "Band comb."), m);
            C(doc(b.freq(p + "comb_freq", n + "Comb Freq", 20, 5000, 300, 300), "Band comb frequency."), m, {{p + "comb_on", 1}});
            C(doc(b.cont(p + "comb_fb", n + "Comb Feedback", -0.99f, 0.99f, 0.6f, Unit::Percent), "Band comb feedback."), m, {{p + "comb_on", 1}});
            C(doc(b.toggle(p + "crush_on", n + "Crush On", false), "Band bitcrusher."), m);
            C(doc(b.cont(p + "crush_bits", n + "Crush Bits", 1, 24, 6, Unit::Bits), "Band bit depth."), m, {{p + "crush_on", 1}});
        }
    }
    {
        b.plugins = kFx;
        std::string m = "vocoder"; mod(m, "Vocoder"); onCtx(m, {});
        C(doc(b.integer("fx_vocoder_bands", "Vocoder Bands", 8, 32, 16), "Number of bands."), m);
        C(doc(b.time("fx_vocoder_attack", "Vocoder Attack", 0.5f, 100, 5, 10, Unit::Ms), "Band follower attack."), m);
        C(doc(b.time("fx_vocoder_release", "Vocoder Release", 5, 1000, 60, 80, Unit::Ms), "Band follower release."), m);
        C(doc(b.cont("fx_vocoder_shift", "Vocoder Formant Shift", -12, 12, 0, Unit::Semitones, Role::Neutral), "Shift carrier bands against modulator bands."), m);
        C(doc(b.cont("fx_vocoder_hf", "Vocoder HF Pass", 0, 1, 0, Unit::Percent, Role::Amount), "Pass modulator highs (sibilance) through."), m);
        b.plugins = kBoth;
    }
    {
        std::string m = "width"; mod(m, "Width"); onCtx(m, {{"fx_width_amount", 1.8f}});
        C(doc(b.cont("fx_width_amount", "Width", 0, 2, 1, Unit::None, Role::Neutral), "Stereo width; 1 = unchanged, 0 = mono."), m);
        b.out.back().neutral = 1;
        C(doc(b.toggle("fx_width_mono_on", "Mono Bass", false), "Make everything below Mono Freq mono."), m, {{"fx_width_mono_hz", 300}});
        C(doc(b.freq("fx_width_mono_hz", "Mono Below", 20, 1000, 120, 150), "Mono crossover frequency."), m, {{"fx_width_mono_on", 1}});
    }
    {
        b.section = "FX Clipper";
        doc(b.toggle("fx_clip_on", "Final Clipper On", true), "Final safety clipper/limiter.");
        ctx(v.back(), {{"fx_clip_drive", 12}});
        doc(ctx(b.choice("fx_clip_mode", "Final Stage Mode", {"Soft Clip", "Hard Clip", "Limiter"}, 0), {{"fx_clip_drive", 12}}), "Clipper type.");
        doc(ctx(b.cont("fx_clip_ceiling", "Ceiling", -12, 0, -0.3f, Unit::Db), {{"fx_clip_drive", 12}}), "Output ceiling.");
        doc(b.cont("fx_clip_drive", "Clip Drive", 0, 24, 0, Unit::Db, Role::Neutral), "Gain into the final stage.");
    }
}

} // namespace

Registry::Registry() {
    params.reserve(1200);
    buildGlobal(params);
    for (int k = 0; k < 3; ++k) buildOsc(params, k);
    buildChain(params);
    buildSubNoise(params);
    for (int k = 0; k < 2; ++k) buildFilter(params, k);
    buildEnvelopes(params);
    buildLfos(params);
    buildSeqRandMacros(params);
    buildRoutes(params);
    buildFx(params);
    for (int i = 0; i < (int) params.size(); ++i) {
        auto& p = params[(size_t) i];
        if (index.count(p.id)) throw std::logic_error("duplicate parameter id " + p.id);
        index[p.id] = i;
        if (p.role == Role::Mix && p.neutral == 0) p.neutral = 0;
    }
}

const Registry& Registry::get() {
    static const Registry r;
    return r;
}

int Registry::indexOf(const std::string& id) const {
    auto it = index.find(id);
    return it == index.end() ? -1 : it->second;
}

std::vector<int> Registry::forPlugin(uint8_t mask) const {
    std::vector<int> v;
    for (int i = 0; i < size(); ++i) if (params[(size_t) i].plugins & mask) v.push_back(i);
    return v;
}

} // namespace gnarl
