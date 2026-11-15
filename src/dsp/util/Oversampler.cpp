#include "Oversampler.h"
#include <cmath>
#include <cstring>

namespace gnarl {

// ---------------- IIR half-band coefficient design (elliptic, after Laurent de Soras' HIIR) ----------------
namespace {
double ipowp(double x, long n) {
    double z = 1;
    while (n != 0) { if (n & 1) z *= x; n >>= 1; x *= x; }
    return z;
}
void transitionParam(double& k, double& q, double transition) {
    k = std::tan((1 - transition * 2) * M_PI / 4);
    k *= k;
    const double kksqrt = std::pow(1 - k * k, 0.25);
    const double e = 0.5 * (1 - kksqrt) / (1 + kksqrt);
    const double e2 = e * e, e4 = e2 * e2;
    q = e * (1 + e4 * (2 + e4 * (15 + 150 * e4)));
}
double accNum(double q, int order, int c) {
    int i = 0, j = 1; double acc = 0, q_ii1;
    do {
        q_ii1 = ipowp(q, i * (i + 1));
        q_ii1 *= std::sin((i * 2 + 1) * c * M_PI / order) * j;
        acc += q_ii1; j = -j; ++i;
    } while (std::fabs(q_ii1) > 1e-100);
    return acc;
}
double accDen(double q, int order, int c) {
    int i = 1, j = -1; double acc = 0, q_i2;
    do {
        q_i2 = ipowp(q, i * i);
        q_i2 *= std::cos(i * 2 * c * M_PI / order) * j;
        acc += q_i2; j = -j; ++i;
    } while (std::fabs(q_i2) > 1e-100);
    return acc;
}
double coefAt(int index, double k, double q, int order) {
    const int c = index + 1;
    const double num = accNum(q, order, c) * std::pow(q, 0.25);
    const double den = accDen(q, order, c) + 0.5;
    const double ww = num / den;
    const double wwsq = ww * ww;
    const double x = std::sqrt((1 - wwsq * k) * (1 - wwsq / k)) / (1 + wwsq);
    return (1 - x) / (1 + x);
}
double besselI0(double x) {
    double s = 1, t = 1;
    for (int k = 1; k < 50; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; if (t < 1e-12 * s) break; }
    return s;
}
} // namespace

HalfbandCoefs HalfbandCoefs::design(int n, double tbw) {
    HalfbandCoefs h;
    double k, q;
    transitionParam(k, q, tbw);
    const int order = n * 2 + 1;
    for (int i = 0; i < n; ++i) h.c.push_back((float) coefAt(i, k, q, order));
    return h;
}

void FirHalfband::init(int K, double beta) {
    const int L = 4 * K + 3, M = L / 2;
    taps.assign((size_t) L, 0);
    double sum = 0;
    for (int n = 0; n < L; ++n) {
        int d = n - M;
        double s = d == 0 ? 0.5 : std::sin(M_PI * d / 2.0) / (M_PI * d);
        double r = (double) d / M;
        double w = besselI0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / besselI0(beta);
        taps[(size_t) n] = (float) (s * w);
        sum += s * w;
    }
    for (auto& t : taps) t = (float) (t / sum);
    taps[(size_t) M] = 0.5f; // exact
    // renormalise the even (non-centre) taps so DC gain is exactly 1
    double s2 = 0; for (int n = 0; n < L; ++n) if (n != M) s2 += taps[(size_t) n];
    for (int n = 0; n < L; ++n) if (n != M && s2 != 0) taps[(size_t) n] = (float) (taps[(size_t) n] * 0.5 / s2);
}

// ---------------- Oversampler ----------------
void Oversampler::prepare(int channels, int maxBlock, int maxFactorLog2, Kind k) {
    kind = k; chans = channels; maxN = maxBlock; maxF = maxFactorLog2;
    buf.assign((size_t) chans, {});
    for (auto& c : buf) for (int lv = 0; lv <= 3; ++lv) c[(size_t) lv].assign((size_t) maxBlock << lv, 0.f);
    static const HalfbandCoefs c0 = HalfbandCoefs::design(10, 0.04), c1 = HalfbandCoefs::design(6, 0.18), c2 = HalfbandCoefs::design(4, 0.3);
    iu.assign((size_t) chans, {}); id.assign((size_t) chans, {});
    for (int ch = 0; ch < chans; ++ch) {
        iu[(size_t) ch][0].init(c0); iu[(size_t) ch][1].init(c1); iu[(size_t) ch][2].init(c2);
        id[(size_t) ch][0].init(c0); id[(size_t) ch][1].init(c1); id[(size_t) ch][2].init(c2);
    }
    fir[0].init(15, 8.0); fir[1].init(6, 7.0); fir[2].init(4, 7.0);
    fu.assign((size_t) chans, {}); fd.assign((size_t) chans, {});
    for (int ch = 0; ch < chans; ++ch)
        for (int s = 0; s < 3; ++s) {
            fu[(size_t) ch][(size_t) s].hist.assign(64, 0.f);
            fd[(size_t) ch][(size_t) s].hist.assign(128, 0.f);
        }
    pad.assign((size_t) chans, std::vector<float>(16, 0.f));
    padPos.assign((size_t) chans, 0);
    setFactorLog2(fl2 <= maxF ? fl2 : maxF);
    computeLinLatency();
}

void Oversampler::computeLinLatency() {
    long T = 0; // latency at top rate
    for (int k = 0; k < fl2; ++k) T += 2L * fir[(size_t) k].centre() * (1L << (fl2 - k - 1));
    const long F = 1L << fl2;
    padLen = (int) ((F - (T % F)) % F);
    linLatency = (int) ((T + padLen) / F);
}

void Oversampler::setFactorLog2(int f) {
    f = std::max(0, std::min(f, maxF));
    if (f != fl2) { fl2 = f; reset(); }
    computeLinLatency();
}

void Oversampler::reset() {
    for (auto& a : iu) for (auto& s : a) s.reset();
    for (auto& a : id) for (auto& s : a) s.reset();
    for (auto& a : fu) for (auto& s : a) { std::fill(s.hist.begin(), s.hist.end(), 0.f); s.pos = 0; }
    for (auto& a : fd) for (auto& s : a) { std::fill(s.hist.begin(), s.hist.end(), 0.f); s.pos = 0; }
    for (auto& p : pad) std::fill(p.begin(), p.end(), 0.f);
    std::fill(padPos.begin(), padPos.end(), 0);
}

void Oversampler::firUp(int ch, int stage, const float* in, float* out, int n) {
    auto& st = fu[(size_t) ch][(size_t) stage];
    const auto& h = fir[(size_t) stage].taps;
    const int L = (int) h.size(), M = L / 2;
    const int mask = (int) st.hist.size() - 1;
    for (int i = 0; i < n; ++i) {
        st.pos = (st.pos + 1) & mask;
        st.hist[(size_t) st.pos] = in[i];
        // even output: sum over even taps j of 2*h[j]*x[i - j/2]
        float acc = 0;
        for (int j = 0; j < L; j += 2) acc += h[(size_t) j] * st.hist[(size_t) ((st.pos - j / 2) & mask)];
        out[2 * i] = 2.f * acc;
        out[2 * i + 1] = st.hist[(size_t) ((st.pos - (M - 1) / 2) & mask)];
    }
}

void Oversampler::firDown(int ch, int stage, const float* in, float* out, int n) {
    auto& st = fd[(size_t) ch][(size_t) stage];
    const auto& h = fir[(size_t) stage].taps;
    const int L = (int) h.size(), M = L / 2;
    const int mask = (int) st.hist.size() - 1;
    for (int i = 0; i < n; ++i) {
        // decimate at the even index 2i so the total delay is exactly an integer
        st.pos = (st.pos + 1) & mask; st.hist[(size_t) st.pos] = in[2 * i];
        float acc = 0.5f * st.hist[(size_t) ((st.pos - M) & mask)];
        for (int j = 0; j < L; j += 2) acc += h[(size_t) j] * st.hist[(size_t) ((st.pos - j) & mask)];
        out[i] = acc;
        st.pos = (st.pos + 1) & mask; st.hist[(size_t) st.pos] = in[2 * i + 1];
    }
}

void Oversampler::up(const float* const* in, int n) {
    for (int ch = 0; ch < chans; ++ch) {
        auto& b = buf[(size_t) ch];
        std::memcpy(b[0].data(), in[ch], sizeof(float) * (size_t) n);
        for (int k = 0; k < fl2; ++k) {
            const int len = n << k;
            const float* src = b[(size_t) k].data();
            float* dst = b[(size_t) k + 1].data();
            if (kind == Kind::Iir) {
                auto& u = iu[(size_t) ch][(size_t) k];
                for (int i = 0; i < len; ++i) u.process(src[i], dst[2 * i], dst[2 * i + 1]);
            } else firUp(ch, k, src, dst, len);
        }
        if (kind == Kind::LinearPhase && padLen > 0) {
            float* top = b[(size_t) fl2].data();
            auto& pd = pad[(size_t) ch];
            int& pp = padPos[(size_t) ch];
            const int len = n << fl2;
            for (int i = 0; i < len; ++i) {
                float v = top[i];
                top[i] = pd[(size_t) ((pp - padLen + 16) & 15)];
                pd[(size_t) pp] = v; pp = (pp + 1) & 15;
            }
        }
    }
}

void Oversampler::down(float* const* out, int n) {
    for (int ch = 0; ch < chans; ++ch) {
        auto& b = buf[(size_t) ch];
        for (int k = fl2 - 1; k >= 0; --k) {
            const int len = n << k; // number of outputs at level k
            const float* src = b[(size_t) k + 1].data();
            float* dst = b[(size_t) k].data();
            if (kind == Kind::Iir) {
                auto& d = id[(size_t) ch][(size_t) k];
                for (int i = 0; i < len; ++i) dst[i] = d.process(src[2 * i], src[2 * i + 1]);
            } else firDown(ch, k, src, dst, len);
        }
        std::memcpy(out[ch], b[0].data(), sizeof(float) * (size_t) n);
    }
}

} // namespace gnarl
