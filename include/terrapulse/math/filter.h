#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace tp::math {

inline double mean(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    return std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
}

inline double rms(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (double value : values) {
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(values.size()));
}

inline double absMax(const std::vector<double>& values) {
    double out = 0.0;
    for (double value : values) {
        out = std::max(out, std::abs(value));
    }
    return out;
}

class StaLta {
public:
    void configure(double staAlpha, double ltaAlpha, double onRatio, double offRatio) {
        if (staAlpha > 0.0) m_staAlpha = staAlpha;
        if (ltaAlpha > 0.0) m_ltaAlpha = ltaAlpha;
        if (onRatio > 0.0) m_onRatio = onRatio;
        if (offRatio > 0.0) m_offRatio = offRatio;
    }

    bool update(double characteristic) {
        const double c = std::abs(characteristic);
        if (!m_primed) {
            m_sta = c;
            m_lta = std::max(c, 1e-12);
            m_primed = true;
        }
        else {
            m_sta = m_staAlpha * c + (1.0 - m_staAlpha) * m_sta;
            m_lta = m_ltaAlpha * c + (1.0 - m_ltaAlpha) * m_lta;
        }

        const double ratio = this->ratio();
        if (!m_triggered && ratio >= m_onRatio) {
            m_triggered = true;
        }
        else if (m_triggered && ratio <= m_offRatio) {
            m_triggered = false;
        }
        return m_triggered;
    }

    double ratio() const { return m_lta <= 1e-12 ? 0.0 : m_sta / m_lta; }
    bool triggered() const { return m_triggered; }
    void reset() { m_sta = 0.0; m_lta = 0.0; m_primed = false; m_triggered = false; }

private:
    double m_staAlpha = 0.34;
    double m_ltaAlpha = 0.02;
    double m_onRatio = 3.0;
    double m_offRatio = 1.5;
    double m_sta = 0.0;
    double m_lta = 0.0;
    bool m_primed = false;
    bool m_triggered = false;
};

} // namespace tp::math
