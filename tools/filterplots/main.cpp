// Measures real filter magnitude responses (impulse response -> FFT through the actual FilterUnit DSP)
// and writes SVG plots to docs/filters/. Also checks the analytic display curve against the measurement.
#include "filter/Filter.h"
#include "util/FFT.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace gnarl;

static std::vector<double> measure(const FilterSettings& s, float rate) {
    FilterUnit f; f.prepare(rate, (int) (rate / 8) + 8); f.reset();
    const int N = 1 << 16;
    std::vector<float> L(N, 0.f), R(N, 0.f);
    L[0] = R[0] = 1e-3f; // low level: measures the linear response
    f.setTarget(s, 1);
    DrawTables dt;
    for (int pos = 0; pos < N; pos += 256) { f.setTarget(s, 256); f.process(L.data() + pos, R.data() + pos, 256, dt); }
    RealFFT fft(16);
    std::vector<float> re(N / 2 + 1), im(N / 2 + 1);
    fft.forward(L.data(), re.data(), im.data());
    std::vector<double> mag(N / 2 + 1);
    for (int k = 0; k <= N / 2; ++k) mag[(size_t) k] = std::sqrt((double) re[(size_t) k] * re[(size_t) k] + (double) im[(size_t) k] * im[(size_t) k]) / 1e-3;
    return mag;
}

struct Curve { std::string label, colour; std::vector<double> mag; };

static void svg(const std::string& path, const std::string& title, const std::vector<Curve>& curves, float rate, double dbLo = -60, double dbHi = 36) {
    const int W = 900, H = 420, ml = 60, mr = 20, mt = 40, mb = 50;
    const int N = 1 << 16;
    auto xOf = [&](double f) { return ml + (W - ml - mr) * std::log(f / 20.0) / std::log(20000.0 / 20.0); };
    auto yOf = [&](double db) { db = std::max(dbLo, std::min(dbHi, db)); return mt + (H - mt - mb) * (dbHi - db) / (dbHi - dbLo); };
    std::ostringstream o;
    o << "<svg xmlns='http://www.w3.org/2000/svg' width='" << W << "' height='" << H << "' font-family='Helvetica,Arial' font-size='12'>\n";
    o << "<rect width='100%' height='100%' fill='#101418'/>\n<text x='" << ml << "' y='24' fill='#e8eef2' font-size='15'>" << title << "</text>\n";
    for (double f : {20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0}) {
        o << "<line x1='" << xOf(f) << "' y1='" << mt << "' x2='" << xOf(f) << "' y2='" << H - mb << "' stroke='#2a333c'/>";
        o << "<text x='" << xOf(f) - 12 << "' y='" << H - mb + 18 << "' fill='#9fb0bd'>" << (f >= 1000 ? std::to_string((int) (f / 1000)) + "k" : std::to_string((int) f)) << "</text>\n";
    }
    for (double db = dbLo; db <= dbHi; db += 12) {
        o << "<line x1='" << ml << "' y1='" << yOf(db) << "' x2='" << W - mr << "' y2='" << yOf(db) << "' stroke='" << (db == 0 ? "#4a5866" : "#2a333c") << "'/>";
        o << "<text x='8' y='" << yOf(db) + 4 << "' fill='#9fb0bd'>" << (int) db << " dB</text>\n";
    }
    int ly = mt + 14;
    for (auto& c : curves) {
        o << "<polyline fill='none' stroke='" << c.colour << "' stroke-width='1.8' points='";
        for (int k = 1; k <= N / 2; ++k) {
            double f = (double) k * rate / N;
            if (f < 20 || f > 20000) continue;
            if (k % 4 && f > 200) continue;
            o << xOf(f) << "," << yOf(20 * std::log10(c.mag[(size_t) k] + 1e-12)) << " ";
        }
        o << "'/>\n<text x='" << W - mr - 230 << "' y='" << ly << "' fill='" << c.colour << "'>" << c.label << "</text>\n";
        ly += 16;
    }
    o << "</svg>\n";
    std::ofstream(path) << o.str();
}

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "docs/filters";
    const float rate = 48000.f * 4; // filters run at the 4x oversampled rate by default
    const char* colours[] = {"#ff6b6b", "#ffd166", "#06d6a0", "#4cc9f0", "#b388ff", "#f78c6b", "#e0e0e0"};
    // every type at three resonance settings
    for (int t = 1; t < FT_Count; ++t) {
        std::vector<Curve> cs;
        int ci = 0;
        for (float reso : {0.0f, 0.5f, 0.85f}) {
            FilterSettings s; s.type = t; s.cutoff = 1000; s.reso = reso; s.sep = 0.4f; s.morph = 0.35f; s.stages = 8;
            char lab[64]; std::snprintf(lab, sizeof lab, "reso %.2f (measured)", reso);
            cs.push_back({lab, colours[ci++], measure(s, rate)});
        }
        std::string name = choices::filterTypes()[(size_t) t];
        std::string file = name; for (auto& c : file) if (c == ' ' || c == '/' || c == '+') c = '_';
        if (t == FT_CombMinus) file = "Comb_minus";
        svg(dir + "/" + file + ".svg", name + " at 1 kHz — measured magnitude (impulse response through the real DSP)", cs, rate);
    }
    // Tri-Peak separation sweep
    {
        std::vector<Curve> cs; int ci = 0;
        for (float sep : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            FilterSettings s; s.type = FT_TriPeak; s.cutoff = 1000; s.reso = 0.75f; s.sep = sep; s.shoulder = 0.6f; s.shelf = 0.5f;
            char lab[64]; std::snprintf(lab, sizeof lab, "Separation %.2f", sep);
            cs.push_back({lab, colours[ci++], measure(s, rate)});
        }
        svg(dir + "/TriPeak_separation_sweep.svg", "Tri-Peak (Reso 3 Boost model): Separation sweep, cutoff 1 kHz, reso 0.75", cs, rate);
    }
    // Tri-Peak shoulder / shelf
    {
        std::vector<Curve> cs; int ci = 0;
        for (float sh : {0.0f, 0.5f, 1.0f}) { FilterSettings s; s.type = FT_TriPeak; s.cutoff = 800; s.reso = 0.7f; s.sep = 0.4f; s.shoulder = sh; s.shelf = 0.8f; char lab[64]; std::snprintf(lab, sizeof lab, "Shoulder %.1f, shelf 0.8", sh); cs.push_back({lab, colours[ci++], measure(s, rate)}); }
        svg(dir + "/TriPeak_shoulders.svg", "Tri-Peak: shoulder level (outer peaks) with raised shelf", cs, rate);
    }
    // Dual-reso opposing vs together
    {
        std::vector<Curve> cs; int ci = 0;
        for (int opp = 0; opp < 2; ++opp) for (float fc : {500.f, 2000.f}) {
            FilterSettings s; s.type = FT_DualResoBP; s.cutoff = fc; s.reso = 0.7f; s.sep = 0.5f; s.opposing = opp;
            char lab[64]; std::snprintf(lab, sizeof lab, "%s cutoff %g", opp ? "opposing" : "together", fc);
            cs.push_back({lab, colours[ci++], measure(s, rate)});
        }
        svg(dir + "/DualReso_opposing.svg", "Dual-Reso BP: peaks together vs opposing as cutoff moves 500 Hz -> 2 kHz", cs, rate);
    }
    // display-curve accuracy check: analytic filterMagnitude vs measurement
    double worst = 0;
    for (int t = 1; t < FT_Count; ++t) {
        if (t == FT_Ladder || t == FT_CombPlus || t == FT_CombMinus || t == FT_Phaser) continue; // display uses the linearised model for these
        FilterSettings s; s.type = t; s.cutoff = 1000; s.reso = 0.5f; s.sep = 0.4f; s.morph = 0.35f;
        auto m = measure(s, rate);
        for (double f : {100.0, 500.0, 1000.0, 3000.0, 10000.0}) {
            int k = (int) std::lround(f * (1 << 16) / rate);
            double a = 20 * std::log10(m[(size_t) k] + 1e-12), b = 20 * std::log10(filterMagnitude(s, (float) f, rate) + 1e-12);
            if (std::fabs(a - b) > 0.5) std::printf("  mismatch %s at %g Hz: measured %.2f dB, display %.2f dB\n", choices::filterTypes()[(size_t) t].c_str(), f, a, b);
            if (a > -40 && b > -40) worst = std::max(worst, std::fabs(a - b)); // ignore points deep inside notches
        }
    }
    std::printf("filter plots written to %s; worst display-vs-measured error (SVF types) %.3f dB\n", dir.c_str(), worst);
    return worst < 0.5 ? 0 : 1;
}
