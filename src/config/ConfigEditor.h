#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <map>

namespace tp {

// ConfigEditor — the model behind tpconfig. Lets an operator see and change the
// things that would otherwise mean hand-editing files: which value is in force
// and where it came from, per-sensor bindings, and the inventory itself.
//
// Edits are staged and only written on save, and a save only writes the keys that
// actually differ from the shipped defaults — so etc/<module>.cfg stays a short,
// reviewable list of deliberate decisions rather than a copy of everything.
class ConfigEditor : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString root READ root CONSTANT)
    Q_PROPERTY(QString inventoryPath READ inventoryPath WRITE setInventoryPath NOTIFY inventoryPathChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString configMode READ configMode NOTIFY configModeChanged)

public:
    explicit ConfigEditor(QObject* parent = nullptr);

    QString root() const { return m_root; }
    QString status() const { return m_status; }
    QString configMode() const { return m_userMode ? QStringLiteral("User") : QStringLiteral("System"); }
    QString inventoryPath() const { return m_inventoryPath; }
    void    setInventoryPath(const QString& p);

    // ── Module settings ──────────────────────────────────────────────────────
    // Modules that ship defaults (etc/defaults/*.cfg).
    Q_INVOKABLE QStringList modules() const;
    Q_INVOKABLE QVariantList moduleRows() const;
    // [{key, value, def, source, overridden, staged}] for one module.
    Q_INVOKABLE QVariantList settings(const QString& module) const;
    Q_INVOKABLE void setSetting(const QString& module, const QString& key, const QString& value);
    Q_INVOKABLE void revertSetting(const QString& module, const QString& key);
    Q_INVOKABLE void resetModule(const QString& module);
    Q_INVOKABLE bool hasChanges(const QString& module) const;
    Q_INVOKABLE bool saveModule(const QString& module);
    Q_INVOKABLE void switchMode();

    // System management facade. It mirrors scconfig's system panel; the current
    // implementation records the intended operation and keeps file editing safe.
    Q_INVOKABLE QVariantList systemModules() const;
    Q_INVOKABLE bool systemAction(const QString& action, const QStringList& modules = {});

    // ── Bindings (per-sensor profiles) ───────────────────────────────────────
    // [{object, sensor, file, entries:[{module, profile}]}]
    Q_INVOKABLE QVariantList bindings() const;
    Q_INVOKABLE QStringList  profiles(const QString& module) const;
    Q_INVOKABLE bool setBinding(int object, int sensor, const QString& module, const QString& profile);
    Q_INVOKABLE bool removeBinding(int object, int sensor);

    // ── Inventory ────────────────────────────────────────────────────────────
    Q_INVOKABLE QVariantMap  loadInventory();
    Q_INVOKABLE bool         saveInventory(const QVariantMap& inv);
    // Push the saved inventory to tpmaster as notifiers (same as tpinv).
    Q_INVOKABLE bool         publishInventory(const QString& host);

signals:
    void statusChanged();
    void inventoryPathChanged();
    void settingsChanged(const QString& module);
    void bindingsChanged();
    void configModeChanged();

private:
    void setStatus(const QString& s);

    QString m_root;
    QString m_inventoryPath;
    QString m_status;
    bool m_userMode = false;
    // module -> staged key/value edits, not yet written to disk
    std::map<QString, std::map<QString, QString>> m_staged;
};

} // namespace tp
