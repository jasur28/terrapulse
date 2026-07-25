#pragma once

#include "config/Config.h"

#include <ostream>
#include <string>

namespace tp::broker {

class Queue;

class Processor {
public:
    virtual ~Processor() = default;

    virtual bool init(const Config& config, const std::string& prefix) = 0;
    virtual bool attach(Queue* queue);
    virtual bool close() = 0;
    virtual void getInfo(const QDateTime& timestampUtc, std::ostream& out) = 0;

    Queue* queue() const { return m_queue; }

private:
    Queue* m_queue = nullptr;
    friend class Queue;
};

} // namespace tp::broker
