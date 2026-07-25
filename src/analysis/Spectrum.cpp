#include "analysis/Spectrum.h"

#include <kiss_fft.h>

#include <algorithm>
#include <cmath>

namespace tp::spec {
namespace {

// Shared front end: mean-remove, Hann-window and transform. Returns the raw
// half-spectrum power |X|^2 plus the window sums needed for scaling.
bool transform(const std::vector<float>& data, uint32_t rate,
               std::vector<double>& power, double& sumW, double& sumW2, double& binHz) {
    const std::size_t n = data.size();
    if (n < 8 || rate == 0) return false;

    double mean = 0.0;
    for (float v : data) mean += v;
    mean /= double(n);

    std::vector<kiss_fft_cpx> in(n), out(n);
    sumW = 0.0; sumW2 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * double(i) / double(n - 1)));
        sumW  += w;
        sumW2 += w * w;
        in[i].r = static_cast<kiss_fft_scalar>((double(data[i]) - mean) * w);
        in[i].i = 0;
    }

    kiss_fft_cfg cfg = kiss_fft_alloc(int(n), 0, nullptr, nullptr);
    if (!cfg) return false;
    kiss_fft(cfg, in.data(), out.data());
    kiss_fft_free(cfg);

    const std::size_t half = n / 2;
    power.assign(half + 1, 0.0);
    for (std::size_t k = 0; k <= half; ++k)
        power[k] = double(out[k].r) * out[k].r + double(out[k].i) * out[k].i;

    binHz = double(rate) / double(n);
    return true;
}

} // namespace

std::vector<double> magnitude(const std::vector<float>& data, uint32_t rate, double* binHz) {
    std::vector<double> power;
    double sumW = 0, sumW2 = 0, bin = 0;
    if (!transform(data, rate, power, sumW, sumW2, bin)) {
        if (binHz) *binHz = 0.0;
        return {};
    }
    std::vector<double> mag(power.size());
    const double scale = sumW > 0 ? 2.0 / sumW : 0.0;   // coherent gain -> true amplitude
    for (std::size_t k = 0; k < power.size(); ++k)
        mag[k] = std::sqrt(power[k]) * scale;
    if (!mag.empty()) mag[0] *= 0.5;                     // DC is not doubled
    if (binHz) *binHz = bin;
    return mag;
}

std::vector<double> psd(const std::vector<float>& data, uint32_t rate, double* binHz) {
    std::vector<double> power;
    double sumW = 0, sumW2 = 0, bin = 0;
    if (!transform(data, rate, power, sumW, sumW2, bin)) {
        if (binHz) *binHz = 0.0;
        return {};
    }
    // Single-window periodogram, noise-power normalisation.
    const double norm = (sumW2 > 0 && rate > 0) ? 2.0 / (double(rate) * sumW2) : 0.0;
    std::vector<double> out(power.size());
    for (std::size_t k = 0; k < power.size(); ++k) out[k] = power[k] * norm;
    if (!out.empty()) out[0] *= 0.5;
    if (binHz) *binHz = bin;
    return out;
}

double dominantFrequency(const std::vector<float>& data, uint32_t rate,
                         double fMin, double fMax) {
    double bin = 0.0;
    const std::vector<double> mag = magnitude(data, rate, &bin);
    if (mag.size() < 3 || bin <= 0.0) return 0.0;

    const double nyquist = double(rate) / 2.0;
    if (fMax <= 0.0 || fMax > nyquist) fMax = nyquist;

    std::size_t kMin = std::size_t(std::ceil(fMin / bin));
    std::size_t kMax = std::size_t(std::floor(fMax / bin));
    kMin = std::max<std::size_t>(kMin, 1);                    // skip DC
    kMax = std::min<std::size_t>(kMax, mag.size() - 1);
    if (kMax <= kMin) return 0.0;

    std::size_t peak = kMin;
    for (std::size_t k = kMin; k <= kMax; ++k)
        if (mag[k] > mag[peak]) peak = k;
    if (mag[peak] <= 0.0) return 0.0;

    // Parabolic interpolation around the peak for sub-bin resolution.
    double delta = 0.0;
    if (peak > 0 && peak + 1 < mag.size()) {
        const double a = mag[peak - 1], b = mag[peak], c = mag[peak + 1];
        const double denom = a - 2.0 * b + c;
        if (std::fabs(denom) > 1e-12) {
            delta = 0.5 * (a - c) / denom;
            delta = std::max(-0.5, std::min(0.5, delta));
        }
    }
    return (double(peak) + delta) * bin;
}

} // namespace tp::spec
