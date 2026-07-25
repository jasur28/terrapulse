#include "terrapulse/config/config.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace tp::config {

void Logger::log(LogLevel, const std::string&, int, const std::string&) {}

std::string Symbol::toString() const {
    std::ostringstream os;
    os << name << " = ";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) os << ", ";
        os << values[i];
    }
    return os.str();
}

void SymbolTable::add(Symbol symbol) {
    auto it = m_symbols.find(symbol.name);
    if (it == m_symbols.end()) {
        auto inserted = m_symbols.emplace(symbol.name, std::move(symbol));
        m_order.push_back(&inserted.first->second);
    } else {
        it->second = std::move(symbol);
    }
}

void SymbolTable::add(const std::string& name, const std::string& ns, const std::string& content,
                      const std::vector<std::string>& values, const std::string& uri,
                      const std::string& comment, int stage, int line) {
    Symbol symbol;
    symbol.name = name;
    symbol.ns = ns;
    symbol.content = content;
    symbol.values = values;
    symbol.uri = uri;
    symbol.comment = comment;
    symbol.stage = stage;
    symbol.line = line;
    add(std::move(symbol));
}

Symbol* SymbolTable::get(const std::string& name) {
    auto it = m_symbols.find(name);
    return it == m_symbols.end() ? nullptr : &it->second;
}

const Symbol* SymbolTable::get(const std::string& name) const {
    auto it = m_symbols.find(name);
    return it == m_symbols.end() ? nullptr : &it->second;
}

bool SymbolTable::remove(const std::string& name) {
    auto it = m_symbols.find(name);
    if (it == m_symbols.end()) return false;
    Symbol* ptr = &it->second;
    m_order.erase(std::remove(m_order.begin(), m_order.end(), ptr), m_order.end());
    m_symbols.erase(it);
    return true;
}

std::string SymbolTable::toString() const {
    std::ostringstream os;
    for (const Symbol* symbol : m_order) os << symbol->toString() << "\n";
    return os.str();
}

bool SymbolTable::hasFileBeenIncluded(const std::string& fileName) const {
    return m_includedFiles.find(fileName) != m_includedFiles.end();
}

void SymbolTable::addToIncludedFiles(const std::string& fileName) {
    m_includedFiles.insert(fileName);
}

bool Config::readConfig(const std::string& file, int stage, bool raw) {
    QFile f(QString::fromStdString(file));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    m_symbols.addToIncludedFiles(file);

    QTextStream in(&f);
    int lineNo = 0;
    while (!in.atEnd()) {
        ++lineNo;
        std::string line = in.readLine().toStdString();
        const auto hash = line.find('#');
        std::string comment;
        if (hash != std::string::npos) {
            comment = line.substr(hash + 1);
            line = line.substr(0, hash);
        }
        line = trim(line);
        if (line.empty()) continue;

        if (line.rfind("include", 0) == 0) {
            std::string includePath = stripQuotes(trim(line.substr(7)));
            if (!includePath.empty() && !m_symbols.hasFileBeenIncluded(includePath))
                readConfig(includePath, stage, raw);
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            if (m_logger) m_logger->log(LogLevel::Warning, file, lineNo, "ignored line without assignment");
            continue;
        }

        const std::string name = trim(line.substr(0, eq));
        std::string content = trim(line.substr(eq + 1));
        if (!raw) content = resolve(content, &m_symbols);
        m_symbols.add(name, {}, content, splitValues(content), file, comment, stage, lineNo);
    }

    return true;
}

bool Config::writeConfig(const std::string& file, bool, bool) const {
    QFile f(QString::fromStdString(file));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    for (auto it = m_symbols.begin(); it != m_symbols.end(); ++it)
        out << QString::fromStdString((*it)->toString()) << "\n";
    return true;
}

void Config::setLogger(Logger* logger) {
    m_logger = logger;
    m_symbols.setLogger(logger);
}

std::vector<std::string> Config::names() const {
    std::vector<std::string> out;
    for (auto it = m_symbols.begin(); it != m_symbols.end(); ++it) out.push_back((*it)->name);
    return out;
}

std::string Config::symbolsToString() const {
    return m_symbols.toString();
}

std::string Config::visitedFilesToString() const {
    std::ostringstream os;
    for (auto it = m_symbols.includesBegin(); it != m_symbols.includesEnd(); ++it) os << *it << "\n";
    return os.str();
}

bool Config::has(const std::string& name) const {
    return m_symbols.get(name) != nullptr;
}

bool Config::remove(const std::string& name) {
    return m_symbols.remove(name);
}

const Symbol& Config::require(const std::string& name, const char* type) const {
    addVariable(name, type);
    const Symbol* symbol = m_symbols.get(name);
    if (!symbol || symbol->values.empty()) throw OptionNotFoundException(name);
    return *symbol;
}

int Config::getInt(const std::string& name) const {
    const auto& s = require(name, "int");
    try { return std::stoi(s.values.front()); }
    catch (...) { throw TypeConversionException(s.values.front()); }
}

double Config::getDouble(const std::string& name) const {
    const auto& s = require(name, "double");
    try { return std::stod(s.values.front()); }
    catch (...) { throw TypeConversionException(s.values.front()); }
}

bool Config::getBool(const std::string& name) const {
    const auto& s = require(name, "bool");
    std::string v = s.values.front();
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (v == "true" || v == "yes" || v == "on" || v == "1") return true;
    if (v == "false" || v == "no" || v == "off" || v == "0") return false;
    throw TypeConversionException(s.values.front());
}

std::string Config::getString(const std::string& name) const {
    return stripQuotes(require(name, "string").values.front());
}

std::vector<int> Config::getInts(const std::string& name) const {
    std::vector<int> out;
    for (const auto& v : require(name, "list<int>").values) out.push_back(std::stoi(v));
    return out;
}

std::vector<double> Config::getDoubles(const std::string& name) const {
    std::vector<double> out;
    for (const auto& v : require(name, "list<double>").values) out.push_back(std::stod(v));
    return out;
}

std::vector<bool> Config::getBools(const std::string& name) const {
    std::vector<bool> out;
    for (const auto& v : require(name, "list<bool>").values) {
        std::string vv = v;
        std::transform(vv.begin(), vv.end(), vv.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        out.push_back(vv == "true" || vv == "yes" || vv == "on" || vv == "1");
    }
    return out;
}

std::vector<std::string> Config::getStrings(const std::string& name) const {
    std::vector<std::string> out;
    for (const auto& v : require(name, "list<string>").values) out.push_back(stripQuotes(v));
    return out;
}

bool Config::setInt(const std::string& name, int value) {
    m_symbols.add(name, {}, std::to_string(value), {std::to_string(value)}, {}, {});
    return true;
}

bool Config::setDouble(const std::string& name, double value) {
    const std::string s = std::to_string(value);
    m_symbols.add(name, {}, s, {s}, {}, {});
    return true;
}

bool Config::setBool(const std::string& name, bool value) {
    const std::string s = value ? "true" : "false";
    m_symbols.add(name, {}, s, {s}, {}, {});
    return true;
}

bool Config::setString(const std::string& name, const std::string& value) {
    m_symbols.add(name, {}, value, {value}, {}, {});
    return true;
}

bool Config::setStrings(const std::string& name, const std::vector<std::string>& values) {
    std::ostringstream content;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) content << ", ";
        content << values[i];
    }
    m_symbols.add(name, {}, content.str(), values, {}, {});
    return true;
}

std::vector<std::string> Config::findSymbols(const std::string& prefix,
                                             const std::string& enabledSymbol,
                                             bool enabledDefault) const {
    std::set<std::string> roots;
    for (auto it = m_symbols.begin(); it != m_symbols.end(); ++it) {
        const std::string& name = (*it)->name;
        if (name.rfind(prefix, 0) != 0) continue;
        std::string tail = name.substr(prefix.size());
        const auto dot = tail.find('.');
        roots.insert(prefix + (dot == std::string::npos ? tail : tail.substr(0, dot)));
    }

    std::vector<std::string> out;
    for (const auto& root : roots) {
        if (!enabledSymbol.empty()) {
            const std::string enabledKey = root + "." + enabledSymbol;
            if (has(enabledKey)) {
                if (!getBool(enabledKey)) continue;
            } else if (!enabledDefault) continue;
        }
        out.push_back(root);
    }
    return out;
}

bool Config::eval(const std::string& rvalue, std::vector<std::string>& result,
                  bool resolveReferences, std::string* error) const {
    return Eval(rvalue, result, resolveReferences, &m_symbols, error);
}

bool Config::Eval(const std::string& rvalue, std::vector<std::string>& result,
                  bool resolveReferences, const SymbolTable* symbols, std::string* error) {
    try {
        result = splitValues(resolveReferences ? resolve(rvalue, symbols) : rvalue);
        return true;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

std::vector<std::string> Config::splitValues(const std::string& content) {
    std::vector<std::string> out;
    std::string token;
    bool quoted = false;
    for (char c : content) {
        if (c == '"') quoted = !quoted;
        if (c == ',' && !quoted) {
            out.push_back(stripQuotes(trim(token)));
            token.clear();
        } else {
            token.push_back(c);
        }
    }
    if (!token.empty()) out.push_back(stripQuotes(trim(token)));
    return out;
}

std::string Config::trim(std::string value) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string Config::stripQuotes(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return value.substr(1, value.size() - 2);
    return value;
}

std::string Config::resolve(const std::string& value, const SymbolTable* symbols) {
    std::string out = value;
    std::size_t pos = 0;
    while ((pos = out.find("${", pos)) != std::string::npos) {
        const auto end = out.find('}', pos + 2);
        if (end == std::string::npos) break;
        const std::string key = out.substr(pos + 2, end - pos - 2);
        std::string repl;
        if (symbols) {
            const Symbol* s = symbols->get(key);
            if (s && !s->values.empty()) repl = s->values.front();
        }
        out.replace(pos, end - pos + 1, repl);
        pos += repl.size();
    }
    return out;
}

void Config::addVariable(const std::string& name, const char* type) const {
    if (m_trackVariables) m_variables[name] = type;
}

} // namespace tp::config
