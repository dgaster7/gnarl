#include "PatchStructure.h"
#include "params/Registry.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace gnarl {

namespace {
struct SrcInfo { const char* id; const char* name; bool bipolar; bool perVoice; };
const SrcInfo kSources[MS_Count] = {
    {"env1", "Env 1", false, true}, {"env2", "Env 2", false, true}, {"env3", "Env 3", false, true}, {"env4", "Env 4", false, true},
    {"lfo1", "LFO 1", true, false}, {"lfo2", "LFO 2", true, false}, {"lfo3", "LFO 3", true, false}, {"lfo4", "LFO 4", true, false},
    {"lfo5", "LFO 5 (voice)", true, true}, {"lfo6", "LFO 6 (voice)", true, true},
    {"seq1", "Step Seq 1", true, false}, {"seq2", "Step Seq 2", true, false},
    {"rnd1", "Random 1", true, false}, {"rnd2", "Random 2", true, false}, {"rnd3", "Random 3", true, false}, {"rnd4", "Random 4", true, false},
    {"macro1", "Macro 1", false, false}, {"macro2", "Macro 2", false, false}, {"macro3", "Macro 3", false, false}, {"macro4", "Macro 4", false, false},
    {"macro5", "Macro 5", false, false}, {"macro6", "Macro 6", false, false}, {"macro7", "Macro 7", false, false}, {"macro8", "Macro 8", false, false},
    {"osca", "Osc A Out", true, true}, {"oscb", "Osc B Out", true, true}, {"oscc", "Osc C Out", true, true},
    {"velocity", "Velocity", false, true}, {"relvel", "Release Velocity", false, true}, {"key", "Key Track", true, true},
    {"voices", "Voice Count", false, false}, {"modwheel", "Mod Wheel", false, false}, {"aftertouch", "Aftertouch", false, true},
    {"pitchbend", "Pitch Bend", true, false}, {"mpepressure", "MPE Pressure", false, true}, {"mpeslide", "MPE Slide", false, true},
    {"follower", "Env Follower", false, false}, {"transient", "Transient", false, false},
    {"fn1", "Function 1", true, true}, {"fn2", "Function 2", true, true}, {"fn3", "Function 3", true, true}, {"fn4", "Function 4", true, true},
};
const char* kCurveNames[RC_Count] = {"Linear", "Slow", "Fast", "Linear Rev", "Slow Rev", "Fast Rev", "±1 semi", "±1 tone", "±1 oct", "±4 oct"};

std::vector<ShapePoint> sinePoints() {
    std::vector<ShapePoint> v;
    for (int i = 0; i <= 32; ++i) { float x = (float) i / 32.f; v.push_back({x, std::sin(6.28318530718f * x), 0}); }
    return v;
}
} // namespace

const char* modSourceId(int s) { return (s >= 0 && s < MS_Count) ? kSources[s].id : "none"; }
const char* modSourceName(int s) { return (s >= 0 && s < MS_Count) ? kSources[s].name : "None"; }
int modSourceFromId(const std::string& id) {
    for (int i = 0; i < MS_Count; ++i) if (id == kSources[i].id) return i;
    return MS_None;
}
bool modSourceIsBipolar(int s) { return s >= 0 && s < MS_Count && kSources[s].bipolar; }
bool modSourceIsPerVoice(int s) { return s >= 0 && s < MS_Count && kSources[s].perVoice; }
const char* routeCurveName(int c) { return (c >= 0 && c < RC_Count) ? kCurveNames[c] : "?"; }

float evalShape(const std::vector<ShapePoint>& pts, float x) {
    if (pts.empty()) return 0;
    if (x <= pts.front().x) return pts.front().y;
    if (x >= pts.back().x) return pts.back().y;
    size_t i = 1;
    while (i < pts.size() && pts[i].x < x) ++i;
    const auto& p0 = pts[i - 1];
    const auto& p1 = pts[i];
    float w = p1.x - p0.x;
    float t = w > 1e-9f ? (x - p0.x) / w : 1.f;
    float c = std::clamp(p1.curve, -1.f, 1.f);
    if (c != 0) t = std::pow(t, std::exp2(c * 3.f));
    return p0.y + (p1.y - p0.y) * t;
}

PatchStructure::PatchStructure() {
    fxSlots.fill(-1);
    for (auto& o : splitOrder) for (int i = 0; i < kSplitMiniModules; ++i) o[(size_t) i] = i;
    const char* names[8] = {"Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6", "Macro 7", "Macro 8"};
    for (int i = 0; i < 8; ++i) macroNames[(size_t) i] = names[i];
    for (auto& l : lfo) { l.a = sinePoints(); l.b = {{0, 1, 0}, {1, -1, 0}}; l.grid = 8; }
    for (auto& s : seq) for (int i = 0; i < 32; ++i) { s.value[(size_t) i] = (i % 4 == 0) ? 1.f : ((i % 2) ? -0.5f : 0.25f); s.slide[(size_t) i] = (i % 4 == 2) ? 1 : 0; }
    for (int k = 0; k < 4; ++k) { fn[(size_t) k].a = MS_Lfo1 + k; fn[(size_t) k].b = MS_Rnd1 + k; }
    for (auto& e : env) { e.pts = {{0, 0, 0}, {0.05f, 1, -0.5f}, {0.4f, 0.3f, -0.5f}, {0.7f, 0.6f, 0.3f}, {1, 0, -0.5f}}; e.sustain = -1; }
    draw[0].pts = {{-1, -1, 0}, {-0.5f, -0.85f, -0.4f}, {0, 0, 0}, {0.5f, 0.85f, 0.4f}, {1, 1, 0}};
    draw[1].pts = {{-1, 0.2f, 0}, {-0.6f, -1, 0}, {-0.2f, 0.6f, 0}, {0, 0, 0}, {0.2f, -0.6f, 0}, {0.6f, 1, 0}, {1, -0.2f, 0}};
    finalize();
}

void PatchStructure::finalize() {
    {
        auto& R = Registry::get();
        globalRoutes.clear(); voiceRoutes.clear(); globalDests.clear(); voiceDests.clear();
        voiceRouted.assign((size_t) R.size(), 0);
        std::vector<uint8_t> gseen((size_t) R.size(), 0);
        for (int k = 0; k < kNumRoutes; ++k) {
            const auto& r = routes[(size_t) k];
            if (!r.active() || r.dest >= R.size() || !R[r.dest].modulatable || R[r.dest].isDiscrete()) continue;
            CompiledRoute c{k, r.source, r.via, r.curve, r.bipolar, r.dest};
            if (R[r.dest].scope == Scope::Voice) {
                voiceRoutes.push_back(c);
                if (!voiceRouted[(size_t) r.dest]) { voiceRouted[(size_t) r.dest] = 1; voiceDests.push_back(r.dest); }
            } else {
                globalRoutes.push_back(c);
                if (!gseen[(size_t) r.dest]) { gseen[(size_t) r.dest] = 1; globalDests.push_back(r.dest); }
            }
        }
    }
    for (size_t k = 0; k < 6; ++k)
        for (int i = 0; i <= kTableSize; ++i) {
            float x = (float) i / kTableSize;
            lfoTableA[k][(size_t) i] = std::clamp(evalShape(lfo[k].a, x), -1.f, 1.f);
            lfoTableB[k][(size_t) i] = std::clamp(evalShape(lfo[k].b, x), -1.f, 1.f);
        }
    for (size_t k = 0; k < 4; ++k) {
        for (int i = 0; i <= kTableSize; ++i)
            envTable[k][(size_t) i] = std::clamp(evalShape(env[k].pts, (float) i / kTableSize), 0.f, 1.f);
        int s = env[k].sustain;
        envSustainSample[k] = (s >= 0 && s < (int) env[k].pts.size()) ? (int) std::lround(env[k].pts[(size_t) s].x * kTableSize) : -1;
    }
    for (size_t k = 0; k < 2; ++k) {
        // Map x in -1..1 to the table; optional smoothing by a 3-tap average proportional to `smooth`.
        auto& t = drawTable[k];
        for (int i = 0; i <= kTableSize; ++i) t[(size_t) i] = std::clamp(evalShape(draw[k].pts, -1.f + 2.f * i / kTableSize), -1.f, 1.f);
        int passes = (int) std::lround(draw[k].smooth * 16);
        for (int p = 0; p < passes; ++p) {
            float prev = t[0];
            for (int i = 1; i < kTableSize; ++i) { float cur = t[(size_t) i]; t[(size_t) i] = 0.25f * prev + 0.5f * cur + 0.25f * t[(size_t) i + 1]; prev = cur; }
        }
    }
}

int PatchStructure::slotOf(int module) const {
    for (int i = 0; i < kNumFxSlots; ++i) if (fxSlots[(size_t) i] == module) return i;
    return -1;
}

void PatchStructure::placeModule(int module) {
    if (slotOf(module) >= 0) return;
    for (auto& s : fxSlots) if (s < 0) { s = module; return; }
}

} // namespace gnarl
