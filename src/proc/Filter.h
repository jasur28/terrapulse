#pragma once
#include <cstddef>
#include <string>
#include <vector>

// Streaming IIR filters for the processing layer (Поток 2). The applied modules
// (structural, strong-motion) currently analyse the raw signal; a configurable
// filter chain lets an engineer remove DC/drift (high-pass), cut noise above the
// band of interest (low-pass), or isolate a structural band (band-pass) before
// analysis — the "Фильтрация: High-pass, Low-pass" step of the data-flow schema.
//
// Second-order Butterworth biquads (RBJ audio-EQ cookbook coefficients, standard
// DSP — not from any reference server). Transposed Direct Form II, one sample in,
// one out, so they drop straight into the per-sample pipeline.
namespace tp::proc {

class Biquad {
public:
    // Factory helpers: cutoff/centre in Hz, sample rate in Hz, Q (0.707 ≈ flat).
    static Biquad lowpass (double fc, double fs, double q = 0.70710678);
    static Biquad highpass(double fc, double fs, double q = 0.70710678);
    static Biquad bandpass(double fc, double fs, double q = 0.70710678);

    double process(double x) {
        const double y = m_b0 * x + m_z1;
        m_z1 = m_b1 * x - m_a1 * y + m_z2;
        m_z2 = m_b2 * x - m_a2 * y;
        return y;
    }
    void reset() { m_z1 = m_z2 = 0.0; }

    Biquad() = default;

private:
    // Coefficients already normalised by a0.
    Biquad(double b0, double b1, double b2, double a1, double a2)
        : m_b0(b0), m_b1(b1), m_b2(b2), m_a1(a1), m_a2(a2) {}

    double m_b0 = 1, m_b1 = 0, m_b2 = 0, m_a1 = 0, m_a2 = 0;   // a0 normalised to 1
    double m_z1 = 0, m_z2 = 0;
};

// An ordered chain of biquads applied to each sample. Empty chain = pass-through.
class FilterChain {
public:
    void add(const Biquad& b) { m_stages.push_back(b); }
    bool empty() const { return m_stages.empty(); }

    double process(double x) {
        for (auto& s : m_stages) x = s.process(x);
        return x;
    }
    void reset() { for (auto& s : m_stages) s.reset(); }

    // Build a chain from a spec string like "hp:0.3,lp:20" (Hz), for the sample
    // rate `fs`. Unknown tokens are ignored. Returns the built chain.
    static FilterChain fromSpec(const std::string& spec, double fs);

private:
    std::vector<Biquad> m_stages;
};

} // namespace tp::proc
