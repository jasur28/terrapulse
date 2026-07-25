#include "analysis/StrongMotion.h"

#include <algorithm>
#include <cmath>

namespace tp::sm {

std::vector<double> detrend(const std::vector<float>& a) {
    std::vector<double> out(a.size());
    if (a.empty()) return out;
    double mean = 0.0;
    for (float v : a) mean += v;
    mean /= double(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) out[i] = double(a[i]) - mean;
    return out;
}

double pga(const std::vector<double>& a) {
    double m = 0.0;
    for (double v : a) m = std::max(m, std::fabs(v));
    return m;
}

double pgv(const std::vector<double>& a, double dt) {
    if (a.size() < 2 || dt <= 0) return 0.0;
    std::vector<double> v(a.size(), 0.0);
    for (std::size_t i = 1; i < a.size(); ++i)
        v[i] = v[i - 1] + 0.5 * (a[i] + a[i - 1]) * dt;   // trapezoidal integral
    double mean = 0.0;
    for (double s : v) mean += s;
    mean /= double(v.size());
    double peak = 0.0;
    for (double s : v) peak = std::max(peak, std::fabs(s - mean));
    return peak;   // cm/s (a in gal = cm/s^2, dt in s)
}

std::vector<double> responseSpectrum(const std::vector<double>& a, double dt,
                                     const std::vector<double>& periods, double damping) {
    std::vector<double> psa(periods.size(), 0.0);
    if (a.size() < 2 || dt <= 0) return psa;

    for (std::size_t p = 0; p < periods.size(); ++p) {
        const double T = periods[p];
        if (T <= 0) continue;
        const double w   = 2.0 * M_PI / T;         // natural angular frequency
        const double xi  = damping;
        const double wd  = w * std::sqrt(1.0 - xi * xi);
        const double e   = std::exp(-xi * w * dt);
        const double cwd = std::cos(wd * dt), swd = std::sin(wd * dt);

        // Nigam-Jennings exact coefficients for piecewise-linear excitation.
        const double A = e * (xi / std::sqrt(1.0 - xi * xi) * swd + cwd);
        const double B = e * (1.0 / wd * swd);
        const double w2 = w * w, w3 = w2 * w;
        const double xi2 = xi * xi;
        const double C = (1.0 / w2) * (2.0 * xi / (w * dt) +
                          e * (((1.0 - 2.0 * xi2) / (wd * dt) - xi / std::sqrt(1.0 - xi2)) * swd -
                               (1.0 + 2.0 * xi / (w * dt)) * cwd));
        const double D = (1.0 / w2) * (1.0 - 2.0 * xi / (w * dt) +
                          e * ((2.0 * xi2 - 1.0) / (wd * dt) * swd + 2.0 * xi / (w * dt) * cwd));
        const double Ap = -e * (w / std::sqrt(1.0 - xi2) * swd);
        const double Bp =  e * (cwd - xi / std::sqrt(1.0 - xi2) * swd);
        const double Cp = (1.0 / w2) * (-1.0 / dt +
                          e * ((w / std::sqrt(1.0 - xi2) + xi / (dt * std::sqrt(1.0 - xi2))) * swd +
                               1.0 / dt * cwd));
        const double Dp = (1.0 / (w2 * dt)) * (1.0 - e * (xi / std::sqrt(1.0 - xi2) * swd + cwd));
        (void)w3;

        double u = 0.0, ud = 0.0, umax = 0.0;
        for (std::size_t i = 1; i < a.size(); ++i) {
            const double ui = A * u + B * ud + C * a[i - 1] + D * a[i];
            const double udi = Ap * u + Bp * ud + Cp * a[i - 1] + Dp * a[i];
            u = ui; ud = udi;
            umax = std::max(umax, std::fabs(u));
        }
        psa[p] = w2 * umax;   // pseudo-spectral acceleration (gal)
    }
    return psa;
}

double jmaIntensity(const std::vector<float>& x, const std::vector<float>& y,
                    const std::vector<float>& z, double dt) {
    const std::size_t n = std::min({x.size(), y.size(), z.size()});
    if (n < 4 || dt <= 0) return 0.0;
    const std::vector<double> ax = detrend({x.begin(), x.begin() + n});
    const std::vector<double> ay = detrend({y.begin(), y.begin() + n});
    const std::vector<double> az = detrend({z.begin(), z.begin() + n});

    std::vector<double> mag(n);
    for (std::size_t i = 0; i < n; ++i)
        mag[i] = std::sqrt(ax[i] * ax[i] + ay[i] * ay[i] + az[i] * az[i]);

    std::sort(mag.begin(), mag.end(), std::greater<double>());
    const std::size_t k = std::size_t(0.3 / dt);   // samples in 0.3 s
    if (k >= mag.size()) return 0.0;
    const double a0 = mag[k];                        // amplitude sustained 0.3 s
    if (a0 <= 0.0) return 0.0;
    const double I = 2.0 * std::log10(a0) + 0.94;
    return std::max(0.0, I);
}

const char* jmaScale(double I) {
    if (I < 0.5)  return "0";
    if (I < 1.5)  return "1";
    if (I < 2.5)  return "2";
    if (I < 3.5)  return "3";
    if (I < 4.5)  return "4";
    if (I < 5.0)  return "5-";
    if (I < 5.5)  return "5+";
    if (I < 6.0)  return "6-";
    if (I < 6.5)  return "6+";
    return "7";
}

} // namespace tp::sm
