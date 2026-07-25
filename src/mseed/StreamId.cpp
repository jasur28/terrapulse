#include "mseed/StreamId.h"

#include <libmseed.h>

#include <cctype>
#include <cstdio>

namespace tp::mseed {

char bandCode(double sampleRate, double cornerPeriodSec) {
    const bool broadband = cornerPeriodSec >= kBroadbandCornerSec;
    if (sampleRate >= 80.0) return broadband ? 'H' : 'E';
    if (sampleRate >= 10.0) return broadband ? 'B' : 'S';
    if (sampleRate >= 1.0)  return 'M';
    return 'L';
}

char instrumentCode(Instrument kind) {
    return kind == Instrument::Accelerometer ? 'N' : 'H';
}

char orientationCode(Instrument kind, int component) {
    if (kind == Instrument::Seismometer) {
        // Classic vertical-first ordering for a 3-component seismometer.
        switch (component) { case 0: return 'Z'; case 1: return 'N'; case 2: return 'E'; }
        return 'Z';
    }
    switch (component) { case 0: return 'X'; case 1: return 'Y'; case 2: return 'Z'; }
    return 'Z';
}

std::string StreamId::sourceId() const {
    char sid[64];
    if (ms_nslc2sid(sid, sizeof sid, 0,
                    network.c_str(), station.c_str(),
                    location.c_str(), channel.c_str()) < 0) {
        // Should not happen for a validated id, but stay deterministic.
        return "FDSN:" + network + "_" + station + "_" + location + "_" + channel;
    }
    return std::string(sid);
}

std::string StreamId::fileStem() const {
    return network + "_" + station + "_" + location + "_" + channel;
}

namespace {

// A SEED code field may hold letters and digits only. Empty network/location
// are legal in SEED, but we require them here so an id is never ambiguous.
bool cleanCode(const std::string& s, std::size_t maxLen, bool exactLen,
               std::string& why, const char* field) {
    if (s.empty()) { why = std::string(field) + " is empty"; return false; }
    if (exactLen ? s.size() != maxLen : s.size() > maxLen) {
        char buf[128];
        std::snprintf(buf, sizeof buf,
                      "%s '%s' is %zu chars, miniSEED v2 allows %s%zu",
                      field, s.c_str(), s.size(), exactLen ? "" : "up to ", maxLen);
        why = buf;
        return false;
    }
    for (char c : s)
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            why = std::string(field) + " '" + s + "' has a non-alphanumeric character";
            return false;
        }
    return true;
}

} // namespace

StreamIdResult makeStreamId(const std::string& network,
                            const std::string& station,
                            unsigned position,
                            Instrument kind,
                            double sampleRate,
                            double cornerPeriodSec,
                            int component) {
    StreamIdResult r;

    if (sampleRate <= 0.0) { r.error = "sample rate must be positive"; return r; }
    if (component < 0 || component > 2) { r.error = "component must be 0, 1 or 2"; return r; }

    // Position is the field that overflows first: 0..99 fit in two chars, 100
    // does not. Reject here, at registration, not at the archive on day 90.
    if (position > 99) {
        r.error = "position " + std::to_string(position) +
                  " does not fit miniSEED v2 location (max 99)";
        return r;
    }
    char loc[4];
    std::snprintf(loc, sizeof loc, "%02u", position);

    StreamId id;
    id.network  = network;
    id.station  = station;
    id.location = loc;
    id.channel  = std::string{}
                + bandCode(sampleRate, cornerPeriodSec)
                + instrumentCode(kind)
                + orientationCode(kind, component);

    std::string why;
    if (!cleanCode(id.network,  SeedLimits::network,  /*exact=*/false, why, "network")  ) { r.error = why; return r; }
    if (!cleanCode(id.station,  SeedLimits::station,  /*exact=*/false, why, "station")  ) { r.error = why; return r; }
    if (!cleanCode(id.location, SeedLimits::location, /*exact=*/false, why, "location") ) { r.error = why; return r; }
    if (!cleanCode(id.channel,  SeedLimits::channel,  /*exact=*/true,  why, "channel")  ) { r.error = why; return r; }

    r.ok = true;
    r.id = id;
    return r;
}

} // namespace tp::mseed
