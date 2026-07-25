#pragma once

#include "terrapulse/core/baseobject.h"

#include <QIODevice>
#include <QString>

#include <memory>

namespace tp::io {

class Exporter : public tp::core::BaseObject {
public:
    using Ptr = std::unique_ptr<Exporter>;
    ~Exporter() override = default;

    virtual bool open(QIODevice* device) = 0;
    virtual bool writeObject(const tp::core::BaseObject& object) = 0;
    virtual void close() = 0;

    static Ptr Create(const QString& format);
};

} // namespace tp::io
