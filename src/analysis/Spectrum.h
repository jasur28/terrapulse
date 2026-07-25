#pragma once
#include <cstdint>
#include <vector>

// Spectral analysis (real FFT via KissFFT) — replaces the zero-crossing estimate,
// which counts noise crossings and badly overestimates frequency on real data.
// For a structure the dominant spectral peak in the structural band IS its
// natural (modal) frequency; tracking its shift over time is the damage
// indicator, so accuracy here matters.
namespace tp::spec {

// Single-sided amplitude spectrum of a real signal: mean removed, Hann-windowed,
// scaled so a pure tone reads its true amplitude. Returns bins [0 .. N/2];
// *binHz (if given) receives the bin width (rate / N).
std::vector<double> magnitude(const std::vector<float>& data, uint32_t rate,
                              double* binHz = nullptr);

// Power spectral density (units^2/Hz), same binning as magnitude().
std::vector<double> psd(const std::vector<float>& data, uint32_t rate,
                        double* binHz = nullptr);

// Dominant frequency (Hz) = largest spectral peak within [fMin, fMax], refined
// by parabolic interpolation for sub-bin accuracy (a short window has coarse
// bins, so interpolation is what makes this usable). fMax <= 0 means Nyquist.
// Returns 0 if nothing usable. fMin skips DC/drift.
double dominantFrequency(const std::vector<float>& data, uint32_t rate,
                         double fMin = 0.3, double fMax = 0.0);

} // namespace tp::spec
