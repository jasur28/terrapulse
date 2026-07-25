#include "mseed/Mseed.h"

#include <libmseed.h>

#include <cstdio>
#include <cstring>

namespace tp::mseed {

std::string sourceId(uint32_t objectId, uint32_t sensorId, int component) {
    const char pos = component == 0 ? 'X' : component == 1 ? 'Y' : 'Z';
    char chan[8]; std::snprintf(chan, sizeof chan, "HN%c", pos);
    char sta[16]; std::snprintf(sta, sizeof sta, "%u", objectId);
    char loc[8];  std::snprintf(loc, sizeof loc, "%02u", sensorId);

    char sid[64];
    if (ms_nslc2sid(sid, sizeof sid, 0, "TP", sta, loc, chan) < 0)
        return std::string("FDSN:TP_") + sta + "_" + loc + "_H_N_" + pos;
    return std::string(sid);
}

namespace {
struct SinkCtx { const RecordSink* sink; };
void recordHandler(char* record, int length, void* handlerdata) {
    auto* c = static_cast<SinkCtx*>(handlerdata);
    (*c->sink)(record, length);
}
} // namespace

bool encode(const std::string& sid, double sampleRate, int64_t startTimeMs,
            const std::vector<int32_t>& samples, const RecordSink& sink, int encoding) {
    if (samples.empty()) return false;

    MS3Record* msr = msr3_init(NULL);
    std::strncpy(msr->sid, sid.c_str(), sizeof(msr->sid) - 1);
    msr->reclen      = 512;
    msr->samprate    = sampleRate;
    msr->starttime   = static_cast<nstime_t>(startTimeMs) * 1000000;   // ms -> ns
    msr->encoding    = encoding;
    msr->pubversion  = 1;
    msr->numsamples  = static_cast<int64_t>(samples.size());
    msr->samplecnt   = static_cast<int64_t>(samples.size());
    msr->sampletype  = 'i';
    msr->datasamples = const_cast<int32_t*>(samples.data());

    SinkCtx ctx{ &sink };
    int64_t packedsamples = 0;
    // Pack fixed-length 512-byte miniSEED v2 records: the format SeedLink streams
    // and SDS archives use, so our files interoperate with third-party tooling.
    int64_t nrec = msr3_pack(msr, recordHandler, &ctx, &packedsamples,
                             MSF_FLUSHDATA | MSF_PACKVER2, 0);

    msr->datasamples = nullptr;   // owned by the caller
    msr3_free(&msr);
    return nrec > 0;
}

std::vector<Decoded> decode(const char* buf, std::size_t len) {
    std::vector<Decoded> out;
    std::size_t offset = 0;

    while (offset < len) {
        MS3Record* msr = nullptr;
        int rv = msr3_parse(buf + offset, static_cast<uint64_t>(len - offset),
                            &msr, MSF_UNPACKDATA, 0);
        if (rv != 0 || !msr) break;   // rv > 0: need more bytes; rv < 0: error

        Decoded d;
        d.sid         = msr->sid;
        d.sampleRate  = msr->samprate;
        d.startTimeMs = msr->starttime / 1000000;   // ns -> ms
        if (msr->sampletype == 'i' && msr->datasamples) {
            const int32_t* s = static_cast<const int32_t*>(msr->datasamples);
            d.samples.assign(s, s + msr->numsamples);
        }
        out.push_back(std::move(d));

        const int rl = msr->reclen > 0 ? msr->reclen : 512;
        offset += static_cast<std::size_t>(rl);
        msr3_free(&msr);
    }
    return out;
}

} // namespace tp::mseed
