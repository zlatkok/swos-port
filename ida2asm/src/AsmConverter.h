#pragma once

#include "SymbolFileParser.h"
#include "Struct.h"
#include "DefinesMap.h"
#include "OutputWriter/OutputFactory.h"
#include "OutputItem/Segment.h"

class AsmConverterWorker;

class AsmConverter
{
public:
    AsmConverter(const char *inputPath, const char *outputPath, const char *symbolsFile,
        const char *swosHeaderFile, const char *format, int numFiles);
    void convert();

private:
    void loadFile(const char *path);
    std::pair<const char *, String> findCodeDataStart() const;
    const char *skipBom(int& length);
    size_t parseCommonPart(int length);
    size_t parse(int commonPartLength, int blockSize);
    void checkForParsingErrors(size_t lineNo);

    using AllowedChunkList = std::vector<int>;

    AllowedChunkList connectRanges();
    void resolveExterns(const AllowedChunkList& activeChunks);

    void collectSegments();
    void output(const String& commonPrefix, const AllowedChunkList& activeChunks);
    void checkForOutputErrors();
    void checkForUnusedSymbols();
    void outputStructsAndDefines();
    void waitForWorkers();
    void error(const std::string& desc, size_t lineNo);

    int m_numFiles;

    const char *m_inputPath;
    const char *m_outputPath;
    const char *m_headerPath;
    const char *m_format;
    std::unique_ptr<const char[]> m_data;
    long m_dataLength = 0;

    SymbolFileParser m_symFileParser;
    StructStream m_structs;
    DefinesMap m_defines;
    SegmentSet m_segments;
    std::unique_ptr<OutputWriter> m_outputWriter;

    std::vector<AsmConverterWorker *> m_workers;
    std::vector<std::future<void>> m_futures;

    std::vector<SymbolTable *> m_symbolTables;
};
