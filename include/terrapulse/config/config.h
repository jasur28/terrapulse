#pragma once

#include "terrapulse/config/exceptions.h"
#include "terrapulse/config/log.h"
#include "terrapulse/config/symboltable.h"

#include <QString>

#include <map>
#include <string>
#include <vector>

namespace tp::config {

using Variables = std::map<std::string, std::string>;

class Config {
public:
    bool readConfig(const std::string& file, int stage = -1, bool raw = false);
    bool writeConfig(const std::string& file, bool localOnly = true, bool multilineLists = false) const;

    void setLogger(Logger* logger);
    std::vector<std::string> names() const;
    std::string symbolsToString() const;
    std::string visitedFilesToString() const;

    bool has(const std::string& name) const;
    bool remove(const std::string& name);

    int getInt(const std::string& name) const;
    double getDouble(const std::string& name) const;
    bool getBool(const std::string& name) const;
    std::string getString(const std::string& name) const;

    std::vector<int> getInts(const std::string& name) const;
    std::vector<double> getDoubles(const std::string& name) const;
    std::vector<bool> getBools(const std::string& name) const;
    std::vector<std::string> getStrings(const std::string& name) const;

    bool setInt(const std::string& name, int value);
    bool setDouble(const std::string& name, double value);
    bool setBool(const std::string& name, bool value);
    bool setString(const std::string& name, const std::string& value);
    bool setStrings(const std::string& name, const std::vector<std::string>& values);

    std::vector<std::string> findSymbols(const std::string& prefix,
                                         const std::string& enabledSymbol = {},
                                         bool enabledDefault = true) const;

    SymbolTable* symbolTable() { return &m_symbols; }
    const SymbolTable* symbolTable() const { return &m_symbols; }

    bool eval(const std::string& rvalue, std::vector<std::string>& result,
              bool resolveReferences = true, std::string* error = nullptr) const;

    static bool Eval(const std::string& rvalue, std::vector<std::string>& result,
                     bool resolveReferences = true, const SymbolTable* symbols = nullptr,
                     std::string* error = nullptr);

    void trackVariables(bool enabled) { m_trackVariables = enabled; }
    const Variables& getVariables() const { return m_variables; }

private:
    const Symbol& require(const std::string& name, const char* type) const;
    static std::vector<std::string> splitValues(const std::string& content);
    static std::string trim(std::string value);
    static std::string stripQuotes(const std::string& value);
    static std::string resolve(const std::string& value, const SymbolTable* symbols);
    void addVariable(const std::string& name, const char* type) const;

    SymbolTable m_symbols;
    Logger* m_logger = nullptr;
    mutable Variables m_variables;
    bool m_trackVariables = false;
};

} // namespace tp::config
