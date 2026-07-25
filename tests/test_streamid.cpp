// test_streamid — locks the six paper-checked cases for channel identity.
// This is the first test wired into the build. It encodes the decisions from
// docs/АРХИТЕКТУРА.md §14 so a future change that reintroduces the hard-wired
// "HN" channel code, or lets a position >= 100 through, fails here instead of
// three months into an archive.

#include "mseed/StreamId.h"

#include <cstdio>
#include <string>

using namespace tp::mseed;

static int failures = 0;

static void expectOk(const char* what, const StreamIdResult& r, const std::string& sid) {
    if (!r.ok) {
        std::printf("FAIL %-40s expected ok, got error: %s\n", what, r.error.c_str());
        ++failures; return;
    }
    if (r.id.sourceId() != sid) {
        std::printf("FAIL %-40s expected %s, got %s\n", what, sid.c_str(), r.id.sourceId().c_str());
        ++failures; return;
    }
    std::printf("ok   %-40s %s\n", what, sid.c_str());
}

static void expectAccepted(const char* what, const StreamIdResult& r) {
    if (!r.ok) {
        std::printf("FAIL %-40s expected ok, got error: %s\n", what, r.error.c_str());
        ++failures; return;
    }
    std::printf("ok   %-40s %s\n", what, r.id.sourceId().c_str());
}

static void expectReject(const char* what, const StreamIdResult& r) {
    if (r.ok) {
        std::printf("FAIL %-40s expected rejection, got %s\n", what, r.id.sourceId().c_str());
        ++failures; return;
    }
    std::printf("ok   %-40s rejected: %s\n", what, r.error.c_str());
}

int main() {
    const double kA = kAccelerometerCorner;   // accelerometer responds to DC
    const double kS = 2.0;                     // SM-3 short-period corner ~2 s

    // Case 1 — accelerometer, 3 axes, 200 Hz -> HNX/HNY/HNZ
    expectOk("accel X", makeStreamId("UZ","BRDG1",7, Instrument::Accelerometer,200,kA,0), "FDSN:UZ_BRDG1_07_H_N_X");
    expectOk("accel Y", makeStreamId("UZ","BRDG1",7, Instrument::Accelerometer,200,kA,1), "FDSN:UZ_BRDG1_07_H_N_Y");
    expectOk("accel Z", makeStreamId("UZ","BRDG1",7, Instrument::Accelerometer,200,kA,2), "FDSN:UZ_BRDG1_07_H_N_Z");

    // Case 2 — SM-3 seismometer (velocity) next to the accelerometer.
    // Different position AND different instrument code: never confused.
    expectOk("SM-3 vertical", makeStreamId("UZ","BRDG1",8, Instrument::Seismometer,200,kS,0), "FDSN:UZ_BRDG1_08_E_H_Z");
    expectOk("SM-3 at 50 Hz", makeStreamId("UZ","BRDG1",8, Instrument::Seismometer,50,kS,0),  "FDSN:UZ_BRDG1_08_S_H_Z");

    // Case 3 — position 99 fits, 100 does not: rejected at registration.
    expectAccepted("position 99",  makeStreamId("UZ","BRDG1",99, Instrument::Accelerometer,200,kA,2));
    expectReject("position 100", makeStreamId("UZ","BRDG1",100,Instrument::Accelerometer,200,kA,2));

    // Case 4 — two Hubs both numbering sensors from 01 do NOT collide:
    // the station code differs, so the source ids differ. (Both codes are 5
    // chars — "TOWERA" would be 6 and is itself illegal, see guard below.)
    {
        auto a = makeStreamId("UZ","BRDG1",1, Instrument::Accelerometer,200,kA,2);
        auto b = makeStreamId("UZ","TOWRA",1, Instrument::Accelerometer,200,kA,2);
        if (a.ok && b.ok && a.id.sourceId() != b.id.sourceId())
            std::printf("ok   %-40s %s != %s\n","two hubs, same sensor no",a.id.sourceId().c_str(),b.id.sourceId().c_str());
        else { std::printf("FAIL two hubs collided\n"); ++failures; }
    }

    // Guard rails the limits exist for. "TOWERA" (6 chars) must be rejected.
    expectReject("station too long", makeStreamId("UZ","TOWERA",1,Instrument::Accelerometer,200,kA,0));
    expectReject("network too long", makeStreamId("UZB","BRDG1",1, Instrument::Accelerometer,200,kA,0));

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures==1?"":"s");
    return failures ? 1 : 0;
}
