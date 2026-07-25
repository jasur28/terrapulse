// tpdump — inspect archived waveforms through a RecordStream URI. Prints one
// line per trace (source id, time span, samples, rate, gaps, amplitude stats).
// The console counterpart of the GUI's waveform review — proves the read path.
//
//   tpdump tds://var/tds                 whole archive
//   tpdump "tds://var/tds?obj=1&sen=1"   one sensor (X/Y/Z)
//   tpdump file://var/tds/FDSN_TP_1_01_H_N_Z.2026.203.mseed
//   tpdump var/tds                       bare path (dir -> tds://)

#include "mseed/RecordStream.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

static std::string isoUtc(int64_t ms) {
    std::time_t s = std::time_t(ms / 1000);
    std::tm tmv{};
#if defined(_WIN32)
    gmtime_s(&tmv, &s);
#else
    gmtime_r(&s, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return std::string(buf) + "." + std::to_string(int(ms % 1000));
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr,
            "usage: tpdump <uri>\n"
            "  tds://<root>[?obj=..&sen=..] | file://<path.mseed> | <bare path>\n");
        return 2;
    }

    auto traces = tp::mseed::readStream(argv[1]);
    if (traces.empty()) {
        std::fprintf(stderr, "tpdump: no waveforms found at %s\n", argv[1]);
        return 1;
    }

    std::printf("%-22s %-23s %10s %6s %5s %12s %12s %12s\n",
                "source-id", "start (UTC)", "samples", "Hz", "gaps", "min", "max", "rms");
    for (const auto& t : traces) {
        double mn = 1e300, mx = -1e300, sumsq = 0.0;
        for (int32_t v : t.samples) {
            const double d = double(v);
            mn = std::min(mn, d); mx = std::max(mx, d); sumsq += d * d;
        }
        const double rms = t.samples.empty() ? 0.0 : std::sqrt(sumsq / t.samples.size());
        std::printf("%-22s %-23s %10zu %6.0f %5d %12.0f %12.0f %12.1f\n",
                    t.sid.c_str(), isoUtc(t.startTimeMs).c_str(), t.samples.size(),
                    t.sampleRate, t.gaps, mn, mx, rms);
    }
    return 0;
}
