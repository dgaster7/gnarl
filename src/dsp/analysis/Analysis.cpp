#include "Analysis.h"
#include "util/FFT.h"
#include <algorithm>
#include <cmath>

namespace gnarl::analysis {

static size_t clampTo(const std::vector<float>& x, size_t to) { return std::min(to, x.size()); }

double rms(const std::vector<float>& x, size_t from, size_t to) {
    to = clampTo(x, to);
    if (to <= from) return 0;
    double s = 0;
    for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return std::sqrt(s / (double) (to - from));
}
double peak(const std::vector<float>& x, size_t from, size_t to) {
    to = clampTo(x, to);
    double p = 0;
    for (size_t i = from; i < to; ++i) p = std::max(p, (double) std::fabs(x[i]));
    return p;
}
double diffRms(const std::vector<float>& a, const std::vector<float>& b, int lag, size_t from) {
    double s = 0; size_t cnt = 0;
    for (size_t i = from; i < a.size(); ++i) {
        long j = (long) i - lag;
        if (j < 0 || j >= (long) b.size()) continue;
        double d = (double) a[i] - b[(size_t) j];
        s += d * d; ++cnt;
    }
    return cnt ? std::sqrt(s / (double) cnt) : 0;
}
double maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b, int lag, size_t from) {
    double m = 0;
    for (size_t i = from; i < a.size(); ++i) {
        long j = (long) i - lag;
        if (j < 0 || j >= (long) b.size()) continue;
        m = std::max(m, std::fabs((double) a[i] - b[(size_t) j]));
    }
    return m;
}
double dc(const std::vector<float>& x, size_t from, size_t to) {
    // Blackman-Harris weighted mean: a plain mean over a non-integer number of cycles leaks the
    // waveform itself into the estimate (about -60 dB for a 65 Hz bass over 2 s); this does not.
    to = clampTo(x, to);
    if (to <= from + 1) return 0;
    const double n = (double) (to - from - 1);
    double s = 0, w = 0;
    for (size_t i = from; i < to; ++i) {
        double t = (double) (i - from) / n;
        double wi = 0.35875 - 0.48829 * std::cos(2 * M_PI * t) + 0.14128 * std::cos(4 * M_PI * t) - 0.01168 * std::cos(6 * M_PI * t);
        s += wi * x[i]; w += wi;
    }
    return s / w;
}

std::vector<double> spectrumDb(const std::vector<float>& x, double sr, int order, size_t from, size_t to) {
    to = clampTo(x, to);
    const int n = 1 << order;
    RealFFT fft(order);
    std::vector<float> frame((size_t) n), re((size_t) n / 2 + 1), im((size_t) n / 2 + 1);
    std::vector<double> acc((size_t) n / 2 + 1, 0);
    int frames = 0;
    for (size_t start = from; start + (size_t) n <= to; start += (size_t) n / 2) {
        for (int i = 0; i < n; ++i) frame[(size_t) i] = x[start + (size_t) i] * (float) (0.5 - 0.5 * std::cos(2 * M_PI * i / n));
        fft.forward(frame.data(), re.data(), im.data());
        for (int k = 0; k <= n / 2; ++k) acc[(size_t) k] += (double) re[(size_t) k] * re[(size_t) k] + (double) im[(size_t) k] * im[(size_t) k];
        ++frames;
    }
    (void) sr;
    for (auto& a : acc) a = 10 * std::log10(a / std::max(frames, 1) + 1e-30);
    return acc;
}

static std::vector<double> powerSpectrum(const std::vector<float>& x, int order, size_t from, size_t to) {
    to = clampTo(x, to);
    const int n = 1 << order;
    RealFFT fft(order);
    std::vector<float> frame((size_t) n), re((size_t) n / 2 + 1), im((size_t) n / 2 + 1);
    std::vector<double> acc((size_t) n / 2 + 1, 0);
    for (size_t start = from; start + (size_t) n <= to; start += (size_t) n / 2) {
        // Blackman-Harris window for high dynamic range
        for (int i = 0; i < n; ++i) {
            double w = 0.35875 - 0.48829 * std::cos(2 * M_PI * i / n) + 0.14128 * std::cos(4 * M_PI * i / n) - 0.01168 * std::cos(6 * M_PI * i / n);
            frame[(size_t) i] = (float) (x[start + (size_t) i] * w);
        }
        fft.forward(frame.data(), re.data(), im.data());
        for (int k = 0; k <= n / 2; ++k) acc[(size_t) k] += (double) re[(size_t) k] * re[(size_t) k] + (double) im[(size_t) k] * im[(size_t) k];
    }
    return acc;
}

double bandEnergyDb(const std::vector<float>& x, double sr, double lo, double hi, size_t from, size_t to) {
    auto p = powerSpectrum(x, 13, from, to);
    const int n = 1 << 13;
    double s = 0;
    for (size_t k = 0; k < p.size(); ++k) { double f = (double) k * sr / n; if (f >= lo && f < hi) s += p[k]; }
    return 10 * std::log10(s + 1e-30);
}

double energyAboveDb(const std::vector<float>& x, double sr, double hz, size_t from, size_t to) {
    auto p = powerSpectrum(x, 14, from, to);
    const int n = 1 << 14;
    double tot = 0, above = 0;
    for (size_t k = 1; k < p.size(); ++k) { tot += p[k]; if ((double) k * sr / n > hz) above += p[k]; }
    return 10 * std::log10((above + 1e-30) / (tot + 1e-30));
}

double nonHarmonicDb(const std::vector<float>& x, double sr, double f0, size_t from, size_t to) {
    auto p = powerSpectrum(x, 15, from, to);
    const int n = 1 << 15;
    const double binHz = sr / n;
    double tot = 0, off = 0;
    for (size_t k = 1; k < p.size(); ++k) {
        double f = (double) k * binHz;
        double h = f / f0;
        double dist = std::fabs(h - std::round(h)) * f0; // Hz from nearest harmonic
        tot += p[k];
        if (dist > 6 * binHz) off += p[k];
    }
    return 10 * std::log10((off + 1e-30) / (tot + 1e-30));
}

double inharmonicEnergyDb(const std::vector<float>& x, double sr, double f0, size_t from, size_t to) { return nonHarmonicDb(x, sr, f0, from, to); }

double maxHfBurstDb(const std::vector<float>& x, double sr, size_t from, size_t to) {
    to = clampTo(x, to);
    // 4th-order high-pass at 8 kHz via two cascaded RBJ biquads
    double w = 2 * M_PI * 8000 / sr, cw = std::cos(w), al = std::sin(w) / (2 * 0.7071);
    double b0 = (1 + cw) / 2, b1 = -(1 + cw), b2 = (1 + cw) / 2, a0 = 1 + al, a1 = -2 * cw, a2 = 1 - al;
    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    double s1[4] = {}, s2[4] = {};
    double best = 0, acc = 0; int cnt = 0;
    for (size_t i = 0; i < to; ++i) {
        double v = x[i];
        double y = b0 * v + b1 * s1[0] + b2 * s1[1] - a1 * s1[2] - a2 * s1[3];
        s1[1] = s1[0]; s1[0] = v; s1[3] = s1[2]; s1[2] = y;
        double z = b0 * y + b1 * s2[0] + b2 * s2[1] - a1 * s2[2] - a2 * s2[3];
        s2[1] = s2[0]; s2[0] = y; s2[3] = s2[2]; s2[2] = z;
        if (i < from) continue;
        acc += z * z;
        if (++cnt == 64) { best = std::max(best, acc / 64); acc = 0; cnt = 0; }
    }
    return 10 * std::log10(best + 1e-30);
}

double maxSecondDiff(const std::vector<float>& x, size_t from, size_t to) {
    to = clampTo(x, to);
    double m = 0;
    for (size_t i = std::max<size_t>(from, 2); i < to; ++i) m = std::max(m, std::fabs((double) x[i] - 2.0 * x[i - 1] + x[i - 2]));
    return m;
}

double loudnessLufs(const std::vector<float>& L, const std::vector<float>& R, double sr) {
    // K-weighting: high shelf + high pass (BS.1770 coefficients recomputed for sr)
    auto kfilter = [&](const std::vector<float>& x) {
        double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        double K = std::tan(M_PI * f0 / sr), Vh = std::pow(10.0, G / 20), Vb = std::pow(Vh, 0.4996667741545416);
        double a0 = 1 + K / Q + K * K;
        double b0 = (Vh + Vb * K / Q + K * K) / a0, b1 = 2 * (K * K - Vh) / a0, b2 = (Vh - Vb * K / Q + K * K) / a0;
        double a1 = 2 * (K * K - 1) / a0, a2 = (1 - K / Q + K * K) / a0;
        double f1 = 38.13547087602444, Q1 = 0.5003270373238773, K1 = std::tan(M_PI * f1 / sr);
        double c1 = 2 * (K1 * K1 - 1) / (1 + K1 / Q1 + K1 * K1), c2 = (1 - K1 / Q1 + K1 * K1) / (1 + K1 / Q1 + K1 * K1);
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, z1 = 0, z2 = 0, sum = 0;
        for (float v : x) {
            double y = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = v; y2 = y1; y1 = y;
            double z = y - 2 * u1 + u2 - c1 * z1 - c2 * z2; u2 = u1; u1 = y; z2 = z1; z1 = z;
            sum += z * z;
        }
        return sum / std::max<size_t>(x.size(), 1);
    };
    double ms = kfilter(L) + kfilter(R);
    return -0.691 + 10 * std::log10(ms + 1e-30);
}

double lowCorrelation(const std::vector<float>& L, const std::vector<float>& R, double sr, double hz) {
    double a = std::exp(-2 * M_PI * hz / sr);
    double l1 = 0, l2 = 0, r1 = 0, r2 = 0, slr = 0, sll = 0, srr = 0;
    for (size_t i = 0; i < L.size(); ++i) {
        l1 = (1 - a) * L[i] + a * l1; l2 = (1 - a) * l1 + a * l2;
        r1 = (1 - a) * R[i] + a * r1; r2 = (1 - a) * r1 + a * r2;
        slr += l2 * r2; sll += l2 * l2; srr += r2 * r2;
    }
    if (sll < 1e-12 && srr < 1e-12) return 1;
    return slr / std::sqrt(sll * srr + 1e-30);
}

std::vector<float> fingerprint(const std::vector<float>& L, double sr) {
    const int order = 11, n = 1 << order;
    RealFFT fft(order);
    std::vector<float> frame((size_t) n), re((size_t) n / 2 + 1), im((size_t) n / 2 + 1), out;
    for (size_t start = 0; start + (size_t) n <= L.size(); start += (size_t) n) {
        for (int i = 0; i < n; ++i) frame[(size_t) i] = L[start + (size_t) i] * (float) (0.5 - 0.5 * std::cos(2 * M_PI * i / n));
        fft.forward(frame.data(), re.data(), im.data());
        float bands[24] = {};
        for (int k = 1; k <= n / 2; ++k) {
            double f = (double) k * sr / n;
            int b = (int) std::floor(std::log2(std::max(f, 20.0) / 20.0) * 2.4);
            if (b < 0 || b >= 24) continue;
            bands[b] += re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
        }
        for (float& b : bands) out.push_back((float) (10 * std::log10(b + 1e-12)));
    }
    // normalise to max 0
    float mx = -1e9f; for (float v : out) mx = std::max(mx, v);
    for (float& v : out) v = std::max(v - mx, -80.f);
    return out;
}

double fingerprintDistance(const std::vector<float>& a, const std::vector<float>& b) {
    size_t n = std::min(a.size(), b.size());
    if (!n) return 1e9;
    double s = 0;
    for (size_t i = 0; i < n; ++i) s += std::fabs(a[i] - b[i]);
    return s / (double) n;
}

size_t lastAbove(const std::vector<float>& L, const std::vector<float>& R, double thresholdDb) {
    double th = std::pow(10.0, thresholdDb / 20);
    for (size_t i = L.size(); i-- > 0;) if (std::fabs(L[i]) > th || std::fabs(R[i]) > th) return i;
    return 0;
}

} // namespace gnarl::analysis
