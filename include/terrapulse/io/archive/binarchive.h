#pragma once

#include "terrapulse/core/archive.h"

#include <QDataStream>
#include <QIODevice>

namespace tp::io::archive {

class BinArchive : public tp::core::Archive {
public:
    BinArchive(QIODevice* device, Mode mode) : Archive(mode), m_stream(device) {}
    void value(const QString& name, QVariant& value) override;

private:
    QDataStream m_stream;
};

} // namespace tp::io::archive
