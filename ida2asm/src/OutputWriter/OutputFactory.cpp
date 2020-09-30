#include "OutputFactory.h"
#include "VerbatimOutput.h"
#include "MasmOutput.h"
#include "CppOutput/CppOutput.h"

std::unique_ptr<OutputWriter> OutputFactory::create(OutputFormatResolver::OutputFormat format, const char *path, int index,
    int extraMemorySize, bool disableOptimizations, const SymbolFileParser& symFileParser, const StructStream& structs,
    const DefinesMap& defines, const References& references, const OutputItemStream& outputItems, const DataBank& dataBank)
{
    switch (format) {
    case OutputFormatResolver::kVerbatim:
        return std::make_unique<VerbatimOutput>(path, symFileParser, structs, defines, outputItems);
    case OutputFormatResolver::kMasm:
        return std::make_unique<MasmOutput>(path, symFileParser, structs, defines, references, outputItems);
    case OutputFormatResolver::kCpp:
        return std::make_unique<CppOutput>(path, index, extraMemorySize, disableOptimizations, symFileParser,
            structs, defines, references, outputItems, dataBank);
    default:
        assert(false);
        return nullptr;
    }
}
