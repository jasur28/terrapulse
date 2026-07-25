#pragma once

#include <QString>

namespace tp::datamodel {

struct WaveformStreamID {
    QString networkCode = "TP";
    QString stationCode;
    QString locationCode;
    QString channelCode;
    QString resourceURI;

    QString streamID() const {
        return QString("%1.%2.%3.%4")
            .arg(networkCode, stationCode, locationCode, channelCode);
    }

    bool isValid() const {
        return !stationCode.isEmpty() && !channelCode.isEmpty();
    }
};

} // namespace tp::datamodel
