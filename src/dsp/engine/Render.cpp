#include "Render.h"
#include "params/Registry.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>

namespace gnarl {

Patch::Patch(bool fxPlugin) : structure(std::make_shared<PatchStructure>()) {
    auto& R = Registry::get();
    values.resize((size_t) R.size());
    for (int i = 0; i < R.size(); ++i) values[(size_t) i] = R[i].defaultFor(fxPlugin);
}

void Patch::set(const std::string& id, float v) {
    int i = Registry::get().indexOf(id);
    if (i < 0) { std::fprintf(stderr, "Patch::set unknown id %s\n", id.c_str()); return; }
    auto& p = Registry::get()[i];
    values[(size_t) i] = std::clamp(v, p.min, p.max);
}

float Patch::get(const std::string& id) const {
    int i = Registry::get().indexOf(id);
    return i < 0 ? 0.f : values[(size_t) i];
}

int Patch::addRoute(int source, const std::string& destId, float depth, int curve, int via, int polarity) {
    const bool bipolar = polarity < 0 ? modSourceIsBipolar(source) : polarity > 0;
    int dest = Registry::get().indexOf(destId);
    for (int k = 0; k < kNumRoutes; ++k) {
        auto& r = structure->routes[(size_t) k];
        if (r.source == MS_None) {
            r.source = source; r.dest = dest; r.curve = (uint8_t) curve; r.via = via; r.bipolar = bipolar; r.bypass = false;
            values[(size_t) Registry::get().indexOf("mod" + std::to_string(k + 1) + "_depth")] = depth;
            return k;
        }
    }
    return -1;
}

void applyPatch(Engine& e, const Patch& p) {
    for (size_t i = 0; i < p.values.size(); ++i) e.setParam((int) i, p.values[i]);
    auto s = std::make_shared<PatchStructure>(*p.structure);
    s->finalize();
    e.setStructure(s);
}

void generateInput(TestInput type, std::vector<float>& L, std::vector<float>& R, double sr, unsigned seed, float gain) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> uni(-1.f, 1.f);
    const size_t n = L.size();
    double ph = 0, ph2 = 0, ph3 = 0;
    for (size_t i = 0; i < n; ++i) {
        double t = (double) i / sr;
        float l = 0, r = 0;
        switch (type) {
            case TestInput::None: break;
            case TestInput::Saw110: {
                // band-limited saw by additive synthesis (exact, no aliasing)
                float s = 0;
                for (int h = 1; h * 110.0 < sr * 0.45; ++h) s += (float) (std::sin(2 * M_PI * 110.0 * h * t) / h);
                l = r = s * 0.55f;
                break;
            }
            case TestInput::Sine220: l = r = (float) std::sin(2 * M_PI * 220 * t); break;
            case TestInput::Noise: l = uni(rng); r = uni(rng); break;
            case TestInput::Chord: {
                l = (float) (std::sin(2 * M_PI * 110 * t) + 0.7 * std::sin(2 * M_PI * 164.8 * t) + 0.5 * std::sin(2 * M_PI * 220 * t) + 0.3 * std::sin(2 * M_PI * 1320 * t)) * 0.4f;
                r = (float) (std::sin(2 * M_PI * 110 * t + 0.3) + 0.7 * std::sin(2 * M_PI * 164.8 * t + 1.1) + 0.5 * std::sin(2 * M_PI * 220 * t) + 0.3 * std::sin(2 * M_PI * 1650 * t)) * 0.4f;
                break;
            }
            case TestInput::Drums: {
                double beat = std::fmod(t, 0.25);
                float kick = (float) (std::sin(2 * M_PI * (50 + 200 * std::exp(-beat * 40)) * beat) * std::exp(-beat * 12));
                float hat = uni(rng) * (float) std::exp(-std::fmod(t + 0.125, 0.25) * 60) * 0.4f;
                l = r = kick + hat;
                break;
            }
            case TestInput::Sweep: {
                double f = 20 * std::pow(1000.0, std::fmod(t, 2.0) / 2.0);
                ph += 2 * M_PI * f / sr;
                l = r = (float) std::sin(ph);
                break;
            }
        }
        (void) ph2; (void) ph3;
        L[i] = l * gain; R[i] = r * gain;
    }
}

RenderResult render(const Patch& patch, const RenderSpec& spec) {
    RenderResult res;
    res.sampleRate = spec.sampleRate;
    const size_t total = (size_t) std::llround(spec.seconds * spec.sampleRate);
    res.L.assign(total, 0); res.R.assign(total, 0);
    std::vector<float> inL(total), inR(total), scL(total), scR(total);
    generateInput(spec.fx ? spec.input : TestInput::None, inL, inR, spec.sampleRate, spec.seed, spec.inputGain);
    generateInput(spec.sidechain, scL, scR, spec.sampleRate, spec.seed + 7, 0.5f);

    Engine e(spec.fx);
    applyPatch(e, patch);
    const int maxBlock = std::max(spec.blockSize, 2048);
    e.prepare(spec.sampleRate, maxBlock);
    e.reset();
    res.latency = e.latencySamples();

    // Build the event list.
    struct Ev { size_t s; MidiEvent m; };
    std::vector<Ev> evs;
    for (auto& n : spec.notes) {
        evs.push_back({(size_t) std::llround(n.on * spec.sampleRate), {0, 0x90, (uint8_t) n.note, (uint8_t) n.velocity}});
        if (n.off > n.on) evs.push_back({(size_t) std::llround(n.off * spec.sampleRate), {0, 0x80, (uint8_t) n.note, (uint8_t) n.releaseVelocity}});
    }
    for (auto& c : spec.ccs) {
        size_t s = (size_t) std::llround(c.time * spec.sampleRate);
        if (c.cc == -1) evs.push_back({s, {0, 0xE0, (uint8_t) (c.value & 0x7f), (uint8_t) ((c.value >> 7) & 0x7f)}});
        else if (c.cc == -2) evs.push_back({s, {0, 0xD0, (uint8_t) c.value, 0}});
        else evs.push_back({s, {0, 0xB0, (uint8_t) c.cc, (uint8_t) c.value}});
    }
    std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.s < b.s; });

    std::mt19937 rng(spec.seed * 31 + 5);
    std::vector<MidiEvent> blockEvents;
    blockEvents.reserve(evs.size() + 1);
    size_t pos = 0, evi = 0;
    auto t0 = std::chrono::steady_clock::now();
    while (pos < total) {
        int bs = spec.blockSize;
        if (spec.randomBlockSizes) bs = std::uniform_int_distribution<int>(1, maxBlock)(rng);
        int n = (int) std::min<size_t>((size_t) bs, total - pos);
        blockEvents.clear();
        while (evi < evs.size() && evs[evi].s < pos + (size_t) n) {
            auto m = evs[evi].m;
            m.offset = (int) (evs[evi].s >= pos ? evs[evi].s - pos : 0);
            blockEvents.push_back(m);
            ++evi;
        }
        if (spec.automation) spec.automation(e, (double) pos / spec.sampleRate);
        HostInfo hi;
        hi.bpm = spec.bpm;
        hi.ppq = (double) pos / spec.sampleRate * spec.bpm / 60.0;
        hi.playing = true;
        hi.offline = spec.offline;
        e.process(spec.fx ? inL.data() + pos : nullptr, spec.fx ? inR.data() + pos : nullptr, res.L.data() + pos, res.R.data() + pos, n,
                  blockEvents.data(), (int) blockEvents.size(), hi,
                  spec.sidechain != TestInput::None ? scL.data() + pos : nullptr, spec.sidechain != TestInput::None ? scR.data() + pos : nullptr);
        pos += (size_t) n;
    }
    res.cpuSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    for (size_t i = 0; i < total; ++i)
        if (!std::isfinite(res.L[i]) || !std::isfinite(res.R[i])) { res.finite = false; break; }
    res.latency = e.latencySamples();
    return res;
}

bool writeWav(const std::string& path, const RenderResult& r) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t n = (uint32_t) r.L.size(), sr = (uint32_t) r.sampleRate;
    const uint32_t dataBytes = n * 2 * 4;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(3); w16(2); w32(sr); w32(sr * 8); w16(8); w16(32);
    std::fwrite("data", 1, 4, f); w32(dataBytes);
    for (uint32_t i = 0; i < n; ++i) { std::fwrite(&r.L[i], 4, 1, f); std::fwrite(&r.R[i], 4, 1, f); }
    std::fclose(f);
    return true;
}

} // namespace gnarl
