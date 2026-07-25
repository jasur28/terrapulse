// tpinv — TerraPulse inventory tool.
// Loads an inventory description (JSON: structures -> sensors -> channels) and
// publishes it to tpmaster as add-notifiers on the INVENTORY group. tpmaster's
// dbstore persists each object (write-before-notify); every client can build its
// own copy of the data model by applying the same notifiers.
//
//   Usage:  tpinv --file inventory.json [--master 127.0.0.1] [--remove]

#include "terrapulse/client/application.h"
#include "terrapulse/messaging/notifier.h"
#include "core/Inventory.h"

#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <cstdio>
#include <fstream>

using json = nlohmann::json;

namespace {

QString qstr(const json& j, const char* k, const char* def = "") {
    return QString::fromStdString(j.value(k, std::string(def)));
}

// One-shot publisher: flatten the inventory tree into notifiers, publish them
// after the slow-joiner settle, then quit.
class InvApplication : public tp::client::Application {
public:
    InvApplication(tp::client::ApplicationSettings settings, json root, tp::messaging::Op op)
        : Application(std::move(settings)), m_root(std::move(root)), m_op(op) {}

    bool init() override {
        if (!Application::init()) return false;
        publishThenQuit([this]() { publishInventory(); });
        return true;
    }

private:
    void publishInventory() {
        int nStruct = 0, nSensor = 0, nChannel = 0;
        for (const auto& js : m_root.value("structures", json::array())) {
            tp::inv::Structure st;
            st.objectId    = js.value("objectId", 0);
            st.name        = qstr(js, "name");
            st.lat         = js.value("lat", 0.0);
            st.lon         = js.value("lon", 0.0);
            st.description = qstr(js, "description");
            // FDSN codes; optional, default network "TP" and empty stationCode
            // (derived from the numeric id) keep old inventory files working.
            st.network     = qstr(js, "network", "TP");
            st.stationCode = qstr(js, "stationCode");
            publish(tp::messaging::notifier(m_op, "structure", st.key(), st.toVariant()));
            ++nStruct;

            for (const auto& jse : js.value("sensors", json::array())) {
                tp::inv::Sensor sn;
                sn.objectId = st.objectId;
                sn.sensorId = jse.value("sensorId", 0);
                sn.model    = qstr(jse, "model");
                sn.location = qstr(jse, "location");
                // Optional: default to a DC-coupled accelerometer when absent,
                // so existing inventory files keep working unchanged.
                sn.sensorKind   = qstr(jse, "kind", "accelerometer");
                sn.cornerPeriod = jse.value("cornerPeriod", 1e9);
                publish(tp::messaging::notifier(m_op, "sensor", sn.key(), sn.toVariant()));
                ++nSensor;

                for (const auto& jc : jse.value("channels", json::array())) {
                    tp::inv::Channel c;
                    c.objectId   = st.objectId;
                    c.sensorId   = sn.sensorId;
                    c.component  = jc.value("component", 0);
                    c.sampleRate = jc.value("sampleRate", 0);
                    c.unit       = qstr(jc, "unit");
                    c.gain       = jc.value("gain", 1.0);
                    publish(tp::messaging::notifier(m_op, "channel", c.key(), c.toVariant()));
                    ++nChannel;
                }
            }
        }
        std::printf("[tpinv] %s -> %s : %d structures, %d sensors, %d channels\n",
                    tp::messaging::opName(m_op), messagingUrl().toUtf8().constData(),
                    nStruct, nSensor, nChannel);
        std::fflush(stdout);
    }

    json m_root;
    tp::messaging::Op m_op;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpinv");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse inventory tool");
    parser.addHelpOption();
    QCommandLineOption fileOpt  ({"f", "file"},   "Inventory JSON file", "path");
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host", "127.0.0.1");
    QCommandLineOption queueOpt ("queue", "Queue: production | playback", "name", "production");
    QCommandLineOption removeOpt("remove", "Send remove-notifiers instead of add");
    parser.addOptions({fileOpt, masterOpt, queueOpt, removeOpt});
    parser.process(app);

    if (!parser.isSet(fileOpt)) {
        std::fprintf(stderr, "tpinv: --file <inventory.json> is required\n");
        return 2;
    }

    json root;
    try {
        std::ifstream f(parser.value(fileOpt).toStdString());
        if (!f) { std::fprintf(stderr, "tpinv: cannot open '%s'\n",
                               parser.value(fileOpt).toUtf8().constData()); return 1; }
        f >> root;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "tpinv: JSON error: %s\n", e.what());
        return 1;
    }

    tp::client::ApplicationSettings settings;
    settings.moduleName = "tpinv";
    settings.masterHost = parser.value(masterOpt);
    settings.queue      = parser.value(queueOpt);
    settings.sohIntervalSeconds = 0;        // one-shot: no heartbeat

    InvApplication inv(std::move(settings), std::move(root),
                       parser.isSet(removeOpt) ? tp::messaging::Op::Remove
                                               : tp::messaging::Op::Add);
    return inv.exec();
}
