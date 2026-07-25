#pragma once

#include "terrapulse/core/metaobject.h"

#include <memory>
#include <string>

namespace tp::core {

class BaseObject {
public:
    virtual ~BaseObject() = default;

    virtual const char* className() const { return "BaseObject"; }
    virtual const MetaObject* meta() const { return nullptr; }
    virtual std::unique_ptr<BaseObject> clone() const;
};

template <typename T>
T* cast(BaseObject* object) {
    return dynamic_cast<T*>(object);
}

template <typename T>
const T* cast(const BaseObject* object) {
    return dynamic_cast<const T*>(object);
}

} // namespace tp::core
