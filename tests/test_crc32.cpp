#include <QtTest/QtTest>
#include "core/Crc32.h"

class TestCrc32 : public QObject {
    Q_OBJECT
private slots:
    void knownValue() {
        // CRC32 of "123456789" = 0xCBF43926
        const char* data = "123456789";
        uint32_t result = tp::crc32(data, 9);
        QCOMPARE(result, 0xCBF43926u);
    }

    void emptyData() {
        uint32_t result = tp::crc32(nullptr, 0);
        QCOMPARE(result, 0x00000000u);
    }

    void roundTrip() {
        const char* msg = "TerraPulse SDF Record";
        uint32_t crc = tp::crc32(msg, 21);
        QVERIFY(crc != 0);
        QCOMPARE(tp::crc32(msg, 21), crc);
    }
};

QTEST_MAIN(TestCrc32)
#include "test_crc32.moc"
