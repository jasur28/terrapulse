#pragma once
#include <cstdint>
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
