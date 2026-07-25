#pragma once

#include "terrapulse/datamodel/creationinfo.h"
#include "terrapulse/datamodel/publicobject.h"

#include <QString>

namespace tp::datamodel {

class JournalEntry : public PublicObject {
public:
    explicit JournalEntry(QString publicID = {}) : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::JournalEntry"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<JournalEntry>(*this); }

    QString objectID;
    QString action;
    QString parameters;
    CreationInfo creationInfo;
};

} // namespace tp::datamodel
