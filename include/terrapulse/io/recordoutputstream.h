#pragma once

#include "terrapulse/core/baseobject.h"
#include "terrapulse/core/interruptible.h"
#include "terrapulse/core/record.h"

#include <QString>

#include <functional>
#include <memory>
#include <ostream>

namespace tp::io {

class RecordOutputStream : public tp::core::BaseObject, public tp::core::Interruptible {
public:
    using Ptr = std::unique_ptr<RecordOutputStream>;
    using Factory = std::function<Ptr()>;

    ~RecordOutputStream() override = default;

    virtual bool setTarget(const QString& target) = 0;
    virtual void close() = 0;
    virtual std::ostream& stream() = 0;
    virtual bool write(const tp::core::Record& record);

    static bool RegisterFactory(const QString& service, Factory factory);
    static Ptr Create(const QString& service);
    static Ptr Open(const QString& url);
};

} // namespace tp::io
