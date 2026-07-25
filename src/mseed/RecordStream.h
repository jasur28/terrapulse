#pragma once
#include "mseed/Mseed.h"

#include <cstdint>
#include <string>
#include <vector>

// RecordStream — the one place modules read archived waveforms from, addressed
// by a URI (like SeisComp's RecordStream: file://, sdsarchive://, ...). tpimport
// replays from it; the console reviews recorded traces through it. Live data
// still arrives over the bus — this covers file/archive sources.
//
//   file://<path.mseed>              one miniSEED file (may hold many source ids)
//   tds://<root>                     whole TDS archive (all channels found)
//   tds://<root>?obj=1&sen=1         one sensor's channels from a TDS archive
//   csv://<path>[?obj=1&sen=1&rate=200]
//                                    the device's own SD-card logs (t_ms,x,y,z);
//                                    <path> may be one file or a directory of them
//   combined://(<uri>,<uri>,...)     merge several sources (e.g. archive + flash)
//   <bare path>                      dir -> tds://, file -> file://
namespace tp::mseed {

// A contiguous run of samples for one source id (concatenated across day files,
// ordered by time). gapMs>0 means at least one timing gap was bridged.
struct Trace {
    std::string          sid;
    double               sampleRate  = 0.0;
    int64_t              startTimeMs = 0;
    std::vector<int32_t> samples;
    int                  gaps        = 0;
    int64_t              endTimeMs() const {
        return sampleRate > 0
            ? startTimeMs + int64_t(samples.size() * 1000.0 / sampleRate)
            : startTimeMs;
    }
};

// Read one channel (all day files) from a TDS root by exact source id.
Trace readTdsChannel(const std::string& root, const std::string& sid);

// Read every source id found in a single miniSEED file.
std::vector<Trace> readFile(const std::string& path);

// Read the accelerometer's own CSV logs (t_ms,x,y,z in gal) into X/Y/Z traces,
// so data recovered from an SD card joins the same pipeline as live data.
// `path` may be a single file or a directory (files are read in name order).
std::vector<Trace> readCsv(const std::string& path, uint32_t object, uint32_t sensor,
                           double sampleRate);

// Dispatch on the URI scheme. Returns one Trace per source id.
std::vector<Trace> readStream(const std::string& uri);

} // namespace tp::mseed
