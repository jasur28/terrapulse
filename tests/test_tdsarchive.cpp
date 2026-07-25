// test_tdsarchive — the "save" layer gate. Writes N known samples to a TDS
// archive and reads them back through the same path production uses
// (readTdsChannel), then measures bytes-per-sample at two record sizes.
//
// It locks three things we already know can break:
//   1. round-trip integrity — N samples in == N out, values identical
//   2. record fill — bytes/sample at recordSamples=200 vs 2000 (Stage 3.1 fact)
//   3. sensorId >= 100 — must not silently produce an empty archive
//
// docs/ЗАМЕРЫ.md records the measured bytes/sample this test prints.

#include "mseed/StreamId.h"
#include "mseed/TdsArchive.h"
#include "mseed/RecordStream.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace tp::mseed;

static int failures = 0;
static void check(bool ok, const char* what, const std::string& detail = {}) {
    std::printf("%-4s %-36s %s\n", ok ? "ok" : "FAIL", what, detail.c_str());
    if (!ok) ++failures;
}

// Realistic structural signal in device counts, so Steim2 compresses like real
// data (a flat ramp would compress unrealistically well).
static std::vector<int32_t> makeSignal(int n, double rate) {
    std::vector<int32_t> v(n);
    uint32_t rng = 12345;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1103515245u + 12345u;
        const double t = i / rate;
        const double a = 0.02 * std::sin(2 * M_PI * 2.28 * t)   // structural tone
                       + 0.002 * ((rng >> 16) / 32768.0 - 1.0); // noise
        v[i] = static_cast<int32_t>(std::lround(a * 10000.0));
    }
    return v;
}

static uint64_t dirBytes(const std::string& dir) {
    uint64_t total = 0;
    for (auto& e : fs::directory_iterator(dir))
        if (e.is_regular_file()) total += e.file_size();
    return total;
}

// Write `signal` to a fresh archive at recordSamples, return bytes/sample and
// the read-back trace.
static double roundtrip(const std::string& dir, const std::string& sid,
                        const std::vector<int32_t>& signal, double rate,
                        int recordSamples, Trace& back) {
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        TdsArchive tds(dir, recordSamples);
        tds.setSourceId(1, 1, 0, sid);
        int64_t t = 1'700'000'000'000LL;   // fixed epoch ms
        for (std::size_t i = 0; i < signal.size(); ++i)
            tds.addSample(1, 1, 0, signal[i], t + int64_t(i * 1000.0 / rate), rate);
    }   // dtor flushes
    back = readTdsChannel(dir, sid);
    return double(dirBytes(dir)) / double(signal.size());
}

int main() {
    const std::string base = (fs::temp_directory_path() / "tp_tds_test").string();
    const double rate = 200.0;
    const int    N    = 4000;               // 20 s at 200 Hz
    const auto   sig  = makeSignal(N, rate);

    const auto id = makeStreamId("UZ", "BRDG1", 1, Instrument::Accelerometer,
                                 rate, kAccelerometerCorner, 0);
    check(id.ok, "build stream id", id.ok ? id.id.sourceId() : id.error);
    if (!id.ok) { std::printf("\nFAILED\n"); return 1; }
    const std::string sid = id.id.sourceId();

    // 1 + 2: round-trip at the current default (200) and at a fuller record.
    Trace b200, b2000;
    const double bps200  = roundtrip(base + "/r200",  sid, sig, rate, 200,  b200);
    const double bps2000 = roundtrip(base + "/r2000", sid, sig, rate, 2000, b2000);

    check(b200.samples.size() == std::size_t(N), "round-trip count (rec 200)",
          std::to_string(b200.samples.size()) + " / " + std::to_string(N));

    bool identical = b200.samples == sig;
    check(identical, "round-trip values identical");

    std::printf("  bytes/sample: recordSamples=200 -> %.2f   recordSamples=2000 -> %.2f"
                "  (raw int32 = 4.00)\n", bps200, bps2000);
    check(bps2000 < bps200, "fuller record is smaller per sample",
          "gain x" + std::to_string(bps200 / bps2000).substr(0, 4));

    // 3: sensorId >= 100 must not silently vanish. With the legacy numeric id
    // (unbound channel) location becomes "100" (3 chars) which miniSEED v2
    // cannot pack — so this documents whether the archive drops it silently.
    {
        const std::string dir = base + "/big";
        fs::remove_all(dir); fs::create_directories(dir);
        {
            TdsArchive tds(dir);               // unbound -> legacy numeric id
            int64_t t = 1'700'000'000'000LL;
            for (int i = 0; i < 400; ++i)
                tds.addSample(1, 100, 0, sig[i], t + int64_t(i * 5), rate);
        }
        const bool wroteSomething = fs::exists(dir) && dirBytes(dir) > 0;
        // Known bug (CLAUDE.md §8.3): legacy path silently writes nothing for
        // sensor>=100. We assert the CURRENT reality so a future fix flips it.
        check(!wroteSomething, "sensor>=100 legacy path writes nothing (known bug)",
              wroteSomething ? "now writes — bug fixed, update test" : "still empty");
    }

    fs::remove_all(base);
    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
