#include "InputConverterWorker.h"
#include "Struct.h"
#include "DefinesMap.h"
#include "DataItem.h"

InputConverterWorker::InputConverterWorker(int index, const char *data, int dataLength,
    int chunkOffset, int chunkLength, const SymbolFileParser& symFileParser, SymbolTable& symbolTable)
:
    m_index(index), m_data(data), m_dataLength(dataLength), m_offset(chunkOffset), m_chunkLength(chunkLength),
    m_symFileParser(symFileParser), m_defines(0), m_tokenizer(), m_parser(symFileParser, symbolTable, m_tokenizer, m_structs, m_defines),
    m_varData(std::make_tuple(DataBank::VarList{}, DataBank::StructVarsMap(0), DataBank::OffsetMap(0)))
{
}

void InputConverterWorker::process()
{
    assert(m_data && m_dataLength && m_chunkLength);

    int from, softLimit, hardLimit;
    std::tie(from, softLimit, hardLimit) = findLimits();

    auto size = softLimit - from;
    auto hardSize = hardLimit - from;
    auto dataStart = m_data + from;

    auto limits = m_tokenizer.tokenize(dataStart, size, hardSize);
    std::tie(m_limitsError, m_noBreakContinued, m_noBreakOverflow) = m_tokenizer.determineBlockLimits(limits);

    m_parser.parse();

    resolveLocalReferences();
    resolveImports();
}

void InputConverterWorker::output(OutputFormatResolver::OutputFormat format, const char *path, int extraMemorySize,
    bool disableOptimizations, const StructStream& structs, const DefinesMap& defines, const std::string& prefix,
    std::pair<CToken *, bool> openingSegments)
{
    assert(m_segments);

    m_filename = formOutputPath(path);
    m_outputWriter = OutputFactory::create(format, m_filename.c_str(), m_index, extraMemorySize, disableOptimizations,
        m_symFileParser, structs, defines, m_parser.references(), m_parser.outputItems(), *m_dataBank);

    auto outputPrefix = segmentsOutput(openingSegments.first);
    m_outputWriter->setOutputPrefix(prefix + outputPrefix);

    m_outputWriter->setCImportSymbols(m_cImportSymbols);
    m_outputWriter->setCExportSymbols(m_cExportSymbols);

    if (openingSegments.second) {
        const auto& range = m_segments->segmentRange(openingSegments.first);
        auto disassemblyPrefix = m_outputWriter->endSegmentDirective(range);
        m_outputWriter->setDisassemblyPrefix(disassemblyPrefix);
    }

    m_outputOk = m_outputWriter->output(OutputWriter::kFullDisasembly, openingSegments.first);
}

void InputConverterWorker::resolveReferences(const std::vector<const InputConverterWorker *>& workers, const SegmentSet& segments,
    const StructStream& structs, const DefinesMap& defines)
{
    m_segments = &segments;

    m_parser.references().resolveSegments(segments);
    resolveStructsAndDefines(structs, defines);

    for (auto worker : workers)
        if (worker != this)
            m_parser.references().resolve(worker->m_parser.references());

    if (m_dataBank)
        m_varData = m_dataBank->processRegion(m_parser.outputItems(), structs, defines);
}

void InputConverterWorker::setCImportSymbols(const StringSet& syms)
{
    m_cImportSymbols = &syms;
}

void InputConverterWorker::setCExportSymbols(const StringList& syms)
{
    m_cExportSymbols = &syms;
}

void InputConverterWorker::setDataBank(DataBank *dataBank)
{
    m_dataBank = dataBank;
}

DataBank::VarData&& InputConverterWorker::variables()
{
    return std::move(m_varData);
}

std::pair<bool, bool> InputConverterWorker::noBreakTagState() const
{
    return { m_noBreakContinued, m_noBreakOverflow };
}

bool InputConverterWorker::outputOk() const
{
    return m_outputOk;
}

std::string InputConverterWorker::limitsError() const
{
    return m_limitsError;
}

std::string InputConverterWorker::getOutputError() const
{
    return m_outputWriter->getOutputError();
}

std::string InputConverterWorker::filename() const
{
    return m_filename;
}

const IdaAsmParser& InputConverterWorker::parser() const
{
    return m_parser;
}

IdaAsmParser& InputConverterWorker::parser()
{
    return m_parser;
}

OutputWriter& InputConverterWorker::outputWriter()
{
    return *m_outputWriter.get();
}

void InputConverterWorker::resolveLocalReferences()
{
    m_parser.references().resolve();
}

void InputConverterWorker::resolveImports()
{
    for (const auto& imp : m_symFileParser.imports())
        m_parser.references().markImport(imp.string());
}

void InputConverterWorker::resolveStructsAndDefines(const StructStream& structs, const DefinesMap& defines)
{
    for (const auto& struc : structs)
        m_parser.references().setIgnored(struc.name(), struc.hash());

    for (const auto& def : defines)
        m_parser.references().setIgnored(def.text, def.hash);
}

std::string InputConverterWorker::segmentsOutput(CToken *openSegment)
{
    assert(m_segments);
    assert(m_outputWriter);

    std::string result;

    for (const auto& segmentRange : *m_segments) {
        if (segmentRange.first != openSegment) {
            result += m_outputWriter->segmentDirective(segmentRange);
            result += m_outputWriter->endSegmentDirective(segmentRange);
        }
    }

    if (openSegment) {
        const auto& range = m_segments->segmentRange(openSegment);
        result += m_outputWriter->segmentDirective(range);
    }

    return result;
}

int InputConverterWorker::getPreviousNonCommentLine(int offset) const
{
    if (isLineComment(offset)) {
        auto prevLineOffset = getPreviousLine(offset);
        while (prevLineOffset < offset && isLineComment(prevLineOffset)) {
            offset = prevLineOffset;
            prevLineOffset = getPreviousLine(prevLineOffset);
        }
    }

    return offset;
}

int InputConverterWorker::getPreviousLine(int offset) const
{
    if (offset < 2)
        return offset;

    for (offset -= 2; offset >= 0; offset--)
        if (m_data[offset] == '\n')
            return offset + 1;

    return offset;
}

bool InputConverterWorker::isLineComment(int offset) const
{
    if (!memcmp(m_data + offset, &Util::kNewLine, Util::kNewLine.size()))
        return true;

    if (m_data[offset])
        while (m_data[offset] != '\n')
            if (m_data[offset++] == ';')
                return true;

    return false;
}

std::tuple<int, int, int> InputConverterWorker::findLimits() const
{
    // tokenize additional area to capture whole procs and to keep named data items inside a single file
    // this area must be larger than any area without data items and procs in asm file, as well as any no break area
    constexpr int kAdditionalLength = 50'000;

    int from = m_offset;
    int to = std::min(m_dataLength - 1, m_offset + m_chunkLength);

    // we're relying on new line always being present at the end of the file
    if (from > 0) {
        while (m_data[from] != '\n')
            from++;
        from++;
    }

    while (m_data[to] != '\n')
        to++;
    to++;

    // try not to break leading comments from procs and data items
    if (from > 0)
        from = getPreviousNonCommentLine(from);

    to = getPreviousNonCommentLine(to);

    // we'll add some extra amount to account for procs and data items that might be cut off at the end
    int hardLimit = std::min(m_dataLength - 1, to + kAdditionalLength);

    while (m_data[hardLimit] != '\n')
        hardLimit++;
    hardLimit++;

    assert(from >= 0 && from <= m_dataLength && to > from && to <= m_dataLength && hardLimit <= m_dataLength && to <= hardLimit);

    return { from, to, hardLimit };
}

std::string InputConverterWorker::formOutputPath(const char *path) const
{
    assert(path && strlen(path));

    std::string outPath(path);
    int i = static_cast<int>(outPath.size()) - 1;

    while (i >= 0) {
        if (outPath[i] == '.' || outPath[i] == '\\')
            break;
        i--;
    }

    if (i < 0 || outPath[i] == '\\')
        i = outPath.size();

    std::string suffix = "-";

    auto indexStr = std::to_string(m_index);
    if (indexStr.size() < 2)
        suffix += '0';

    suffix += indexStr;
    outPath.insert(i, suffix);

    return outPath;
}
