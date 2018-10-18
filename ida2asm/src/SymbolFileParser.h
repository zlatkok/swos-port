#pragma once

#include "StringMap.h"
#include "StringList.h"
#include "Tokenizer.h"

class SymbolFileParser
{
public:
    enum SymbolAction : uint8_t {
        kNone = 0,
        kRemove = 1,
        kNull = 2,
        kExport = 4,
        kImport = 8,
        kSaveCppRegisters = 16,
        kOnEnter = 32,
        kReplace = 64,
    };

    SymbolFileParser(const char *symbolsFilePath, const char *headerFilePath);
    void outputHeaderFile(const char *path);
    std::pair<SymbolAction, String> symbolAction(CToken *sym) const;
    std::pair<SymbolAction, String> lookupSymbolAction(const String& sym) const;
    String symbolReplacement(const String& sym) const;
    const std::vector<String> unusedSymbolsForRemoval() const;
    const StringList& exports() const;
    const StringList& imports() const;

private:
    void parseSymbolFile();
    SymbolAction getSectionName(const char *begin, const char *end);
    const char *addExportEntry(const char *start, const char *p);
    std::pair<const char *, const char *> getNextToken(const char *p);
    void error(const std::string& desc);

    void xfwrite(FILE *file, const char *buf, size_t len);
    void xfwrite(FILE *file, const String& str);
    template <size_t N> void xfwrite(FILE *file, const char (&buf)[N]);

    const char *m_path;
    const char *m_headerPath;
    std::unique_ptr<const char []> m_data;
    size_t m_dataSize = 0;
    size_t m_lineNo = 1;

    enum Namespace { kGlobal, kEndRange, kSaveRegs, kReplaceNs, kOnEnterNs, };

    struct Hasher {
        size_t operator()(const std::pair<std::string, Namespace>& key) const {
            return std::hash<std::string>{}(key.first);
        }
    };

    std::unordered_map<std::pair<std::string, Namespace>, size_t, Hasher> m_symbolLine;

    void ensureUniqueSymbol(const char *start, const char *end, Namespace symNamespace);

    StringList m_exports;
    StringList m_imports;

    class SymbolRangeHolder
    {
    public:
        SymbolRangeHolder(SymbolAction action, const char *endSym, size_t endSymLen) : m_action(action) {
            Util::assignSize(m_endSymLength, endSymLen);
            memcpy(endSymPtr(), endSym, endSymLen);
        }
        static size_t requiredSize(SymbolAction, const char *, size_t endLen) {
            return sizeof(SymbolRangeHolder) + endLen;
        }
        SymbolAction action() const { return m_action; }
        void setAction(SymbolAction action) { m_action = action; }
        String endSymbol() const { return { endSymPtr(), m_endSymLength }; }

    private:
        char *endSymPtr() const { return (char *)(this + 1); }

        SymbolAction m_action;
        uint8_t m_endSymLength;
    };

    StringMap<SymbolRangeHolder> m_procs;
    StringMap<SymbolRangeHolder> m_replacements;

    struct ExportEntry {
        String symbol;
        String prefix;
        bool function = false;
        String type;
        String arraySize;
        bool array = false;
    };

    std::vector<ExportEntry> m_exportEntries;

    enum KeywordType { kNotKeyword, kPrefix, kFunction, kPointer, kPtr, kArray };

    std::pair<KeywordType, const char *> lookupKeyword(const char *p, const char *limit);
};

ENABLE_FLAGS(SymbolFileParser::SymbolAction)
