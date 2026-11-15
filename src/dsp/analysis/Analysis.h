#pragma once
#include <string>
#include <vector>

namespace gnarl::analysis {

inline double toDb(double lin) { return lin > 1e-30 ? 20.0 * std::log10(lin) : -600.0; }
double rms(const std::vector<float>& x, size_t from = 0, size_t to = (size_t) -1);
double peak(const std::vector<float>& x, size_t from = 0, size_t to = (size_t) -1);
// RMS of (a - b) where b is shifted by `lagB` samples (b[i - lagB] compared to a[i]).
double diffRms(const std::vector<float>& a, const std::vector<float>& b, int lagB = 0, size_t from = 0);
double maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b, int lagB = 0, size_t from = 0);
double dc(const std::vector<float>& x, size_t from = 0, size_t to = (size_t) -1);
// Magnitude spectrum (dB) averaged over Hann-windowed frames of size 2^order.
std::vector<double> spectrumDb(const std::vector<float>& x, double sr, int order, size_t from = 0, size_t to = (size_t) -1);
double bandEnergyDb(const std::vector<float>& x, double sr, double lo, double hi, size_t from = 0, size_t to = (size_t) -1);
// Largest short-window (64 samples) high-frequency (> 8 kHz) energy, dB. Used by the click test.
double maxHfBurstDb(const std::vector<float>& x, double sr, size_t from = 0, size_t to = (size_t) -1);
// Largest sample-to-sample step of the second difference: click metric independent of spectrum.
double maxSecondDiff(const std::vector<float>& x, size_t from = 0, size_t to = (size_t) -1);
// Integrated loudness (ITU-R BS.1770 K-weighting, ungated), LUFS.
double loudnessLufs(const std::vector<float>& L, const std::vector<float>& R, double sr);
// Correlation of L and R low-passed below `hz` (1 = mono-compatible).
double lowCorrelation(const std::vector<float>& L, const std::vector<float>& R, double sr, double hz);
// Coarse spectrogram fingerprint: frames x 24 log bands, normalised.
std::vector<float> fingerprint(const std::vector<float>& L, double sr);
double fingerprintDistance(const std::vector<float>& a, const std::vector<float>& b);
// Last sample index above threshold (dBFS), or 0.
size_t lastAbove(const std::vector<float>& L, const std::vector<float>& R, double thresholdDb);
// Total harmonic distortion style measure: energy away from bins near multiples of f0, dB relative to total.
double inharmonicEnergyDb(const std::vector<float>& x, double sr, double f0, size_t from, size_t to);
// Energy above `hz` relative to total, dB (aliasing measure when the signal should be band-limited).
double energyAboveDb(const std::vector<float>& x, double sr, double hz, size_t from, size_t to);
// Fraction of energy that lies in bins that are NOT harmonics of f0 (aliasing measure), dB
double nonHarmonicDb(const std::vector<float>& x, double sr, double f0, size_t from, size_t to);

} // namespace gnarl::analysis
