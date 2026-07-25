#pragma once
#include <cstdint>
#include <string>

// StreamId — the identity of one waveform channel, FDSN scheme
// (network.station.location.channel), the same one carried inside miniSEED.
//
// This is the ONE place that decides a channel's name. It replaces the hard
// wired "HN<axis>" in sourceId(): the channel code is computed from what the
// instrument actually is (accelerometer vs seismometer) and how fast it samples,
// so an SM-3 velocity sensor is never mislabelled as an accelerometer.
//
// It also enforces the hard miniSEED v2 field widths on the way IN — an invalid
// id is rejected when a sensor is registered, not silently dropped by libmseed
// three months into recording. See docs/АРХИТЕКТУРА.md §14 and docs/ЗАМЕРЫ.md §6.
namespace tp::mseed {

// What kind of transducer produced the samples. Decides the SEED instrument
// code: accelerometer -> 'N', seismometer (velocity) -> 'H'.
enum class Instrument {
    Accelerometer,   // measures acceleration  -> instrument code 'N'
    Seismometer,     // measures ground velocity -> instrument code 'H' (e.g. SM-3)
};

// miniSEED v2 field-width limits (libmseed refuses to pack anything longer).
struct SeedLimits {
    static constexpr std::size_t network  = 2;
    static constexpr std::size_t station  = 5;
    static constexpr std::size_t location = 2;
    static constexpr std::size_t channel  = 3;   // exactly 3
};

struct StreamId {
    std::string network;    // region / operator, e.g. "UZ"      (<= 2)
    std::string station;    // building / object,  e.g. "BRDG1"  (<= 5)
    std::string location;   // position on the structure, "07"   (<= 2)
    std::string channel;    // band + instrument + orientation, "HNZ" (== 3)

    // Full FDSN source id as libmseed expects it, e.g. "FDSN:UZ_BRDG1_07_H_N_Z".
    std::string sourceId() const;

    // File-name-safe form for the TDS archive, e.g. "UZ_BRDG1_07_HNZ".
    std::string fileStem() const;
};

// FDSN threshold separating a broadband response from a short-period one.
constexpr double kBroadbandCornerSec = 10.0;

// SEED band code from sample rate and the sensor's corner period. FDSN
// convention (broadband = corner period >= 10 s):
//   >= 80 Hz : broadband 'H', short-period 'E'
//   10..80 Hz: broadband 'B', short-period 'S'
//   1..10  Hz: 'M'   (< 1 Hz: 'L')
//
// The corner period is a physical property of the sensor and lives in the
// inventory. An accelerometer responds down to DC (we subtract gravity from its
// signal), so its corner period is effectively infinite -> broadband -> 'H'.
// A classic short-period SM-3 has a corner near 1.5-2 s -> short-period -> 'E'.
char bandCode(double sampleRate, double cornerPeriodSec);

char instrumentCode(Instrument kind);   // 'N' or 'H'

// SEED orientation letter for a 3-axis component index.
//   Accelerometer: 0->X 1->Y 2->Z
//   Seismometer:   0->Z 1->N 2->E  (classic vertical-first ordering)
char orientationCode(Instrument kind, int component);

// Validation result — carries WHY it failed so the caller can tell the operator.
struct StreamIdResult {
    bool        ok = false;
    StreamId    id;
    std::string error;   // human-readable reason when !ok
};

// Build and validate a StreamId from inventory facts. Rejects (does not throw)
// when any field breaks the v2 limits — e.g. a location >= 100.
//   network:  region code           (1..2 chars)
//   station:  building/object code  (1..5 chars)
//   position: sensor position on the structure (0..99 -> "00".."99")
//   kind:     accelerometer / seismometer
//   sampleRate, cornerPeriodSec: decide the band code (from inventory)
//   component: 0/1/2 axis index
// For a DC-coupled accelerometer pass a corner period >= kBroadbandCornerSec
// (or simply kAccelerometerCorner) to get the 'H'/'B' band.
StreamIdResult makeStreamId(const std::string& network,
                            const std::string& station,
                            unsigned position,
                            Instrument kind,
                            double sampleRate,
                            double cornerPeriodSec,
                            int component);

// Sentinel corner period for a DC-coupled accelerometer (responds to static
// acceleration). Any value >= kBroadbandCornerSec works; this names the intent.
constexpr double kAccelerometerCorner = 1.0e9;

} // namespace tp::mseed
