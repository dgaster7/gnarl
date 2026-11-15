#pragma once
#include "engine/PatchStructure.h"
#include "util/Math.h"

namespace gnarl {

// Curve shaping: c in -1..1, 0 = linear, negative = fast start, positive = slow start.
inline float shapeCurve(float x, float c) {
    x = clampf(x, 0, 1);
    float k = c * 7.f;
    if (std::fabs(k) < 1e-3f) return x;
    return (std::exp(k * x) - 1.f) / (std::exp(k) - 1.f);
}

struct EnvParams {
    float a = 0.002f, d = 0.3f, s = 1, r = 0.1f;   // seconds
    float ac = 0, dc = -0.5f, rc = -0.5f;
    bool drawn = false; float len = 1;                // drawn: length in seconds
    const float* table = nullptr; int sustainIdx = -1;
    float minAttack = 0, minRelease = 0;
};

class Envelope {
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    void noteOn(bool legato) {
        startLevel = legato ? level : 0.f;
        if (!legato) level = 0;
        stage = Attack; t = 0; drawPos = 0; released = false;
    }
    void noteOff() { if (stage != Idle) { relStart = level; stage = Release; t = 0; released = true; } }
    void kill() { stage = Idle; level = 0; }
    bool active() const { return stage != Idle; }
    Stage getStage() const { return stage; }
    float value() const { return level; }
    // Advance by dt seconds (one sub-block); returns the level at the end.
    float advance(const EnvParams& p, float dt) {
        if (p.drawn && p.table) return advanceDrawn(p, dt);
        const float A = std::max(p.a, p.minAttack), D = p.d, R = std::max(p.r, p.minRelease);
        float remaining = dt;
        while (remaining > 0 && stage != Idle) {
            switch (stage) {
                case Attack: {
                    if (A <= 0) { level = 1; stage = Decay; t = 0; break; }
                    float step = std::min(remaining, A - t);
                    t += step; remaining -= step;
                    level = startLevel + (1.f - startLevel) * shapeCurve(t / A, p.ac);
                    if (t >= A) { level = 1; stage = Decay; t = 0; }
                    break;
                }
                case Decay: {
                    if (D <= 0) { level = p.s; stage = Sustain; break; }
                    float step = std::min(remaining, D - t);
                    t += step; remaining -= step;
                    level = 1.f - (1.f - p.s) * shapeCurve(t / D, p.dc);
                    if (t >= D) { level = p.s; stage = Sustain; }
                    break;
                }
                case Sustain: level = p.s; remaining = 0; if (p.s <= 0) { level = 0; } break;
                case Release: {
                    if (R <= 0) { level = 0; stage = Idle; break; }
                    float step = std::min(remaining, R - t);
                    t += step; remaining -= step;
                    level = relStart * (1.f - shapeCurve(t / R, p.rc));
                    if (t >= R) { level = 0; stage = Idle; }
                    break;
                }
                default: remaining = 0;
            }
        }
        return level;
    }
private:
    float advanceDrawn(const EnvParams& p, float dt) {
        const int N = PatchStructure::kTableSize;
        float len = std::max(p.len, 1e-3f);
        if (stage == Release && !released) released = true;
        float pos = drawPos + dt / len * N;
        if (!released && p.sustainIdx >= 0 && pos >= (float) p.sustainIdx) pos = (float) p.sustainIdx;
        if (pos >= (float) N) { pos = (float) N; level = p.table[N]; stage = Idle; drawPos = pos; return level; }
        drawPos = pos;
        int i = (int) pos; float f = pos - (float) i;
        float target = p.table[i] + (p.table[std::min(i + 1, N)] - p.table[i]) * f;
        // de-click note-on from a previous level
        level = target;
        if (stage == Attack) stage = Decay;
        return level;
    }
    Stage stage = Idle;
    float level = 0, startLevel = 0, relStart = 0, t = 0, drawPos = 0;
    bool released = false;
};

struct LfoParams {
    float rateHz = 1; bool sync = false; double beats = 1;
    float phase = 0;     // 0..1 start phase
    float offset = 0, delay = 0, fade = 0, smooth = 0, morph = 0, morphTime = 0;
    int mode = 0;        // 0 retrigger, 1 free, 2 one-shot
    const float* tabA = nullptr; const float* tabB = nullptr;
};

class Lfo {
public:
    void trigger() { if (mode != 1) { ph = 0; done = false; } sinceTrig = 0; triggered = true; }
    float out() const { return value; }
    float advance(const LfoParams& p, float dt, double ppq, double bpm, bool hostPlaying) {
        mode = p.mode;
        double hz = p.sync ? (bpm / 60.0) / std::max(p.beats, 1e-6) : p.rateHz;
        if (p.sync && p.mode == 1 && hostPlaying) {
            // transport-locked: bounce matches playback
            ph = std::fmod(ppq / std::max(p.beats, 1e-6), 1.0);
        } else if (!done) {
            ph += hz * dt;
            if (p.mode == 2 && ph >= 1.0) { ph = 1.0; done = true; }
            else ph -= std::floor(ph);
        }
        sinceTrig += dt;
        double phase = ph + p.phase;
        phase -= std::floor(phase);
        if (done) phase = std::fmod(1.0 + p.phase - 1e-9, 1.0);
        const int N = PatchStructure::kTableSize;
        float pos = (float) phase * N; int i = (int) pos; float f = pos - (float) i; if (i >= N) { i = N - 1; f = 1; }
        float m = p.morph;
        if (p.morphTime > 0 && triggered) m = m + (1.f - m) * clampf(sinceTrig / p.morphTime, 0, 1);
        float a = p.tabA ? p.tabA[i] + (p.tabA[i + 1] - p.tabA[i]) * f : std::sin(kTwoPi * (float) phase);
        float b = p.tabB ? p.tabB[i] + (p.tabB[i + 1] - p.tabB[i]) * f : a;
        float v = a + (b - a) * m;
        // delay + fade-in after trigger
        float env = 1;
        if (triggered && (p.delay > 0 || p.fade > 0)) {
            if (sinceTrig < p.delay) env = 0;
            else env = p.fade > 0 ? clampf((sinceTrig - p.delay) / p.fade, 0, 1) : 1.f;
        }
        v *= env;
        if (p.smooth > 0) {
            double period = 1.0 / std::max(hz, 1e-3);
            float tau = (float) (period * p.smooth * 0.25);
            float c = std::exp(-dt / std::max(tau, 1e-5f));
            sm = v + (sm - v) * c;
            v = sm;
        } else sm = v;
        value = clampf(v + p.offset, -1.f, 1.f);
        return value;
    }
    void resetPhase(float p) { ph = p; done = false; sm = 0; }
private:
    double ph = 0; float value = 0, sm = 0, sinceTrig = 0; bool done = false, triggered = false; int mode = 0;
};

class StepSeq {
public:
    void trigger() { trigPos = 0; }
    float advance(const StepSeqData& d, int steps, double beats, float slide, bool retrigger, float dt, double ppq, double bpm) {
        steps = std::clamp(steps, 1, 32);
        double stepBeats = std::max(beats, 1e-6);
        double posSteps;
        if (retrigger) { trigPos += dt * bpm / 60.0 / stepBeats; posSteps = trigPos; }
        else posSteps = ppq / stepBeats;
        double cyc = std::fmod(posSteps, (double) steps); if (cyc < 0) cyc += steps;
        int i = (int) cyc; float f = (float) (cyc - i);
        float v = d.value[(size_t) i];
        int prev = (i + steps - 1) % steps;
        if (d.slide[(size_t) i] && slide > 0) {
            float t = clampf(f / slide, 0, 1);
            t = shapeCurve(t, d.curve[(size_t) i]);
            v = d.value[(size_t) prev] + (v - d.value[(size_t) prev]) * t;
        }
        value = v;
        return v;
    }
    float out() const { return value; }
private:
    double trigPos = 0; float value = 0;
};

class RandomSource {
public:
    explicit RandomSource(uint32_t seed = 1) : rng(seed) { a = rng.bi(); b = rng.bi(); chaos = 0.37f + 0.1f * rng.uni(); }
    void noteOn() { perNote = rng.bi(); }
    float advance(int type, float hz, float dt) {
        float v;
        switch (type) {
            default:
            case 0: ph += hz * dt; while (ph >= 1) { ph -= 1; a = rng.bi(); } v = a; break;           // sample & hold
            case 1: ph += hz * dt; while (ph >= 1) { ph -= 1; a = b; b = rng.bi(); }                  // smooth
                v = a + (b - a) * (0.5f - 0.5f * std::cos(kPi * (float) ph)); break;
            case 2: ph += hz * dt; while (ph >= 1) { ph -= 1; chaosPrev = chaos; chaos = 3.99f * chaos * (1 - chaos); } // chaos (logistic map)
                v = (chaosPrev + (chaos - chaosPrev) * (float) ph) * 2 - 1; break;
            case 3: v = perNote; break;                                                              // per note
        }
        value = v;
        return v;
    }
    float out() const { return value; }
private:
    Rng rng; double ph = 0; float a = 0, b = 0, chaos = 0.4f, chaosPrev = 0.4f, perNote = 0, value = 0;
};

} // namespace gnarl
