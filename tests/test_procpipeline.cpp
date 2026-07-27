// test_procpipeline — the structural detector end to end (Поток 2). Drives the
// real ProcPipeline (RMS -> baseline calibration -> STA/LTA with hysteresis) and
// checks the two things that matter operationally:
//   1. a steady quiet signal raises NO alarm (no false positives);
//   2. a sustained amplitude burst DOES trip the STA/LTA trigger.

#include "proc/ProcPipeline.h"

#include <QVariantMap>
#include <cmath>
#include <cstdio>
#include <string>

using namespace tp;

static int failures = 0;
static void expect(bool ok, const char* what, const std::string& d = {}) {
    std::printf("%-4s %-40s %s\n", ok ? "ok" : "FAIL", what, d.c_str());
    if (!ok) ++failures;
}

int main() {
    const double fs = 200.0;
    const int    win = 100;           // 0.5 s windows

    ProcPipeline pipe;
    pipe.setWindowSize(win);
    pipe.setCalibrationWindows(5);    // learn baseline over 5 windows, then judge
    pipe.setStaLta(0.5, 0.02, 3.0, 1.5);

    int triggeredAfterCalib = 0;      // triggers seen on the quiet signal (want 0)
    int triggeredOnBurst = 0;         // triggers seen on the burst (want > 0)
    bool inBurst = false;
    pipe.onSaf = [&](const QVariantMap& m) {
        if (m.value("component").toInt() != 2) return;   // count the Z axis once
        if (m.value("triggered").toBool()) { if (inBurst) ++triggeredOnBurst; else ++triggeredAfterCalib; }
    };

    // Feed one 0.5 s window of a sine at `amp` (structural tone 3 Hz).
    int64_t t = 1'700'000'000'000LL;
    auto feedWindow = [&](double amp) {
        for (int i = 0; i < win; ++i, t += int64_t(1000.0 / fs)) {
            const double s = amp * std::sin(2 * M_PI * 3.0 * i / fs);
            pipe.addSample(1, 1, 1, s, s, s, t, uint32_t(fs));
        }
    };

    // Quiet phase: calibration (5) + settle + observe. No alarm expected.
    for (int w = 0; w < 30; ++w) feedWindow(1.0);
    expect(triggeredAfterCalib == 0, "quiet signal: no false trigger",
           "triggers=" + std::to_string(triggeredAfterCalib));

    // Burst phase: 10x amplitude, sustained. STA/LTA must trip.
    inBurst = true;
    for (int w = 0; w < 12; ++w) feedWindow(10.0);
    expect(triggeredOnBurst > 0, "sustained burst: STA/LTA triggers",
           "triggers=" + std::to_string(triggeredOnBurst));

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
