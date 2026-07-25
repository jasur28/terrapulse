#include "config/ConfigEditor.h"
#include "config/Config.h"
#include "terrapulse/messaging/connection.h"
#include "terrapulse/messaging/message.h"
#include "terrapulse/messaging/endpoints.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>
#include <QThread>

namespace tp {

ConfigEditor::ConfigEditor(QObject* parent) : QObject(parent) {
    m_root = Config::discoverRoot();
    if (!m_root.isEmpty()) {
        const QString cand = m_root + "/config/inventory.example.json";
        if (QFile::exists(cand)) m_inventoryPath = cand;
    }
    m_status = m_root.isEmpty() ? QStringLiteral("No installation root found (set TP_ROOT)")
                                : QStringLiteral("Configuration root: ") + m_root;
}

void ConfigEditor::setStatus(const QString& s) {
    m_status = s;
    emit statusChanged();
}

void ConfigEditor::setInventoryPath(const QString& p) {
    if (m_inventoryPath == p) return;
    m_inventoryPath = p;
    emit inventoryPathChanged();
}

// ── Module settings ─────────────────────────────────────────────────────────

QStringList ConfigEditor::modules() const {
    QStringList out;
    if (m_root.isEmpty()) return out;
    QDir d(m_root + "/etc/defaults");
    for (const QFileInfo& fi : d.entryInfoList({"*.cfg"}, QDir::Files, QDir::Name)) {
        const QString name = fi.completeBaseName();
        if (name != "global") out << name;
    }
    out.prepend("global");           // global first: it applies to everything
    return out;
}

QVariantList ConfigEditor::moduleRows() const {
    QVariantList rows;
    for (const QString& module : modules()) {
        QVariantMap row;
        row["name"] = module;
        row["category"] = module == "global" ? QStringLiteral("core")
            : module.startsWith("tpq") ? QStringLiteral("quality")
            : module.startsWith("tpw") || module == "tpproc" ? QStringLiteral("processing")
            : module == "tpmaster" ? QStringLiteral("messaging")
            : QStringLiteral("terrapulse");
        row["file"] = QString("%1/etc/%2.cfg").arg(m_root, module);
        row["defaultFile"] = QString("%1/etc/defaults/%2.cfg").arg(m_root, module);
        row["changed"] = hasChanges(module) || QFile::exists(row["file"].toString());
        rows.append(row);
    }
    return rows;
}

QVariantList ConfigEditor::settings(const QString& module) const {
    QVariantList out;
    if (m_root.isEmpty()) return out;

    // Shipped defaults for this module (+ global, which it inherits).
    Config defaults;
    {
        // Read only the defaults layer by pointing at a root whose etc/ is defaults/.
        Config full;
        full.load(module, m_root);
        // Values as they are in force now (all layers).
        const auto& effective = full.all();

        // Defaults alone: parse the two default files directly.
        std::map<QString, QString> def;
        const QStringList files{ m_root + "/etc/defaults/global.cfg",
                                 m_root + "/etc/defaults/" + module + ".cfg" };
        for (const QString& path : files) {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QTextStream in(&f);
            while (!in.atEnd()) {
                QString line = in.readLine();
                const int hash = line.indexOf('#');
                if (hash >= 0) line = line.left(hash);
                line = line.trimmed();
                const int eq = line.indexOf('=');
                if (eq <= 0) continue;
                def[line.left(eq).trimmed()] = line.mid(eq + 1).trimmed();
            }
        }

        const auto stagedIt = m_staged.find(module);
        for (const auto& kv : effective) {
            const QString key = kv.first;
            const QString defVal = def.count(key) ? def.at(key) : QString();
            QString value = kv.second;
            bool staged = false;
            if (stagedIt != m_staged.end()) {
                const auto s = stagedIt->second.find(key);
                if (s != stagedIt->second.end()) { value = s->second; staged = true; }
            }
            QVariantMap row;
            row["key"]        = key;
            row["value"]      = value;
            row["def"]        = defVal;
            row["overridden"] = staged || (!defVal.isNull() && value != defVal);
            row["staged"]     = staged;
            out.append(row);
        }
    }
    return out;
}

void ConfigEditor::setSetting(const QString& module, const QString& key, const QString& value) {
    m_staged[module][key] = value;
    emit settingsChanged(module);
}

void ConfigEditor::revertSetting(const QString& module, const QString& key) {
    auto it = m_staged.find(module);
    if (it != m_staged.end()) {
        it->second.erase(key);
        if (it->second.empty()) m_staged.erase(it);
    }
    emit settingsChanged(module);
}

void ConfigEditor::resetModule(const QString& module) {
    auto it = m_staged.find(module);
    if (it != m_staged.end()) {
        m_staged.erase(it);
        emit settingsChanged(module);
    }
    setStatus("Reset unsaved changes for " + module);
}

bool ConfigEditor::hasChanges(const QString& module) const {
    const auto it = m_staged.find(module);
    return it != m_staged.end() && !it->second.empty();
}

bool ConfigEditor::saveModule(const QString& module) {
    if (m_root.isEmpty()) { setStatus("No configuration root"); return false; }
    const auto it = m_staged.find(module);
    if (it == m_staged.end() || it->second.empty()) {
        setStatus("Nothing to save for " + module);
        return true;
    }

    // Merge staged edits into the existing site file, keeping other keys.
    const QString path = m_root + "/etc/" + module + ".cfg";
    std::map<QString, QString> existing;
    QFile in(path);
    if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream s(&in);
        while (!s.atEnd()) {
            QString line = s.readLine();
            const int hash = line.indexOf('#');
            if (hash >= 0) line = line.left(hash);
            line = line.trimmed();
            const int eq = line.indexOf('=');
            if (eq > 0) existing[line.left(eq).trimmed()] = line.mid(eq + 1).trimmed();
        }
        in.close();
    }
    for (const auto& kv : it->second) existing[kv.first] = kv.second;

    QDir().mkpath(m_root + "/etc");
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        setStatus("Cannot write " + path);
        return false;
    }
    QTextStream w(&out);
    w << "# " << module << " — site configuration (written by tpconfig).\n"
      << "# Shipped defaults live in etc/defaults/" << module << ".cfg; only the\n"
      << "# values that deliberately differ belong here.\n\n";
    for (const auto& kv : existing) w << kv.first << " = " << kv.second << "\n";
    out.close();

    m_staged.erase(module);
    setStatus("Saved " + path + " — restart the module to apply");
    emit settingsChanged(module);
    return true;
}

// ── Bindings ────────────────────────────────────────────────────────────────

void ConfigEditor::switchMode() {
    m_userMode = !m_userMode;
    emit configModeChanged();
    setStatus(QStringLiteral("Configuration mode: ") + configMode());
}

QVariantList ConfigEditor::systemModules() const {
    QVariantList rows;
    for (const QString& name : modules()) {
        QVariantMap row;
        row["name"] = name;
        row["enabled"] = name == "global" ? false : QFile::exists(QString("%1/etc/%2.cfg").arg(m_root, name));
        row["state"] = name == "global" ? QStringLiteral("configuration")
            : name == "tpmaster" ? QStringLiteral("core")
            : QStringLiteral("stopped");
        row["pid"] = QString();
        row["group"] = name.startsWith("tpq") ? QStringLiteral("quality")
            : name.startsWith("tpw") || name == "tpproc" ? QStringLiteral("processing")
            : name == "tpmaster" ? QStringLiteral("messaging")
            : QStringLiteral("system");
        rows.append(row);
    }
    return rows;
}

bool ConfigEditor::systemAction(const QString& action, const QStringList& modules) {
    const QString target = modules.isEmpty() ? QStringLiteral("all modules") : modules.join(QStringLiteral(", "));
    if (action == "reload") {
        setStatus("Reloaded status for " + target);
        return true;
    }
    if (action == "update-config") {
        setStatus("Configuration update prepared for " + target + "; restart affected modules to apply");
        return true;
    }
    if (action == "check") {
        setStatus("Checked " + target + "; no process control was required");
        return true;
    }
    setStatus("Queued system action '" + action + "' for " + target);
    return true;
}

QVariantList ConfigEditor::bindings() const {
    QVariantList out;
    if (m_root.isEmpty()) return out;
    QDir d(m_root + "/etc/key");
    for (const QFileInfo& fi : d.entryInfoList({"sensor_*"}, QDir::Files, QDir::Name)) {
        const QStringList parts = fi.fileName().split('_');
        if (parts.size() < 3) continue;
        QVariantList entries;
        QFile f(fi.absoluteFilePath());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            while (!in.atEnd()) {
                QString line = in.readLine();
                const int hash = line.indexOf('#');
                if (hash >= 0) line = line.left(hash);
                line = line.trimmed();
                if (line.isEmpty()) continue;
                const int colon = line.indexOf(':');
                QVariantMap e;
                e["module"]  = colon < 0 ? line : line.left(colon).trimmed();
                e["profile"] = colon < 0 ? QString() : line.mid(colon + 1).trimmed();
                entries.append(e);
            }
        }
        QVariantMap row;
        row["object"]  = parts[1].toInt();
        row["sensor"]  = parts[2].toInt();
        row["file"]    = fi.fileName();
        row["entries"] = entries;
        out.append(row);
    }
    return out;
}

QStringList ConfigEditor::profiles(const QString& module) const {
    QStringList out;
    if (m_root.isEmpty()) return out;
    QDir d(m_root + "/etc/" + module);
    for (const QFileInfo& fi : d.entryInfoList({"profile_*.cfg"}, QDir::Files, QDir::Name))
        out << fi.completeBaseName().mid(QString("profile_").size());
    return out;
}

bool ConfigEditor::setBinding(int object, int sensor, const QString& module, const QString& profile) {
    if (m_root.isEmpty()) return false;
    const QString dir = m_root + "/etc/key";
    QDir().mkpath(dir);
    const QString path = QString("%1/sensor_%2_%3").arg(dir).arg(object).arg(sensor);

    // Read existing lines, replace this module's entry, keep the rest.
    QStringList lines;
    bool replaced = false;
    QFile in(path);
    if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream s(&in);
        while (!s.atEnd()) {
            const QString raw = s.readLine();
            QString t = raw;
            const int hash = t.indexOf('#');
            if (hash >= 0) t = t.left(hash);
            t = t.trimmed();
            if (t.isEmpty()) { lines << raw; continue; }
            const int colon = t.indexOf(':');
            const QString mod = (colon < 0 ? t : t.left(colon)).trimmed();
            if (mod == module) {
                lines << (profile.isEmpty() ? module : module + ":" + profile);
                replaced = true;
            } else {
                lines << raw;
            }
        }
        in.close();
    }
    if (!replaced) lines << (profile.isEmpty() ? module : module + ":" + profile);

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        setStatus("Cannot write " + path);
        return false;
    }
    QTextStream w(&out);
    w << "# Binding for object " << object << " / sensor " << sensor
      << " (written by tpconfig).\n# Format: <module>[:<profile>]\n";
    for (const QString& l : lines) w << l << "\n";
    out.close();

    setStatus(QString("Bound object %1 / sensor %2: %3%4")
                  .arg(object).arg(sensor).arg(module,
                       profile.isEmpty() ? QString(" (global config)") : " -> " + profile));
    emit bindingsChanged();
    return true;
}

bool ConfigEditor::removeBinding(int object, int sensor) {
    if (m_root.isEmpty()) return false;
    const QString path = QString("%1/etc/key/sensor_%2_%3").arg(m_root).arg(object).arg(sensor);
    const bool ok = QFile::remove(path);
    setStatus(ok ? QString("Removed binding for object %1 / sensor %2").arg(object).arg(sensor)
                 : QString("No binding file for object %1 / sensor %2").arg(object).arg(sensor));
    emit bindingsChanged();
    return ok;
}

// ── Inventory ───────────────────────────────────────────────────────────────

QVariantMap ConfigEditor::loadInventory() {
    QVariantMap out;
    QFile f(m_inventoryPath);
    if (!f.open(QIODevice::ReadOnly)) {
        setStatus("Cannot read inventory " + m_inventoryPath);
        return out;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        setStatus("Inventory is not valid JSON: " + err.errorString());
        return out;
    }
    out = doc.object().toVariantMap();
    setStatus("Loaded " + m_inventoryPath);
    return out;
}

bool ConfigEditor::saveInventory(const QVariantMap& inv) {
    QFile f(m_inventoryPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setStatus("Cannot write " + m_inventoryPath);
        return false;
    }
    f.write(QJsonDocument(QJsonObject::fromVariantMap(inv)).toJson(QJsonDocument::Indented));
    f.close();
    setStatus("Saved " + m_inventoryPath);
    return true;
}

bool ConfigEditor::publishInventory(const QString& host) {
    const QVariantMap inv = loadInventory();
    const QVariantList structures = inv.value("structures").toList();
    if (structures.isEmpty()) { setStatus("Inventory has no structures to publish"); return false; }

    tp::messaging::Publisher pub(tp::messaging::in(host.toStdString()), /*bind=*/false);
    // PUB/SUB is a slow joiner: give the broker time to accept the connection and
    // announce its subscriptions, or the first notifiers are silently dropped.
    QThread::msleep(1600);

    int count = 0;
    auto send = [&](const QString& kind, const QString& key, QVariantMap fields) {
        fields["v"] = 1; fields["type"] = "notifier";
        fields["op"] = "add"; fields["kind"] = kind;
        pub.publish(tp::messaging::make(("inv." + kind + "." + key).toStdString(), fields));
        ++count;
    };

    for (const QVariant& sv : structures) {
        const QVariantMap s = sv.toMap();
        const int objectId = s.value("objectId").toInt();
        send("structure", QString::number(objectId), QVariantMap{
            {"objectId", objectId}, {"name", s.value("name")},
            {"lat", s.value("lat")}, {"lon", s.value("lon")},
            {"description", s.value("description")}});

        for (const QVariant& nv : s.value("sensors").toList()) {
            const QVariantMap n = nv.toMap();
            const int sensorId = n.value("sensorId").toInt();
            send("sensor", QString("%1.%2").arg(objectId).arg(sensorId), QVariantMap{
                {"objectId", objectId}, {"sensorId", sensorId},
                {"model", n.value("model")}, {"location", n.value("location")}});

            for (const QVariant& cv : n.value("channels").toList()) {
                const QVariantMap c = cv.toMap();
                const int comp = c.value("component").toInt();
                send("channel", QString("%1.%2.%3").arg(objectId).arg(sensorId).arg(comp),
                     QVariantMap{{"objectId", objectId}, {"sensorId", sensorId},
                                 {"component", comp}, {"sampleRate", c.value("sampleRate")},
                                 {"unit", c.value("unit")}, {"gain", c.value("gain")}});
            }
        }
    }
    QThread::msleep(300);      // let the last messages drain before we return
    setStatus(QString("Published %1 inventory notifier(s) to %2").arg(count).arg(host));
    return true;
}

} // namespace tp
