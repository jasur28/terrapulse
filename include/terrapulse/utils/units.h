#pragma once

namespace tp::utils {

inline double galToMetersPerSecond2(double gal) { return gal / 100.0; }
inline double metersPerSecond2ToGal(double value) { return value * 100.0; }
inline double gToGal(double g) { return g * 980.665; }
inline double galToG(double gal) { return gal / 980.665; }
inline double percent(double value, double total) { return total == 0.0 ? 0.0 : 100.0 * value / total; }

} // namespace tp::utils
