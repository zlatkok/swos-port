#pragma once

#include "OutputWriter.h"

class SymbolFileParser;
class StructStream;
class DefinesMap;
class References;
class OutputItemStream;

class OutputFactory
{
public:
    OutputFactory() = delete;

    static std::string OutputFactory::getSupportedFormats();
    static bool formatSupported(const char *format);
    static std::unique_ptr<OutputWriter> create(const char *format, const char *path, const SymbolFileParser& symFileParser,
        const StructStream& structs, const DefinesMap& defines, const References& references, const OutputItemStream& outputItems);
};
