#include "terrapulse/io/archive/binarchive.h"
#include "terrapulse/io/archive/xmlarchive.h"
#include "terrapulse/io/database.h"
#include "terrapulse/io/exporter.h"
#include "terrapulse/io/importer.h"
#include "terrapulse/io/recordfilter/crop.h"
#include "terrapulse/io/recordinput.h"
#include "terrapulse/io/recordoutputstream.h"
#include "terrapulse/io/recordstream/memory.h"
#include "terrapulse/io/recordstreamexceptions.h"

#include "mseed/RecordStream.h"

#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QUrl>

#include <fstream>
#include <sstream>

namespace tp::io {
namespace {

using RecordStreamFactories = QHash<QString, RecordStream::Factory>;
using RecordOutputFactories = QHash<QString, RecordOutputStream::Factory>;

RecordStreamFactories& recordStreamFactories() {
    static RecordStreamFactories factories;
    return factories;
}

RecordOutputFactories& recordOutputFactories() {
    static RecordOutputFactories factories;
    return factories;
}

QString serviceFromUrl(const QString& url, const QString& fallback) {
    const QUrl parsed(url);
    return parsed.scheme().isEmpty() ? fallback : parsed.scheme().toLower();
}

QString targetFromUrl(const QString& url) {
    const QUrl parsed(url);
    if (parsed.scheme().isEmpty()) {
        return url;
    }
    if (parsed.isLocalFile()) {
        return parsed.toLocalFile();
    }
    auto result = parsed.path();
    if (result.isEmpty()) {
        result = parsed.host();
    }
    if (!parsed.query().isEmpty()) {
        result += "?" + parsed.query();
    }
    return result;
}

void splitStreamID(const std::string& sid, std::string& network, std::string& station,
                   std::string& location, std::string& channel) {
    const auto text = QString::fromStdString(sid);
    const auto parts = text.split(QRegularExpression("[._]"));
    network = parts.value(0, "TP").toStdString();
    station = parts.value(1, text).toStdString();
    location = parts.value(2).toStdString();
    channel = parts.value(3, "X").toStdString();
}

class FileRecordStream : public RecordStream {
public:
    bool setSource(const QString& source) override {
        m_source = source;
        m_records.clear();

        for (const auto& trace : tp::mseed::readStream(source.toStdString())) {
            auto record = std::make_unique<tp::core::AccelerationRecord>();
            std::string network;
            std::string station;
            std::string location;
            std::string channel;
            splitStreamID(trace.sid, network, station, location, channel);
            record->setStream(network, station, location, channel);
            record->setStartTime(tp::core::Time::fromMSecsSinceEpoch(trace.startTimeMs));
            record->setSampling(static_cast<int>(trace.samples.size()), trace.sampleRate);
            record->samples().values().reserve(trace.samples.size());
            for (const auto sample : trace.samples) {
                record->samples().values().push_back(static_cast<double>(sample));
            }
            setupRecord(record.get());
            m_records.push_back(std::move(record));
        }
        m_cursor = 0;
        return true;
    }

    void close() override {
        m_records.clear();
        m_cursor = 0;
    }

    bool addStream(const QString& networkCode, const QString& stationCode,
                   const QString& locationCode, const QString& channelCode) override {
        return addStream(networkCode, stationCode, locationCode, channelCode, {}, {});
    }

    bool addStream(const QString& networkCode, const QString& stationCode,
                   const QString& locationCode, const QString& channelCode,
                   std::optional<tp::core::Time>, std::optional<tp::core::Time>) override {
        m_requestedStreams.insert(QString("%1.%2.%3.%4")
            .arg(networkCode, stationCode, locationCode, channelCode));
        return true;
    }

    std::unique_ptr<tp::core::Record> next() override {
        while (m_cursor < m_records.size()) {
            auto record = std::move(m_records[m_cursor++]);
            if (m_requestedStreams.isEmpty()
                || m_requestedStreams.contains(QString::fromStdString(record->streamID()))) {
                return record;
            }
        }
        return {};
    }

private:
    QString m_source;
    QSet<QString> m_requestedStreams;
    std::vector<std::unique_ptr<tp::core::Record>> m_records;
    std::size_t m_cursor = 0;
};

class FileRecordOutputStream : public RecordOutputStream {
public:
    bool setTarget(const QString& target) override {
        m_stream = std::make_unique<std::ofstream>(target.toStdString(), std::ios::binary | std::ios::trunc);
        return m_stream->good();
    }

    void close() override {
        if (m_stream) {
            m_stream->flush();
            m_stream.reset();
        }
    }

    std::ostream& stream() override {
        if (!m_stream) {
            throw RecordStreamError("Output stream is not open");
        }
        return *m_stream;
    }

private:
    std::unique_ptr<std::ofstream> m_stream;
};

struct Builtins {
    Builtins() {
        RecordStream::RegisterFactory("memory", [] { return std::make_unique<MemoryRecordStream>(); });
        RecordStream::RegisterFactory("file", [] { return std::make_unique<FileRecordStream>(); });
        RecordStream::RegisterFactory("tds", [] { return std::make_unique<FileRecordStream>(); });
        RecordStream::RegisterFactory("csv", [] { return std::make_unique<FileRecordStream>(); });
        RecordOutputStream::RegisterFactory("file", [] { return std::make_unique<FileRecordOutputStream>(); });
    }
};

Builtins builtins;

} // namespace

bool RecordStream::setStartTime(std::optional<tp::core::Time> startTime) {
    m_startTime = std::move(startTime);
    return true;
}

bool RecordStream::setEndTime(std::optional<tp::core::Time> endTime) {
    m_endTime = std::move(endTime);
    return true;
}

bool RecordStream::setTimeWindow(const tp::core::TimeWindow& timeWindow) {
    return setStartTime(timeWindow.startTime()) && setEndTime(timeWindow.endTime());
}

bool RecordStream::setTimeout(int seconds) {
    m_timeoutSeconds = seconds;
    return true;
}

bool RecordStream::setRecordType(const QString& type) {
    m_recordType = type;
    return true;
}

bool RecordStream::RegisterFactory(const QString& service, Factory factory) {
    if (service.isEmpty() || !factory) {
        return false;
    }
    recordStreamFactories().insert(service.toLower(), std::move(factory));
    return true;
}

RecordStream::Ptr RecordStream::Create(const QString& service) {
    const auto factory = recordStreamFactories().value(service.toLower());
    return factory ? factory() : nullptr;
}

RecordStream::Ptr RecordStream::Open(const QString& url) {
    auto stream = Create(serviceFromUrl(url, QFileInfo(url).isDir() ? "tds" : "file"));
    if (!stream) {
        return {};
    }
    const auto fragment = QUrl(url).fragment();
    if (!fragment.isEmpty()) {
        stream->setRecordType(fragment);
    }
    if (!stream->setSource(url)) {
        return {};
    }
    return stream;
}

void RecordStream::setupRecord(tp::core::Record*) const {}

bool MemoryRecordStream::setSource(const QString&) {
    return true;
}

void MemoryRecordStream::close() {
    m_closed = true;
}

bool MemoryRecordStream::addStream(const QString&, const QString&, const QString&, const QString&) {
    return true;
}

bool MemoryRecordStream::addStream(const QString&, const QString&, const QString&, const QString&,
                                   std::optional<tp::core::Time>, std::optional<tp::core::Time>) {
    return true;
}

std::unique_ptr<tp::core::Record> MemoryRecordStream::next() {
    if (m_records.empty() || m_closed) {
        return {};
    }
    auto record = std::move(m_records.front());
    m_records.pop_front();
    setupRecord(record.get());
    return record;
}

void MemoryRecordStream::push(std::unique_ptr<tp::core::Record> record) {
    if (record) {
        m_records.push_back(std::move(record));
    }
}

RecordInput::RecordInput(RecordStream* stream, tp::core::Array::DataType dataType,
                         tp::core::Record::Hint hint)
    : m_stream(stream) {
    if (m_stream) {
        m_stream->setDataType(dataType);
        m_stream->setDataHint(hint);
    }
}

RecordIterator RecordInput::begin() {
    return RecordIterator(this);
}

RecordIterator RecordInput::end() {
    return {};
}

std::unique_ptr<tp::core::Record> RecordInput::next() {
    return m_stream ? m_stream->next() : nullptr;
}

RecordIterator::RecordIterator(RecordInput* source)
    : m_source(source), m_current(source ? source->next() : nullptr) {
    if (!m_current) {
        m_source = nullptr;
    }
}

RecordIterator::RecordIterator(const RecordIterator& other)
    : m_source(other.m_source), m_current(other.m_current ? other.m_current->copy() : nullptr) {}

RecordIterator& RecordIterator::operator=(const RecordIterator& other) {
    if (this == &other) {
        return *this;
    }
    m_source = other.m_source;
    m_current = other.m_current ? other.m_current->copy() : nullptr;
    return *this;
}

RecordIterator& RecordIterator::operator++() {
    m_current = m_source ? m_source->next() : nullptr;
    if (!m_current) {
        m_source = nullptr;
    }
    return *this;
}

RecordIterator RecordIterator::operator++(int) {
    auto copy = *this;
    ++(*this);
    return copy;
}

bool RecordIterator::operator==(const RecordIterator& other) const {
    return m_source == other.m_source && m_current.get() == other.m_current.get();
}

void RecordFilterChain::add(std::unique_ptr<RecordFilter> filter) {
    if (filter) {
        m_filters.push_back(std::move(filter));
    }
}

std::unique_ptr<tp::core::Record> RecordFilterChain::feed(std::unique_ptr<tp::core::Record> record) {
    for (auto& filter : m_filters) {
        if (!record) {
            return {};
        }
        record = filter->feed(std::move(record));
    }
    return record;
}

std::unique_ptr<tp::core::Record> RecordFilterChain::flush() {
    return {};
}

void RecordFilterChain::reset() {
    for (auto& filter : m_filters) {
        filter->reset();
    }
}

std::unique_ptr<RecordFilter> RecordFilterChain::clone() const {
    auto chain = std::make_unique<RecordFilterChain>();
    for (const auto& filter : m_filters) {
        chain->add(filter->clone());
    }
    return chain;
}

std::unique_ptr<tp::core::Record> CropFilter::feed(std::unique_ptr<tp::core::Record> record) {
    if (!record) {
        return {};
    }
    return record->timeWindow().overlaps(m_window) ? std::move(record) : nullptr;
}

bool RecordOutputStream::write(const tp::core::Record& record) {
    stream() << record.streamID() << " " << record.startTime().toString().toStdString()
             << " " << record.sampleCount() << "\n";
    return stream().good();
}

bool RecordOutputStream::RegisterFactory(const QString& service, Factory factory) {
    if (service.isEmpty() || !factory) {
        return false;
    }
    recordOutputFactories().insert(service.toLower(), std::move(factory));
    return true;
}

RecordOutputStream::Ptr RecordOutputStream::Create(const QString& service) {
    const auto factory = recordOutputFactories().value(service.toLower());
    return factory ? factory() : nullptr;
}

RecordOutputStream::Ptr RecordOutputStream::Open(const QString& url) {
    auto output = Create(serviceFromUrl(url, "file"));
    if (!output || !output->setTarget(targetFromUrl(url))) {
        return {};
    }
    return output;
}

Importer::Ptr Importer::Create(const QString&) {
    return {};
}

Exporter::Ptr Exporter::Create(const QString&) {
    return {};
}

bool Database::openSQLite(const QString& path, const QString& connectionName) {
    close();
    m_connectionName = connectionName.isEmpty()
        ? QString("terrapulse-%1").arg(reinterpret_cast<quintptr>(this))
        : connectionName;
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(path);
    return m_db.open();
}

void Database::close() {
    if (m_db.isValid()) {
        m_db.close();
        const auto name = m_connectionName;
        m_db = {};
        QSqlDatabase::removeDatabase(name);
    }
}

bool Database::isOpen() const {
    return m_db.isOpen();
}

QSqlQuery Database::exec(const QString& sql) {
    return QSqlQuery(sql, m_db);
}

} // namespace tp::io

namespace tp::io::archive {

void BinArchive::value(const QString&, QVariant& value) {
    if (isWriting()) {
        m_stream << value;
    }
    else {
        m_stream >> value;
    }
}

void XmlArchive::value(const QString& name, QVariant& value) {
    if (isWriting()) {
        auto root = m_document.documentElement();
        if (root.isNull()) {
            root = m_document.createElement("archive");
            m_document.appendChild(root);
        }
        auto node = m_document.createElement(name);
        node.appendChild(m_document.createTextNode(value.toString()));
        root.appendChild(node);
    }
    else {
        const auto nodes = m_document.elementsByTagName(name);
        if (!nodes.isEmpty()) {
            value = nodes.at(0).toElement().text();
        }
    }
}

} // namespace tp::io::archive
