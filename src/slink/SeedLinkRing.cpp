#include "slink/SeedLinkRing.h"

#include <cstring>

namespace tp::slink {

SeedLinkRing::SeedLinkRing(std::size_t capacityPackets)
    : m_slots(capacityPackets ? capacityPackets : 1),
      m_cap(capacityPackets ? capacityPackets : 1) {}

void SeedLinkRing::writeSeqDigits(uint32_t seq, char out6[6]) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 5; i >= 0; --i) { out6[i] = hex[seq & 0xF]; seq >>= 4; }
}

uint32_t SeedLinkRing::push(const char* record512) {
    Slot& s = m_slots[m_head];
    s.abs = m_produced;
    s.data[0] = 'S';
    s.data[1] = 'L';
    writeSeqDigits(uint32_t(m_produced & kSeqMask), s.data + 2);
    std::memcpy(s.data + kHeaderSize, record512, kRecordSize);

    const uint32_t seq = uint32_t(m_produced & kSeqMask);
    m_head = (m_head + 1) % m_cap;
    if (m_count < m_cap) ++m_count;
    ++m_produced;
    return seq;
}

ReadResult SeedLinkRing::packet(uint32_t seq, char* out) const {
    if (m_count == 0) return ReadResult::Future;

    // Modular position of `seq` relative to the oldest retained packet. The
    // retained window is [oldestAbs, m_produced); anything within m_count is
    // present, anything "a little ahead" is Future, the rest has aged out.
    const uint64_t oldestAbs  = m_produced - m_count;
    const uint32_t oldestWire = uint32_t(oldestAbs & kSeqMask);
    const uint32_t fwd        = (seq - oldestWire) & kSeqMask;

    if (fwd < m_count) {
        const uint64_t abs = oldestAbs + fwd;
        const Slot& s = m_slots[abs % m_cap];
        if (s.abs != abs) return ReadResult::TooOld;   // defensive: overwritten
        std::memcpy(out, s.data, kPacketSize);
        return ReadResult::Ok;
    }
    // Outside the window: near-ahead is the future, far-ahead is really behind
    // (24-bit wrap), i.e. already overwritten.
    return (fwd < (kSeqMask / 2)) ? ReadResult::Future : ReadResult::TooOld;
}

} // namespace tp::slink
