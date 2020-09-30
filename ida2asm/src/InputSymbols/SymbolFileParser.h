#pragma once

#include "StringList.h"
#include "Tokenizer.h"
#include "BlockList.h"
#include "ProcHookList.h"
#include "SymbolAction.h"
#include "SymbolTable.h"
#include "PascalString.h"
#include "StringSet.h"

class SymbolFileParser
{
public:
    static constexpr char kEndMarker[5] = { "@end" };
    static constexpr char kOnEnterSuffix[9] = { "_OnEnter" };

    SymbolFileParser(const char *symbolsFilePath, const char *headerFilePath, const char *outputPath);
    SymbolFileParser(const SymbolFileParser& other);
    SymbolFileParser& operator=(const SymbolFileParser&) = delete;

    SymbolTable& symbolTable();
    const SymbolTable& symbolTable() const;

    void outputHeaderFile(const char *path);

    const StringList& exports() const;
    const StringSet& imports() const;
    int getTypeSize(const String& type) const;

    bool isImport(const String& var) const;
    void traverseExportedProcs(std::function<void(const String& name)> f) const;
    void traverseExports(std::function<bool(const String& symbol, const String& type, const String& arraySize,
        bool function, bool functionPointer, bool array)> process, std::function<size_t(const String&)> write) const;
    void traverseExportedArrays(std::function<void(const String&, const String&, int)> process) const;

    bool cppOutput() const;
    std::tuple<String, String, int> exportedDeclaration(const String& var) const;

private:
    struct ExportEntry {
        String symbol;
        String type;
        String arraySize;
        bool function = false;
        bool functionPointer = false;
        bool array = false;
        bool trailingArray = false;
    };

    void output68kRegisters();
    void outputExports();
    void outputImports();
    void outputSizeAssertations();
    template <typename F> void getExportFunctionDeclaration(const ExportEntry& exp, F write) const;
    template <typename F> void getExportVariableDeclaration(const ExportEntry& exp, F write) const;

    void parseSymbolFile();
    static const char *parseComment(const char *p);
    std::tuple<const char *, const char *, SymbolAction> parseSectionName(const char *p) const;
    static SymbolAction getSectionName(const char *begin, const char *end);
    const char *handlePotentialArray(const char *start, const char *p, ExportEntry& e);
    void parseHookProcLine(const char *symStart, const char *symEnd, const char *start, const char *end);
    void parseRemoveAndNullLine(SymbolAction action, const char *symStart, const char *symEnd, const char *start, const char *end);
    static bool isRemoveHook(const char *start, const char *end);
    void addHookProcs();
    void parseTypeSize(const char *symStart, const char *symEnd, const char *start, const char *end);

    const char *addExportEntry(const char *start, const char *p);
    const char *addImportEntry(const char *start, const char *p);

    std::pair<const char *, const char *> getNextToken(const char *p);
    static const char *skipWhiteSpace(const char *p);
    std::pair<int32_t, const char *> parseInt32(const char *start, const char *end);
    void error(const std::string& desc, int lineNo = -1) const;
    void commaExpectedError() const;

    void xfwrite(const char *buf, size_t len);
    void xfwrite(const String& str);
    template <size_t N> void xfwrite(const char (&buf)[N]);

    enum Namespace { kGlobal, kEndRange, kSaveRegs, kReplaceNs, kOnEnterNs, };

    struct Hasher {
        size_t operator()(const std::pair<std::string, Namespace>& key) const {
            return std::hash<std::string>{}(key.first);
        }
    };

    void ensureUniqueSymbol(const char *start, const char *end, Namespace symNamespace, SymbolAction action);

#pragma pack(push, 1)
    enum ImportReturnType : uint8_t { kVoid, kInt, };

    class ImportEntry {
    public:
        ImportEntry(const String& name, ImportReturnType returnType = kVoid) : m_returnType(returnType) {
            assert(name.length());
            Util::assignSize(m_nameLength, name.length());
            name.copy((char *)this + sizeof(*this));
        }
        static size_t requiredSize(const String& name, ImportReturnType = kVoid) {
            return sizeof(ImportEntry) + name.length();
        }
        size_t size() const {
            return sizeof(*this) + m_nameLength;
        }
        const String name() const {
            return { reinterpret_cast<const char *>(this) + sizeof(*this), m_nameLength };
        }
        ImportReturnType returnType() const { return m_returnType; }
        ImportEntry *next() const { return reinterpret_cast<ImportEntry *>((char *)this + size()); }

    private:
        ImportReturnType m_returnType;
        uint8_t m_nameLength;
    };
#pragma pack(pop)

    enum KeywordType { kNotKeyword, kFunction, kFunctionPointer, kPointer, kPtr, kArray };

    std::pair<KeywordType, const char *> lookupKeyword(const char *p, const char *limit);

    std::unordered_map<std::pair<std::string, Namespace>, std::pair<size_t, SymbolAction>, Hasher> m_symbolLine;

    const char *m_path;
    const char *m_headerPath;
    bool m_cppOutput;
    std::unique_ptr<const char[]> m_data;
    size_t m_dataSize = 0;
    size_t m_lineNo = 1;
    FILE *m_headerFile = nullptr;

    StringList m_exports;
    StringSet m_imports;

    std::vector<ExportEntry> m_exportEntries;
    BlockList<ImportEntry> m_importEntries;

    SymbolTable m_symbolTable;
    ProcHookList m_procHookList;
    StringMap<int> m_typeSizes;

    struct ExportTypeData {
        ExportTypeData(const char *str, size_t len, const String& baseType, int arraySize)
            : type(str, len), arraySize(arraySize) {
            new (baseTypePtr()) PascalString(baseType);
        }
        static size_t requiredSize(const char *, size_t len, const String& baseType, int) {
            return sizeof(ExportTypeData) + len + PascalString::requiredSize(baseType);
        }
        PascalString *baseTypePtr() const { return (PascalString *)((char *)(this + 1) + type.length()); }
        const PascalString& baseType() const { return *baseTypePtr(); }
        int arraySize;
        PascalString type;
        // baseType follows
    };
    StringMap<ExportTypeData> m_exportTypes;
};
