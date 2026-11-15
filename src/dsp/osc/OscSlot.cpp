#include "OscSlot.h"
#include <cstring>

namespace gnarl {

namespace {
// FM algorithms: mod[alg][dst] = bitmask of source operators (bit i = op i), carriers bitmask.
struct FmAlg { uint8_t mods[4]; uint8_t carriers; };
const FmAlg kAlgs[8] = {
    {{0b0010, 0b0100, 0b1000, 0}, 0b0001},      // 4>3>2>1
    {{0b1010, 0b0100, 0, 0}, 0b0001},           // 3>2>1, 4>1
    {{0b0110, 0, 0b1000, 0}, 0b0001},           // 4>3>1, 2>1
    {{0b1110, 0, 0, 0}, 0b0001},                // 2+3+4 > 1
    {{0b0010, 0, 0b1000, 0}, 0b0101},           // 2>1, 4>3 (out 1+3)
    {{0b1000, 0b1000, 0b1000, 0}, 0b0111},      // 4 > 1,2,3
    {{0, 0b0100, 0b1000, 0}, 0b0011},           // 1, 4>3>2 (out 1+2)
    {{0, 0, 0, 0}, 0b1111},                     // additive
};
inline float fmWave(int w, float ph) { // ph in cycles, any value
    float x = ph - std::floor(ph);
    float s = std::sin(kTwoPi * x);
    switch (w) {
        default: case 0: return s;
        case 1: return x < 0.5f ? s * 2.f - 0.5f * 0.f : 0.f;   // half sine
        case 2: return std::fabs(s) * 2.f - 1.f;                // abs sine
        case 3: return (x < 0.25f || (x >= 0.5f && x < 0.75f)) ? std::fabs(s) : 0.f; // quarter sine
        case 4: return x < 0.25f ? 4 * x : (x < 0.75f ? 2 - 4 * x : 4 * x - 4);
        case 5: return fastTanh(3.f * s);
        case 6: return 0.6f * (s + 0.5f * std::sin(2 * kTwoPi * x) + 0.33f * std::sin(3 * kTwoPi * x) + 0.25f * std::sin(4 * kTwoPi * x));
    }
}
inline uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
} // namespace

void OscSlot::prepare(float sampleRate) {
    sr = sampleRate;
    static const HalfbandCoefs dc = HalfbandCoefs::design(6, 0.1);
    for (auto& f : fm) f.dec.init(dc);
    haveTable = false;
}

void OscSlot::setupUnison() {
    uni = P.multi ? 8 : std::clamp(P.unison, 1, kMaxUni);
    float sumsq = 0;
    for (int u = 0; u < uni; ++u) {
        float pos = uni == 1 ? 0.f : ((float) u / (uni - 1)) * 2.f - 1.f; // -1..1
        float cents = pos * P.detune;
        detRatio[(size_t) u] = std::exp2(cents / 1200.f);
        float pan = pos * (P.multi ? 0.6f : P.spread);
        balance(pan, panL[(size_t) u], panR[(size_t) u]);
        bool centre = uni == 1 || (uni % 2 == 1 && u == uni / 2);
        float g = centre ? 1.f : (uni == 1 ? 1.f : (uni % 2 == 0 ? 1.f : P.blend));
        if (uni % 2 == 0 && !P.multi) g = (u == uni / 2 - 1 || u == uni / 2) ? 1.f : P.blend; // two central voices full level
        if (P.multi) g = 1;
        uniGain[(size_t) u] = g;
        bus[(size_t) u] = P.multi ? (uint8_t) (u & 1) : 0;
        sumsq += g * g;
    }
    float norm = 1.f / std::sqrt(std::max(sumsq, 1e-6f));
    for (int u = 0; u < uni; ++u) uniGain[(size_t) u] *= norm * (uni > 1 ? std::sqrt((float) std::min(uni, 3)) / std::sqrt(3.f) * 1.4f : 1.f);
}

void OscSlot::noteOn(const OscBlockParams& p, uint32_t seed, bool legato) {
    P = p;
    setupUnison();
    if (legato) return;
    for (int u = 0; u < kMaxUni; ++u) {
        double start;
        uint32_t h = hash32(seed * 977u + (uint32_t) u * 7919u);
        if (P.phaseMode == 0) start = P.phase / 360.0 + (u == 0 ? 0.0 : (double) (hash32((uint32_t) u * 2654435761u) % 10000) / 10000.0);
        else if (P.phaseMode == 2) start = (double) (h % 100000) / 100000.0;
        else start = fph[(size_t) u]; // free run: keep running phase
        start -= std::floor(start);
        if (P.phaseMode != 1) {
            fph[(size_t) u] = start; slave[(size_t) u] = start;
            for (int j = 0; j < 9; ++j) hs[(size_t) u][(size_t) j] = (j == 0 && u == 0) ? start : (double) (hash32(h + (uint32_t) j * 31u) % 10000) / 10000.0;
            for (auto& ph : fm[(size_t) u].ph) ph = start;
            fm[(size_t) u].out.fill(0); fm[(size_t) u].dec.reset();
            for (auto& st : gs[(size_t) u]) { st.ph = 0.999999; for (auto& g : st.g) g.age = -1; }
            subPh[(size_t) u] = start;
            for (int k = 0; k < kPartials; ++k) {
                float a = kTwoPi * (float) start * (float) (k + 1);
                hv[(size_t) u].c[(size_t) k] = std::cos(a); hv[(size_t) u].s[(size_t) k] = std::sin(a);
            }
        }
        blepPrev[(size_t) u] = 0; prevOut[(size_t) u] = 0;
    }
    for (auto& e : opEnv) e.noteOn(false);
    opEnvStart.fill(0); opEnvEnd.fill(0);
    xfade = 0;
    haveTable = false;
    bandIndex = -1000;
}

void OscSlot::noteOff() { for (auto& e : opEnv) e.noteOff(); }

float OscSlot::cycleSample(float phase) const {
    float v = readCycle(tabCur.data(), phase);
    if (xfade > 0) {
        float t = (float) xfade / kXfade;
        v = v + (readCycle(tabPrev.data(), phase) - v) * t;
    }
    return v;
}

void OscSlot::rebuildIfNeeded(CycleBuilder& b, bool force) {
    const bool cycleEngine = P.engine == OE_VA || P.engine == OE_Wavetable || P.engine == OE_GrainFormant;
    if (!cycleEngine) return;
    CycleInputs in;
    in.source = P.engine == OE_VA ? 0 : 1;
    in.vaShape = P.vaShape; in.vaPw = P.vaPw;
    in.table = P.table;
    in.framePos = P.engine == OE_GrainFormant ? P.gfIndex : P.wtPos;
    in.interpFrames = P.engine == OE_GrainFormant ? P.gfInterp : true;
    in.warpMode = P.engine == OE_Wavetable ? P.warpMode : 0;
    in.warp = P.engine == OE_Wavetable ? P.warp : 0;
    in.sh = P.sh;
    // band limit from the highest frequency this table will be read at
    float readHz = std::max(P.hzStart, P.hzEnd);
    if (P.engine == OE_VA) readHz *= std::max(1.f, P.vaSync);
    if (P.engine == OE_GrainFormant) {
        float fg = P.gfMode == 1 ? 261.63f * std::exp2(P.gfShift / 12.f) : readHz * std::exp2(P.gfShift / 12.f);
        readHz = std::max(fg * (1.f + P.gfSpread * 2.f), readHz);
    }
    readHz *= std::exp2(std::max(P.detune, 0.f) / 1200.f);
    int band = (int) std::floor(std::log2(std::max(readHz, 1.f)) * 2.f);
    float bandTop = std::exp2((band + 1) * 0.5f);
    in.maxHarmonic = std::clamp((int) (0.45f * sr / bandTop), 1, kCycle / 2 - 1);
    if (!force && haveTable && in.sameAs(inputs)) return;
    if (haveTable) {
        // crossfade from what is currently audible
        if (xfade > 0) {
            float t = (float) xfade / kXfade;
            for (int i = 0; i < CycleBuilder::kPadded; ++i) tabPrev[(size_t) i] = tabCur[(size_t) i] + (tabPrev[(size_t) i] - tabCur[(size_t) i]) * t;
        } else tabPrev = tabCur;
        xfade = kXfade;
    }
    b.build(in, tabCur.data());
    inputs = in;
    bandIndex = band;
    haveTable = true;
}

void OscSlot::harmonicSetup(float hz) {
    const int N = std::clamp(P.haPartials, 1, kPartials);
    hCount = N;
    const float expo = -1.f + P.haTilt;
    float norm = 0;
    for (int k = 1; k <= kPartials; ++k) {
        float a = 0;
        if (k <= N) {
            a = std::pow((float) k, expo);
            if (P.haOddEven < 0 && (k % 2 == 0)) a *= 1.f + P.haOddEven;
            if (P.haOddEven > 0 && (k % 2 == 1) && k > 1) a *= 1.f - P.haOddEven;
            if (P.haComb > 0) a *= 1.f - P.haComb * (0.5f + 0.5f * std::cos(kTwoPi * (float) k / (float) std::max(P.haCombPeriod, 2)));
            float fk = hz * std::pow((float) k, 1.f + 0.12f * P.haStretch);
            if (P.haFormantGain > 0) {
                float d = std::log2(std::max(fk, 1.f) / P.haFormantHz) / std::max(P.haFormantWidth, 0.05f);
                a *= 1.f + P.haFormantGain * 6.f * std::exp(-d * d);
            }
            // fade partials approaching Nyquist (no aliasing, no clicks when the pitch sweeps)
            a *= clampf((0.47f * sr - fk) / (0.04f * sr), 0.f, 1.f);
        }
        hAmpE[(size_t) k - 1] = a;
        norm += a * a;
    }
    float g = 0.55f / std::sqrt(std::max(norm, 1e-9f)) * 1.25f;
    for (auto& a : hAmpE) a *= g;
}

void OscSlot::beginBlock(const OscBlockParams& p, int nSamples, CycleBuilder& builder, float dt) {
    const bool uniChanged = p.unison != P.unison || p.detune != P.detune || p.spread != P.spread || p.blend != P.blend || p.multi != P.multi;
    const int prevEngine = P.engine;
    P = p;
    n = std::max(1, nSamples);
    if (uniChanged) setupUnison();
    incStart = P.hzStart / sr;
    incEnd = P.hzEnd / sr;
    if (P.engine == OE_Wavetable && P.warpMode == WM_PWM && P.pwmDetune && P.warp > 0) {
        float f = 1.f - 0.95f * P.warp;
        incStart *= f; incEnd *= f;
    }
    if (++rebuildCounter >= 4 || !haveTable || prevEngine != P.engine) { rebuildCounter = 0; rebuildIfNeeded(builder, prevEngine != P.engine); }
    if (P.engine == OE_FM) {
        for (int k = 0; k < 4; ++k) {
            EnvParams ep; ep.a = P.op[(size_t) k].a; ep.d = P.op[(size_t) k].d; ep.s = P.op[(size_t) k].s; ep.r = P.op[(size_t) k].r; ep.dc = -0.5f; ep.rc = -0.5f;
            opEnvStart[(size_t) k] = opEnv[(size_t) k].value();
            opEnvEnd[(size_t) k] = opEnv[(size_t) k].advance(ep, dt);
        }
    }
    if (P.engine == OE_Harmonic) {
        hAmpS = hAmpE;
        float hz = 0.5f * (P.hzStart + P.hzEnd);
        harmonicSetup(hz);
        if (prevEngine != OE_Harmonic) hAmpS = hAmpE;
        const float midInc = 0.5f * (incStart + incEnd);
        for (int u = 0; u < uni; ++u) {
            auto& h = hv[(size_t) u];
            for (int k = 0; k < hCount; ++k) {
                float w = kTwoPi * midInc * detRatio[(size_t) u] * std::pow((float) (k + 1), 1.f + 0.12f * P.haStretch);
                h.cw[(size_t) k] = std::cos(w); h.sw[(size_t) k] = std::sin(w);
                // renormalise the rotator to stop amplitude drift
                float m = 1.f / std::sqrt(h.c[(size_t) k] * h.c[(size_t) k] + h.s[(size_t) k] * h.s[(size_t) k] + 1e-20f);
                h.c[(size_t) k] *= m; h.s[(size_t) k] *= m;
            }
        }
    }
}

OscOut OscSlot::tick(int i, const ChainIn& ci) {
    OscOut o;
    const float t = (float) i / (float) n;
    float baseInc = incStart + (incEnd - incStart) * t;
    if (ci.mode == CM_FM && ci.amount > 0) baseInc *= 1.f + ci.amount * 4.f * ci.src; // through-zero linear FM
    const float pmOff = (ci.mode == CM_PM && ci.amount > 0) ? ci.amount * 0.5f * ci.src : 0.f;
    const bool syncIn = ci.mode == CM_Sync && ci.amount > 0 && ci.srcWrapped;
    wrapped = false;
    float sum = 0;
    for (int u = 0; u < uni; ++u) {
        const float inc = baseInc * detRatio[(size_t) u];
        double& ph = fph[(size_t) u];
        double before = ph;
        ph += inc;
        bool wrap = false; float frac = 0;
        if (ph >= 1.0) { ph -= std::floor(ph); wrap = true; frac = inc > 0 ? (float) (ph / inc) : 0.f; }
        else if (ph < 0.0) { ph -= std::floor(ph); }
        bool reset = false; float resetFrac = 0;
        if (syncIn) { reset = true; resetFrac = ci.srcFrac; double target = before * (1.0 - ci.amount); ph = target + inc * resetFrac; ph -= std::floor(ph); }
        if (u == 0 && wrap) { wrapped = true; wrapFrac = frac; }
        float v = 0;
        switch (P.engine) {
            case OE_VA:
            case OE_Wavetable: {
                double& sp = slave[(size_t) u];
                const float ratio = P.engine == OE_VA ? std::max(1.f, P.vaSync) : 1.f;
                const double sInc = (double) inc * ratio;
                double sBefore = sp;
                sp += sInc;
                float jump = 0, jfrac = 0; bool jumpEv = false;
                if ((ratio > 1.f && wrap) || reset) {
                    // hard sync: reset slave at the fractional position of the master wrap (polyBLEP-corrected)
                    float fr = reset ? resetFrac : frac;
                    double atEvent = sBefore + sInc * (1.0 - fr);
                    float vb = cycleSample((float) (atEvent - std::floor(atEvent)) + pmOff - std::floor(pmOff));
                    double nsp = reset ? ph - inc * fr : 0.0;
                    nsp -= std::floor(nsp);
                    float va = cycleSample((float) nsp + pmOff - std::floor(pmOff));
                    jump = va - vb; jfrac = fr; jumpEv = true;
                    sp = nsp + sInc * fr;
                }
                sp -= std::floor(sp);
                float rp = (float) sp + pmOff; rp -= std::floor(rp);
                float cur = cycleSample(rp);
                if (jumpEv) {
                    blepPrev[(size_t) u] += 0.5f * jump * jfrac * jfrac;
                    cur += -0.5f * jump * (1.f - jfrac) * (1.f - jfrac);
                }
                v = blepPrev[(size_t) u];
                blepPrev[(size_t) u] = cur;
                break;
            }
            case OE_HyperSaw: {
                const float dens = clampf(P.hsDensity, 0, 1) * 8.f;
                const int nsaw = 1 + (int) std::ceil(dens - 1e-6f);
                float acc = 0, gsum = 0, pre = 0, post = 0;
                const float ratio = std::max(1.f, P.hsSync);
                for (int j = 0; j < nsaw; ++j) {
                    float pos = nsaw == 1 ? 0.f : ((float) j / (nsaw - 1)) * 2.f - 1.f;
                    float det = std::exp2(pos * std::fabs(pos) * P.hsSpread * 60.f / 1200.f);
                    float g = (j == nsaw - 1 && j > 0 && dens < (float) (nsaw - 1)) ? dens - (float) (nsaw - 2) : 1.f;
                    double& sp = hs[(size_t) u][(size_t) j];
                    double sInc = (double) inc * ratio * det;
                    double sBefore = sp;
                    sp += sInc;
                    if ((ratio > 1.f && wrap) || reset) {
                        // hard sync of every saw to the fundamental, polyBLEP-corrected on both sides of the reset
                        float fr = reset ? resetFrac : frac;
                        double atEvent = sBefore + sInc * (1.0 - fr);
                        float vb = 1.f - 2.f * (float) (atEvent - std::floor(atEvent));
                        float jump = 1.f - vb;
                        pre += g * 0.5f * jump * fr * fr;
                        post += -g * 0.5f * jump * (1.f - fr) * (1.f - fr);
                        sp = sInc * fr;
                    }
                    if (sp >= 1.0) sp -= std::floor(sp);
                    float x = (float) sp + pmOff; x -= std::floor(x);
                    float s = 1.f - 2.f * x;
                    s += polyBlep(x, (float) std::min(sInc, 0.5));
                    acc += s * g; gsum += g * g;
                }
                const float norm = 0.6f / std::sqrt(std::max(gsum, 1e-6f));
                v = blepPrev[(size_t) u] + pre * norm;
                blepPrev[(size_t) u] = (acc + post) * norm;
                break;
            }
            case OE_FM: {
                auto& f = fm[(size_t) u];
                const FmAlg& alg = kAlgs[std::clamp(P.fmAlg, 0, 7)];
                float outs[2];
                for (int sub = 0; sub < 2; ++sub) {
                    const float tt = ((float) i + 0.5f * (float) sub) / (float) n;
                    float o[4];
                    for (int k = 3; k >= 0; --k) {
                        const auto& op = P.op[(size_t) k];
                        double opInc = op.fixed ? op.freq / (2.0 * sr) : (double) inc * op.ratio * 0.5;
                        f.ph[(size_t) k] += opInc;
                        f.ph[(size_t) k] -= std::floor(f.ph[(size_t) k]);
                        float env = opEnvStart[(size_t) k] + (opEnvEnd[(size_t) k] - opEnvStart[(size_t) k]) * tt;
                        float mod = 0;
                        for (int s2 = 0; s2 < 4; ++s2) if (alg.mods[k] & (1 << s2)) mod += o[s2];
                        mod += op.fb * f.out[(size_t) k] * 0.35f;
                        float w = fmWave(op.wave, (float) f.ph[(size_t) k] + mod + pmOff);
                        float lvl = op.level * env;
                        f.out[(size_t) k] = w;
                        o[k] = w * lvl * ((alg.carriers & (1 << k)) ? 1.f : 1.2f); // modulation index in cycles (up to ~1.2 = 7.5 rad)
                    }
                    float c = 0; int nc = 0;
                    for (int k = 0; k < 4; ++k) if (alg.carriers & (1 << k)) { c += o[k]; ++nc; }
                    outs[sub] = c / (float) std::max(1, nc) * (nc > 1 ? 1.5f : 1.f);
                }
                v = f.dec.process(outs[0], outs[1]);
                break;
            }
            case OE_GrainFormant: {
                auto& streams = gs[(size_t) u];
                const float shiftRatio = std::exp2(P.gfShift / 12.f);
                float acc = 0;
                for (int sIdx = 0; sIdx < 3; ++sIdx) {
                    float mix = sIdx == 0 ? 1.f : (sIdx == 1 ? P.gfSpread : P.gfDetune);
                    if (mix <= 0) continue;
                    auto& st = streams[(size_t) sIdx];
                    float fInc = sIdx == 0 ? inc : (sIdx == 1 ? inc * 0.5f : inc * 2.f * (1.f + 0.03f * P.gfDetune));
                    float fundHz = fInc * sr;
                    float fg = P.gfMode == 1 ? 261.63f * shiftRatio : fundHz * shiftRatio;
                    if (sIdx == 1) fg *= 1.f + 2.f * P.gfSpread;
                    st.ph += fInc;
                    if (st.ph >= 1.0 || (sIdx == 0 && reset)) {
                        st.ph -= std::floor(st.ph);
                        auto& g = st.g[(size_t) st.next]; st.next = (st.next + 1) % 3;
                        g.age = 0; g.rp = 0;
                        g.len = (P.gfComplex ? 1.f : 2.f) / std::max(fInc, 1e-6f);
                        g.inc = fg / sr;
                    }
                    float sv = 0;
                    for (auto& g : st.g) {
                        if (g.age < 0) continue;
                        float w = 0.5f - 0.5f * std::cos(kTwoPi * g.age / g.len);
                        float rp = (float) g.rp + pmOff; rp -= std::floor(rp);
                        sv += cycleSample(rp) * w;
                        g.rp += g.inc; g.rp -= std::floor(g.rp);
                        g.age += 1.f;
                        if (g.age >= g.len) g.age = -1;
                    }
                    acc += sv * mix * (P.gfComplex ? 1.6f : 1.f);
                }
                if (!P.gfComplex) { subPh[(size_t) u] = ph; acc += 0.35f * std::sin(kTwoPi * (float) ph); }
                v = acc * 0.8f;
                break;
            }
            case OE_Harmonic: {
                auto& h = hv[(size_t) u];
                float acc = 0;
                const float base = kTwoPi * inc;
                (void) base;
                for (int k = 0; k < hCount; ++k) {
                    float a = hAmpS[(size_t) k] + (hAmpE[(size_t) k] - hAmpS[(size_t) k]) * t;
                    float c = h.c[(size_t) k], s = h.s[(size_t) k];
                    acc += a * s;
                    float cw = h.cw[(size_t) k], sw = h.sw[(size_t) k];
                    h.c[(size_t) k] = c * cw - s * sw;
                    h.s[(size_t) k] = s * cw + c * sw;
                }
                v = acc;
                break;
            }
            default: break;
        }
        // chain modes acting on the output
        if (ci.amount > 0) {
            switch (ci.mode) {
                case CM_Add: v += ci.amount * ci.src; break;
                case CM_Ring: v = v * (1.f - ci.amount) + v * ci.src * ci.amount; break;
                case CM_AM: v *= (1.f - ci.amount) + ci.amount * (0.5f + 0.5f * ci.src); break;
                default: break;
            }
        }
        const float g = uniGain[(size_t) u];
        sum += v * g;
        const float l = v * g * panL[(size_t) u], r = v * g * panR[(size_t) u];
        if (bus[(size_t) u] == 0) { o.l1 += l; o.r1 += r; } else { o.l2 += l; o.r2 += r; }
    }
    if (xfade > 0) --xfade;
    raw = sum;
    o.raw = sum;
    return o;
}

} // namespace gnarl
