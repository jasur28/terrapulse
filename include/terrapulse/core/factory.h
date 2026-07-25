#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tp::core {

template <typename Root>
class Factory {
public:
    using Creator = std::function<std::unique_ptr<Root>()>;

    static bool registerClass(const std::string& name, Creator creator) {
        return registry().emplace(name, std::move(creator)).second;
    }

    static std::unique_ptr<Root> create(const std::string& name) {
        auto it = registry().find(name);
        if (it == registry().end()) return {};
        return it->second();
    }

    static std::vector<std::string> classes() {
        std::vector<std::string> out;
        for (const auto& item : registry()) out.push_back(item.first);
        return out;
    }

private:
    static std::map<std::string, Creator>& registry() {
        static std::map<std::string, Creator> r;
        return r;
    }
};

} // namespace tp::core
