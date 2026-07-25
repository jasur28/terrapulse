#pragma once

#include "terrapulse/config/log.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace tp::config {

struct Symbol {
    using Values = std::vector<std::string>;

    std::string name;
    std::string ns;
    std::string content;
    Values values;
    std::string uri;
    std::string comment;
    int stage = -1;
    int line = -1;

    std::string toString() const;
};

class SymbolTable {
public:
    using Symbols = std::map<std::string, Symbol>;
    using SymbolOrder = std::vector<Symbol*>;
    using IncludedFiles = std::set<std::string>;

    void setLogger(Logger* logger) { m_logger = logger; }
    Logger* logger() const { return m_logger; }

    void add(Symbol symbol);
    void add(const std::string& name, const std::string& ns, const std::string& content,
             const std::vector<std::string>& values, const std::string& uri,
             const std::string& comment = {}, int stage = -1, int line = -1);

    Symbol* get(const std::string& name);
    const Symbol* get(const std::string& name) const;
    bool remove(const std::string& name);

    std::string toString() const;

    bool hasFileBeenIncluded(const std::string& fileName) const;
    void addToIncludedFiles(const std::string& fileName);

    SymbolOrder::const_iterator begin() const { return m_order.begin(); }
    SymbolOrder::const_iterator end() const { return m_order.end(); }
    IncludedFiles::const_iterator includesBegin() const { return m_includedFiles.begin(); }
    IncludedFiles::const_iterator includesEnd() const { return m_includedFiles.end(); }

private:
    Symbols m_symbols;
    SymbolOrder m_order;
    IncludedFiles m_includedFiles;
    Logger* m_logger = nullptr;
};

} // namespace tp::config
