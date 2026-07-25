#pragma once

#include "terrapulse/core/baseobject.h"

#include <QIODevice>
#include <QString>

#include <memory>

namespace tp::io {

class Importer : public tp::core::BaseObject {
public:
    using Ptr = std::unique_ptr<Importer>;
    ~Importer() override = default;

    virtual bool open(QIODevice* device) = 0;
    virtual std::shared_ptr<tp::core::BaseObject> readObject() = 0;
    virtual void close() = 0;

    static Ptr Create(const QString& format);
};

} // namespace tp::io
