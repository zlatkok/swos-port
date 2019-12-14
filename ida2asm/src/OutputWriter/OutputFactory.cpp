#include "OutputFactory.h"
#include "VerbatimOutput.h"
#include "MasmOutput.h"
#include "COutput.h"

enum OutputFormatEnum {
    kVerbatim,
    kMasm,
    kC,
    kNumFormats
};

const struct OutputFormat {
    const char *name;
    OutputFormatEnum type;
};

std::array<OutputFormat, kNumFormats> kFormats = {
    "verbatim", kVerbatim,
    "MASM", kMasm,
    "C", kC,
};

std::string OutputFactory::getSupportedFormats()
{
    std::string formats;
    for (auto format : kFormats)
        formats += std::string(", ") + format.name;

    return formats.substr(2);
}

static const OutputFormat *findOutputFormat(const char *inFormat)
{
    for (const auto& format : kFormats)
        if (!_stricmp(format.name, inFormat))
            return &format;

    return nullptr;
}

bool OutputFactory::formatSupported(const char *format)
{
    return findOutputFormat(format) != nullptr;
}

std::unique_ptr<OutputWriter> OutputFactory::create(const char *formatStr, const char *path, const SymbolFileParser& symFileParser,
    const StructStream& structs, const DefinesMap& defines, const References& references, const OutputItemStream& outputItems)
{
    auto format = findOutputFormat(formatStr);
    if (!format)
        return nullptr;

    switch (format->type) {
    case kVerbatim:
        return std::make_unique<VerbatimOutput>(path, symFileParser, structs, defines, outputItems);
    case kMasm:
        return std::make_unique<MasmOutput>(path, symFileParser, structs, defines, references, outputItems);
    case kC:
        return std::make_unique<COutput>(path, symFileParser, structs, defines, references, outputItems);
    default:
        assert(false);
        return nullptr;
    }
}
