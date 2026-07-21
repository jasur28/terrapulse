// tpinv — TerraPulse inventory tool.
// Loads an inventory description (JSON: structures -> sensors -> channels) and
// publishes it to tpmaster as add-notifiers on the INVENTORY group. tpmaster's
// dbstore persists each object (write-before-notify); every client can build its
// own copy of the data model by applying the same notifiers.
//
//   Usage:  tpinv --file inventory.json [--master 127.0.0.1] [--remove]

#include "bus/Bus.h"
#include "bus/Notifier.h"
#include "bus/Master.h"
#include "core/Inventory.h"

#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <cstdio>
#include <fstream>

using json = nlohmann::json;

static QString qstr(const json& j, const char* k, const char* def = "") {
    return QString::fromStdString(j.value(k, std::string(def)));
}

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

    const tp::Op op = parser.isSet(removeOpt) ? tp::Op::Remove : tp::Op::Add;
    const auto queue = tp::master::queueFromName(parser.value(queueOpt).toStdString());
    const std::string endpoint = tp::master::in(parser.value(masterOpt).toStdString(), queue);
    tp::Publisher pub(endpoint, /*bind=*/false);

    // Flatten the tree into notifiers. Published after a short delay so the
    // broker's subscription has propagated to this fresh PUB (PUB/SUB slow-joiner).
    QTimer::singleShot(1600, [&]() {
        int nStruct = 0, nSensor = 0, nChannel = 0;
        for (const auto& js : root.value("structures", json::array())) {
            tp::inv::Structure st;
            st.objectId    = js.value("objectId", 0);
            st.name        = qstr(js, "name");
            st.lat         = js.value("lat", 0.0);
            st.lon         = js.value("lon", 0.0);
            st.description = qstr(js, "description");
            pub.publish(tp::notifier(op, "structure", st.key(), st.toVariant()));
            ++nStruct;

            for (const auto& jse : js.value("sensors", json::array())) {
                tp::inv::Sensor sn;
                sn.objectId = st.objectId;
                sn.sensorId = jse.value("sensorId", 0);
                sn.model    = qstr(jse, "model");
                sn.location = qstr(jse, "location");
                pub.publish(tp::notifier(op, "sensor", sn.key(), sn.toVariant()));
                ++nSensor;

                for (const auto& jc : jse.value("channels", json::array())) {
                    tp::inv::Channel c;
                    c.objectId   = st.objectId;
                    c.sensorId   = sn.sensorId;
                    c.component  = jc.value("component", 0);
                    c.sampleRate = jc.value("sampleRate", 0);
                    c.unit       = qstr(jc, "unit");
                    c.gain       = jc.value("gain", 1.0);
                    pub.publish(tp::notifier(op, "channel", c.key(), c.toVariant()));
                    ++nChannel;
                }
            }
        }
        std::printf("[tpinv] %s -> %s : %d structures, %d sensors, %d channels\n",
                    tp::opName(op), endpoint.c_str(), nStruct, nSensor, nChannel);
        std::fflush(stdout);
        // Give ZeroMQ a moment to flush, then quit.
        QTimer::singleShot(400, &app, &QCoreApplication::quit);
    });

    return app.exec();
}
