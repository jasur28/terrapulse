#pragma once

#include "terrapulse/core/baseobject.h"

#include <memory>
#include <vector>

namespace tp::core {

class Message : public BaseObject {
public:
    bool empty() const { return m_objects.empty(); }
    int size() const { return static_cast<int>(m_objects.size()); }
    void clear() { m_objects.clear(); }

    void add(std::shared_ptr<BaseObject> object) { m_objects.push_back(std::move(object)); }
    const std::vector<std::shared_ptr<BaseObject>>& objects() const { return m_objects; }

private:
    std::vector<std::shared_ptr<BaseObject>> m_objects;
};

} // namespace tp::core
