#include "PresetGates.h"
#include "analysis/Analysis.h"
#include "params/Registry.h"
#include <cmath>

namespace gnarl {
namespace A = analysis;

RenderResult renderNoteForGates(const Patch& patch, bool fx, int note, double seconds, double noteOff, bool clipperOff) {
    Patch p = patch;
    if (clipperOff) p.set("fx_clip_on", 0);
    RenderSpec s;
    s.fx = fx; s.seconds = seconds; s.bpm = 140; s.blockSize = 256;
    s.input = fx ? TestInput::Saw110 : TestInput::None; s.inputGain = 0.35f;
    s.notes = {{0.0, noteOff, note, 100, 64}};
    return render(p, s);
}

static RenderResult renderWith(const Patch& patch, bool fx, int note, double seconds, const std::vector<CcEvent>& ccs, bool clipperOff) {
    Patch p = patch;
    if (clipperOff) p.set("fx_clip_on", 0);
    RenderSpec s;
    s.fx = fx; s.seconds = seconds; s.bpm = 140; s.blockSize = 256;
    s.input = fx ? TestInput::Saw110 : TestInput::None; s.inputGain = 0.35f;
    s.notes = {{0.0, seconds * 0.8, note, 100, 64}};
    s.ccs = ccs;
    return render(p, s);
}

GateResult evaluatePreset(const Patch& patch, const GateSpec& spec) {
    GateResult g;
    auto fail = [&](const std::string& m) { g.pass = false; g.failures.push_back(m); };
    const double sr = 48000;

    // --- C1 with a long tail: silence, tail length, peak, DC ---
    auto c1 = renderNoteForGates(patch, spec.fx, 24, 12.5, 1.5, true);
    auto c2 = renderNoteForGates(patch, spec.fx, 36, 2.5, 2.0, true);
    if (!c1.finite || !c2.finite) fail("non-finite output");
    const double rmsC2 = std::max(A::rms(c2.L, 2400, 96000), A::rms(c2.R, 2400, 96000));
    if (A::toDb(rmsC2) < -60) fail("silent at C2");
    if (A::toDb(std::max(A::rms(c1.L, 2400, 72000), A::rms(c1.R, 2400, 72000))) < -60) fail("silent at C1");
    // tail: signal more than 10 s after note-off (note-off at 1.5 s)
    size_t last = A::lastAbove(c1.L, c1.R, -80);
    g.tailSeconds = std::max(0.0, (double) last / sr - 1.5);
    if (!spec.fx && g.tailSeconds > 10.0) fail("tail longer than 10 s after note-off");
    // phrase: 2-bar legato glide line at 140 BPM
    Patch phraseP = patch; phraseP.set("fx_clip_on", 0);
    RenderSpec ph; ph.fx = spec.fx; ph.seconds = 4.0; ph.bpm = 140; ph.input = spec.fx ? TestInput::Saw110 : TestInput::None; ph.inputGain = 0.35f;
    const double beat = 60.0 / 140.0;
    const int line[8] = {36, 36, 39, 43, 41, 36, 34, 31};
    for (int i = 0; i < 8; ++i) ph.notes.push_back({i * beat, (i + 1) * beat + 0.03, line[i], 90 + (i % 3) * 10, 64});
    auto phr = render(phraseP, ph);
    g.peakPreClipDb = A::toDb(std::max({A::peak(c1.L), A::peak(c1.R), A::peak(c2.L), A::peak(c2.R), A::peak(phr.L), A::peak(phr.R)}));
    if (g.peakPreClipDb > -0.3) fail("peak " + std::to_string(g.peakPreClipDb).substr(0, 5) + " dBFS before the safety clipper");
    g.dcDb = A::toDb(std::max(std::fabs(A::dc(c2.L, 2400, 96000)), std::fabs(A::dc(c2.R, 2400, 96000))));
    if (g.dcDb > -60) fail("DC offset");
    // sub / mono below 120 Hz
    g.lowCorr = A::lowCorrelation(c2.L, c2.R, sr, 120);
    if (A::toDb(rmsC2) > -60 && g.lowCorr < 0.9) fail("content below 120 Hz is not mono-compatible");
    const double f0 = 65.406;
    g.fundamentalDb = A::bandEnergyDb(c2.L, sr, f0 * 0.8, f0 * 1.25, 4800, 96000) - A::bandEnergyDb(c2.L, sr, 20, 20000, 4800, 96000);
    if (spec.needsSub && g.fundamentalDb < -18) fail("no fundamental energy (category needs a sub)");
    // loudness (note-on part of C2, real output including the final stage)
    auto c2out = renderNoteForGates(patch, spec.fx, 36, 2.0, 2.0, false);
    std::vector<float> l(c2out.L.begin() + 2400, c2out.L.end()), r(c2out.R.begin() + 2400, c2out.R.end());
    g.lufs = A::loudnessLufs(l, r, sr);
    if (std::fabs(g.lufs - spec.targetLufs) > spec.lufsTolerance) fail("loudness " + std::to_string(g.lufs).substr(0, 6) + " LUFS (target " + std::to_string((int) spec.targetLufs) + ")");
    g.cpuPercent = c2out.cpuSeconds / 2.0 * 100.0;
    // mod wheel 0 vs 127
    auto w0 = renderWith(patch, spec.fx, 36, 1.5, {{0.0, 1, 0}}, true);
    auto w1 = renderWith(patch, spec.fx, 36, 1.5, {{0.0, 1, 127}}, true);
    g.modWheelDiffDb = A::toDb(std::max(A::diffRms(w0.L, w1.L), A::diffRms(w0.R, w1.R)));
    if (g.modWheelDiffDb < -60) fail("mod wheel changes nothing measurable");
    g.worstVariantPeakDb = std::max(A::toDb(A::peak(w1.L)), A::toDb(A::peak(w1.R)));
    // each macro at 0 and 1
    g.minMacroDiffDb = 1e9;
    for (int k = 1; k <= 8; ++k) {
        Patch a = patch, b = patch;
        a.set("macro" + std::to_string(k), 0); b.set("macro" + std::to_string(k), 1);
        auto ra = renderWith(a, spec.fx, 36, 1.5, {}, true), rb = renderWith(b, spec.fx, 36, 1.5, {}, true);
        double d = A::toDb(std::max(A::diffRms(ra.L, rb.L), A::diffRms(ra.R, rb.R)));
        g.macroDiffDb.push_back(d);
        g.minMacroDiffDb = std::min(g.minMacroDiffDb, d);
        g.worstVariantPeakDb = std::max({g.worstVariantPeakDb, A::toDb(A::peak(rb.L)), A::toDb(A::peak(rb.R))});
        if (d < -60) fail("macro " + std::to_string(k) + " changes nothing measurable");
    }
    if (g.worstVariantPeakDb > 3.0) fail("a macro/mod-wheel extreme drives the safety clipper hard (" + std::to_string(g.worstVariantPeakDb).substr(0, 5) + " dBFS)");
    // fingerprint and movement (for the duplicate check and the audition list)
    g.fingerprint = A::fingerprint(phr.L, sr);
    double mv = 0; int frames = (int) g.fingerprint.size() / 24;
    for (int f = 1; f < frames; ++f) for (int b = 0; b < 24; ++b) mv += std::fabs(g.fingerprint[(size_t) (f * 24 + b)] - g.fingerprint[(size_t) ((f - 1) * 24 + b)]);
    g.movement = frames > 1 ? mv / (frames - 1) / 24.0 : 0;
    return g;
}

} // namespace gnarl
