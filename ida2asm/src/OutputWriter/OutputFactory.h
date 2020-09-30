#pragma once

#include "OutputWriter.h"
#include "OutputFormatResolver.h"

class SymbolFileParser;
class StructStream;
class DefinesMap;
class References;
class OutputItemStream;
class DataBank;

class OutputFactory
{
public:
    OutputFactory() = delete;

    static std::unique_ptr<OutputWriter> create(OutputFormatResolver::OutputFormat format, const char *path, int index,
        int extraMemorySize, bool disableOptimizations, const SymbolFileParser& symFileParser, const StructStream& structs,
        const DefinesMap& defines, const References& references, const OutputItemStream& outputItems, const DataBank& dataBank);
};
