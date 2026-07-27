// test_filter — the streaming Butterworth biquads (Поток 2 filter chain).
// Drives a pure sine at several frequencies through each filter, measures the
// steady-state output amplitude, and checks the frequency response: passband
// ~1.0, cutoff ~-3 dB (0.707), stopband strongly attenuated. This is what makes
// "add a high-pass / low-pass" a verified capability, not a claim.

#include "proc/Filter.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace tp::proc;

static int failures = 0;

// Steady-state gain of `chain` at frequency f (Hz), sample rate fs. Runs long
// enough for the transient to die, then measures peak over a few cycles.
static double gain(FilterChain chain, double f, double fs) {
    const int warm = 4000;
    const int meas = std::max(400, int(fs / f * 8));   // several cycles
    double peak = 0.0;
    for (int i = 0; i < warm + meas; ++i) {
        const double x = std::sin(2 * M_PI * f * i / fs);
        const double y = chain.process(x);
        if (i >= warm) peak = std::max(peak, std::fabs(y));
    }
    return peak;   // input amplitude is 1.0, so peak == gain
}

static void check(bool ok, const char* what, double got, double lo, double hi) {
    if (!ok) ++failures;
    std::printf("%-4s %-34s gain=%.3f  want [%.2f..%.2f]\n",
                ok ? "ok" : "FAIL", what, got, lo, hi);
}
static void band(FilterChain c, double f, double fs, const char* what, double lo, double hi) {
    const double g = gain(std::move(c), f, fs);
    check(g >= lo && g <= hi, what, g, lo, hi);
}

int main() {
    const double fs = 200.0;

    // Low-pass at 10 Hz: pass 1 Hz, -3 dB at 10 Hz, kill 60 Hz.
    { FilterChain c; c.add(Biquad::lowpass(10, fs));
      band(c, 1,  fs, "lp10 @1Hz  pass",   0.90, 1.05);
      band(c, 10, fs, "lp10 @10Hz -3dB",   0.60, 0.80);
      band(c, 60, fs, "lp10 @60Hz stop",   0.00, 0.10); }

    // High-pass at 1 Hz: kill 0.1 Hz, -3 dB at 1 Hz, pass 20 Hz.
    { FilterChain c; c.add(Biquad::highpass(1, fs));
      band(c, 0.1, fs, "hp1 @0.1Hz stop",  0.00, 0.20);
      band(c, 1,   fs, "hp1 @1Hz  -3dB",   0.60, 0.80);
      band(c, 20,  fs, "hp1 @20Hz pass",   0.90, 1.05); }

    // Band-pass around 3 Hz (structural band): peak ~1 at centre, sides down.
    { FilterChain c; c.add(Biquad::bandpass(3, fs, 2.0));
      band(c, 3,   fs, "bp3 @3Hz  peak",   0.85, 1.05);
      band(c, 0.3, fs, "bp3 @0.3Hz stop",  0.00, 0.35);
      band(c, 40,  fs, "bp3 @40Hz stop",   0.00, 0.30); }

    // A chain from a spec "hp:0.3,lp:20": DC-drift removed, high noise cut,
    // structural band (2.28 Hz measured on the device) passes.
    { FilterChain c = FilterChain::fromSpec("hp:0.3,lp:20", fs);
      band(c, 2.28, fs, "chain @2.28Hz pass", 0.85, 1.05); }
    { FilterChain c = FilterChain::fromSpec("hp:0.3,lp:20", fs);
      band(c, 0.05, fs, "chain @0.05Hz stop", 0.00, 0.30); }
    { FilterChain c = FilterChain::fromSpec("hp:0.3,lp:20", fs);
      band(c, 70,   fs, "chain @70Hz stop",   0.00, 0.20); }

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
