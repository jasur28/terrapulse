#pragma once

#include "terrapulse/core/baseobject.h"
#include "terrapulse/core/datetime.h"

#include <memory>

namespace tp::datamodel {

class PublicObject;

enum class Operation {
    Undefined,
    Add,
    Remove,
    Update
};

class Object : public tp::core::BaseObject {
public:
    const char* className() const override { return "DataModel::Object"; }
    std::unique_ptr<tp::core::BaseObject> clone() const override;

    PublicObject* parent() const { return m_parent; }
    virtual bool setParent(PublicObject* parent);

    virtual bool assign(const Object& other);
    virtual std::unique_ptr<Object> cloneObject() const;

    tp::core::Time lastModifiedInArchive() const { return m_lastModifiedInArchive; }
    void setLastModifiedInArchive(tp::core::Time time) { m_lastModifiedInArchive = time; }

private:
    PublicObject* m_parent = nullptr;
    tp::core::Time m_lastModifiedInArchive;
};

using ObjectPtr = std::shared_ptr<Object>;

} // namespace tp::datamodel
