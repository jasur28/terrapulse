#pragma once

#include <QVariant>

#include <functional>
#include <string>
#include <vector>

namespace tp::core {

class BaseObject;

struct MetaProperty {
    std::string name;
    std::string type;
    bool isArray = false;
    bool isClass = false;
    bool isOptional = false;
    std::function<QVariant(const BaseObject*)> read;
    std::function<bool(BaseObject*, const QVariant&)> write;
};

class MetaObject {
public:
    explicit MetaObject(std::string className = {}, const MetaObject* base = nullptr);

    const std::string& className() const { return m_className; }
    const MetaObject* base() const { return m_base; }

    void addProperty(MetaProperty property);
    int propertyCount() const { return static_cast<int>(m_properties.size()); }
    const MetaProperty* property(int index) const;
    const MetaProperty* property(const std::string& name) const;

private:
    std::string m_className;
    const MetaObject* m_base = nullptr;
    std::vector<MetaProperty> m_properties;
};

} // namespace tp::core
