#include "config/Config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace tp {

QString Config::discoverRoot(const QString& hint) {
    if (!hint.isEmpty() && QDir(hint).exists()) return hint;

    const QString env = qEnvironmentVariable("TP_ROOT");
    if (!env.isEmpty() && QDir(env).exists()) return env;

    // Walk up from the executable looking for a tree that has etc/.
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 5; ++i) {
        if (QDir(d.absoluteFilePath("etc")).exists()) return d.absolutePath();
        if (!d.cdUp()) break;
    }
    // Fall back to the current working directory.
    if (QDir("etc").exists()) return QDir::currentPath();
    return QString();
}

void Config::readFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine();
        const int hash = line.indexOf('#');
        if (hash >= 0) line = line.left(hash);
        const int slashes = line.indexOf("//");
        if (slashes >= 0) line = line.left(slashes);
        line = line.trimmed();
        if (line.isEmpty()) continue;
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();
        if (!key.isEmpty()) m_values[key] = val;      // later layer wins
    }
    m_files << path;
}

void Config::load(const QString& module, const QString& root) {
    m_root = discoverRoot(root);
    if (m_root.isEmpty()) return;

    const QString home = QDir::homePath() + "/.terrapulse";
    const QStringList dirs = {
        m_root + "/etc/defaults",
        m_root + "/etc",
        home
    };
    for (const QString& d : dirs) {
        readFile(d + "/global.cfg");
        if (!module.isEmpty()) readFile(d + "/" + module + ".cfg");
    }
}

QString Config::keyFilePath(const QString& root, unsigned object, unsigned sensor) {
    return QString("%1/etc/key/sensor_%2_%3").arg(root).arg(object).arg(sensor);
}

QString Config::loadBinding(const QString& module, unsigned object, unsigned sensor) {
    if (m_root.isEmpty()) return QString();
    QFile f(keyFilePath(m_root, object, sensor));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();

    QString profile;
    bool    bound = false;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine();
        const int hash = line.indexOf('#');
        if (hash >= 0) line = line.left(hash);
        line = line.trimmed();
        if (line.isEmpty()) continue;
        const int colon = line.indexOf(':');
        const QString mod = (colon < 0 ? line : line.left(colon)).trimmed();
        if (mod != module) continue;
        bound = true;
        if (colon >= 0) profile = line.mid(colon + 1).trimmed();
        break;
    }
    if (!bound) return QString();
    m_files << f.fileName();

    if (!profile.isEmpty())
        readFile(QString("%1/etc/%2/profile_%3.cfg").arg(m_root, module, profile));
    return profile;
}

bool Config::has(const QString& key) const {
    return m_values.find(key) != m_values.end();
}

QString Config::str(const QString& key, const QString& def) const {
    const auto it = m_values.find(key);
    return it == m_values.end() ? def : it->second;
}

int Config::integer(const QString& key, int def) const {
    const auto it = m_values.find(key);
    if (it == m_values.end()) return def;
    bool ok = false;
    const int v = it->second.toInt(&ok);
    return ok ? v : def;
}

double Config::number(const QString& key, double def) const {
    const auto it = m_values.find(key);
    if (it == m_values.end()) return def;
    bool ok = false;
    const double v = it->second.toDouble(&ok);
    return ok ? v : def;
}

bool Config::boolean(const QString& key, bool def) const {
    const auto it = m_values.find(key);
    if (it == m_values.end()) return def;
    const QString v = it->second.toLower();
    if (v == "true" || v == "yes" || v == "1" || v == "on")  return true;
    if (v == "false"|| v == "no"  || v == "0" || v == "off") return false;
    return def;
}

} // namespace tp
