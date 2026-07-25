#pragma once
#include <QString>
#include <QStringList>
#include <map>
#include <string>

namespace tp {

// Layered configuration, SeisComp-style. Every tunable a domain expert may want
// to change lives in a text file, not in the code — so thresholds, filter bands
// and window lengths can be adjusted without recompiling.
//
// Layers, lowest priority first (later overrides earlier):
//   <root>/etc/defaults/global.cfg      shipped defaults, documents every key
//   <root>/etc/defaults/<module>.cfg
//   <root>/etc/global.cfg               site configuration (operator edits these)
//   <root>/etc/<module>.cfg
//   ~/.terrapulse/global.cfg            per-user overrides
//   ~/.terrapulse/<module>.cfg
// Command-line flags override everything (each module applies them last).
//
// Format: `key = value`, one per line; `#` or `//` starts a comment; blank lines
// ignored. Keys are dotted (e.g. `proc.staLta.onRatio`).
class Config {
public:
    // Read all layers for `module`. `root` overrides discovery (else TP_ROOT env,
    // else the tree containing `etc/` relative to the executable).
    void load(const QString& module, const QString& root = QString());

    bool        has(const QString& key) const;
    QString     str(const QString& key, const QString& def = QString()) const;
    int         integer(const QString& key, int def = 0) const;
    double      number(const QString& key, double def = 0.0) const;
    bool        boolean(const QString& key, bool def = false) const;

    // Everything that was loaded, and from which files (for tpdumpcfg / --check).
    const std::map<QString, QString>& all() const { return m_values; }
    const QStringList& loadedFiles() const { return m_files; }
    QString root() const { return m_root; }

    // Apply a per-sensor BINDING on top of the loaded layers. Reads the key file
    // `etc/key/sensor_<object>_<sensor>`, finds the line naming `module`
    // (`module` or `module:profile`), and loads `etc/<module>/profile_<name>.cfg`.
    // This is how one structure gets different thresholds from another without
    // touching the global files. Returns the applied profile name ("" if none).
    QString loadBinding(const QString& module, unsigned object, unsigned sensor);

    // Path of the key file for a sensor (whether or not it exists).
    static QString keyFilePath(const QString& root, unsigned object, unsigned sensor);

    // Resolve the installation root without loading (used by tools).
    static QString discoverRoot(const QString& hint = QString());

private:
    void readFile(const QString& path);

    std::map<QString, QString> m_values;
    QStringList                m_files;
    QString                    m_root;
};

} // namespace tp
