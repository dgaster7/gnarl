// Generates docs/PARAMETERS.md from the registry.
#include "params/Registry.h"
#include <cstdio>
#include <fstream>
#include <sstream>
using namespace gnarl;
static const char* unitName(Unit u) {
    switch (u) { case Unit::Hz: return "Hz"; case Unit::Semitones: return "st"; case Unit::Cents: return "ct"; case Unit::Percent: return "%";
    case Unit::Gain: return "gain (dB shown)"; case Unit::Db: return "dB"; case Unit::Ms: return "ms"; case Unit::Seconds: return "s";
    case Unit::Beats: return "beats"; case Unit::Degrees: return "deg"; case Unit::Ratio: return "ratio"; case Unit::Bits: return "bits";
    case Unit::Octaves: return "oct"; default: return ""; }
}
static const char* roleName(Role r) {
    switch (r) { case Role::Mix: return "Mix"; case Role::Amount: return "Amount"; case Role::Drive: return "Drive"; case Role::Depth: return "Depth";
    case Role::Send: return "Send"; case Role::Level: return "Level"; case Role::Neutral: return "Neutral"; case Role::Choice: return "Choice";
    case Role::Toggle: return "Toggle"; case Role::Integer: return "Integer"; default: return "Continuous"; }
}
int main(int argc, char** argv) {
    auto& R = Registry::get();
    std::ostringstream o;
    o << "# GNARL parameters\n\nGenerated from `src/dsp/params/Registry.cpp` by `gnarl_paramdoc` — do not edit by hand.\n\n";
    o << "Total: " << R.size() << " parameters (synth: " << R.forPlugin(kSynth).size() << ", FX: " << R.forPlugin(kFx).size() << ").\n\n";
    o << "Skew is JUCE `NormalisableRange` skew (normalised = proportion^skew). Smoothing is a one-pole time constant applied to the base value; modulation is interpolated per 16-sample sub-block. Roles: **Mix** 0 = dry exactly / 1 = wet only; **Amount/Drive/Send/Depth** at zero = no effect; **Level** 0 = silence; **Neutral** at its neutral value = unchanged.\n\n";
    std::string section;
    for (auto& p : R.all()) {
        if (p.section != section) {
            section = p.section;
            o << "\n## " << section << "\n\n| ID | Name | Range | Default | Unit | Skew | Smooth | Role | Plugins | Must audibly do |\n|---|---|---|---|---|---|---|---|---|---|\n";
        }
        char range[128];
        if (!p.choices.empty() && p.role == Role::Choice) {
            std::string c; for (size_t i = 0; i < p.choices.size(); ++i) c += (i ? " / " : "") + p.choices[i];
            snprintf(range, sizeof range, "%s", c.size() < 120 ? c.c_str() : "see choices");
        } else snprintf(range, sizeof range, "%g … %g", p.min, p.max);
        std::string def = (p.role == Role::Choice && (size_t) p.def < p.choices.size()) ? p.choices[(size_t) p.def] : std::to_string(p.def);
        if (def.find('.') != std::string::npos && p.role != Role::Choice) { while (def.back() == '0') def.pop_back(); if (def.back() == '.') def.pop_back(); }
        char skew[32]; snprintf(skew, sizeof skew, "%.3g", p.skew);
        o << "| `" << p.id << "` | " << p.name << " | " << range << " | " << def << " | " << unitName(p.unit) << " | " << skew << " | "
          << (p.smoothMs > 0 ? std::to_string((int) p.smoothMs) + " ms" : "—") << " | " << roleName(p.role) << " | "
          << (p.plugins == kBoth ? "both" : (p.plugins == kSynth ? "synth" : "fx")) << " | " << p.doc << " |\n";
    }
    if (argc > 1) { std::ofstream(argv[1]) << o.str(); std::printf("wrote %s (%d params)\n", argv[1], R.size()); }
    else std::fputs(o.str().c_str(), stdout);
}
