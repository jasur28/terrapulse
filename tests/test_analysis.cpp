#include <QtTest/QtTest>
#include "analysis/AnalysisEngine.h"
#include "simulator/SensorSimulator.h"

using namespace tp;

class TestAnalysis : public QObject {
    Q_OBJECT
private slots:
    void normalScenarioNoAnomaly() {
        SimulatorConfig cfg;
        cfg.scenario     = SimulationScenario::Normal;
        cfg.samplingRate = 100;
        cfg.windowMs     = 1000;
        SensorSimulator sim(cfg);
        AnalysisEngine  eng;

        // Build baseline from first 5 windows
        for (int i = 0; i < 5; ++i) eng.updateBaseline(sim.nextWindow());

        // Normal windows should not trigger anomaly
        bool anyAnomaly = false;
        for (int i = 0; i < 5; ++i) {
            auto sdf = sim.nextWindow();
            auto saf = eng.analyse(sdf, static_cast<uint64_t>(i + 1));
            if (saf.anomalyDetected) anyAnomaly = true;
        }
        QVERIFY(!anyAnomaly);
    }

    void overloadScenarioCritical() {
        SimulatorConfig cfg;
        cfg.scenario     = SimulationScenario::Overload;
        cfg.samplingRate = 100;
        cfg.windowMs     = 1000;
        cfg.noiseLevel   = 0.1f;
        SensorSimulator sim(cfg);
        AnalysisEngine  eng;

        auto sdf = sim.nextWindow();
        auto saf = eng.analyse(sdf, 1);
        QVERIFY(saf.anomalyDetected);
        QCOMPARE(saf.warningLevel, WarningLevel::Critical);
        QCOMPARE(saf.anomalyType, AnomalyType::Overload);
    }

    void healthIndexRange() {
        SimulatorConfig cfg;
        cfg.samplingRate = 100;
        cfg.windowMs     = 1000;
        SensorSimulator sim(cfg);
        AnalysisEngine  eng;

        for (auto scenario : {SimulationScenario::Normal, SimulationScenario::Vibration,
                               SimulationScenario::Overload}) {
            sim.setScenario(scenario);
            auto sdf = sim.nextWindow();
            auto saf = eng.analyse(sdf, 1);
            QVERIFY(saf.healthIndex >= 0.0f);
            QVERIFY(saf.healthIndex <= 1.0f);
        }
    }
};

QTEST_MAIN(TestAnalysis)
#include "test_analysis.moc"
