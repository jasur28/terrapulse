#include "proc/Filter.h"

#include <cmath>
#include <cstdlib>

namespace tp::proc {

// The three constructions share w0/alpha; only the b-coefficients differ. Each
// returns coefficients already normalised by a0 (the private ctor takes them so).
Biquad Biquad::lowpass(double fc, double fs, double q) {
    const double w0 = 2.0 * M_PI * (fc / fs);
    const double c = std::cos(w0), alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1 + alpha;
    return Biquad((1 - c) / 2 / a0, (1 - c) / a0, (1 - c) / 2 / a0,
                  -2 * c / a0, (1 - alpha) / a0);
}

Biquad Biquad::highpass(double fc, double fs, double q) {
    const double w0 = 2.0 * M_PI * (fc / fs);
    const double c = std::cos(w0), alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1 + alpha;
    return Biquad((1 + c) / 2 / a0, -(1 + c) / a0, (1 + c) / 2 / a0,
                  -2 * c / a0, (1 - alpha) / a0);
}

Biquad Biquad::bandpass(double fc, double fs, double q) {
    const double w0 = 2.0 * M_PI * (fc / fs);
    const double c = std::cos(w0), alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1 + alpha;
    return Biquad(alpha / a0, 0.0, -alpha / a0, -2 * c / a0, (1 - alpha) / a0);  // 0 dB peak
}

FilterChain FilterChain::fromSpec(const std::string& spec, double fs) {
    FilterChain chain;
    std::size_t i = 0;
    while (i < spec.size()) {
        std::size_t comma = spec.find(',', i);
        if (comma == std::string::npos) comma = spec.size();
        const std::string tok = spec.substr(i, comma - i);
        i = comma + 1;
        const std::size_t colon = tok.find(':');
        if (colon == std::string::npos) continue;
        const std::string kind = tok.substr(0, colon);
        const double fc = std::strtod(tok.substr(colon + 1).c_str(), nullptr);
        if (fc <= 0.0 || fc >= fs / 2.0) continue;      // must be within (0, Nyquist)
        if      (kind == "hp") chain.add(Biquad::highpass(fc, fs));
        else if (kind == "lp") chain.add(Biquad::lowpass(fc, fs));
        else if (kind == "bp") chain.add(Biquad::bandpass(fc, fs));
    }
    return chain;
}

} // namespace tp::proc
