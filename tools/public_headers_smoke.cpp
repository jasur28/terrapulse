#include "terrapulse/client.h"
#include "terrapulse/broker/client.h"
#include "terrapulse/broker/group.h"
#include "terrapulse/broker/hashset.h"
#include "terrapulse/broker/message.h"
#include "terrapulse/broker/messagedispatcher.h"
#include "terrapulse/broker/messageprocessor.h"
#include "terrapulse/broker/processor.h"
#include "terrapulse/broker/protocol.h"
#include "terrapulse/broker/queue.h"
#include "terrapulse/broker/statistics.h"
#include "terrapulse/broker/utils/circular.h"
#include "terrapulse/broker/utils/utils.h"
#include "terrapulse/core.h"
#include "terrapulse/config/config.h"
#include "terrapulse/datamodel/dataavailability.h"
#include "terrapulse/datamodel/event.h"
#include "terrapulse/datamodel/inventory.h"
#include "terrapulse/datamodel/journalentry.h"
#include "terrapulse/datamodel/notifier.h"
#include "terrapulse/datamodel/qualitycontrol.h"
#include "terrapulse/datamodel/strongmotion/strongmotionparameters.h"
#include "terrapulse/geo.h"
#include "terrapulse/gui/core/application.h"
#include "terrapulse/gui/core/connectionstatelabel.h"
#include "terrapulse/gui/core/recordpolyline.h"
#include "terrapulse/gui/core/recordview.h"
#include "terrapulse/gui/core/timescale.h"
#include "terrapulse/gui/datamodel/eventdatarepository.h"
#include "terrapulse/gui/datamodel/eventlistview.h"
#include "terrapulse/gui/datamodel/inventorylistview.h"
#include "terrapulse/gui/datamodel/stationdata.h"
#include "terrapulse/gui/datamodel/stationsymbol.h"
#include "terrapulse/gui/plot.h"
#include "terrapulse/gui/map/mapwidget.h"
#include "terrapulse/io/archive/jsonarchive.h"
#include "terrapulse/io/recordfilter/crop.h"
#include "terrapulse/io/recordinput.h"
#include "terrapulse/io/recordstream/memory.h"
#include "terrapulse/io/recordstream.h"
#include "terrapulse/logging/log.h"
#include "terrapulse/math/spectrum.h"
#include "terrapulse/math/filter.h"
#include "terrapulse/messaging/connection.h"
#include "terrapulse/messaging/message.h"
#include "terrapulse/messaging/packet.h"
#include "terrapulse/messaging/status.h"
#include "terrapulse/processing/processor.h"
#include "terrapulse/processing/pipeline.h"
#include "terrapulse/processing/strongmotion.h"
#include "terrapulse/processing/streambuffer.h"
#include "terrapulse/processing/timewindowprocessor.h"
#include "terrapulse/processing/waveformprocessor.h"
#include "terrapulse/qc/qcprocessor.h"
#include "terrapulse/qc/soh.h"
#include "terrapulse/system/environment.h"
#include "terrapulse/system/settings.h"
#include "terrapulse/utils/keyvalues.h"
#include "terrapulse/utils/units.h"
#include "terrapulse/utils/url.h"
#include "terrapulse/version.h"

int main() {
    const auto v = tp::version();
    tp::core::AccelerationRecord record({1.0, 2.0, 3.0});
    record.setStream("TP", "OBJ1", "S01", "X");
    record.setStartTime(tp::core::Time::fromMSecsSinceEpoch(1000));
    record.setSampling(3, 100.0);
    tp::core::TimeWindow window = record.timeWindow();
    tp::core::JsonArchive archive({}, tp::core::Archive::Mode::Write);
    QVariant value = QStringLiteral("ok");
    archive.value(QStringLiteral("state"), value);
    tp::client::Application app({QStringLiteral("headers_smoke")});
    app.addMessagingSubscription("raw.");
    tp::client::ThreadedQueue<int> threadedQueue(4);
    threadedQueue.push(7);
    tp::client::ObjectMonitor monitor(60);
    auto* log = monitor.add("records", "raw");
    log->push(tp::core::Time::now(), 2);
    tp::client::Inventory::instance().setStructures({tp::inv::Structure{1, "Bridge", 41.0, 69.0, "test"}});
    const auto* structure = tp::client::Inventory::instance().structure(1);
    tp::config::Config cfg;
    cfg.setInt("profiles.A.priority", 4);
    cfg.setBool("profiles.A.enabled", true);
    cfg.setString("module", "tpqc");
    auto symbols = cfg.findSymbols("profiles.", "enabled");
    tp::broker::QueueStatistics stats;
    tp::broker::Queue queue("headers", 1024);
    tp::broker::utils::CircularBuffer<int> ring(4);
    ring.pushBack(1);
    tp::datamodel::Inventory dataInventory;
    auto dmStructure = std::make_shared<tp::datamodel::Structure>("tp/structure/bridge-a");
    dmStructure->code = "BRGA";
    dmStructure->name = "Bridge A";
    dataInventory.addStructure(dmStructure);
    auto dmSensor = std::make_shared<tp::datamodel::Sensor>("tp/sensor/acc-1");
    dmSensor->code = "ACC1";
    dataInventory.addSensor(dmStructure->publicID(), dmSensor);
    tp::datamodel::Notifier::Clear();
    tp::datamodel::Notifier::Create(dataInventory.publicID(), tp::datamodel::Operation::Add, dmStructure);
    auto notifiers = tp::datamodel::Notifier::Take();
    tp::datamodel::QualityControl qc;
    auto quality = std::make_shared<tp::datamodel::WaveformQuality>("tp/qc/latency/1");
    quality->waveformID.stationCode = "BRGA";
    quality->waveformID.locationCode = "ACC1";
    quality->waveformID.channelCode = "X";
    quality->type = "latency";
    quality->parameter = "seconds";
    quality->value = 0.2;
    qc.add(quality);
    tp::datamodel::DataAvailability availability;
    auto extent = std::make_shared<tp::datamodel::DataExtent>("tp/availability/1");
    extent->waveformID = quality->waveformID;
    extent->availability = 99.9;
    availability.add(extent);
    tp::datamodel::JournalEntry journal("tp/journal/1");
    journal.objectID = dmStructure->publicID();
    journal.action = "operator-note";
    tp::datamodel::EventParameters eventParameters;
    auto event = std::make_shared<tp::datamodel::Event>("tp/event/1");
    event->structureID = dmStructure->publicID();
    event->severity = tp::SeverityLevel::Medium;
    eventParameters.add(event);
    tp::datamodel::strongmotion::StrongMotionParameters smp;
    auto smRecord = std::make_shared<tp::datamodel::strongmotion::Record>("tp/strongmotion/record/1");
    smRecord->eventID = event->publicID();
    smRecord->waveformID = quality->waveformID;
    tp::datamodel::strongmotion::PeakMotion peak;
    peak.waveformID = quality->waveformID;
    peak.pga = 0.18;
    smRecord->peaks.push_back(peak);
    smp.add(smRecord);
    tp::geo::Coordinate coordinate(41.0, 181.0);
    coordinate.normalize();
    tp::geo::BoundingBox bbox(40.0, 68.0, 42.0, 70.0);
    auto geoFeature = std::make_shared<tp::geo::Feature>("Bridge zone");
    geoFeature->addVertex(40.0, 68.0);
    geoFeature->addVertex(40.0, 70.0);
    geoFeature->addVertex(42.0, 70.0);
    geoFeature->addVertex(42.0, 68.0);
    geoFeature->setClosedPolygon(true);
    tp::geo::FeatureSet featureSet;
    featureSet.addFeature(geoFeature);
    tp::geo::index::QuadTree geoIndex;
    geoIndex.add(featureSet);
    int geoHits = 0;
    geoIndex.query(bbox.center(), [&geoHits](const tp::geo::Feature*) {
        ++geoHits;
        return true;
    });
    tp::io::MemoryRecordStream memoryStream;
    auto memoryRecord = std::make_unique<tp::core::AccelerationRecord>(std::vector<double>{0.1, 0.2});
    memoryRecord->setStream("TP", "BRGA", "ACC1", "X");
    memoryRecord->setStartTime(tp::core::Time::fromMSecsSinceEpoch(2000));
    memoryRecord->setSampling(2, 100.0);
    auto memoryRecordCopy = std::shared_ptr<tp::core::Record>(memoryRecord->copy().release());
    memoryStream.push(std::move(memoryRecord));
    tp::io::RecordInput input(&memoryStream);
    int ioRecords = 0;
    for (auto it = input.begin(); it != input.end(); ++it) {
        ioRecords += (*it) ? 1 : 0;
    }
    tp::gui::ApplicationContext guiContext("tpmap");
    tp::gui::ConnectionStateLabel connectionLabel;
    connectionLabel.setState(tp::gui::ConnectionState::Connected);
    tp::gui::RecordViewModel recordView;
    recordView.addRecord(std::make_shared<tp::core::AccelerationRecord>(std::vector<double>{1.0, -1.0, 0.5}));
    tp::gui::TimeScale timeScale;
    timeScale.setTimeWindow(tp::core::TimeWindow(tp::core::Time::fromMSecsSinceEpoch(0), tp::core::TimeSpan::seconds(10)));
    auto points = tp::gui::buildRecordPolyline(record, 100.0, 40.0);
    tp::gui::datamodel::EventListModel eventListModel;
    tp::gui::datamodel::InventoryListModel inventoryListModel;
    inventoryListModel.setInventory(dataInventory);
    tp::gui::datamodel::StationDataModel stationDataModel;
    stationDataModel.setInventory(dataInventory);
    tp::gui::datamodel::GroundMotionHandler groundMotionHandler;
    tp::gui::datamodel::TriggerHandler triggerHandler;
    tp::gui::datamodel::QualityHandler qualityHandler;
    if (auto* stationData = stationDataModel.station(dmSensor->publicID())) {
        groundMotionHandler.handle(*stationData, record);
        triggerHandler.handlePick(*stationData, 0.4, tp::core::Time::now());
        qualityHandler.handle(*stationData, *quality);
        stationDataModel.notifyStationChanged(stationData->id);
    }
    tp::gui::datamodel::EventDataRepository eventRepository;
    eventRepository.addEvent(event);
    auto symbol = tp::gui::datamodel::symbolForStructure(*dmStructure, guiContext.scheme().colors.map.normal);
    tp::gui::plot::Plot plot;
    auto* graph = plot.addGraph("ACC1.X");
    auto plotData = std::make_shared<tp::gui::plot::DataY>();
    plotData->x = tp::gui::plot::Range(0, 2);
    plotData->y = QVector<double>{0.0, 1.0, -0.5};
    graph->setData(plotData);
    plot.updateRanges();
    auto projectedGraphs = plot.project(320, 120);
    tp::gui::ApplicationShell shell;
    tp::gui::map::MapWorkbench map;
    tp::processing::StreamBuffer streamBuffer;
    streamBuffer.push(memoryRecordCopy);
    tp::math::StaLta staLta;
    const bool triggered = staLta.update(4.0);
    tp::qc::LatencyProcessor latencyProcessor;
    tp::qc::RmsProcessor rmsProcessor;
    latencyProcessor.feed(record);
    rmsProcessor.feed(record);
    tp::messaging::ConnectionStatus messagingStatus;
    messagingStatus.state = tp::messaging::ConnectionState::Connected;
    tp::messaging::Packet packet;
    packet.group = QStringLiteral("WAVEFORM");
    packet.topic = QStringLiteral("raw.TP.BRGA.ACC1.X");
    tp::system::Settings settings;
    settings.set(QStringLiteral("module"), QStringLiteral("tpqc"));
    const QVariantMap parsedKv = tp::utils::parseKeyValues({QStringLiteral("a = 1"), QStringLiteral("b = two")});
    tp::utils::Url parsedUrl(QStringLiteral("tcp://127.0.0.1:18180"));
    const double gal = tp::utils::gToGal(0.1);
    return v.major + record.sampleCount() + static_cast<int>(window.length().microseconds())
        + static_cast<int>(stats.clients) + static_cast<int>(queue.maxPayloadSize())
        + static_cast<int>(ring.size()) + static_cast<int>(symbols.size())
        + static_cast<int>(notifiers.size()) + static_cast<int>(qc.waveformQualities().size())
        + static_cast<int>(availability.extents().size()) + static_cast<int>(eventParameters.events().size())
        + static_cast<int>(smp.records().size()) + geoHits + (coordinate.isValid() ? 1 : 0)
        + (bbox.contains(bbox.center()) ? 1 : 0) + ioRecords + (structure ? 1 : 0)
        + recordView.rowCount() + static_cast<int>(points.size())
        + eventListModel.rowCount() + inventoryListModel.rowCount()
        + stationDataModel.rowCount() + eventRepository.eventCount()
        + static_cast<int>(projectedGraphs.size())
        + (connectionLabel.text.isEmpty() ? 0 : 1) + (symbol.radius > 0 ? 1 : 0)
        + streamBuffer.size() + (triggered ? 1 : 0)
        + latencyProcessor.results().size() + rmsProcessor.results().size()
        + (messagingStatus.connected() ? 1 : 0) + (packet.empty() ? 0 : 1)
        + (settings.contains(QStringLiteral("module")) ? 1 : 0)
        + parsedKv.size() + (parsedUrl.isValid() ? 1 : 0) + static_cast<int>(gal)
        + shell.qmlType[0] + map.qmlType[0] > 0 ? 0 : 1;
}
