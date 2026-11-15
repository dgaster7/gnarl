#include "Wavetable.h"
#include "util/FFT.h"
#include "util/Math.h"
#include <cstring>
#include <functional>
#include <mutex>

namespace gnarl {

namespace {

constexpr int kFactoryFrames = 32;
constexpr int H = kCycle / 2; // max harmonic

struct Gen {
    RealFFT fft{11};
    std::vector<float> re = std::vector<float>(H + 1), im = std::vector<float>(H + 1), buf = std::vector<float>(kCycle);

    // amps/phases indexed by harmonic 1..H (sine phase convention)
    void harmonics(const std::function<float(int)>& amp, const std::function<float(int)>& phase, float* out) {
        std::fill(re.begin(), re.end(), 0.f); std::fill(im.begin(), im.end(), 0.f);
        for (int h = 1; h < H; ++h) {
            float a = amp(h);
            if (a == 0) continue;
            float ph = phase ? phase(h) : 0.f;
            // sin(wt + ph) = Im(e^{i(wt+ph)}) -> DFT bin: X[h] = (N/2) * (-i) e^{i ph}
            float m = a * (float) kCycle * 0.5f;
            re[(size_t) h] = m * std::sin(ph);
            im[(size_t) h] = -m * std::cos(ph);
        }
        fft.inverse(re.data(), im.data(), out);
    }
    void function(const std::function<float(float)>& f, float* out) {
        for (int i = 0; i < kCycle; ++i) out[i] = f((float) i / kCycle);
    }
};

void normalise(float* x) {
    double dc = 0; for (int i = 0; i < kCycle; ++i) dc += x[i];
    dc /= kCycle;
    float pk = 0; for (int i = 0; i < kCycle; ++i) { x[i] -= (float) dc; pk = std::max(pk, std::fabs(x[i])); }
    if (pk > 1e-9f) for (int i = 0; i < kCycle; ++i) x[i] /= pk;
}

float saw(float t) { return 1.f - 2.f * t; }
float tri(float t) { return t < 0.25f ? 4 * t : (t < 0.75f ? 2 - 4 * t : 4 * t - 4); }
float sq(float t, float w = 0.5f) { return t < w ? 1.f : -1.f; }
float formantGain(float hz, float f1, float f2, float f3) {
    auto pk = [](float f, float c, float bw) { float d = (f - c) / bw; return 1.f / (1.f + d * d); };
    return 0.05f + pk(hz, f1, 90) + 0.6f * pk(hz, f2, 120) + 0.3f * pk(hz, f3, 160);
}
const float kVow[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
void vowelAt(float p, float& f1, float& f2, float& f3) {
    float v = clampf(p, 0, 1) * 4; int i = std::min(3, (int) v); float t = v - (float) i;
    f1 = lerp(kVow[i][0], kVow[i + 1][0], t); f2 = lerp(kVow[i][1], kVow[i + 1][1], t); f3 = lerp(kVow[i][2], kVow[i + 1][2], t);
}
float fold(float v) { float t = v * 0.25f + 0.25f; t -= std::floor(t); return 1.f - 4.f * std::fabs(t - 0.5f); }

using FrameFn = std::function<void(Gen&, float p, float* out)>;

std::vector<std::pair<std::string, FrameFn>> generators() {
    std::vector<std::pair<std::string, FrameFn>> g;
    auto F = [&](const char* n, std::function<float(float p, float t)> f) {
        g.push_back({n, [f](Gen& G, float p, float* o) { G.function([&](float t) { return f(p, t); }, o); }});
    };
    auto S = [&](const char* n, std::function<float(float p, int h)> a, std::function<float(float p, int h)> ph = nullptr) {
        g.push_back({n, [a, ph](Gen& G, float p, float* o) {
            G.harmonics([&](int h) { return a(p, h); }, ph ? std::function<float(int)>([&](int h) { return ph(p, h); }) : nullptr, o);
        }});
    };
    const float f0ref = 110.f; // reference pitch used to place formants in harmonic space
    F("Basic Shapes", [](float p, float t) {
        float s = p * 4;
        float a = std::sin(kTwoPi * t), b = tri(t), c = saw(t), d = sq(t), e = sq(t, 0.15f);
        if (s < 1) return lerp(a, b, s); if (s < 2) return lerp(b, c, s - 1); if (s < 3) return lerp(c, d, s - 2); return lerp(d, e, s - 3);
    });
    S("Saw Opening", [](float p, int h) { float c = 1 + p * p * 200; return (1.f / h) / (1.f + std::pow(h / c, 4.f)); });
    S("Square Opening", [](float p, int h) { if (!(h & 1)) return 0.f; float c = 1 + p * p * 200; return (1.f / h) / (1.f + std::pow(h / c, 4.f)); });
    F("PWM", [](float p, float t) { return sq(t, 0.5f - p * 0.48f); });
    S("Vowels AEIOU", [f0ref](float p, int h) { float a, b, c; vowelAt(p, a, b, c); return (1.f / std::sqrt((float) h)) * formantGain(h * f0ref, a, b, c); });
    S("Formant Sweep", [f0ref](float p, int h) { float c = 200 + p * p * 4000; return (1.f / h) * (0.1f + 8.f / (1.f + std::pow((h * f0ref - c) / (c * 0.12f), 2.f))); });
    S("Dual Formant", [f0ref](float p, int h) {
        float c1 = 300 + p * 900, c2 = 2600 - p * 1500;
        auto pk = [](float f, float c) { float d = (f - c) / (c * 0.1f); return 1.f / (1 + d * d); };
        return (1.f / std::sqrt((float) h)) * (0.05f + pk(h * f0ref, c1) + 0.7f * pk(h * f0ref, c2));
    });
    S("Ya Wa", [f0ref](float p, int h) { float a, b, c; vowelAt(p < 0.5f ? 2 * p * 0.25f + 0.5f : (1 - p) * 0.5f + 0.75f, a, b, c); return formantGain(h * f0ref, a, b, c) / h; });
    S("Grain Voice", [](float p, int h) { float a, b, c; vowelAt(p, a, b, c); return formantGain(h * 55.f, a, b, c) / std::pow((float) h, 0.3f); });
    F("FM 1:1", [](float p, float t) { return std::sin(kTwoPi * t + p * 6 * std::sin(kTwoPi * t)); });
    F("FM 1:2", [](float p, float t) { return std::sin(kTwoPi * t + p * 5 * std::sin(2 * kTwoPi * t)); });
    F("FM 1:3", [](float p, float t) { return std::sin(kTwoPi * t + p * 4 * std::sin(3 * kTwoPi * t)); });
    F("FM 2:1", [](float p, float t) { return std::sin(2 * kTwoPi * t + p * 5 * std::sin(kTwoPi * t)); });
    F("FM 3:2", [](float p, float t) { return std::sin(3 * kTwoPi * t + p * 4 * std::sin(2 * kTwoPi * t)); });
    F("FM Feedback", [](float p, float t) {
        float y = 0; for (int k = 0; k < 6; ++k) y = std::sin(kTwoPi * t + p * 1.6f * y); return y;
    });
    F("FM Stack", [](float p, float t) { float m2 = std::sin(4 * kTwoPi * t) * p * 3; float m1 = std::sin(2 * kTwoPi * t + m2) * p * 4; return std::sin(kTwoPi * t + m1); });
    F("Folded Sine", [](float p, float t) { return fold(std::sin(kTwoPi * t) * (1 + p * 7)); });
    F("Folded Triangle", [](float p, float t) { return fold(tri(t) * (1 + p * 7)); });
    F("Asym Fold", [](float p, float t) { return fold(std::sin(kTwoPi * t) * (1 + p * 6) + p * 0.7f); });
    F("Sine Fold", [](float p, float t) { return std::sin(std::sin(kTwoPi * t) * (1 + p * 9) * kPi * 0.5f); });
    F("Sync Sine", [](float p, float t) { return std::sin(kTwoPi * t * (1 + p * 7)); });
    F("Sync Saw", [](float p, float t) { float x = t * (1 + p * 7); return saw(x - std::floor(x)); });
    F("Sync Square", [](float p, float t) { float x = t * (1 + p * 7); return sq(x - std::floor(x)); });
    F("Phase Distort", [](float p, float t) { float k = 0.5f - p * 0.48f; float ph = t < k ? t * 0.5f / k : 0.5f + (t - k) * 0.5f / (1 - k); return std::cos(kTwoPi * ph); });
    F("Reso Saw", [](float p, float t) { return std::cos(kTwoPi * t * (1 + p * 15)) * (1 - t); });
    F("Reso Pulse", [](float p, float t) { float w = t < 0.5f ? 1 - 2 * t : 2 * t - 1; return std::cos(kTwoPi * t * (1 + p * 15)) * w; });
    F("Reso Tri", [](float p, float t) { float w = 1 - 2 * std::fabs(t - 0.5f); return std::sin(kTwoPi * t * (1 + p * 15)) * w; });
    S("Harmonic Comb", [](float p, int h) { float period = 2 + p * 10; return (1.f / h) * (0.5f + 0.5f * std::cos(kTwoPi * h / period)); });
    S("Bell", [](float p, int h) { const int set[] = {1, 2, 3, 5, 7, 11, 13, 17}; for (int s : set) if (h == s) return 1.f / (1 + 0.3f * s * (1 - p)); return 0.f; });
    S("Drawbars", [](float p, int h) { const int bars[] = {1, 2, 3, 4, 6, 8, 10, 12, 16}; for (int i = 0; i < 9; ++i) if (h == bars[i]) return 0.5f + 0.5f * std::sin(kTwoPi * (p + i * 0.17f)); return 0.f; });
    S("Even Rise", [](float p, int h) { return (h & 1) ? 1.f / h : p * 2.f / h; });
    S("Tilt Sweep", [](float p, int h) { return std::pow((float) h, -2.f + p * 2.f); });
    S("Noisy Spectrum", [](float p, int h) { Rng r((uint32_t) h * 2654435761u + (uint32_t) (p * 31) * 97); return r.uni() / std::sqrt((float) h); },
      [](float p, int h) { Rng r((uint32_t) h * 40503u + 7); return r.uni() * kTwoPi; });
    S("Cluster", [](float p, int h) { float c = 20 + p * 200; float d = (h - c) / (6 + p * 20); return std::exp(-d * d) + (h == 1 ? 0.6f : 0.f); });
    S("Prime Metal", [](float p, int h) { auto prime = [](int n) { if (n < 2) return false; for (int d = 2; d * d <= n; ++d) if (n % d == 0) return false; return true; }; return (prime(h) || h == 1) ? std::pow((float) h, -0.6f - p * 0.8f) : 0.f; });
    F("Crushed Saw", [](float p, float t) { float lv = std::exp2(8 - p * 7); return std::round(saw(t) * lv) / lv; });
    F("Ringing Square", [](float p, float t) { return sq(t) + 0.6f * p * std::sin(kTwoPi * t * 9) * std::exp(-std::fmod(t, 0.5f) * 10); });
    S("Growl", [f0ref](float p, int h) { float a, b, c; vowelAt(0.5f + 0.5f * std::sin(kTwoPi * p), a, b, c); return (1.f / h) * (0.2f + formantGain(h * f0ref * 0.5f, a, b, c)); });
    S("Phase Spread Saw", [](float, int h) { return 1.f / h; }, [](float p, int h) { Rng r((uint32_t) h * 7919u + 1); return p * r.bi() * kPi; });
    S("Squelch", [](float p, int h) { float c = 2 + p * p * 120; float q = 8; float d = (h - c) / (c / q); return (1.f / h) / (1 + std::pow(h / (c * 1.05f), 6.f)) + 1.2f / h / (1 + d * d) * 3; });
    S("Wow", [f0ref](float p, int h) { float a, b, c; vowelAt(p < 0.5f ? 1 - 2 * p * 0.25f : 0.75f + (p - 0.5f) * 0.5f, a, b, c); return formantGain(h * f0ref, a, b, c) / std::pow((float) h, 0.7f); });
    F("Window Stack", [](float p, float t) { float w = std::sin(kPi * t); return std::sin(kTwoPi * t * (1 + std::floor(p * 8))) * w; });
    F("Tanh Sine", [](float p, float t) { float g = 1 + p * 20; return std::tanh(std::sin(kTwoPi * t) * g) / std::tanh(g); });
    F("Rectified", [](float p, float t) { float s = std::sin(kTwoPi * t); return lerp(s, std::fabs(s) * 2 - 1, p); });
    F("Chebyshev", [](float p, float t) { float x = std::cos(kTwoPi * t); int n = 1 + (int) (p * 7.99f); return std::cos(n * std::acos(clampf(x, -1, 1))); });
    F("Pulse Train", [](float p, float t) { float w = 0.25f - p * 0.24f; return t < w ? std::sin(kPi * t / w) : 0.f; });
    F("Tri To Square", [](float p, float t) { return clampf(tri(t) * (1 + p * 20), -1, 1); });
    F("Staircase", [](float p, float t) { int steps = 2 + (int) (p * 14); return std::floor(t * steps) / (steps - 1) * 2 - 1; });
    F("Exp Pluck", [](float p, float t) { return std::exp(-t * (1 + p * 12)) * 2 - 1; });
    S("Oo Ee", [f0ref](float p, int h) { float a, b, c; vowelAt(p < 0.5f ? 1.0f - p * 1.2f : 0.4f + (p - 0.5f) * 0.4f, a, b, c); return formantGain(h * f0ref, a, b, c) / h; });
    S("Band Noise", [](float p, int h) { Rng r((uint32_t) h * 3266489917u + (uint32_t) (p * 31)); float c = 10 + p * 300; float d = (h - c) / (c * 0.4f); return r.uni() / (1 + d * d); },
      [](float, int h) { Rng r((uint32_t) h * 668265263u); return r.uni() * kTwoPi; });
    F("Dirty Square", [](float p, float t) { return clampf((sq(t) + 0.3f * std::sin(kTwoPi * t * 3)) * (1 + p * 3), -1, 1) + p * 0.3f * saw(t); });
    S("Sub Octave", [](float p, int h) { return h == 1 ? 1.f : (h == 2 ? p : (h == 3 ? p * 0.5f : 0.f)); });
    F("Bitwise", [](float p, float t) { int a = (int) (t * 256); int b = (int) (p * 255); return ((a ^ b) & 0xff) / 127.5f - 1; });
    F("Half Sine Sweep", [](float p, float t) { float s = std::sin(kTwoPi * t); return s > 0 ? s : s * (1 - p * 2); });
    F("Saw Bend", [](float p, float t) { float k = 1 + p * 6; return saw(std::pow(t, k)); });
    F("Sine Bend", [](float p, float t) { float k = std::exp2((p - 0.5f) * 5); return std::sin(kTwoPi * std::pow(t, k)); });
    S("Odd Formant", [f0ref](float p, int h) { if (!(h & 1)) return 0.f; float a, b, c; vowelAt(p, a, b, c); return formantGain(h * f0ref, a, b, c) / std::sqrt((float) h); });
    F("Twin Peaks", [](float p, float t) { return std::sin(kTwoPi * t) * 0.6f + std::sin(kTwoPi * t * (2 + std::floor(p * 10))) * 0.4f; });
    F("Mirror Saw", [](float p, float t) { float x = t < 0.5f ? t * 2 : 2 - t * 2; return std::pow(x, 1 + p * 5) * 2 - 1; });
    F("Quantized Sine", [](float p, float t) { int lv = 2 + (int) ((1 - p) * 30); return std::round(std::sin(kTwoPi * t) * lv) / lv; });
    F("Pinch", [](float p, float t) { float s = std::sin(kTwoPi * t); return s * std::pow(std::fabs(s), p * 6); });
    F("Spike", [](float p, float t) { float d = t - 0.5f; return std::exp(-d * d * (20 + p * 3000)) * 2 - 0.5f; });
    S("Vocal Sweep Odd/Even", [f0ref](float p, int h) { float a, b, c; vowelAt(p, a, b, c); return formantGain(h * f0ref, a, b, c) * ((h & 1) ? 1.f : 1 - p) / h; });
    F("Chaos Saw", [](float p, float t) { Rng r(1234); float y = 0; for (int k = 1; k <= 6; ++k) y += std::sin(kTwoPi * t * k + r.uni() * kTwoPi * p) / k; return y; });
    F("Noise Cycle", [](float p, float t) { Rng r((uint32_t) (t * 2048) * 2246822519u + (uint32_t) (p * 31) * 3266489917u); return r.bi() * p + std::sin(kTwoPi * t) * (1 - p); });
    return g;
}

std::once_flag gOnce;
std::vector<std::shared_ptr<const WavetableData>> gTables;

void buildFactory() {
    Gen G;
    auto gens = generators();
    for (auto& [name, fn] : gens) {
        auto t = std::make_shared<WavetableData>();
        t->name = name; t->frames = kFactoryFrames;
        t->data.resize((size_t) kFactoryFrames * kCycle);
        for (int f = 0; f < kFactoryFrames; ++f) {
            float* o = t->data.data() + (size_t) f * kCycle;
            fn(G, (float) f / (kFactoryFrames - 1), o);
            normalise(o);
        }
        gTables.push_back(t);
        if (gTables.size() == 64) break;
    }
}
} // namespace

const std::vector<std::shared_ptr<const WavetableData>>& factoryTables() {
    std::call_once(gOnce, buildFactory);
    return gTables;
}
int numFactoryTables() { return (int) factoryTables().size(); }

// ---------------- WAV I/O ----------------
std::shared_ptr<WavetableData> loadWavetableWav(const std::vector<unsigned char>& b, const std::string& name, std::string* err) {
    auto fail = [&](const char* m) { if (err) *err = m; return std::shared_ptr<WavetableData>(); };
    if (b.size() < 44 || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4)) return fail("not a WAV file");
    auto u16 = [&](size_t o) { return (unsigned) b[o] | ((unsigned) b[o + 1] << 8); };
    auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
    unsigned fmt = 0, chans = 0, bits = 0; size_t dataOff = 0, dataLen = 0; int frameSize = kCycle;
    for (size_t o = 12; o + 8 <= b.size();) {
        unsigned len = u32(o + 4);
        if (!std::memcmp(b.data() + o, "fmt ", 4) && o + 24 <= b.size()) { fmt = u16(o + 8); chans = u16(o + 10); bits = u16(o + 22); if (fmt == 0xFFFE && o + 34 <= b.size()) fmt = u16(o + 32); }
        else if (!std::memcmp(b.data() + o, "data", 4)) { dataOff = o + 8; dataLen = std::min<size_t>(len, b.size() - dataOff); }
        else if (!std::memcmp(b.data() + o, "clm ", 4) && len >= 7) {
            std::string s((const char*) b.data() + o + 8, std::min<size_t>(len, 16));
            auto p = s.find("<!>");
            if (p != std::string::npos) { int fs = std::atoi(s.c_str() + p + 3); if (fs >= 64 && fs <= 8192) frameSize = fs; }
        }
        o += 8 + len + (len & 1);
    }
    if (!dataOff || !chans) return fail("missing fmt/data chunk");
    if (!((fmt == 1 && (bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && bits == 32))) return fail("unsupported sample format");
    const size_t bps = bits / 8, n = dataLen / (bps * chans);
    std::vector<float> mono(n);
    for (size_t i = 0; i < n; ++i) {
        size_t o = dataOff + i * bps * chans; float v = 0;
        if (fmt == 3) std::memcpy(&v, &b[o], 4);
        else if (bits == 16) v = (float) (int16_t) u16(o) / 32768.f;
        else if (bits == 24) { int32_t x = (int32_t) ((b[o] << 8) | (b[o + 1] << 16) | (b[o + 2] << 24)); v = (float) x / 2147483648.f; }
        else { int32_t x = (int32_t) u32(o); v = (float) x / 2147483648.f; }
        mono[i] = v;
    }
    if (n < (size_t) frameSize) frameSize = (int) n; // single short cycle: resample below
    int frames = (int) std::min<size_t>(kMaxFrames, n / (size_t) frameSize);
    if (frames < 1) return fail("file too short");
    auto t = std::make_shared<WavetableData>();
    t->name = name; t->frames = frames; t->data.resize((size_t) frames * kCycle);
    for (int f = 0; f < frames; ++f) {
        float* o = t->data.data() + (size_t) f * kCycle;
        const float* src = mono.data() + (size_t) f * frameSize;
        for (int i = 0; i < kCycle; ++i) {
            float pos = (float) i * frameSize / kCycle; int i0 = (int) pos; float fr = pos - i0;
            float a = src[i0 % frameSize], c = src[(i0 + 1) % frameSize];
            o[i] = a + (c - a) * fr;
        }
        normalise(o);
    }
    return t;
}

std::vector<unsigned char> saveWavetableWav(const WavetableData& t) {
    std::vector<unsigned char> o;
    auto w32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) o.push_back((unsigned char) (v >> (8 * i))); };
    auto w16 = [&](uint16_t v) { o.push_back((unsigned char) v); o.push_back((unsigned char) (v >> 8)); };
    const uint32_t n = (uint32_t) t.data.size();
    const char clm[] = "<!>2048 00000000 wavetable GNARL";
    const uint32_t clmLen = (uint32_t) sizeof(clm) - 1 + ((sizeof(clm) - 1) & 1);
    o.insert(o.end(), {'R', 'I', 'F', 'F'}); w32(4 + 24 + 8 + clmLen + 8 + n * 4); o.insert(o.end(), {'W', 'A', 'V', 'E'});
    o.insert(o.end(), {'f', 'm', 't', ' '}); w32(16); w16(3); w16(1); w32(48000); w32(48000 * 4); w16(4); w16(32);
    o.insert(o.end(), {'c', 'l', 'm', ' '}); w32(clmLen); for (size_t i = 0; i < clmLen; ++i) o.push_back(i < sizeof(clm) - 1 ? (unsigned char) clm[i] : 0);
    o.insert(o.end(), {'d', 'a', 't', 'a'}); w32(n * 4);
    for (float v : t.data) { uint32_t u; std::memcpy(&u, &v, 4); w32(u); }
    return o;
}

} // namespace gnarl
