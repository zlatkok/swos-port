#pragma once

#include "Util.h"
#include "Tokenizer.h"

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

    OutputWriter(const char *path);
    virtual ~OutputWriter();
    virtual void setOutputPrefix(const std::string& prefix) = 0;
    virtual void setCImportSymbols(const StringList *syms) {}
    virtual void setCExportSymbols(const StringList *syms) {}
    virtual void setDisassemblyPrefix(const std::string& prefix) = 0;
    virtual bool output(OutputFlags flags, CToken *openingSegment = nullptr) = 0;
    virtual std::string outputError() const { return {}; }
    virtual std::string segmentDirective(const TokenRange& range) const = 0;
    virtual std::string endSegmentDirective(const TokenRange& range) const = 0;

protected:
    static constexpr int kTabSize = 4;
    static constexpr char kIndent[kTabSize + 1] = { ' ', ' ', ' ', ' ', '\0' };

    void openOutputFile(size_t requiredSize);
    bool isOutputFileOpen() const;
    void closeOutputFile();
    int outputTab(int column);
    int outputComment(const String& comment, int column = 0);
    bool save();

#include "OutputWriterTemplates.h"

private:
    const char *m_path;
    FILE *m_file = nullptr;
    std::unique_ptr<char[]> m_outBuffer;
    char *m_outPtr = nullptr;
    size_t m_outBufferSize = 0;
};

ENABLE_FLAGS(OutputWriter::OutputFlags)
