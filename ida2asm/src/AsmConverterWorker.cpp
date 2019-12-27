#include "AsmConverterWorker.h"
#include "Struct.h"
#include "DefinesMap.h"
#include "DataItem.h"

constexpr int kFirstStructFieldsCapacity = 786;
constexpr int kStructVarsCapacity = 3'300;

AsmConverterWorker::AsmConverterWorker(int index, const char *data, int dataLength,
    int chunkOffset, int chunkLength, const SymbolFileParser& symFileParser, SymbolTable& symbolTable)
:
    m_index(index), m_data(data), m_dataLength(dataLength), m_offset(chunkOffset), m_chunkLength(chunkLength),
    m_symFileParser(symFileParser), m_defines(0), m_tokenizer(),
    m_parser(symFileParser, symbolTable, m_tokenizer, m_structs, m_defines), m_structVars(kStructVarsCapacity)
{
}

void AsmConverterWorker::process()
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

void AsmConverterWorker::output(const char *format, const char *path, const StructStream& structs,
    const DefinesMap& defines, const std::string& prefix, std::pair<CToken *, bool> openingSegments)
{
    assert(m_segments);

    m_filename = formOutputPath(path);
    m_outputWriter = OutputFactory::create(format, m_filename.c_str(), m_index, m_symFileParser, structs, defines,
        m_parser.references(), m_parser.outputItems());

    auto outputPrefix = segmentsOutput(openingSegments.first);
    m_outputWriter->setOutputPrefix(prefix + outputPrefix);

    m_outputWriter->setCImportSymbols(m_cImportSymbols);
    m_outputWriter->setCExportSymbols(m_cExportSymbols);
    m_outputWriter->setGlobalStructVars(m_globalStructVars);

    if (openingSegments.second) {
        const auto& range = m_segments->segmentRange(openingSegments.first);
        auto disassemblyPrefix = m_outputWriter->endSegmentDirective(range);
        m_outputWriter->setDisassemblyPrefix(disassemblyPrefix);
    }

    m_outputOk = m_outputWriter->output(OutputWriter::kFullDisasembly, openingSegments.first);
}

void AsmConverterWorker::resolveReferences(const std::vector<const AsmConverterWorker *>& workers, const SegmentSet& segments,
    const StructStream& structs, const DefinesMap& defines)
{
    m_segments = &segments;

    m_parser.references().resolveSegments(segments);
    resolveStructsAndDefines(structs, defines);

    for (auto worker : workers)
        if (worker != this)
            m_parser.references().resolve(worker->m_parser.references());

    ignoreContiguousTableExterns(workers);
    gatherStructVars(structs);
}

void AsmConverterWorker::setCImportSymbols(const StringList& syms)
{
    m_cImportSymbols = &syms;
}

void AsmConverterWorker::setCExportSymbols(const StringList& syms)
{
    m_cExportSymbols = &syms;
}

void AsmConverterWorker::setGlobalStructVarsTable(const StringMap<PascalString> *structVars)
{
    m_globalStructVars = structVars;
}

std::pair<bool, bool> AsmConverterWorker::noBreakTagState() const
{
    return { m_noBreakContinued, m_noBreakOverflow };
}

bool AsmConverterWorker::outputOk() const
{
    return m_outputOk;
}

std::string AsmConverterWorker::limitsError() const
{
    return m_limitsError;
}

std::string AsmConverterWorker::getOutputError() const
{
    return m_outputWriter->getOutputError();
}

std::string AsmConverterWorker::filename() const
{
    return m_filename;
}

const IdaAsmParser& AsmConverterWorker::parser() const
{
    return m_parser;
}

const StringMap<PascalString>& AsmConverterWorker::structVars() const
{
    return m_structVars;
}

IdaAsmParser& AsmConverterWorker::parser()
{
    return m_parser;
}

OutputWriter& AsmConverterWorker::outputWriter()
{
    return *m_outputWriter.get();
}

void AsmConverterWorker::resolveLocalReferences()
{
    m_parser.references().resolve();
}

void AsmConverterWorker::resolveImports()
{
    for (const auto& imp : m_symFileParser.imports())
        m_parser.references().markImport(imp);
}

void AsmConverterWorker::resolveStructsAndDefines(const StructStream& structs, const DefinesMap& defines)
{
    for (const auto& struc : structs)
        m_parser.references().setIgnored(struc.name(), struc.hash());

    for (const auto& def : defines)
        m_parser.references().setIgnored(def.text, def.hash);
}

// If creating C files, each file must not generate extern declaration for any variable that's marked as contiguous
// in any other file (since they get converted to struct members and then we get linker error).
void AsmConverterWorker::ignoreContiguousTableExterns(const std::vector<const AsmConverterWorker *>& workers)
{
    if (!m_symFileParser.cOutput())
        return;

    auto ignoreExtern = [this](const DataItem *dataItem) {
        const auto& name = dataItem->name();
        if (!name.empty()) {
            auto hash = Util::hash(name.data(), name.length());
            m_parser.references().setIgnored(name, hash);
        }
    };

    for (auto worker : workers) {
        if (worker == this)
            continue;

        const auto& outputItems = worker->parser().outputItems();
        for (auto item = outputItems.begin(); item != outputItems.end(); item = item->next()) {
            if (item->type() == OutputItem::kDataItem) {
                auto dataItem = item->getItem<DataItem>();
                if (dataItem->contiguous()) {
                    do {
                        ignoreExtern(dataItem);

                        item = item->next();
                        assert(item != outputItems.end() && item->type() == OutputItem::kDataItem);
                        dataItem = item->getItem<DataItem>();
                    } while (!dataItem->contiguous());

                    ignoreExtern(dataItem);
                }
            }
        }
    }
}

void AsmConverterWorker::gatherStructVars(const StructStream& structs)
{
    if (!m_symFileParser.cOutput())
        return;

    const auto& firstMembersMap = getFirstMembersStructMap(structs);

    for (const auto& item : m_parser.outputItems()) {
        if (item.type() != OutputItem::kDataItem)
            continue;

        auto dataItem = item.getItem<DataItem>();
        if (dataItem->name().empty())
            continue;

        const auto& comment = item.comment();
        int semiColonIndex = comment.indexOf(';');

        if (semiColonIndex >= 0 && comment.length() - semiColonIndex > 1) {
            auto p = comment.data() + semiColonIndex + 1;
            auto sentinel = comment.data() + comment.length();

            while (p < sentinel && Util::isSpace(*p))
                p++;

            // handle advertIngameSprites which is an array, and has a "[0].teamNumber" comment
            if (*p == '[') {
                p++;
                while (p < sentinel && Util::isDigit(*p))
                    p++;
                if (p < sentinel && *p == ']')
                    p++;
                if (p < sentinel && *p == '.')
                    p++;
            }

            String trimmedComment(p, comment.length() - (p - comment.data()));

            if (!trimmedComment.empty())
                if (auto structName = firstMembersMap.get(trimmedComment))
                    m_structVars.add(dataItem->name(), structName->data(), structName->length());
        }
    }

    m_structVars.seal();
    assert(!m_structVars.hasDuplicates());
}

StringMap<PascalString> AsmConverterWorker::getFirstMembersStructMap(const StructStream& structs)
{
    constexpr int kFirstMemmbersMapCapacity = 1'200;
    static const String kBlackListedMembers[] = { "headerSize", "driver" };

    StringMap<PascalString> firstMembers(kFirstMemmbersMapCapacity);

    for (const auto& struc : structs) {
        auto firstField = struc.begin();

        auto it = std::find(std::begin(kBlackListedMembers), std::end(kBlackListedMembers), firstField->name());
        if (it != std::end(kBlackListedMembers))
            continue;

        firstMembers.add(firstField->name(), struc.name());
    }

    firstMembers.seal();
    assert(!firstMembers.hasDuplicates());

    return firstMembers;
}

std::string AsmConverterWorker::segmentsOutput(CToken *openSegment)
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

int AsmConverterWorker::getPreviousNonCommentLine(int offset) const
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

int AsmConverterWorker::getPreviousLine(int offset) const
{
    if (offset < 2)
        return offset;

    for (offset -= 2; offset >= 0; offset--)
        if (m_data[offset] == '\n')
            return offset + 1;

    return offset;
}

bool AsmConverterWorker::isLineComment(int offset) const
{
    if (!memcmp(m_data + offset, &Util::kNewLine, Util::kNewLine.size()))
        return true;

    if (m_data[offset])
        while (m_data[offset] != '\n')
            if (m_data[offset++] == ';')
                return true;

    return false;
}

std::tuple<int, int, int> AsmConverterWorker::findLimits() const
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

std::string AsmConverterWorker::formOutputPath(const char *path) const
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
