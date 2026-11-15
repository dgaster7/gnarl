#pragma once
// Distortion transfer functions shared by the between-filter slot, per-filter drive, post drive and FX.
#include "params/Registry.h"
#include "util/Math.h"

namespace gnarl {

struct DistState {
    float hold = 0, phase = 1; // sample-rate reduction
    DcBlocker dc;
    void reset() { hold = 0; phase = 1; dc.reset(); }
};

struct DrawTables { const float* a = nullptr; const float* b = nullptr; int size = 1024; };

// drive01: 0..1 (0 = identity, enforced by the caller crossfade), bias: -1..1, rate: sample rate the stage runs at.
inline float drawLookup(const DrawTables& d, float v, float morph) {
    if (!d.a) return clampf(v, -1, 1);
    float pos = (clampf(v, -1.f, 1.f) + 1.f) * 0.5f * (float) d.size;
    int i = (int) pos; if (i >= d.size) i = d.size - 1;
    float f = pos - (float) i;
    float a = d.a[i] + (d.a[i + 1] - d.a[i]) * f;
    float b = d.b[i] + (d.b[i + 1] - d.b[i]) * f;
    return a + (b - a) * morph;
}

inline float distortSample(int type, float x, float drive01, float bias, DistState& st, float rate, const DrawTables& draw, float morph) {
    const float g = std::exp2(drive01 * 6.f); // up to +36 dB
    float v = x * g + bias;
    float y;
    switch (type) {
        default:
        case DT_Soft: y = fastTanh(v); break;
        case DT_Hard: y = clampf(v, -1.f, 1.f); break;
        case DT_Tube: y = fastTanh(v + 0.3f) - 0.2913126f; break; // asymmetric, tanh(0.3)
        case DT_Valve: { float a = v / (1.f + std::fabs(v)); y = a + 0.2f * a * a; break; }
        case DT_Diode: y = v > 0 ? 1.f - std::exp(-v) : -0.6f * (1.f - std::exp(0.6f * v)); break;
        case DT_Fold: {
            float t = v * 0.25f + 0.25f;          // triangle fold into -1..1
            t = t - std::floor(t);
            y = 4.f * std::fabs(t - 0.5f) - 1.f;
            y = -y;
            break;
        }
        case DT_SineFold: y = std::sin(v * kPi * 0.5f); break;
        case DT_Rectify: y = fastTanh(std::fabs(v)) * 2.f - 1.f + (1.f - drive01) * 0.f; break;
        case DT_Fuzz: { float w = v + 0.4f * bias; y = (w > 0 ? 1.f : -1.f) * (1.f - std::exp(-std::fabs(w) * 2.f)); break; }
        case DT_Bits: {
            float bits = 16.f - drive01 * 15.f;   // 16 .. 1 bit
            float levels = std::exp2(bits - 1.f);
            y = std::round(clampf(x + bias * 0.5f, -1.f, 1.f) * levels) / levels;
            break;
        }
        case DT_Downsample: {
            // target rate from the stage rate down to ~200 Hz
            float target = rate * std::exp2(-drive01 * std::log2(rate / 200.f));
            st.phase += target / rate;
            if (st.phase >= 1.f) { st.phase -= std::floor(st.phase); st.hold = x + bias * 0.5f; }
            y = st.hold;
            break;
        }
        case DT_Draw: y = drawLookup(draw, v, morph); break;
    }
    return y;
}

// Every active distortion stage DC-blocks its output: asymmetric curves make DC directly, and even a
// symmetric curve makes DC from a signal whose amplitude distribution is not symmetric (e.g. a phased saw).
inline bool distNeedsDcBlock(int, float) { return true; }

// Crossfade so that drive 0 is exactly the identity and the effect fades in continuously.
inline float driveBlend(float drive01) { return clampf(drive01 * 8.f, 0.f, 1.f); }

} // namespace gnarl
