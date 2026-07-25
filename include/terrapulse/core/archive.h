#pragma once

#include "terrapulse/core/baseobject.h"

#include <QJsonObject>
#include <QString>
#include <QVariant>

namespace tp::core {

class Archive {
public:
    enum class Mode { Read, Write };

    explicit Archive(Mode mode) : m_mode(mode) {}
    virtual ~Archive() = default;

    Mode mode() const { return m_mode; }
    bool isReading() const { return m_mode == Mode::Read; }
    bool isWriting() const { return m_mode == Mode::Write; }

    virtual void value(const QString& name, QVariant& value) = 0;

private:
    Mode m_mode;
};

class JsonArchive : public Archive {
public:
    explicit JsonArchive(QJsonObject object = {}, Mode mode = Mode::Write);

    QJsonObject object() const { return m_object; }
    void value(const QString& name, QVariant& value) override;

private:
    QJsonObject m_object;
};

} // namespace tp::core
