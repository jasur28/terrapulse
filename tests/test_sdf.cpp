#include <QtTest/QtTest>
#include "core/SdfRecord.h"
#include "storage/SdfFile.h"
#include <QTemporaryFile>

using namespace tp;

class TestSdf : public QObject {
    Q_OBJECT
private slots:
    void crcRoundTrip() {
        SdfRecord r;
        r.waveformData = {1.0f, 2.0f, 3.0f, -1.5f};
        r.sampleCount  = 4;
        r.updateCrc();
        QVERIFY(r.verifyCrc());
        r.waveformData[0] = 99.0f;
        QVERIFY(!r.verifyCrc()); // corruption detected
    }

    void fileRoundTrip() {
        SdfRecord src;
        src.stationId   = 7;
        src.objectId    = 3;
        src.sensorId    = 12;
        src.component   = Axis::Z;
        src.samplingRate = 200;
        src.startTime   = 1700000000000LL;
        src.endTime     = 1700000001000LL;
        src.temperature = 22.5f;
        src.batteryLevel = 87;
        for (int i = 0; i < 200; ++i)
            src.waveformData.push_back(static_cast<float>(i) * 0.1f);
        src.sampleCount = 200;
        src.updateCrc();

        QTemporaryFile tmp;
        tmp.open();
        QString path = tmp.fileName();
        tmp.close();

        QVERIFY(SdfFile::write(path, src));
        auto dst = SdfFile::read(path);
        QVERIFY(dst.has_value());
        QCOMPARE(dst->stationId, src.stationId);
        QCOMPARE(dst->sensorId, src.sensorId);
        QCOMPARE(dst->samplingRate, src.samplingRate);
        QCOMPARE(dst->waveformData.size(), src.waveformData.size());
        QVERIFY(dst->verifyCrc());
    }

    void multiRecordFile() {
        std::vector<SdfRecord> records(3);
        for (int i = 0; i < 3; ++i) {
            records[i].sensorId = static_cast<uint32_t>(i + 1);
            records[i].waveformData = {1.0f, float(i)};
            records[i].sampleCount = 2;
            records[i].updateCrc();
        }
        QTemporaryFile tmp;
        tmp.open();
        QString path = tmp.fileName();
        tmp.close();
        QVERIFY(SdfFile::writeAll(path, records));
        auto loaded = SdfFile::readAll(path);
        QCOMPARE(loaded.size(), 3u);
        for (int i = 0; i < 3; ++i)
            QVERIFY(loaded[i].verifyCrc());
    }
};

QTEST_MAIN(TestSdf)
#include "test_sdf.moc"
