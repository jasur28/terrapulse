#pragma once
#include <cstdint>
#include <unordered_map>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// Thin C++ wrapper over libmseed — the one place the modules touch miniSEED.
// Encode windows of integer samples into 512-byte miniSEED records and decode
// them back. Lets tpacq archive/stream and tpimport replay a vendor-neutral
// waveform format (any miniSEED-capable device interoperates).
namespace tp::mseed {

// libmseed data encodings we use.
constexpr int Steim2 = 11;   // DE_STEIM2 — lossless compression (default)
constexpr int Int32  = 3;    // DE_INT32  — uncompressed

// FDSN source id from our identity, e.g. objectId=1 sensorId=1 component Z ->
// "FDSN:TP_1_01_H_N_Z". Channel = band H, instrument N (accelerometer),
// orientation X/Y/Z.
std::string sourceId(uint32_t objectId, uint32_t sensorId, int component);

// Inverse of sourceId for our legacy numeric ids: parse object/sensor/component
// back out of a source id like "FDSN:TP_1_01_H_N_Z" (station=object,
// location=sensor, orientation last). Returns false if it doesn't parse as one
// of our numeric ids. Component: X/1->0, Y/N/2->1, Z/3->2.
bool parseSourceId(const std::string& sid, uint32_t& object, uint32_t& sensor, int& component);

// Resolve a source id to (object, sensor, component) for BOTH id styles:
//   * legacy numeric  "FDSN:TP_1_07_H_N_Z"      -> object 1  (station is numeric)
//   * FDSN            "FDSN:UZ_BRDG1_07_H_N_Z"  -> object from stationToObject
// The sensor is always the (numeric) location field; only the station->object
// step needs the map. Key format in the map is "<network>_<station>", e.g.
// "UZ_BRDG1". Returns false only if genuinely unresolvable (bad location, or an
// unknown non-numeric station) — the caller must NOT silently drop such records.
bool resolveSourceId(const std::string& sid,
                     const std::unordered_map<std::string, uint32_t>& stationToObject,
                     uint32_t& object, uint32_t& sensor, int& component);

// Each packed record is handed to the sink (bytes are only valid during the call).
using RecordSink = std::function<void(const char* record, int length)>;

// Encode one window. startTimeMs = Unix ms of the first sample. Returns true if
// at least one record was produced.
bool encode(const std::string& sid, double sampleRate, int64_t startTimeMs,
            const std::vector<int32_t>& samples, const RecordSink& sink,
            int encoding = Steim2);

struct Decoded {
    std::string          sid;
    double               sampleRate  = 0.0;
    int64_t              startTimeMs = 0;
    std::vector<int32_t> samples;
};

// Decode every record found in [buf, buf+len).
std::vector<Decoded> decode(const char* buf, std::size_t len);

} // namespace tp::mseed
