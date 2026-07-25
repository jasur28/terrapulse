#pragma once

#include <QString>
#include <QVariant>

#include <map>
#include <utility>

namespace tp::system {

class Settings {
public:
    void set(QString key, QVariant value) { m_values[std::move(key)] = std::move(value); }
    QVariant value(const QString& key, QVariant fallback = {}) const {
        const auto it = m_values.find(key);
        return it == m_values.end() ? fallback : it->second;
    }
    bool contains(const QString& key) const { return m_values.find(key) != m_values.end(); }
    const std::map<QString, QVariant>& all() const { return m_values; }
    void clear() { m_values.clear(); }

private:
    std::map<QString, QVariant> m_values;
};

} // namespace tp::system
