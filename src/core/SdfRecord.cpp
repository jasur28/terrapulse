#include "core/SdfRecord.h"
#include "core/Crc32.h"

namespace tp {

void SdfRecord::updateCrc() {
    crc = tp::crc32(waveformData.data(), waveformData.size() * sizeof(float));
}

bool SdfRecord::verifyCrc() const {
    return crc == tp::crc32(waveformData.data(), waveformData.size() * sizeof(float));
}

} // namespace tp
