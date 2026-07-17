#include <QtTest/QtTest>
#include "core/SafRecord.h"
#include "storage/SafFile.h"
#include <QTemporaryFile>

using namespace tp;

class TestSaf : public QObject {
    Q_OBJECT
private slots:
    void crcRoundTrip() {
        SafRecord r;
        r.safId  = 42;
        r.rms    = 3.14f;
        r.energy = 9.87f;
        r.updateCrc();
        QVERIFY(r.verifyCrc());
        r.rms = 99.0f;
        QVERIFY(!r.verifyCrc());
    }

    void fileRoundTrip() {
        SafRecord src;
        src.safId             = 1001;
        src.objectId          = 2;
        src.sensorId          = 5;
        src.component         = Axis::Y;
        src.rms               = 5.5f;
        src.dominantFrequency = 12.3f;
        src.healthIndex       = 0.72f;
        src.anomalyDetected   = true;
        src.anomalyType       = AnomalyType::Vibration;
        src.warningLevel      = WarningLevel::Warning;
        src.confidenceLevel   = 0.80f;
        src.updateCrc();

        QTemporaryFile tmp; tmp.open();
        QString path = tmp.fileName(); tmp.close();
        QVERIFY(SafFile::write(path, src));
        auto dst = SafFile::read(path);
        QVERIFY(dst.has_value());
        QCOMPARE(dst->safId, src.safId);
        QCOMPARE(dst->anomalyType, src.anomalyType);
        QVERIFY(std::abs(dst->healthIndex - src.healthIndex) < 0.001f);
        QVERIFY(dst->verifyCrc());
    }
};

QTEST_MAIN(TestSaf)
#include "test_saf.moc"
