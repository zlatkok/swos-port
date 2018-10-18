#pragma once

#include "Tokenizer.h"
#include "SymbolFileParser.h"
#include "IdaAsmParser.h"
#include "Struct.h"
#include "DefinesMap.h"
#include "OutputWriter/OutputFactory.h"

class AsmConverterWorker
{
public:
    AsmConverterWorker(int index, const char *data, int dataLength, int chunkOffset, int chunkLength,
        const SymbolFileParser& symFileParser);
    void process();
    void output(const char *format, const char *path, const StructStream& structs,
        const DefinesMap& defines, const std::string& prefix, std::pair<CToken *, bool> openSegment);
    void resolveReferences(const std::vector<const AsmConverterWorker *>& workers, const SegmentSet& segments,
        const StructStream& structs, const DefinesMap& defines);
    void setCImportSymbols(const StringList& syms);
    void setCExportSymbols(const StringList& syms);
    std::pair<bool, bool> noBreakTagState() const;
    bool outputOk() const;
    std::string limitsError() const;
    std::string outputError() const;
    std::string filename() const;
    const IdaAsmParser& parser() const;
    IdaAsmParser& parser();

private:
    void resolveLocalReferences();
    void resolveImports();
    void resolveStructsAndDefines(const StructStream& structs, const DefinesMap& defines);
    std::string segmentsOutput(CToken *openSegment);
    int getPreviousNonCommentLine(int offset) const;
    int getPreviousLine(int offset) const;
    bool isLineComment(int offset) const;
    std::tuple<int, int, int> findLimits() const;
    std::string formOutputPath(const char *path) const;

    const char *m_data = nullptr;
    int m_dataLength = 0;
    int m_offset = 0;
    int m_chunkLength = 0;
    int m_index;

    const SymbolFileParser& m_symFileParser;

    StructStream m_structs;
    DefinesMap m_defines;
    Tokenizer m_tokenizer;
    IdaAsmParser m_parser;
    std::unique_ptr<OutputWriter> m_outputWriter;

    const StringList *m_cImportSymbols = nullptr;
    const StringList *m_cExportSymbols = nullptr;

    std::string m_filename;
    std::string m_limitsError;
    bool m_noBreakOverflow = false;
    bool m_noBreakContinued = false;
    bool m_outputOk = false;

    const SegmentSet *m_segments{};
};
