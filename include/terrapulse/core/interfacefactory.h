#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tp::core {

template <typename Interface>
class InterfaceFactory {
public:
    using Creator = std::function<std::unique_ptr<Interface>()>;

    static bool registerService(const std::string& service, Creator creator) {
        return registry().emplace(service, std::move(creator)).second;
    }

    static std::unique_ptr<Interface> create(const std::string& service) {
        auto it = registry().find(service);
        if (it == registry().end()) return {};
        return it->second();
    }

    static std::vector<std::string> services() {
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
