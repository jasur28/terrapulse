#pragma once

#include "terrapulse/datamodel/object.h"

#include <QString>

#include <memory>

namespace tp::datamodel {

class PublicObject : public Object {
public:
    explicit PublicObject(QString publicID = {});
    PublicObject(const PublicObject& other);
    PublicObject& operator=(const PublicObject& other);
    ~PublicObject() override;

    const char* className() const override { return "DataModel::PublicObject"; }
    std::unique_ptr<Object> cloneObject() const override;

    QString publicID() const { return m_publicID; }
    bool setPublicID(QString publicID);

    bool registered() const { return m_registered; }
    bool registerMe();
    void deregisterMe();

    static PublicObject* Find(const QString& publicID);
    static std::size_t ObjectCount();
    static void SetRegistrationEnabled(bool enabled);
    static bool RegistrationEnabled();
    static QString GenerateId(const QString& prefix);

private:
    QString m_publicID;
    bool m_registered = false;
};

using PublicObjectPtr = std::shared_ptr<PublicObject>;

} // namespace tp::datamodel
