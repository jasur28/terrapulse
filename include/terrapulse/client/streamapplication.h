#pragma once

#include "terrapulse/client/application.h"
#include "terrapulse/recordstream/recordstream.h"
#include "terrapulse/core/optional.h"
#include "terrapulse/core/record.h"
#include "terrapulse/core/timewindow.h"

namespace tp::client {

class StreamApplication : public Application {
public:
    using Application::Application;

    bool openStream();
    void closeStream();
    bool addStream(const std::string& networkCode,
                   const std::string& stationCode,
                   const std::string& locationCode,
                   const std::string& channelCode);
    void setTimeWindow(const tp::core::TimeWindow& window);

protected:
    bool run() override;
    virtual void handleRecord(std::unique_ptr<tp::core::Record> record) = 0;

private:
    std::vector<std::string> m_streams;
    tp::core::Optional<tp::core::TimeWindow> m_timeWindow;
};

} // namespace tp::client
