#include <QtTest/QtTest>
#include "core/ShfRecord.h"
#include "storage/ShfFile.h"
#include <QTemporaryFile>

using namespace tp;

class TestShf : public QObject {
    Q_OBJECT
private slots:
    void crcRoundTrip() {
        ShfRecord r;
        r.shfId       = 7;
        r.anomalyType = AnomalyType::Crack;
        r.maxValue    = 110.0f;
        r.notes       = "Test note";
        r.timeline    = {{1, 1700000000000LL}, {2, 1700000001000LL}};
        r.updateCrc();
        QVERIFY(r.verifyCrc());
        r.notes = "Modified";
        QVERIFY(!r.verifyCrc());
    }

    void fileRoundTrip() {
        ShfRecord src;
        src.shfId          = 55;
        src.objectId       = 1;
        src.sensorId       = 3;
        src.anomalyType    = AnomalyType::Settlement;
        src.severityLevel  = SeverityLevel::Medium;
        src.trend          = Trend::Worsening;
        src.notes          = "Observed during maintenance";
        src.timeline       = {{10, 1700000000000LL}};
        src.sampleCount    = 1;
        src.updateCrc();

        QTemporaryFile tmp; tmp.open();
        QString path = tmp.fileName(); tmp.close();
        QVERIFY(ShfFile::write(path, src));
        auto dst = ShfFile::read(path);
        QVERIFY(dst.has_value());
        QCOMPARE(dst->shfId, src.shfId);
        QCOMPARE(dst->notes, src.notes);
        QCOMPARE(dst->trend, src.trend);
        QVERIFY(dst->verifyCrc());
    }
};

QTEST_MAIN(TestShf)
#include "test_shf.moc"
