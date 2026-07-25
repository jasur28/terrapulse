#pragma once

#include "config/Config.h"

namespace tp::client {

class ConfigDB {
public:
    static ConfigDB& instance();
    static void reset();

    void load(const QString& module, const QString& root = {});
    const tp::Config& config() const { return m_config; }
    tp::Config& config() { return m_config; }

private:
    tp::Config m_config;
};

} // namespace tp::client
