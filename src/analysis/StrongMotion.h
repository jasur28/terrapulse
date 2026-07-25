#pragma once
#include <cstddef>
#include <vector>

// Strong-motion parameters — the structural-health analog of seismic "magnitude"
// (SeisComp scwfparam). Pure functions over a window of acceleration samples in
// gal (cm/s^2). Inputs are expected mean-removed (gravity/DC gone); helpers here
// remove the mean where it matters. No external deps.
namespace tp::sm {

// Subtract the mean (removes static gravity/DC bias).
std::vector<double> detrend(const std::vector<float>& a);

// Peak ground acceleration: max |a| (gal). Input should be detrended.
double pga(const std::vector<double>& a);

// Peak ground velocity (cm/s): trapezoidal integration of acceleration, with the
// resulting velocity mean-removed to suppress integration drift, then peak |v|.
double pgv(const std::vector<double>& a, double dt);

// 5%-damped pseudo-spectral acceleration (gal) at each period, via the exact
// Nigam-Jennings recursive filter for a SDOF oscillator under the ground motion.
std::vector<double> responseSpectrum(const std::vector<double>& a, double dt,
                                     const std::vector<double>& periods,
                                     double damping = 0.05);

// JMA instrumental seismic intensity (計測震度), simplified time-domain estimate:
// vector magnitude of the three mean-removed components, take the amplitude a0
// sustained for a cumulative 0.3 s, I = 2*log10(a0) + 0.94 (a0 in gal), clamped.
// (A full JMA computation also applies period/high-cut/low-cut filters first;
// this captures the sustained-shaking definition without the frequency filter.)
double jmaIntensity(const std::vector<float>& x, const std::vector<float>& y,
                    const std::vector<float>& z, double dt);

// JMA intensity 0..7+ -> shindo scale label (0,1,2,3,4,5-,5+,6-,6+,7).
const char* jmaScale(double intensity);

} // namespace tp::sm
