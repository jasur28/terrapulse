#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>

// WaveformService — reads recorded waveforms from the TDS miniSEED archive for
// the operator console's review panel (the GUI side of tpdump). Given a sensor
// and a time window, returns decimated X/Y/Z traces ready to plot. Read-only;
// the archive is written by tpacq/tpslink.
class WaveformService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
public:
    explicit WaveformService(QString tdsRoot, QObject* parent = nullptr);

    bool available() const { return !m_root.isEmpty(); }

    // Load X/Y/Z for [startMs, endMs] (ms, Unix). Returns a map:
    //   ok, object, sensor, startMs, endMs, rate, n,
    //   x/y/z: [double] (physical units, gal), min/max per axis.
    // Points per axis are min/max-decimated down to ~maxPoints to keep peaks.
    Q_INVOKABLE QVariantMap load(int object, int sensor,
                                 qlonglong startMs, qlonglong endMs,
                                 int maxPoints = 1200) const;

private:
    QString m_root;
};
