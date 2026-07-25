#pragma once

#include "terrapulse/core/archive.h"

#include <QDomDocument>

namespace tp::io::archive {

class XmlArchive : public tp::core::Archive {
public:
    explicit XmlArchive(QDomDocument document = {}, Mode mode = Mode::Write)
        : Archive(mode), m_document(std::move(document)) {}

    QDomDocument document() const { return m_document; }
    void value(const QString& name, QVariant& value) override;

private:
    QDomDocument m_document;
};

} // namespace tp::io::archive
