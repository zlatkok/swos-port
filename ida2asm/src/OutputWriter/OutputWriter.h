#pragma once

#include "Util.h"
#include "Tokenizer.h"
#include "SymbolFileParser.h"
#include "Struct.h"
#include "DefinesMap.h"
#include "References.h"
#include "OutputItem/OutputItem.h"

class StringList;

class OutputWriter
{
public:
    enum OutputFlags {
        kStructs = 1,
        kDefines = 2,
        kDisassembly = 4,
        kExterns = 8,
        kPublics = 16,
        kLast,
        kFullDisasembly = kDisassembly | kExterns | kPublics,
        kOutputAll = (kLast - 1) * 2 - 1,
    };

    static_assert(((kOutputAll + 1) & kOutputAll) == 0, "flags are roNNg");

    OutputWriter(const char *path, const SymbolFileParser& symFileParser, const StructStream& structs, const DefinesMap& defines,
        const References& references, const OutputItemStream& outputItems);
    virtual ~OutputWriter();
    virtual void setOutputPrefix(const std::string& prefix) = 0;
    virtual void setCImportSymbols(const StringList *syms) {}
    virtual void setCExportSymbols(const StringList *syms) {}
    virtual void setGlobalStructVars(const StringMap<PascalString> *structVars);
    virtual void setDisassemblyPrefix(const std::string& prefix) = 0;
    virtual bool output(OutputFlags flags, CToken *openingSegment = nullptr) = 0;
    virtual const char *getDefsFilename() const = 0;
    virtual std::string getOutputError() const { return m_error; }
    virtual std::string segmentDirective(const TokenRange& range) const = 0;
    virtual std::string endSegmentDirective(const TokenRange& range) const = 0;
    virtual std::pair<const char *, size_t> getContiguousVariablesStructData() { return {}; }

protected:
    static constexpr int kTabSize = 4;
    static constexpr char kIndent[kTabSize + 1] = { ' ', ' ', ' ', ' ', '\0' };

    bool openOutputFile(OutputFlags flags, int anticipatedSize = -1);
    std::string getOutputFilename(OutputFlags flags);
    std::string getOutputBaseDir();
    void closeOutputFile();
    int outputTab(int column);
    int outputComment(const String& comment, int column = 0);
    bool save();
    size_t outputLength() const;
    char *getOutputPtr() const;
    char lastOutputChar() const;
    void setOutputPtr(char *ptr);
    void resetOutputPtr();
    void removeOutputChar(size_t numChars = 1);

    std::string m_error;

    const SymbolFileParser& m_symFileParser;
    const StructStream& m_structs;
    const DefinesMap& m_defines;
    const References& m_references;
    const OutputItemStream& m_outputItems;
    const StringMap<PascalString> *m_globalStructVars = nullptr;

#include "OutputWriterTemplates.h"

private:
    bool openOutputFile(const char *path, size_t requiredSize);

    const char *m_path;
    FILE *m_file = nullptr;
    std::unique_ptr<char[]> m_outBuffer;
    char *m_outPtr = nullptr;
    size_t m_outBufferSize = 0;
};

ENABLE_FLAGS(OutputWriter::OutputFlags)
