// test_streammonitor — the per-stream integrity layer (Поток 2 WaveformProcessor
// core). Feeds crafted sample streams and checks each verdict + the counters:
// clean flow, a gap, an overlap, a duplicate, clipping, and a rate change.

#include "proc/StreamMonitor.h"

#include <cstdio>
#include <string>

using namespace tp::proc;

static int failures = 0;
static void expect(bool ok, const char* what, const std::string& d = {}) {
    std::printf("%-4s %-38s %s\n", ok ? "ok" : "FAIL", what, d.c_str());
    if (!ok) ++failures;
}

int main() {
    const double fs = 200.0;              // 5 ms per sample
    const int64_t dt = 5;

    // Clean 200 Hz stream: every verdict Ok, no faults.
    {
        StreamMonitor m;
        bool allOk = true;
        int64_t t = 1000;
        for (int i = 0; i < 100; ++i, t += dt)
            if (m.feed(0.1 * i, t, fs) != StreamStatus::Ok) allOk = false;
        expect(allOk && m.faults() == 0, "clean stream: all Ok, 0 faults",
               "faults=" + std::to_string(m.faults()));
    }

    // Gap: a jump of 10 sample periods is flagged once.
    {
        StreamMonitor m;
        m.feed(1.0, 1000, fs);
        m.feed(1.0, 1005, fs);
        const auto st = m.feed(1.0, 1005 + 50, fs);   // +50 ms = 10 periods
        expect(st == StreamStatus::Gap && m.gaps() == 1, "forward jump -> Gap");
    }

    // Overlap: time goes backwards.
    {
        StreamMonitor m;
        m.feed(1.0, 2000, fs);
        m.feed(1.0, 2005, fs);
        const auto st = m.feed(1.0, 2003, fs);
        expect(st == StreamStatus::Overlap && m.overlaps() == 1, "backward time -> Overlap");
    }

    // Duplicate: same timestamp AND same value.
    {
        StreamMonitor m;
        m.feed(7.0, 3000, fs);
        const auto st = m.feed(7.0, 3000, fs);
        expect(st == StreamStatus::Duplicate && m.duplicates() == 1, "same t & value -> Duplicate");
    }

    // Clipping: |value| reaches the saturation limit.
    {
        StreamMonitor m(1000.0);          // clip limit 1000 counts
        m.feed(10.0, 4000, fs);
        const auto lo = m.feed(999.0, 4005, fs);
        const auto hi = m.feed(1000.0, 4010, fs);
        expect(lo == StreamStatus::Ok, "below limit -> Ok");
        expect(hi == StreamStatus::Clipped && m.clipped() == 1, "at limit -> Clipped");
    }

    // Rate change: reported sample rate departs from the established one.
    {
        StreamMonitor m;
        m.feed(1.0, 5000, 200.0);
        m.feed(1.0, 5005, 200.0);
        const auto st = m.feed(1.0, 5010, 100.0);   // 200 -> 100 Hz
        expect(st == StreamStatus::RateChanged && m.rateChanges() == 1, "rate 200->100 -> RateChanged");
    }

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
