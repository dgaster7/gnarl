#include "Engine.h"
#include "Audit.h"
#include "ParamIx.h"
#include "PatchStructure.h"
#include "filter/Filter.h"
#include "fx/FxModule.h"
#include "mod/ModSources.h"
#include "osc/OscSlot.h"
#include <atomic>
#include <cstring>
#include <mutex>

namespace gnarl {

namespace {
constexpr int kSub = 16;
constexpr int kPool = 12;      // internal voices (polyphony limit + room for stolen voices fading out)
constexpr int kMaxOsLog2 = 3;
constexpr int kMaxOs = 1 << kMaxOsLog2;
const int kFixedSemis[4] = {1, 2, 12, 48};

inline float curveApply(float s, int curve, bool bip) {
    switch (curve) {
        case RC_Slow: return s * std::fabs(s);
        case RC_Fast: return (s < 0 ? -1.f : 1.f) * std::sqrt(std::fabs(s));
        case RC_LinearRev: return bip ? -s : 1.f - s;
        case RC_SlowRev: return bip ? -(s * std::fabs(s)) : 1.f - s * s;
        case RC_FastRev: return bip ? -((s < 0 ? -1.f : 1.f) * std::sqrt(std::fabs(s))) : 1.f - std::sqrt(std::fabs(s));
        default: return s;
    }
}
} // namespace

// ---------------------------------------------------------------------------------------------
// Snapshot exchange: message thread publishes, audio thread acquires; frees happen on publish.
// ---------------------------------------------------------------------------------------------
class SnapshotExchange {
public:
    void publish(std::shared_ptr<const PatchStructure> s) {
        std::lock_guard<std::mutex> g(lock); // message-thread side only
        pendingHold = std::move(s);
        pending.store(pendingHold.get(), std::memory_order_release);
        // free retired snapshots the audio thread no longer uses
        const PatchStructure* inUse = audioUsing.load(std::memory_order_acquire);
        retired.erase(std::remove_if(retired.begin(), retired.end(), [&](const std::shared_ptr<const PatchStructure>& p) { return p.get() != inUse && p.get() != pendingHold.get(); }), retired.end());
        if (current) retired.push_back(current);
        current = pendingHold;
    }
    // audio thread
    const PatchStructure* acquire() {
        const PatchStructure* p = pending.load(std::memory_order_acquire);
        audioUsing.store(p, std::memory_order_release);
        return p;
    }
private:
    std::mutex lock;
    std::shared_ptr<const PatchStructure> pendingHold, current;
    std::vector<std::shared_ptr<const PatchStructure>> retired;
    std::atomic<const PatchStructure*> pending{nullptr};
    std::atomic<const PatchStructure*> audioUsing{nullptr};
};

struct EngineCtx;

// ---------------------------------------------------------------------------------------------
class Voice {
public:
    bool active = false, gate = false, stealing = false;
    int note = 60; float velocity = 0.8f, releaseVel = 0.5f;
    uint64_t age = 0;
    float pressure = 0, slide = 0;
    std::array<Envelope, 4> env;
    std::array<float, 4> envPrev{}, envCur{};
    std::array<Lfo, 2> lfo;
    std::array<OscSlot, 3> osc;
    std::array<FilterUnit, 2> filt;
    Oversampler os;
    std::array<DistState, 2> sat;
    std::array<float, MS_Count> src{};
    std::array<float, 4> fnState{};
    std::vector<float> vCur, vPrev;
    // glide
    float pitch = 60, pitchPrev = 60, glideFrom = 60, glideTo = 60, glideT = 1, glideDur = 0;
    // sub / noise
    double subPh = 0;
    Rng rng{1};
    float pinkB[7] = {}; float crackle = 0; double metal[6] = {}; double digPh = 0; uint32_t lfsr = 0xACE1u; float digVal = 0;
    float stealGain = 1;
    // scratch (base rate)
    std::vector<float> b1L, b1R, b2L, b2R, sendL, sendR, sub, outL, outR, osTmpL, osTmpR;
    uint32_t seed = 1;
    std::array<float, 3> oscRawPrev{};
    bool oscWrapPrev[3] = {}; float oscWrapFracPrev[3] = {};

    void prepare(float sr, int nParams) {
        for (auto& o : osc) o.prepare(sr);
        os.prepare(4, kSub, kMaxOsLog2, Oversampler::Kind::Iir);
        for (auto& f : filt) f.prepare(sr * kMaxOs, (int) (sr * kMaxOs / 8.f) + 8);
        vCur.assign((size_t) nParams, 0); vPrev.assign((size_t) nParams, 0);
        for (auto* v : {&b1L, &b1R, &b2L, &b2R, &sendL, &sendR, &sub, &outL, &outR}) v->assign(kSub, 0.f);
        osTmpL.assign((size_t) kSub * kMaxOs, 0.f); osTmpR.assign((size_t) kSub * kMaxOs, 0.f);
        for (auto& s : sat) s.dc.setRate(sr * kMaxOs);
    }
};

// ---------------------------------------------------------------------------------------------
class EngineImpl {
public:
    bool fx = false;
    float sr = 48000;
    int maxBlock = 2048;
    const Registry& R = Registry::get();
    int NP = 0;
    std::vector<float> raw, base, effPrev, effCur;
    std::vector<Smoother> smooth;
    std::vector<uint8_t> smoothed;
    GlobalIx G;
    std::array<OscIx, 3> O{OscIx(0), OscIx(1), OscIx(2)};
    std::array<FilterIx, 2> F{FilterIx(0), FilterIx(1)};
    std::array<EnvIx, 4> E{EnvIx(0), EnvIx(1), EnvIx(2), EnvIx(3)};
    std::array<LfoIx, 6> L{LfoIx(0), LfoIx(1), LfoIx(2), LfoIx(3), LfoIx(4), LfoIx(5)};
    SnapshotExchange exchange;
    std::shared_ptr<const PatchStructure> defaultStructure;
    const PatchStructure* st = nullptr;
    std::array<Voice, kPool> voices;
    uint64_t ageCounter = 0;
    int heldNotes[16] = {}; int numHeld = 0; float lastNotePitch = 60;
    // global sources
    std::array<Lfo, 4> glfo;
    std::array<StepSeq, 2> seq;
    std::array<RandomSource, 4> rnd{RandomSource(11), RandomSource(23), RandomSource(37), RandomSource(51)};
    std::array<float, MS_Count> gsrc{};
    float modWheel = 0, bend = 0, chanAT = 0;
    float follower = 0, fast = 0, slow = 0, transient = 0, lastOutRms = 0;
    int lastNote = 60;
    CycleBuilder builder;
    std::unique_ptr<FxChain> chainPtr;
    FinalStage finalStage;
    Oversampler postOs;
    DistState postDist; DcBlocker postDc; DcBlocker outDc[2];
    std::vector<float> mixL, mixR, subMix, fxSendL, fxSendR, tmpL, tmpR, inGainL, inGainR;
    std::array<const WavetableData*, 3> tablePtr{};
    double ppqAtBlock = 0;
    bool fxEnvGate = false;
    float normDelta[4096]; float stDelta[4096];
    std::unique_ptr<std::atomic<float>[]> uiVals{new std::atomic<float>[4096]};
    std::unique_ptr<std::atomic<float>[]> uiSrc{new std::atomic<float>[MS_Count]};
    std::atomic<float> uiPeakL{0}, uiPeakR{0}; std::atomic<int> uiVoices{0};
    void publishUi(const float* L, const float* R, int n) {
        Voice* nv = fx ? &voices[0] : newestVoice();
        for (int i = 0; i < NP; ++i) {
            float v = (nv && st->voiceRouted[(size_t) i]) ? nv->vCur[(size_t) i] : effCur[(size_t) i];
            uiVals[(size_t) i].store(v, std::memory_order_relaxed);
        }
        for (int s2 = 0; s2 < MS_Count; ++s2) uiSrc[(size_t) s2].store(nv ? nv->src[(size_t) s2] : gsrc[(size_t) s2], std::memory_order_relaxed);
        float pl = 0, pr = 0;
        for (int i = 0; i < n; ++i) { pl = std::max(pl, std::fabs(L[i])); pr = std::max(pr, std::fabs(R[i])); }
        uiPeakL.store(std::max(pl, uiPeakL.load(std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        uiPeakR.store(std::max(pr, uiPeakR.load(std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        int c = 0; for (auto& v : voices) if (v.active && !v.stealing) ++c;
        uiVoices.store(c, std::memory_order_relaxed);
    }

    EngineImpl() {
        NP = R.size();
        raw.resize((size_t) NP); base.resize((size_t) NP); effPrev.resize((size_t) NP); effCur.resize((size_t) NP);
        smooth.resize((size_t) NP); smoothed.resize((size_t) NP);
        for (int i = 0; i < NP; ++i) { raw[(size_t) i] = base[(size_t) i] = effPrev[(size_t) i] = effCur[(size_t) i] = R[i].def; smoothed[(size_t) i] = R[i].smoothMs > 0 && !R[i].isDiscrete(); }
        auto ds = std::make_shared<PatchStructure>();
        defaultStructure = ds;
        exchange.publish(ds);
        factoryTables(); // generate before any audio callback
        for (int i = 0; i < 4096; ++i) uiVals[(size_t) i].store(i < NP ? R[i].def : 0.f);
        for (int i = 0; i < MS_Count; ++i) uiSrc[(size_t) i].store(0);
    }

    const PatchStructure& S() const { return *st; }
    float g(int p) const { return effCur[(size_t) p]; }
    float gp(int p) const { return effPrev[(size_t) p]; }
    bool b(int p) const { return effCur[(size_t) p] > 0.5f; }
    int ci(int p) const { return (int) std::lround(effCur[(size_t) p]); }
    // per-voice value (routed per-voice destinations override the global value)
    float vv(const Voice& v, int p) const { return st->voiceRouted[(size_t) p] ? v.vCur[(size_t) p] : effCur[(size_t) p]; }
    float vvp(const Voice& v, int p) const { return st->voiceRouted[(size_t) p] ? v.vPrev[(size_t) p] : effPrev[(size_t) p]; }

    void prepare(double rate, int block) {
        sr = (float) rate; maxBlock = std::max(block, kSub);
        for (int i = 0; i < NP; ++i) smooth[(size_t) i].setTime(R[i].smoothMs, sr / kSub);
        for (auto& v : voices) v.prepare(sr, NP);
        if (!chainPtr) chainPtr = std::make_unique<FxChain>(fx);
        chainPtr->prepare(sr, kSub);
        finalStage.prepare(sr, kSub);
        postOs.prepare(1, kSub, kMaxOsLog2, Oversampler::Kind::Iir);
        postDc.setRate(sr * kMaxOs); postDist.dc.setRate(sr * kMaxOs); outDc[0].setRate(sr); outDc[1].setRate(sr);
        for (auto* v : {&mixL, &mixR, &subMix, &fxSendL, &fxSendR, &tmpL, &tmpR, &inGainL, &inGainR}) v->assign((size_t) kSub * kMaxOs, 0.f);
        reset();
    }

    void reset() {
        for (int i = 0; i < NP; ++i) { base[(size_t) i] = raw[(size_t) i]; smooth[(size_t) i].snap(raw[(size_t) i]); effPrev[(size_t) i] = effCur[(size_t) i] = raw[(size_t) i]; }
        for (auto& v : voices) { v.active = false; v.gate = false; for (auto& e : v.env) e.kill(); for (auto& f : v.filt) f.reset(); v.os.reset(); }
        numHeld = 0;
        for (auto& l : glfo) l.resetPhase(0);
        if (chainPtr) chainPtr->reset();
        finalStage.reset();
        postOs.reset(); postDist.reset(); postDc.reset(); outDc[0].reset(); outDc[1].reset();
        follower = fast = slow = transient = 0;
        modWheel = bend = chanAT = 0;
    }

    // ---------------- modulation ----------------
    float mapSource(float s, int src, bool bipolar) const {
        bool natural = modSourceIsBipolar(src);
        if (bipolar && !natural) return s * 2.f - 1.f;
        if (!bipolar && natural) return 0.5f * s + 0.5f;
        return s;
    }

    void applyRoutes(const std::vector<PatchStructure::CompiledRoute>& routes, const std::vector<int>& dests, const float* srcv, const float* baseVals, float* out) {
        for (int d : dests) { normDelta[d] = 0; stDelta[d] = 0; }
        for (const auto& r : routes) {
            const float depth = effPrev[(size_t) G.routeDepth[(size_t) r.k]];
            if (depth == 0) continue;
            float s = mapSource(srcv[r.src], r.src, r.bipolar);
            s = curveApply(s, r.curve, r.bipolar);
            if (r.via >= 0) s *= clampf(modSourceIsBipolar(r.via) ? 0.5f * srcv[r.via] + 0.5f : srcv[r.via], 0, 1);
            const auto& p = R[r.dest];
            if (r.curve >= RC_Semi1 && (p.unit == Unit::Hz || p.unit == Unit::Cents || p.unit == Unit::Semitones || p.unit == Unit::Ratio))
                stDelta[r.dest] += s * depth * (float) kFixedSemis[r.curve - RC_Semi1];
            else
                normDelta[r.dest] += s * depth;
        }
        for (int d : dests) {
            const float b0 = baseVals[d];
            if (normDelta[d] == 0 && stDelta[d] == 0) { out[d] = b0; continue; }
            const auto& p = R[d];
            float v = normDelta[d] != 0 ? p.fromNorm(clampf(p.toNorm(b0) + normDelta[d], 0, 1)) : b0;
            if (stDelta[d] != 0) {
                if (p.unit == Unit::Hz || p.unit == Unit::Ratio) v = clampf(v * std::exp2(stDelta[d] / 12.f), p.unit == Unit::Hz ? 1.f : p.min * 0.01f, p.unit == Unit::Hz ? 40000.f : p.max * 16.f);
                else if (p.unit == Unit::Cents) v += stDelta[d] * 100.f;
                else v += stDelta[d];
            }
            out[d] = v;
        }
    }

    Voice* newestVoice() {
        Voice* best = nullptr;
        for (auto& v : voices) if (v.active && !v.stealing && (!best || v.age > best->age)) best = &v;
        return best;
    }

    // ---------------- notes ----------------
    void envParams(int k, EnvParams& ep, const Voice* v, double bpm) const {
        auto val = [&](int p) { return v ? vv(*v, p) : g(p); };
        const auto& e = E[(size_t) k];
        const double spb = 60.0 / std::max(bpm, 1.0);
        if (val(e.beats) > 0.5f) { ep.a = (float) (val(e.ab) * spb); ep.d = (float) (val(e.db) * spb); ep.r = (float) (val(e.rb) * spb); ep.len = (float) (val(e.lenb) * spb); }
        else { ep.a = val(e.a); ep.d = val(e.d); ep.r = val(e.r); ep.len = val(e.len); }
        ep.s = val(e.s); ep.ac = val(e.ac); ep.dc = val(e.dc); ep.rc = val(e.rc);
        ep.drawn = val(e.mode) > 0.5f;
        ep.table = st->envTable[(size_t) k].data();
        ep.sustainIdx = st->envSustainSample[(size_t) k];
        if (k == 0) { ep.minAttack = 0.001f; ep.minRelease = 0.003f; }
    }

    void startVoice(Voice& v, int note, int vel, bool legato) {
        const bool wasActive = v.active && !v.stealing;
        v.active = true; v.gate = true; v.stealing = false; v.stealGain = 1;
        v.velocity = vel / 127.f; v.age = ++ageCounter;
        v.seed = (uint32_t) (ageCounter * 2654435761u + 17u);
        v.rng = Rng(v.seed);
        const float gt = g(G.glideTime);
        const bool anyHeld = numHeld > 1 || (legato && wasActive);
        const bool glide = gt > 0 && (!b(G.glideLegato) || anyHeld);
        v.glideFrom = glide ? (wasActive ? v.pitch : lastNotePitch) : (float) note;
        v.glideTo = (float) note; v.glideT = 0; v.glideDur = glide ? gt * 0.001f : 0.f;
        if (!glide) v.pitch = (float) note;
        if (!wasActive) v.pitchPrev = v.pitch = glide ? v.glideFrom : (float) note;
        v.note = note;
        lastNotePitch = (float) note; lastNote = note;
        const bool retrig = !(legato && wasActive && b(G.legato) && ci(G.voices) == 1);
        if (retrig) {
            for (auto& e : v.env) e.noteOn(wasActive); // start from the current level if the voice was sounding (click-free)
            for (int k = 0; k < 2; ++k) v.lfo[(size_t) k].trigger();
            if (!wasActive) { v.envPrev.fill(0); v.envCur.fill(0); }
            for (int k = 0; k < 3; ++k) { OscBlockParams p; fillOsc(v, k, p, 0, 1); v.osc[(size_t) k].noteOn(p, v.seed + (uint32_t) k, wasActive && legato); }
            if (!wasActive) { for (auto& f : v.filt) f.reset(); v.os.reset(); for (auto& s : v.sat) s.reset(); v.subPh = 0; v.fnState.fill(0); }
        }
        for (auto& r : rnd) r.noteOn();
        for (auto& l : glfo) l.trigger();
        for (auto& s : seq) s.trigger();
    }

    void noteOn(int note, int vel) {
        if (vel == 0) { noteOff(note, 64); return; }
        // held-note stack
        for (int i = 0; i < numHeld; ++i) if (heldNotes[i] == note) { std::memmove(heldNotes + i, heldNotes + i + 1, sizeof(int) * (size_t) (numHeld - i - 1)); --numHeld; break; }
        if (numHeld < 16) heldNotes[numHeld++] = note;
        const int poly = fx ? 1 : ci(G.voices);
        if (fx) { lastNote = note; lastNotePitch = (float) note; fxEnvGate = true; Voice& v = voices[0]; triggerFxVoice(v, vel); return; }
        if (poly == 1) {
            Voice* cur = nullptr;
            for (auto& v : voices) if (v.active && !v.stealing) { cur = &v; break; }
            if (cur) { startVoice(*cur, note, vel, true); return; }
        }
        // poly: free voice or steal the oldest
        int activeCount = 0;
        for (auto& v : voices) if (v.active && !v.stealing) ++activeCount;
        if (activeCount >= poly) {
            Voice* oldest = nullptr;
            for (auto& v : voices) if (v.active && !v.stealing && (!oldest || (!v.gate && oldest->gate) || (v.gate == oldest->gate && v.age < oldest->age))) oldest = &v;
            if (oldest) oldest->stealing = true;
        }
        Voice* fresh = nullptr;
        for (auto& v : voices) if (!v.active) { fresh = &v; break; }
        if (!fresh) { // all busy (stolen voices still fading): take the quietest stealing one
            for (auto& v : voices) if (v.stealing && (!fresh || v.stealGain < fresh->stealGain)) fresh = &v;
            if (fresh) fresh->active = false;
        }
        if (fresh) startVoice(*fresh, note, vel, false);
    }

    void noteOff(int note, int relVel) {
        for (int i = 0; i < numHeld; ++i) if (heldNotes[i] == note) { std::memmove(heldNotes + i, heldNotes + i + 1, sizeof(int) * (size_t) (numHeld - i - 1)); --numHeld; break; }
        if (fx) { if (numHeld == 0) { fxEnvGate = false; for (auto& e : voices[0].env) e.noteOff(); } return; }
        const int poly = ci(G.voices);
        for (auto& v : voices) {
            if (!v.active || v.stealing || !v.gate || v.note != note) continue;
            if (poly == 1 && numHeld > 0) { startVoice(v, heldNotes[numHeld - 1], (int) (v.velocity * 127), true); continue; } // return to the previous held note
            v.gate = false; v.releaseVel = relVel / 127.f;
            for (auto& e : v.env) e.noteOff();
            for (auto& o : v.osc) o.noteOff();
        }
    }

    void triggerFxVoice(Voice& v, int vel) {
        v.active = true; v.velocity = vel / 127.f; v.age = ++ageCounter; v.note = lastNote; v.pitch = (float) lastNote;
        for (auto& e : v.env) e.noteOn(true);
        for (auto& l : v.lfo) l.trigger();
        for (auto& l : glfo) l.trigger();
        for (auto& s : seq) s.trigger();
        for (auto& r : rnd) r.noteOn();
    }

    void handleMidi(const MidiEvent& e) {
        switch (e.type()) {
            case 0x90: noteOn(e.d1, e.d2); break;
            case 0x80: noteOff(e.d1, e.d2); break;
            case 0xB0:
                if (e.d1 == 1) modWheel = e.d2 / 127.f;
                else if (e.d1 == 74) for (auto& v : voices) if (v.active) v.slide = e.d2 / 127.f;
                else if (e.d1 == 123 || e.d1 == 120) { for (auto& v : voices) { v.gate = false; for (auto& en : v.env) en.noteOff(); } numHeld = 0; }
                break;
            case 0xE0: bend = (float) (((int) e.d2 << 7 | e.d1) - 8192) / 8192.f; break;
            case 0xD0: chanAT = e.d1 / 127.f; for (auto& v : voices) if (v.active) v.pressure = chanAT; break;
            case 0xA0: for (auto& v : voices) if (v.active && v.note == e.d1) v.pressure = e.d2 / 127.f; break;
            default: break;
        }
    }

    // ---------------- per-voice parameter blocks ----------------
    void fillOsc(const Voice& v, int k, OscBlockParams& p, float t0, float t1) {
        (void) t0; (void) t1;
        const auto& o = O[(size_t) k];
        auto V = [&](int i) { return vv(v, i); };
        auto VP = [&](int i) { return vvp(v, i); };
        p.engine = (int) std::lround(V(o.engine));
        const float bendSt = bend * g(G.bendRange);
        const float trans = std::lround(V(o.octave)) * 12.f + std::lround(V(o.semi));
        auto hz = [&](float pitchNote, float fine) {
            if (V(o.fixed) > 0.5f) return V(o.fixedHz) * std::exp2(fine / 1200.f);
            return noteToHz(pitchNote + trans + bendSt + fine / 100.f);
        };
        const bool fineRem = audit::isRemoved(o.fine);
        p.hzStart = hz(v.pitchPrev, fineRem ? 0.f : VP(o.fine));
        p.hzEnd = hz(v.pitch, fineRem ? 0.f : V(o.fine));
        p.unison = (int) std::lround(V(o.unison)); p.detune = V(o.detune); p.spread = V(o.spread); p.blend = V(o.blend); p.multi = V(o.multi) > 0.5f;
        p.phaseMode = (int) std::lround(V(o.phaseMode)); p.phase = V(o.phase);
        p.vaShape = V(o.vaShape); p.vaPw = V(o.vaPw); p.vaSync = V(o.vaSync);
        p.hsDensity = V(o.hsDensity); p.hsSpread = V(o.hsSpread); p.hsSync = V(o.hsSync);
        p.table = st->userTable[(size_t) k] ? st->userTable[(size_t) k].get() : factoryTables()[(size_t) std::clamp((int) std::lround(V(o.wtTable)), 0, numFactoryTables() - 1)].get();
        p.wtPos = V(o.wtPos); p.warpMode = (int) std::lround(V(o.wtWarpMode)); p.warp = V(o.wtWarp); p.pwmDetune = V(o.wtPwmDetune) > 0.5f;
        p.fmAlg = (int) std::lround(V(o.fmAlg));
        for (int i = 0; i < 4; ++i) {
            const auto& oi = o.op[(size_t) i]; auto& q = p.op[(size_t) i];
            q.ratio = V(oi.ratio); q.fixed = V(oi.fixed) > 0.5f; q.freq = V(oi.freq); q.level = V(oi.level); q.fb = V(oi.fb);
            q.wave = (int) std::lround(V(oi.wave)); q.a = V(oi.a); q.d = V(oi.d); q.s = V(oi.s); q.r = V(oi.r);
            if (audit::isRemoved(oi.level)) q.level = 0;
            if (audit::isRemoved(oi.fb)) q.fb = 0;
        }
        p.gfIndex = V(o.gfIndex); p.gfShift = V(o.gfShift); p.gfSpread = V(o.gfSpread); p.gfDetune = V(o.gfDetune);
        p.gfInterp = V(o.gfInterp) > 0.5f; p.gfComplex = V(o.gfComplex) > 0.5f; p.gfMode = (int) std::lround(V(o.gfMode));
        if (audit::isRemoved(o.gfSpread)) p.gfSpread = 0;
        if (audit::isRemoved(o.gfDetune)) p.gfDetune = 0;
        p.haPartials = (int) std::lround(V(o.haPartials)); p.haTilt = V(o.haTilt); p.haOddEven = V(o.haOddEven); p.haComb = V(o.haComb);
        p.haCombPeriod = (int) std::lround(V(o.haCombPeriod)); p.haStretch = V(o.haStretch); p.haFormantHz = V(o.haFormantHz);
        p.haFormantGain = V(o.haFormantGain); p.haFormantWidth = V(o.haFormantWidth);
        if (audit::isRemoved(o.haComb)) p.haComb = 0;
        if (audit::isRemoved(o.haFormantGain)) p.haFormantGain = 0;
        if (audit::isRemoved(o.haStretch)) p.haStretch = 0;
        if (audit::isRemoved(o.haOddEven)) p.haOddEven = 0;
        for (int i = 0; i < 19; ++i) { p.sh[(size_t) i] = clampf(V(o.sh[(size_t) i]), 0, 1); if (i != SH_MpWidth && audit::isRemoved(o.sh[(size_t) i])) p.sh[(size_t) i] = 0; }
        if (audit::isRemoved(o.detune)) p.detune = 0;
        if (audit::isRemoved(o.spread)) p.spread = 0;
        if (audit::isRemoved(o.vaSync)) p.vaSync = 1;
        if (audit::isRemoved(o.hsSync)) p.hsSync = 1;
        if (audit::isRemoved(o.hsSpread)) p.hsSpread = 0;
        if (audit::isRemoved(o.wtWarp)) p.warp = 0;
    }

    void filterSettings(const Voice& v, int k, FilterSettings& s, float envVal) {
        const bool linked = !fx ? b(G.link) : b(G.link);
        const int src = (k == 1 && linked) ? 0 : k;
        const auto& f = F[(size_t) src];
        auto V = [&](int i) { return vv(v, i); };
        s.type = (int) std::lround(V(f.type));
        float cutoff = V(f.cutoff);
        float kt = V(f.keytrack); if (audit::isRemoved(f.keytrack)) kt = 0;
        if (kt > 0) cutoff *= std::exp2((v.pitch - 60.f) / 12.f * kt);
        float ea = V(f.env); if (audit::isRemoved(f.env)) ea = 0;
        if (ea != 0) cutoff *= std::exp2(ea * 8.f * envVal);
        float reso = V(f.reso);
        if (k == 1 && linked) {
            const float lc = audit::isRemoved(G.linkCutoff) ? 0.f : g(G.linkCutoff), lr = audit::isRemoved(G.linkReso) ? 0.f : g(G.linkReso);
            cutoff *= std::exp2(lc / 12.f); reso = clampf(reso + lr, 0, 1);
        }
        s.cutoff = cutoff; s.reso = reso;
        s.sep = V(f.sep); s.morph = V(f.morph); s.gender = audit::isRemoved(f.gender) ? 0.f : V(f.gender); s.shoulder = V(f.shoulder); s.shelf = V(f.shelf);
        s.poles = (int) std::lround(V(f.poles)); s.stages = (int) std::lround(V(f.stages)); s.opposing = V(f.opposing) > 0.5f; s.boost = V(f.boost) > 0.5f;
        const auto& own = F[(size_t) k];
        s.drive = vv(v, own.drive); if (audit::isRemoved(own.drive)) s.drive = 0;
        s.driveType = (int) std::lround(vv(v, own.driveType)); s.drivePos = (int) std::lround(vv(v, own.drivePos));
        s.limiter = (int) std::lround(vv(v, own.limiter));
    }

    // ---------------- voice rendering ----------------
    void renderVoice(Voice& v, int n, const float* inL, const float* inR, double bpm) {
        const float dt = (float) n / sr;
        // envelopes
        for (int k = 0; k < 4; ++k) {
            EnvParams ep; envParams(k, ep, &v, bpm);
            v.envPrev[(size_t) k] = v.envCur[(size_t) k];
            v.envCur[(size_t) k] = v.env[(size_t) k].advance(ep, dt);
        }
        // glide
        v.pitchPrev = v.pitch;
        if (v.glideT < 1.f) {
            v.glideT = v.glideDur > 0 ? std::min(1.f, v.glideT + dt / v.glideDur) : 1.f;
            v.pitch = v.glideFrom + (v.glideTo - v.glideFrom) * shapeCurve(v.glideT, -g(G.glideCurve));
            if (audit::isRemoved(G.glideTime)) v.pitch = v.glideTo;
        }
        float* b1L = v.b1L.data(); float* b1R = v.b1R.data(); float* b2L = v.b2L.data(); float* b2R = v.b2R.data();
        std::memset(b1L, 0, sizeof(float) * (size_t) n); std::memset(b1R, 0, sizeof(float) * (size_t) n);
        std::memset(b2L, 0, sizeof(float) * (size_t) n); std::memset(b2R, 0, sizeof(float) * (size_t) n);
        std::memset(v.sendL.data(), 0, sizeof(float) * (size_t) n); std::memset(v.sendR.data(), 0, sizeof(float) * (size_t) n);
        std::memset(v.sub.data(), 0, sizeof(float) * (size_t) n);

        if (!fx) {
            // ---- oscillators with the A>B>C>A chain ----
            OscBlockParams op[3];
            float lv0[3], lv1[3], pan0[3], pan1[3], bal0[3], bal1[3], snd0[3], snd1[3]; bool inv[3]; bool on[3];
            for (int k = 0; k < 3; ++k) {
                const auto& o = O[(size_t) k];
                fillOsc(v, k, op[k], 0, 1);
                lv0[k] = vvp(v, o.level); lv1[k] = vv(v, o.level);
                if (audit::isRemoved(o.level)) lv0[k] = lv1[k] = 0;
                pan0[k] = vvp(v, o.pan); pan1[k] = vv(v, o.pan);
                if (audit::isRemoved(o.pan)) pan0[k] = pan1[k] = 0;
                bal0[k] = vvp(v, o.balance); bal1[k] = vv(v, o.balance);
                snd0[k] = vvp(v, o.fxSend); snd1[k] = vv(v, o.fxSend);
                if (audit::isRemoved(o.fxSend)) snd0[k] = snd1[k] = 0;
                inv[k] = vv(v, o.invert) > 0.5f;
                v.osc[(size_t) k].beginBlock(op[k], n, builder, dt);
            }
            // which oscillators must run: audible, or the source of an active link, or a routed mod source
            int lmode[3]; float lam0[3], lam1[3];
            for (int i = 0; i < 3; ++i) {
                lmode[i] = (int) std::lround(vv(v, G.linkMode[(size_t) i]));
                lam0[i] = vvp(v, G.linkAmount[(size_t) i]); lam1[i] = vv(v, G.linkAmount[(size_t) i]);
                if (audit::isRemoved(G.linkAmount[(size_t) i])) lam0[i] = lam1[i] = 0;
            }
            for (int k = 0; k < 3; ++k) on[k] = lv0[k] > 0 || lv1[k] > 0 || snd1[k] > 0;
            // link i: source i -> target (i+1)%3 ; ab: A->B, bc: B->C, ca: C->A
            for (int i = 0; i < 3; ++i) if ((lam0[i] > 0 || lam1[i] > 0) && on[(i + 1) % 3]) on[i] = true;
            on[0] |= oscSourceUsed(MS_OscA); on[1] |= oscSourceUsed(MS_OscB); on[2] |= oscSourceUsed(MS_OscC);
            for (int i = 0; i < n; ++i) {
                const float t = (float) i / (float) n;
                OscOut outs[3];
                for (int k = 0; k < 3; ++k) {
                    if (!on[k]) { outs[k] = OscOut(); v.oscRawPrev[(size_t) k] = 0; continue; }
                    const int li = (k + 2) % 3; // link feeding this oscillator: A<-ca(2), B<-ab(0), C<-bc(1)
                    ChainIn cin;
                    const float am = lam0[li] + (lam1[li] - lam0[li]) * t;
                    if (am > 0) {
                        const int sk = li; // source oscillator index of link li
                        cin.mode = lmode[li]; cin.amount = am;
                        if (sk < k) { cin.src = outs[sk].raw; cin.srcWrapped = v.osc[(size_t) sk].wrapped; cin.srcFrac = v.osc[(size_t) sk].wrapFrac; }
                        else { cin.src = v.oscRawPrev[(size_t) sk]; cin.srcWrapped = v.oscWrapPrev[sk]; cin.srcFrac = v.oscWrapFracPrev[sk]; }
                    }
                    outs[k] = v.osc[(size_t) k].tick(i, cin);
                    v.oscRawPrev[(size_t) k] = outs[k].raw;
                    v.oscWrapPrev[k] = v.osc[(size_t) k].wrapped; v.oscWrapFracPrev[k] = v.osc[(size_t) k].wrapFrac;
                }
                for (int k = 0; k < 3; ++k) {
                    if (!on[k]) continue;
                    const float lvl = (lv0[k] + (lv1[k] - lv0[k]) * t) * (inv[k] ? -1.f : 1.f);
                    float pl, pr; balance(pan0[k] + (pan1[k] - pan0[k]) * t, pl, pr);
                    const float bal = bal0[k] + (bal1[k] - bal0[k]) * t;
                    const float w2 = 0.5f + 0.5f * clampf(bal, -1, 1), w1 = 1.f - w2;
                    const auto& o = outs[k];
                    const float L1 = o.l1 * lvl * pl, R1 = o.r1 * lvl * pr, L2 = o.l2 * lvl * pl, R2 = o.r2 * lvl * pr;
                    if (op[k].multi) { b1L[i] += L1; b1R[i] += R1; b2L[i] += L2; b2R[i] += R2; }
                    else { b1L[i] += L1 * w1; b1R[i] += R1 * w1; b2L[i] += L1 * w2; b2R[i] += R1 * w2; }
                    const float snd = snd0[k] + (snd1[k] - snd0[k]) * t;
                    if (snd > 0) { v.sendL[(size_t) i] += (L1 + L2) * snd; v.sendR[(size_t) i] += (R1 + R2) * snd; }
                }
            }
            for (int k = 0; k < 3; ++k) v.src[(size_t) (MS_OscA + k)] = clampf(v.oscRawPrev[(size_t) k], -1, 1);
            renderSubNoise(v, n, b1L, b1R, b2L, b2R);
        } else {
            // GNARL FX: the input is the source for both filter buses
            for (int i = 0; i < n; ++i) { b1L[i] = inL[i]; b1R[i] = inR[i]; }
        }
        const bool fxBypass = fx && ci(F[0].type) == FT_Off && ci(F[1].type) == FT_Off && gp(G.satDrive) <= 0 && g(G.satDrive) <= 0 && !(b(G.link) && ci(F[0].type) != FT_Off);
        if (fxBypass) {
            // GNARL FX with both filters off: the filter section is bit-transparent (no oversampling round trip)
            std::memcpy(v.outL.data(), b1L, sizeof(float) * (size_t) n); std::memcpy(v.outR.data(), b1R, sizeof(float) * (size_t) n);
            v.os.reset(); for (auto& f : v.filt) f.reset();
        } else renderFilters(v, n, b1L, b1R, b2L, b2R);
        // amp envelope (synth only)
        float* oL = v.outL.data(); float* oR = v.outR.data();
        if (!fx) {
            const float a0 = v.envPrev[0], a1 = v.envCur[0];
            for (int i = 0; i < n; ++i) {
                float a = (a0 + (a1 - a0) * (float) i / (float) n) * v.stealGain;
                if (v.stealing) { v.stealGain = std::max(0.f, v.stealGain - 1.f / (0.003f * sr)); }
                oL[i] *= a; oR[i] *= a;
                v.sendL[(size_t) i] *= a; v.sendR[(size_t) i] *= a; v.sub[(size_t) i] *= a;
            }
            if (!v.env[0].active() || (v.stealing && v.stealGain <= 0)) v.active = false;
        }
    }

    bool oscSourceUsed(int src) const {
        for (auto& r : st->voiceRoutes) if (r.src == src || r.via == src) return true;
        for (auto& r : st->globalRoutes) if (r.src == src || r.via == src) return true;
        return false;
    }

    void renderSubNoise(Voice& v, int n, float* b1L, float* b1R, float* b2L, float* b2R) {
        float sl0 = vvp(v, G.subLevel), sl1 = vv(v, G.subLevel);
        if (audit::isRemoved(G.subLevel)) sl0 = sl1 = 0;
        if (sl0 > 0 || sl1 > 0) {
            const int wave = (int) std::lround(g(G.subWave));
            const float oct = (int) std::lround(g(G.subOctave)) == 0 ? -12.f : -24.f;
            const double inc = noteToHz(v.pitch + oct + bend * g(G.bendRange)) / sr;
            const bool direct = b(G.subDirect);
            for (int i = 0; i < n; ++i) {
                v.subPh += inc; v.subPh -= std::floor(v.subPh);
                const float ph = (float) v.subPh;
                float s;
                if (wave == 0) s = std::sin(kTwoPi * ph);
                else if (wave == 1) s = ph < 0.25f ? 4 * ph : (ph < 0.75f ? 2 - 4 * ph : 4 * ph - 4);
                else s = fastTanh(2.5f * std::sin(kTwoPi * ph)) * 1.0f;
                s *= sl0 + (sl1 - sl0) * (float) i / (float) n;
                if (direct) v.sub[(size_t) i] += s;
                else { b1L[i] += s; b1R[i] += s; }
            }
        }
        float nl0 = vvp(v, G.noiseLevel), nl1 = vv(v, G.noiseLevel);
        if (audit::isRemoved(G.noiseLevel)) nl0 = nl1 = 0;
        if (nl0 > 0 || nl1 > 0) {
            const int type = (int) std::lround(g(G.noiseType));
            const bool track = b(G.noiseTrack);
            const float noteHz = noteToHz(v.pitch);
            const float bal0 = vvp(v, G.noiseBalance), bal1 = vv(v, G.noiseBalance);
            for (int i = 0; i < n; ++i) {
                float w = v.rng.bi(), x;
                switch (type) {
                    default: case 0: x = w; break;
                    case 1: { // Paul Kellet pink
                        float* b = v.pinkB;
                        b[0] = 0.99886f * b[0] + w * 0.0555179f; b[1] = 0.99332f * b[1] + w * 0.0750759f; b[2] = 0.96900f * b[2] + w * 0.1538520f;
                        b[3] = 0.86650f * b[3] + w * 0.3104856f; b[4] = 0.55000f * b[4] + w * 0.5329522f; b[5] = -0.7616f * b[5] - w * 0.0168980f;
                        x = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362f) * 0.11f; b[6] = w * 0.115926f; break;
                    }
                    case 2: { // crackle: sparse impulses with decay
                        float density = track ? noteHz / 200.f : 1.f;
                        if (v.rng.uni() < 0.002f * density) v.crackle = v.rng.bi();
                        x = v.crackle; v.crackle *= 0.96f; break;
                    }
                    case 3: { // metallic: inharmonic square cluster
                        static const float ratios[6] = {1.f, 1.483f, 1.932f, 2.546f, 3.17f, 3.81f};
                        float base = track ? noteHz : 540.f;
                        x = 0;
                        for (int k = 0; k < 6; ++k) { v.metal[k] += base * ratios[k] / sr; v.metal[k] -= std::floor(v.metal[k]); x += v.metal[k] < 0.5 ? 0.16f : -0.16f; }
                        x = x * 0.7f + w * 0.15f; break;
                    }
                    case 4: { // digital: LFSR noise at a (pitch-tracked) clock
                        float clock = track ? noteHz * 8.f : 6000.f;
                        v.digPh += clock / sr;
                        while (v.digPh >= 1) { v.digPh -= 1; unsigned bit = ((v.lfsr >> 0) ^ (v.lfsr >> 2) ^ (v.lfsr >> 3) ^ (v.lfsr >> 5)) & 1u; v.lfsr = (v.lfsr >> 1) | (bit << 15); v.digVal = (v.lfsr & 1) ? 0.8f : -0.8f; }
                        x = v.digVal; break;
                    }
                }
                const float t = (float) i / (float) n;
                x *= nl0 + (nl1 - nl0) * t;
                const float w2 = 0.5f + 0.5f * clampf(bal0 + (bal1 - bal0) * t, -1, 1), w1 = 1.f - w2;
                b1L[i] += x * w1; b1R[i] += x * w1; b2L[i] += x * w2; b2R[i] += x * w2;
            }
        }
    }

    void renderFilters(Voice& v, int n, float* b1L, float* b1R, float* b2L, float* b2R) {
        const int routing = (int) std::lround(g(G.routing));
        const int l2 = currentOsLog2();
        v.os.setFactorLog2(l2);
        const int fac = 1 << l2;
        const float rate = sr * (float) fac;
        for (auto& f : v.filt) if (f.rate() != rate) f.prepare(rate, (int) (rate / 8.f) + 8);
        const float* ins[4] = {b1L, b1R, b2L, b2R};
        v.os.up(ins, n);
        float* uL1 = v.os.data(0); float* uR1 = v.os.data(1); float* uL2 = v.os.data(2); float* uR2 = v.os.data(3);
        const int N = n * fac;
        FilterSettings fs[2];
        const bool linked = b(G.link);
        for (int k = 0; k < 2; ++k) {
            const int srcF = (k == 1 && linked) ? 0 : k;
            const int es = (int) std::lround(vv(v, F[(size_t) srcF].envSrc));
            filterSettings(v, k, fs[k], v.envCur[(size_t) std::clamp(es, 0, 3)]);
            v.filt[(size_t) k].setTarget(fs[k], N);
        }
        // filter levels/pans (with link pan offset)
        float lv0[2], lv1[2], pn0[2], pn1[2];
        for (int k = 0; k < 2; ++k) {
            lv0[k] = vvp(v, F[(size_t) k].level); lv1[k] = vv(v, F[(size_t) k].level);
            if (audit::isRemoved(F[(size_t) k].level)) lv0[k] = lv1[k] = 0; // removing the output path = silence
            pn0[k] = vvp(v, F[(size_t) k].pan); pn1[k] = vv(v, F[(size_t) k].pan);
            if (audit::isRemoved(F[(size_t) k].pan)) pn0[k] = pn1[k] = 0;
        }
        if (linked) {
            float lp = g(G.linkPan);
            if (audit::isRemoved(G.linkPan)) lp = 0;
            pn0[1] = pn0[0] + lp; pn1[1] = pn1[0] + lp; pn0[0] -= lp; pn1[0] -= lp;
        }
        auto levelPan = [&](float* L, float* R, int k) {
            for (int i = 0; i < N; ++i) {
                const float t = (float) i / (float) N;
                float gl, gr; balance(clampf(pn0[k] + (pn1[k] - pn0[k]) * t, -1, 1), gl, gr);
                const float lv = lv0[k] + (lv1[k] - lv0[k]) * t;
                L[i] *= lv * gl; R[i] *= lv * gr;
            }
        };
        DrawTables dt{st->drawTable[0].data(), st->drawTable[1].data(), PatchStructure::kTableSize};
        float* tL = v.osTmpL.data(); float* tR = v.osTmpR.data();
        if (routing == 0) { // serial
            for (int i = 0; i < N; ++i) { uL1[i] += uL2[i]; uR1[i] += uR2[i]; }
            v.filt[0].process(uL1, uR1, N, dt);
            levelPan(uL1, uR1, 0);
            float sd0 = gp(G.satDrive), sd1 = g(G.satDrive);
            if (audit::isRemoved(G.satDrive)) sd0 = sd1 = 0;
            if (sd0 > 0 || sd1 > 0) {
                const int type = (int) std::lround(g(G.satType));
                float m0 = gp(G.satMix), m1 = g(G.satMix);
                if (audit::isRemoved(G.satMix)) m0 = m1 = 0;
                const bool zw = audit::zeroWet(G.satMix);
                const bool dcb = distNeedsDcBlock(type, 0);
                float* io[2] = {uL1, uR1};
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < N; ++i) {
                        const float t = (float) i / (float) N;
                        const float d = sd0 + (sd1 - sd0) * t, m = m0 + (m1 - m0) * t;
                        float x = io[c][i];
                        float xin = zw ? 0.f : x;
                        float s = distortSample(type, xin, d, 0, v.sat[(size_t) c], rate, dt, 0);
                        if (dcb) s = v.sat[(size_t) c].dc.process(s);
                        float wet = xin + (s - xin) * driveBlend(d);
                        io[c][i] = x * (1.f - m) + wet * m;
                    }
            }
            v.filt[1].process(uL1, uR1, N, dt);
            levelPan(uL1, uR1, 1);
        } else {
            if (routing == 1) { // parallel: both filters get both buses
                for (int i = 0; i < N; ++i) { uL1[i] += uL2[i]; uR1[i] += uR2[i]; tL[i] = uL1[i]; tR[i] = uR1[i]; }
            } else { // split
                std::memcpy(tL, uL2, sizeof(float) * (size_t) N); std::memcpy(tR, uR2, sizeof(float) * (size_t) N);
            }
            v.filt[0].process(uL1, uR1, N, dt);
            levelPan(uL1, uR1, 0);
            v.filt[1].process(tL, tR, N, dt);
            levelPan(tL, tR, 1);
            for (int i = 0; i < N; ++i) { uL1[i] += tL[i]; uR1[i] += tR[i]; }
        }
        // downsample buses 1 (L,R) into the voice output
        float* outs[4] = {v.outL.data(), v.outR.data(), tL, tR};
        v.os.down(outs, n);
    }

    int currentOsLog2() const {
        int l2 = ci(G.oversampling);
        if (offline) l2 += ci(G.renderOs);
        return std::clamp(l2, 0, kMaxOsLog2);
    }
    bool offline = false;

    // ---------------- global sources ----------------
    void updateGlobalSources(int n, const HostInfo& h, double ppq) {
        const float dt = (float) n / sr;
        for (int k = 0; k < 4; ++k) {
            LfoParams lp = lfoParams(k, nullptr);
            gsrc[(size_t) (MS_Lfo1 + k)] = glfo[(size_t) k].advance(lp, dt, ppq, h.bpm, h.playing);
        }
        auto P = [&](int p) { return effPrev[(size_t) p]; };
        auto PI = [&](int p) { return (int) std::lround(effPrev[(size_t) p]); };
        for (int k = 0; k < 2; ++k) {
            double beats = syncDivisions()[(size_t) std::clamp(PI(G.seqDiv[(size_t) k]), 0, (int) syncDivisions().size() - 1)].beats;
            gsrc[(size_t) (MS_Seq1 + k)] = seq[(size_t) k].advance(st->seq[(size_t) k], PI(G.seqSteps[(size_t) k]), beats, P(G.seqSlide[(size_t) k]), PI(G.seqMode[(size_t) k]) == 0, dt, ppq, h.bpm);
        }
        for (int k = 0; k < 4; ++k) {
            float hz = P(G.rndRate[(size_t) k]);
            if (P(G.rndSync[(size_t) k]) > 0.5f) hz = (float) (h.bpm / 60.0 / syncDivisions()[(size_t) std::clamp(PI(G.rndDiv[(size_t) k]), 0, (int) syncDivisions().size() - 1)].beats);
            float v = rnd[(size_t) k].advance(PI(G.rndType[(size_t) k]), hz, dt);
            if (P(G.rndBipolar[(size_t) k]) < 0.5f) v = 0.5f * v + 0.5f;
            gsrc[(size_t) (MS_Rnd1 + k)] = v;
        }
        for (int k = 0; k < 8; ++k) gsrc[(size_t) (MS_Macro1 + k)] = effPrev[(size_t) G.macro[(size_t) k]];
        int count = 0; for (auto& v : voices) if (v.active && !v.stealing) ++count;
        gsrc[MS_VoiceCount] = count / 8.f;
        gsrc[MS_ModWheel] = modWheel; gsrc[MS_PitchBend] = bend;
        gsrc[MS_Follower] = follower; gsrc[MS_Transient] = transient;
    }

    LfoParams lfoParams(int k, const Voice* v) const {
        // global LFOs are advanced before this block's global routes are applied, so they read the
        // previous block's modulated values (one 16-sample block of modulation latency)
        auto val = [&](int p) { return v ? vv(*v, p) : effPrev[(size_t) p]; };
        const auto& l = L[(size_t) k];
        LfoParams lp;
        lp.rateHz = val(l.rate); lp.sync = val(l.sync) > 0.5f;
        lp.beats = syncDivisions()[(size_t) std::clamp((int) std::lround(val(l.div)), 0, (int) syncDivisions().size() - 1)].beats;
        lp.phase = val(l.phase) / 360.f; lp.offset = audit::isRemoved(l.offset) ? 0.f : val(l.offset); lp.delay = val(l.delay); lp.fade = val(l.fade);
        lp.smooth = val(l.smooth); if (audit::isRemoved(l.smooth)) lp.smooth = 0;
        lp.mode = (int) std::lround(val(l.mode)); lp.morph = val(l.morph); lp.morphTime = val(l.morphTime);
        if (audit::isRemoved(l.morphTime)) lp.morphTime = 0;
        lp.tabA = st->lfoTableA[(size_t) k].data(); lp.tabB = st->lfoTableB[(size_t) k].data();
        return lp;
    }

    void updateVoiceSources(Voice& v, int n, const HostInfo& h, double ppq) {
        const float dt = (float) n / sr;
        v.src = gsrc;
        for (int k = 0; k < 4; ++k) v.src[(size_t) (MS_Env1 + k)] = v.envCur[(size_t) k];
        for (int k = 0; k < 2; ++k) { LfoParams lp = lfoParams(4 + k, &v); v.src[(size_t) (MS_Lfo5 + k)] = v.lfo[(size_t) k].advance(lp, dt, ppq, h.bpm, h.playing); }
        v.src[MS_Velocity] = v.velocity; v.src[MS_ReleaseVel] = v.releaseVel;
        v.src[MS_KeyTrack] = clampf((v.pitch - 60.f) / 48.f, -1, 1);
        v.src[MS_Aftertouch] = std::max(v.pressure, chanAT); v.src[MS_MpePressure] = v.pressure; v.src[MS_MpeSlide] = v.slide;
        for (int k = 0; k < 3; ++k) v.src[(size_t) (MS_OscA + k)] = clampf(v.oscRawPrev[(size_t) k], -1, 1);
        // functions
        for (int k = 0; k < 4; ++k) {
            const auto& fd = st->fn[(size_t) k];
            float a = fd.a >= 0 ? v.src[(size_t) fd.a] : 0.f, bb = fd.b >= 0 ? v.src[(size_t) fd.b] : 0.f;
            float out;
            switch (ci(G.fnOp[(size_t) k])) {
                default: case 0: out = clampf(a + bb, -1, 1); break;
                case 1: out = a * (fd.b >= 0 ? bb : 1.f); break;
                case 2: out = std::min(a, bb); break;
                case 3: out = std::max(a, bb); break;
                case 4: out = a; break;
            }
            float lag = g(G.fnLag[(size_t) k]);
            if (audit::isRemoved(G.fnLag[(size_t) k])) lag = 0;
            if (ci(G.fnOp[(size_t) k]) == 4 && lag <= 0) lag = 50; // Lag op always lags
            if (lag > 0) { float c = std::exp(-dt / (lag * 0.001f)); v.fnState[(size_t) k] = out + (v.fnState[(size_t) k] - out) * c; out = v.fnState[(size_t) k]; }
            else v.fnState[(size_t) k] = out;
            v.src[(size_t) (MS_Fn1 + k)] = out;
        }
    }

    // ---------------- main process ----------------
    void process(const float* inL, const float* inR, float* outL, float* outR, int total, const MidiEvent* ev, int nev, const HostInfo& h, const float* scL, const float* scR) {
        ScopedFlushDenormals fz;
        st = exchange.acquire();
        if (!st) st = defaultStructure.get();
        offline = h.offline;
        int evi = 0, pos = 0;
        while (pos < total) {
            while (evi < nev && ev[evi].offset <= pos) handleMidi(ev[evi++]);
            int n = std::min(kSub, total - pos);
            if (evi < nev && ev[evi].offset < pos + n) n = std::max(1, ev[evi].offset - pos);
            const double ppq = h.ppq + (double) pos / sr * h.bpm / 60.0;
            subBlock(inL ? inL + pos : nullptr, inR ? inR + pos : nullptr, outL + pos, outR + pos, n, h, ppq, scL ? scL + pos : nullptr, scR ? scR + pos : nullptr);
            pos += n;
        }
        while (evi < nev) handleMidi(ev[evi++]);
        publishUi(outL, outR, total);
    }

    void subBlock(const float* inL, const float* inR, float* outL, float* outR, int n, const HostInfo& h, double ppq, const float* scL, const float* scR) {
        // 1. smooth base values
        for (int i = 0; i < NP; ++i) {
            if (smoothed[(size_t) i]) { smooth[(size_t) i].set(raw[(size_t) i]); base[(size_t) i] = smooth[(size_t) i].step(); }
            else base[(size_t) i] = raw[(size_t) i];
        }
        std::memcpy(effPrev.data(), effCur.data(), sizeof(float) * (size_t) NP);
        std::memcpy(effCur.data(), base.data(), sizeof(float) * (size_t) NP);
        // FX plugin input gain
        float* inGL = inGainL.data(); float* inGR = inGainR.data();
        if (fx) {
            const bool igRem = audit::isRemoved(G.inGain);
            float g0 = dbToGain(gp(G.inGain)), g1 = dbToGain(g(G.inGain));
            if (igRem || (gp(G.inGain) == 0 && g(G.inGain) == 0)) { std::memcpy(inGL, inL, sizeof(float) * (size_t) n); std::memcpy(inGR, inR, sizeof(float) * (size_t) n); }
            else for (int i = 0; i < n; ++i) { float gg = g0 + (g1 - g0) * (float) i / (float) n; inGL[i] = inL[i] * gg; inGR[i] = inR[i] * gg; }
            followerUpdate(b(G.efSidechain) && scL ? scL : inGL, b(G.efSidechain) && scR ? scR : inGR, n);
        }
        // 2. global sources + global destinations (per-voice sources from the newest voice)
        updateGlobalSources(n, h, ppq);
        std::array<float, MS_Count> srcForGlobal = gsrc;
        if (Voice* nv = newestVoice()) {
            for (int s = 0; s < MS_Count; ++s) if (modSourceIsPerVoice(s)) srcForGlobal[(size_t) s] = nv->src[(size_t) s];
        } else if (fx) {
            for (int s = 0; s < MS_Count; ++s) if (modSourceIsPerVoice(s)) srcForGlobal[(size_t) s] = voices[0].src[(size_t) s];
        }
        if (!st->globalRoutes.empty()) applyRoutes(st->globalRoutes, st->globalDests, srcForGlobal.data(), base.data(), effCur.data());

        std::memset(mixL.data(), 0, sizeof(float) * (size_t) n); std::memset(mixR.data(), 0, sizeof(float) * (size_t) n);
        std::memset(subMix.data(), 0, sizeof(float) * (size_t) n);
        // 3. voices
        if (fx) {
            Voice& v = voices[0];
            if (!v.active) { v.active = true; v.pitch = (float) lastNote; v.note = lastNote; }
            fxTransientTriggers(v);
            updateVoiceSources(v, n, h, ppq);
            voiceRoutes(v);
            renderVoice(v, n, inGL, inGR, h.bpm);
            std::memcpy(mixL.data(), v.outL.data(), sizeof(float) * (size_t) n);
            std::memcpy(mixR.data(), v.outR.data(), sizeof(float) * (size_t) n);
        } else {
            for (auto& v : voices) {
                if (!v.active) continue;
                updateVoiceSources(v, n, h, ppq);
                voiceRoutes(v);
                renderVoice(v, n, nullptr, nullptr, h.bpm);
                for (int i = 0; i < n; ++i) {
                    mixL[(size_t) i] += v.outL[(size_t) i] + v.sendL[(size_t) i];
                    mixR[(size_t) i] += v.outR[(size_t) i] + v.sendR[(size_t) i];
                    subMix[(size_t) i] += v.sub[(size_t) i];
                }
            }
            postDrive(n);
        }
        // 4. FX chain
        FxContext ctx;
        ctx.sr = sr; ctx.bpm = h.bpm; ctx.ppq = ppq; ctx.playing = h.playing; ctx.lastNoteHz = noteToHz((float) lastNote);
        ctx.scL = scL; ctx.scR = scR; ctx.st = st; ctx.fxPlugin = fx;
        ctx.draw = DrawTables{st->drawTable[0].data(), st->drawTable[1].data(), PatchStructure::kTableSize};
        ctx.osLog2 = std::clamp(ci(G.oversampling), 1, kMaxOsLog2); // FX latency must not change between realtime and bounce
        chainPtr->process(mixL.data(), mixR.data(), n, effPrev.data(), effCur.data(), ctx);
        // 5. clean sub after FX, master, final stage
        float m0 = gp(G.master), m1 = g(G.master);
        if (audit::isRemoved(G.master)) m0 = m1 = 1;
        for (int i = 0; i < n; ++i) {
            const float m = m0 + (m1 - m0) * (float) i / (float) n;
            outL[i] = (mixL[(size_t) i] + subMix[(size_t) i]) * m;
            outR[i] = (mixR[(size_t) i] + subMix[(size_t) i]) * m;
        }
        // Synth only: 5 Hz DC blocker on the output (FM half/abs-sine waves, grains and asymmetric curves make DC).
        // GNARL FX has none, so neutral settings stay bit-transparent; its nonlinear modules block their own DC.
        if (!fx) for (int i = 0; i < n; ++i) { outL[i] = outDc[0].process(outL[i]); outR[i] = outDc[1].process(outR[i]); }
        const bool cdRem = audit::isRemoved(G.clipDrive);
        finalStage.process(outL, outR, n, b(G.clipOn), ci(G.clipMode), gp(G.clipCeiling), g(G.clipCeiling), cdRem ? 0.f : gp(G.clipDrive), cdRem ? 0.f : g(G.clipDrive));
        if (!fx) followerUpdate(outL, outR, n);
    }

    void voiceRoutes(Voice& v) {
        if (st->voiceRoutes.empty()) return;
        for (int d : st->voiceDests) v.vPrev[(size_t) d] = v.vCur[(size_t) d];
        const bool first = v.age == ageCounter && v.vPrev.size() == 0;
        (void) first;
        applyRoutes(st->voiceRoutes, st->voiceDests, v.src.data(), effCur.data(), v.vCur.data());
    }

    void followerUpdate(const float* L, const float* R, int n) {
        float atk = g(G.efAttack) * 0.001f, rel = g(G.efRelease) * 0.001f;
        float gain = dbToGain(g(G.efGain));
        float ca = std::exp(-1.f / (std::max(atk, 1e-5f) * sr)), cr = std::exp(-1.f / (std::max(rel, 1e-5f) * sr));
        float fa = std::exp(-1.f / (0.001f * sr)), fr = std::exp(-1.f / (0.02f * sr)), sa = std::exp(-1.f / (0.03f * sr)), srl = std::exp(-1.f / (0.3f * sr));
        float sens = g(G.trSens);
        float td = std::exp(-1.f / (std::max(g(G.trDecay) * 0.001f, 1e-4f) * sr));
        for (int i = 0; i < n; ++i) {
            float x = std::max(std::fabs(L[i]), std::fabs(R[i])) * gain;
            follower = x > follower ? x + (follower - x) * ca : x + (follower - x) * cr;
            fast = x > fast ? x + (fast - x) * fa : x + (fast - x) * fr;
            slow = x > slow ? x + (slow - x) * sa : x + (slow - x) * srl;
            float thr = 1.f + (1.f - sens) * 3.f;
            if (fast > slow * thr + 0.01f && transient < 0.5f) { transient = 1; transientEvent = true; }
            else transient *= td;
        }
        follower = std::min(follower, 1.f);
    }
    bool transientEvent = false;

    void fxTransientTriggers(Voice& v) {
        int mode = ci(G.envTrigger);
        if (transientEvent && (mode == 1 || mode == 2)) {
            for (auto& e : v.env) { e.noteOn(true); }
            for (auto& l : v.lfo) l.trigger();
            if (numHeld == 0) { pendingFxRelease = true; }
        } else if (pendingFxRelease && numHeld == 0) {
            for (auto& e : v.env) e.noteOff();
            pendingFxRelease = false;
        }
        transientEvent = false;
    }
    bool pendingFxRelease = false;

    void postDrive(int n) {
        float d0 = gp(G.postDrive), d1 = g(G.postDrive);
        if (audit::isRemoved(G.postDrive)) d0 = d1 = 0;
        if (d0 <= 0 && d1 <= 0) return;
        const int type = ci(G.postDriveType);
        const int l2 = currentOsLog2();
        postOs.setFactorLog2(l2);
        const int N = n << l2;
        float* mid = tmpL.data();
        for (int i = 0; i < n; ++i) mid[i] = 0.5f * (mixL[(size_t) i] + mixR[(size_t) i]);
        const float* in[1] = {mid};
        postOs.up(in, n);
        float* u = postOs.data(0);
        DrawTables dt{st->drawTable[0].data(), st->drawTable[1].data(), PatchStructure::kTableSize};
        const bool dcb = distNeedsDcBlock(type, 0);
        for (int i = 0; i < N; ++i) {
            const float d = d0 + (d1 - d0) * (float) i / (float) N;
            float s = distortSample(type, u[i], d, 0, postDist, sr * (float) (1 << l2), dt, 0);
            if (dcb) s = postDc.process(s);
            u[i] = u[i] + (s - u[i]) * driveBlend(d);
        }
        float* outp[1] = {tmpR.data()};
        postOs.down(outp, n);
        // mid replaced by the driven mid; side untouched (keeps width)
        for (int i = 0; i < n; ++i) {
            const float side = 0.5f * (mixL[(size_t) i] - mixR[(size_t) i]);
            const float m = tmpR[(size_t) i];
            mixL[(size_t) i] = m + side; mixR[(size_t) i] = m - side;
        }
    }
};

// ---------------------------------------------------------------------------------------------
Engine::Engine(bool fxPlugin) : d(std::make_unique<EngineImpl>()) {
    d->fx = fxPlugin;
    for (int i = 0; i < d->NP; ++i) { float v = d->R[i].defaultFor(fxPlugin); d->raw[(size_t) i] = d->base[(size_t) i] = d->effPrev[(size_t) i] = d->effCur[(size_t) i] = v; }
}
Engine::~Engine() = default;
void Engine::prepare(double sampleRate, int maxBlock) { d->prepare(sampleRate, maxBlock); }
void Engine::reset() { d->reset(); }
void Engine::setParam(int i, float v) { if (i >= 0 && i < d->NP) { const auto& p = d->R[i]; d->raw[(size_t) i] = clampf(v, p.min, p.max); } }
float Engine::getParam(int i) const { return (i >= 0 && i < d->NP) ? d->raw[(size_t) i] : 0.f; }
void Engine::setStructure(std::shared_ptr<const PatchStructure> s) { if (s) d->exchange.publish(std::move(s)); }
void Engine::process(const float* inL, const float* inR, float* outL, float* outR, int n, const MidiEvent* events, int numEvents, const HostInfo& host, const float* sideL, const float* sideR) {
    static const std::vector<float> silence(16384, 0.f);
    if (d->fx && (!inL || !inR)) { inL = silence.data(); inR = silence.data(); n = std::min(n, 16384); }
    d->process(inL, inR, outL, outR, n, events, numEvents, host, sideL, sideR);
}
int Engine::latencySamples() const {
    const PatchStructure* s = d->st ? d->st : d->defaultStructure.get();
    return d->chainPtr ? d->chainPtr->latency(d->effCur.data(), s) : 0;
}
double Engine::tailSeconds() const {
    const PatchStructure* s = d->st ? d->st : d->defaultStructure.get();
    return d->chainPtr ? d->chainPtr->tail(d->effCur.data(), s) : 0;
}
bool Engine::isFx() const { return d->fx; }
const std::atomic<float>* Engine::displayValues() const { return d->uiVals.get(); }
const std::atomic<float>* Engine::sourceValues() const { return d->uiSrc.get(); }
float Engine::peakL() const { return d->uiPeakL.load(std::memory_order_relaxed); }
float Engine::peakR() const { return d->uiPeakR.load(std::memory_order_relaxed); }
int Engine::activeVoices() const { return d->uiVoices.load(std::memory_order_relaxed); }

} // namespace gnarl
