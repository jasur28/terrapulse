#pragma once
#include "mseed/Mseed.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// TdsArchive — TerraPulse Data Store: writes raw waveforms as miniSEED files on
// disk, one file per channel per day (like SeisComp's SDS archive). tpacq feeds
// it samples; tpstream/tpimport read it back for waveform review and replay.
// Works for ANY source (our STM32 via the hub converter, or third-party miniSEED).
namespace tp::mseed {

class TdsArchive {
public:
    // recordSamples = how many samples to buffer before packing miniSEED. It
    // trades archive size against write latency: a 512-byte record is only ~20%
    // full at 200, so the archive was LARGER than raw data. Measured bytes/sample
    // (clean data, ЗАМЕРЫ §1): 200 -> 2.56, 1000 -> 1.54, 2000 -> 1.28 (raw 4.00).
    // Default 1000 (~5 s at 200 Hz) is the knee: most of the gain, moderate
    // latency and crash-loss window. Deployments tune it via tpacq --record-samples.
    explicit TdsArchive(std::string root, int recordSamples = 1000, int encoding = Steim2);
    ~TdsArchive();

    // Bind a channel's FDSN source id BEFORE its samples arrive. The caller
    // owns identity resolution: a raw source (tpacq) resolves it from inventory
    // (see StreamId), a SeedLink source (tpslink) passes the id it received —
    // never relabelled. If a channel is never bound, addSample() falls back to
    // the legacy numeric sourceId() so existing behaviour is unchanged.
    void setSourceId(uint32_t objectId, uint32_t sensorId, int component,
                     std::string sid);

    // One integer sample (device counts) for a channel.
    void addSample(uint32_t objectId, uint32_t sensorId, int component,
                   int32_t value, int64_t timeMs, double sampleRate);

    void flushAll();
    uint64_t recordsWritten() const { return m_records; }
    std::string root() const { return m_root; }

private:
    struct Chan {
        std::string          sid;
        double               sampleRate = 0.0;
        int64_t              startMs = 0;   // time of buf[0]
        std::vector<int32_t> buf;
    };

    void flush(Chan& c);
    std::string filePath(const std::string& sid, int64_t timeMs) const;

    std::string                 m_root;
    int                         m_recSamples;
    int                         m_encoding;
    std::map<uint64_t, Chan>    m_chans;     // key = obj<<20 | sen<<4 | comp
    uint64_t                    m_records = 0;
};

} // namespace tp::mseed
