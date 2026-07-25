#pragma once

#include "terrapulse/io/recordstream.h"
#include "terrapulse/gui/qt.h"

#include <QThread>

#include <memory>

namespace tp::gui {

class TP_GUI_API RecordStreamThread : public QThread {
    Q_OBJECT

public:
    explicit RecordStreamThread(QString recordStreamURL, QObject* parent = nullptr);
    ~RecordStreamThread() override;

    QString recordStreamURL() const { return m_recordStreamURL; }
    void stop(bool waitForTermination = true);

signals:
    void receivedRecord(std::shared_ptr<tp::core::Record> record);
    void handleError(const QString& message);

protected:
    void run() override;

private:
    QString m_recordStreamURL;
    bool m_requestedClose = false;
};

} // namespace tp::gui
