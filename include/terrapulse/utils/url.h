#pragma once

#include <QUrl>

#include <utility>

namespace tp::utils {

class Url {
public:
    Url() = default;
    explicit Url(QString value) : m_url(std::move(value)) {}

    bool isValid() const { return m_url.isValid(); }
    QString scheme() const { return m_url.scheme(); }
    QString host() const { return m_url.host(); }
    int port(int fallback = -1) const { return m_url.port(fallback); }
    QString path() const { return m_url.path(); }
    QString toString() const { return m_url.toString(); }
    const QUrl& qUrl() const { return m_url; }

private:
    QUrl m_url;
};

} // namespace tp::utils
