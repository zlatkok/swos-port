#pragma once

#include "SymbolAction.h"
#include "StringMap.h"

class SymbolTable {
public:
    SymbolTable();
    SymbolTable(const SymbolTable& other);
    SymbolTable& operator=(const SymbolTable&) = delete;

    void addSymbolAction(const char *symStart, const char *symEnd, SymbolAction action,
        const char *auxSymStart = nullptr, const char *auxSymEnd = nullptr);
    void addSymbolAction(const String& symStr, SymbolAction action, const String& auxSymStr);
    void addSymbolReplacement(const char *symStart, const char *symEnd, SymbolAction action,
        const char *replacementStart, const char *replacementEnd);

    void seal();

    std::pair<SymbolAction, String> symbolAction(CToken *sym, size_t removeMask = 0) const;
    std::pair<SymbolAction, String> symbolAction(const String& sym, size_t removeMask = 0) const;
    void clearAction(const String& sym, size_t removeMask);

    String symbolReplacement(const String& sym) const;

    void mergeSymbols(const SymbolTable& other);
    const std::vector<String> unusedSymbolsForRemoval() const;

private:
#pragma pack(push, 1)
    class SymbolRangeHolder
    {
    public:
        SymbolRangeHolder(SymbolAction action, const char *endSym, size_t endSymLen) : m_action(action) {
            Util::assignSize(m_endSymLength, endSymLen);
            memcpy(endSymPtr(), endSym, endSymLen);
        }
        SymbolRangeHolder(SymbolAction action, const String& endSym)
            : SymbolRangeHolder(action, endSym.data(), endSym.length()) {}
        static size_t requiredSize(SymbolAction, const char *, size_t endSymLen) {
            return sizeof(SymbolRangeHolder) + endSymLen;
        }
        static size_t requiredSize(SymbolAction action, const String& endSym) {
            return requiredSize(action, endSym.data(), endSym.length());
        }
        SymbolAction action() const { return m_action; }
        void setAction(SymbolAction action) { m_action = action; }
        String endSymbol() const { return { endSymPtr(), m_endSymLength }; }

    private:
        char *endSymPtr() const { return (char *)(this + 1); }

        SymbolAction m_action;
        uint8_t m_endSymLength;
    };
#pragma pack(pop)

    StringMap<SymbolRangeHolder> m_procs;
    StringMap<SymbolRangeHolder> m_replacements;
};
