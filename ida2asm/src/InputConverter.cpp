#include "InputConverter.h"
#include "InputConverterWorker.h"
#include "Tokenizer.h"
#include "IdaAsmParser.h"
#include "Util.h"

constexpr int kDefinesCapacity = 45'000;

static size_t findSymbolDefinitionLine(const char *data, size_t dataLength, const String& symbol)
{
    auto end = data + dataLength;
    auto lineStart = data;
    size_t lineNo = 1;

    if (dataLength >= 3 && !memcmp(data, "\xef\xbb\xbf", 3))
        lineStart += 3;

    while (lineStart < end) {
        auto lineEnd = static_cast<const char *>(memchr(lineStart, '\n', end - lineStart));
        if (!lineEnd)
            lineEnd = end;

        auto token = lineStart;
        while (token < lineEnd && (*token == ' ' || *token == '\t'))
            token++;

        auto remaining = static_cast<size_t>(lineEnd - token);
        if (remaining >= symbol.length() && !memcmp(token, symbol.data(), symbol.length())) {
            auto next = token + symbol.length();
            if (next == lineEnd || Util::isSpace(*next) || *next == ':' || *next == '=')
                return lineNo;
        }

        lineStart = lineEnd + (lineEnd < end);
        lineNo++;
    }

    return 0;
}

static std::vector<std::string> getSourceItemNames(const char *data, size_t dataLength)
{
    std::vector<std::string> result;
    bool inProc = false;
    auto end = data + dataLength;
    auto lineStart = data;

    const auto isIgnoredToken = [](const std::string& token) {
        static const char *ignored[] = {
            "align", "assume", "db", "dd", "df", "dq", "dt", "dw", "end", "even", "extrn",
            "include", "option", "org", "public", "title",
        };
        return std::find(std::begin(ignored), std::end(ignored), token) != std::end(ignored);
    };

    while (lineStart < end) {
        auto lineEnd = static_cast<const char *>(memchr(lineStart, '\n', end - lineStart));
        if (!lineEnd)
            lineEnd = end;

        auto token = lineStart;
        while (token < lineEnd && (*token == ' ' || *token == '\t'))
            token++;
        const bool startsAtColumnZero = token == lineStart;

        if (token < lineEnd && *token != ';' && *token != '.') {
            auto firstEnd = token;
            while (firstEnd < lineEnd && !Util::isSpace(*firstEnd) && *firstEnd != ':' && *firstEnd != '=')
                firstEnd++;
            std::string first(token, firstEnd);

            auto second = firstEnd;
            while (second < lineEnd && Util::isSpace(*second))
                second++;
            auto secondEnd = second;
            while (secondEnd < lineEnd && !Util::isSpace(*secondEnd) && *secondEnd != ';')
                secondEnd++;
            std::string secondToken(second, secondEnd);

            if (secondToken == "proc") {
                result.push_back(std::move(first));
                inProc = true;
            } else if (secondToken == "endp") {
                inProc = false;
            } else if (!inProc && startsAtColumnZero && !first.empty() && !isIgnoredToken(first)) {
                const bool label = firstEnd < lineEnd && *firstEnd == ':';
                const bool data = !secondToken.empty() && secondToken != "=" && secondToken != "equ" &&
                    secondToken != "ends" && secondToken != "macro" && secondToken != "segment" &&
                    secondToken != "struc";
                if (label || data)
                    result.push_back(std::move(first));
            }
        }

        lineStart = lineEnd + (lineEnd < end);
    }

    return result;
}

InputConverter::InputConverter(const char *inputPath, const char *outputPath, const char *swosHeaderFile,
    OutputFormatResolver::OutputFormat format, int numFiles, int extraMemorySize, bool disableOptimizations,
    bool disableAlignmentChecks, const char *unreferencedReportPath, SymbolFileParser& symFileParser)
:
    m_inputPath(inputPath), m_outputPath(outputPath), m_headerPath(swosHeaderFile), m_format(format), m_numFiles(numFiles),
    m_extraMemorySize(extraMemorySize), m_disableOptimizations(disableOptimizations), m_disableAlignmentChecks(disableAlignmentChecks),
    m_unreferencedReportPath(unreferencedReportPath), m_defines(kDefinesCapacity), m_symFileParser(symFileParser), m_dataBank(symFileParser)
{
    loadFile(inputPath);
}

void InputConverter::convert()
{
    auto [codeStart, commonPrefix] = findCodeDataStart();

    int commonPartLength = codeStart - m_data.get();
    int remainingLength = m_dataLength - commonPartLength;

    int blockSize = remainingLength / m_numFiles;

    parse(commonPartLength, blockSize);

    checkForParsingErrors();
    auto workersToOutput = connectRanges();
    resolveExterns(workersToOutput);
    consolidateVariables();
    if (m_unreferencedReportPath)
        reportUnreferencedItems(workersToOutput);
    output(commonPrefix, workersToOutput);
    checkForUnusedSymbols();
}

void InputConverter::loadFile(const char *path)
{
    // make sure new line is guaranteed to end the file, so the tokenizer can be a bit simpler
    auto result = Util::loadFile(path, true);
    m_data.reset(result.first);
    m_dataLength = result.second;

    if (!m_dataLength)
        Util::exit("Could not process file %s, it seems to be empty", EXIT_FAILURE, path);
}

std::pair<const char *, String> InputConverter::findCodeDataStart() const
{
    // skip structs, defines and intro comments
    auto start = strstr(m_data.get(), ".586");
    assert(start);
    auto p = start;

    while (*p != ';')
        p++;

    auto end = p - 1;
    while (Util::isSpace(*end))
        end--;

    while (*end != '\n')
        end++;

    String commonPrefix(start, end - start + 1);

    return { p, commonPrefix };
}

const char *InputConverter::skipBom(int& length)
{
    auto data = m_data.get();

    if (length >= 3 && !memcmp(data, "\xef\xbb\xbf", 3)) {
        data += 3;
        length -= 3;
    }

    return data;
}

size_t InputConverter::parseCommonPart(int length)
{
    auto data = skipBom(length);

    Tokenizer tokenizer;
    tokenizer.tokenize(data, length);

    auto symbolTableCopy = std::make_unique<SymbolTable>(m_symFileParser.symbolTable());
    IdaAsmParser parser(m_symFileParser, *symbolTableCopy, tokenizer, m_structs, m_defines);
    parser.parse();
    m_structs.seal();

    if (!parser.ok())
        error(parser.errorDescription(), parser.errorLine());

    return parser.lineCount();
}

void InputConverter::parse(int commonPartLength, int blockSize)
{
    for (int i = 0; i < m_numFiles; i++) {
        m_symbolTables.push_back(new SymbolTable(m_symFileParser.symbolTable()));
        int chunkOffset = commonPartLength - Util::kNewLine.size() + i * blockSize;

        m_workers.push_back(new InputConverterWorker(i + 1, m_data.get(), m_dataLength,
            chunkOffset, blockSize, m_symFileParser, *m_symbolTables.back()));

        if (m_format == OutputFormatResolver::kCpp)
            m_workers.back()->setDataBank(&m_dataBank);

        auto future = std::async(std::launch::async, &InputConverterWorker::process, m_workers.back());
        m_futures.push_back(std::move(future));
    }

    // utilize main thread too, this should finish first
    parseCommonPart(commonPartLength);

    // workers should still be busy, so we basically we get this for free :)
    m_symFileParser.outputHeaderFile(m_headerPath);

    waitForWorkers();

}

void InputConverter::checkForParsingErrors()
{
    for (auto worker : m_workers) {
        const auto& parser = worker->parser();

        if (!parser.ok())
            error(parser.errorDescription(), worker->errorLine());
    }
}

std::vector<int> InputConverter::connectRanges()
{
    std::vector<int> activeChunks(m_numFiles, false);
    String missingSymbol;

    for (int i = 0; i < m_numFiles; i++) {
        const auto& parser = m_workers[i]->parser();

        if (missingSymbol.empty()) {
            if (!parser.foundEndRangeSymbol().empty())
                Util::exit("Missing starting range symbol for end range symbol `%s'", 1, parser.foundEndRangeSymbol().c_str());

            activeChunks[i] = true;
            missingSymbol = parser.missingEndRangeSymbol();
        } else if (missingSymbol == SymbolFileParser::kEndMarker) {
            // An @end range consumes every chunk following the one containing its start.
            continue;
        } else {
            if (missingSymbol == parser.foundEndRangeSymbol()) {
                missingSymbol.clear();
                activeChunks[i] = true;
            } else {
                const auto& unmatchedEndRangeSymbol = parser.foundEndRangeSymbol();
                if (!unmatchedEndRangeSymbol.empty())
                    Util::exit("Encountered unmatched end range symbol `%s' while already looking for end range symbol `%s'", 1,
                        unmatchedEndRangeSymbol.string().c_str(), missingSymbol.string().c_str());
            }
        }
    }

    if (!missingSymbol.empty() && missingSymbol != SymbolFileParser::kEndMarker)
        Util::exit("End range symbol `%s' is missing", EXIT_FAILURE, missingSymbol.string().c_str());

    return activeChunks;
}

void InputConverter::resolveExterns(const AllowedChunkList& allowedChunks)
{
    collectSegments();

    auto constWorkers = std::vector<const InputConverterWorker *>{ m_workers.begin(), m_workers.end() };

    for (size_t i = 0; i < m_futures.size(); i++) {
        if (allowedChunks[i]) {
            auto future = std::async(std::launch::async, &InputConverterWorker::resolveReferences, m_workers[i],
                std::ref(constWorkers), std::ref(m_segments), std::ref(m_structs), std::ref(m_defines));
            m_futures[i] = std::move(future);
        }
    }

    waitForWorkers();
}

void InputConverter::collectSegments()
{
    for (auto worker : m_workers) {
        for (const auto& segmentRange : worker->parser().segments()) {
            assert(segmentRange.first->isId() && segmentRange.first->textLength);
            m_segments.add(segmentRange);
        }
    }
}

void InputConverter::consolidateVariables()
{
    if (m_format == OutputFormatResolver::kCpp) {
        for (auto worker : m_workers)
            m_dataBank.addVariables(std::move(worker->variables()));

        m_dataBank.consolidateVariables(m_structs);
    }
}

void InputConverter::output(const String& commonPrefix, const AllowedChunkList& activeChunks)
{
    assert(!m_segments.empty());

    // put 1st chunk externs in the same segment to avoid implicit far jump/call error
    CToken *openSegment = m_workers[0]->parser().firstSegment();
    CToken *externDefSegment{};
    std::string prefix = commonPrefix.string() + Util::kNewLineString();

    for (size_t i = 0; i < m_futures.size(); i++) {
        if (!activeChunks[i])
            continue;

        CToken *lastSegment{};
        if (i > 0) {
            lastSegment = m_workers[i - 1]->parser().currentSegment();

            if (lastSegment)
                openSegment = lastSegment;

            externDefSegment = nullptr;
        } else {
            externDefSegment = openSegment;
        }

        m_workers[i]->setCImportSymbols(m_symFileParser.imports());
        m_workers[i]->setCExportSymbols(m_symFileParser.exports());

        auto future = std::async(std::launch::async, &InputConverterWorker::output, m_workers[i], m_format, m_outputPath,
            m_extraMemorySize, m_disableOptimizations, m_disableAlignmentChecks, std::ref(m_structs), std::ref(m_defines),
            std::cref(prefix), std::make_pair(openSegment, i == 0));
        m_futures[i] = std::move(future);
    }

    outputStructsAndDefines();

    waitForWorkers();

    checkForOutputErrors();
}

void InputConverter::checkForOutputErrors()
{
    bool failed = false;

    for (auto worker : m_workers) {
        if (!worker->outputOk()) {
            std::cout << "Error in output file " << worker->filename() << ": " << worker->getOutputError() << '\n';
            failed = true;
        }
    }

    assert(m_numFiles == m_workers.size() && m_numFiles == m_futures.size());

    for (int i = 0; i < m_numFiles - 1; i++) {
        const auto& limitsError = m_workers[i]->limitsError();
        if (!limitsError.empty()) {
            std::cout << "Check limits of file " << m_workers[i]->filename() <<
                "! Possible loss of data at beginning and/or end: " << limitsError << '\n';
            failed = true;
        }
    }

    for (size_t i = 1; i < m_workers.size(); i++) {
        auto [prevNoBreakContinued, prevNoBreakOverflow] = m_workers[i - 1]->noBreakTagState();
        auto [noBreakContinued, noBreakOverflow] = m_workers[i]->noBreakTagState();

        if (!prevNoBreakOverflow && noBreakContinued) {
            std::cout << "Unexpected closing no break tag found in " << m_workers[i]->filename() << "!\n";
            failed = true;
        }
    }

    if (failed)
        std::exit(EXIT_FAILURE);
}

void InputConverter::checkForUnusedSymbols()
{
    auto exitIfUndefinedSymbols = [this](const char *lead, const std::vector<String>& symbols) {
        if (!symbols.empty()) {
            std::unordered_set<String> uniqueSymbols;
            for (const auto& str : symbols)
                uniqueSymbols.insert(str);

            std::string error = lead;
            auto removalRanges = m_symFileParser.symbolTable().removalRanges();

            for (const auto& str : uniqueSymbols) {
                error += "\n  " + str.string();

                auto definitionLine = findSymbolDefinitionLine(m_data.get(), m_dataLength, str);
                if (definitionLine) {
                    error += " -- defined in " + std::string(m_inputPath) + ":" + std::to_string(definitionLine);

                    bool removalRangeFound = false;
                    for (const auto& range : removalRanges) {
                        auto rangeStartLine = findSymbolDefinitionLine(m_data.get(), m_dataLength, range.first);
                        auto rangeEndLine = range.second == SymbolFileParser::kEndMarker ? m_dataLength :
                            findSymbolDefinitionLine(m_data.get(), m_dataLength, range.second);

                        if (rangeStartLine && rangeStartLine <= definitionLine && definitionLine < rangeEndLine) {
                            error += ", removed by @remove entry `" + range.first.string();
                            if (!range.second.empty())
                                error += " - " + range.second.string();
                            error += "'";
                            removalRangeFound = true;
                            break;
                        }
                    }

                    if (!removalRangeFound)
                        error += ", but not present after parsing (possibly removed or skipped)";
                }
            }

            Util::exit("%s", EXIT_FAILURE, error.c_str());
        }
    };

    assert(m_symbolTables.size() == m_workers.size() && m_symbolTables[0]);

    for (size_t i = 1; i < m_symbolTables.size(); i++)
        m_symbolTables.front()->mergeSymbols(*m_symbolTables[i]);

    auto possiblyUnusedSymbols = m_symbolTables.front()->unusedSymbolsForRemoval();
    for (const auto& exp : m_symFileParser.exports())
        possiblyUnusedSymbols.push_back(exp);

    if (!possiblyUnusedSymbols.empty()) {
        decltype(possiblyUnusedSymbols) unusedSymbols;

        for (const auto& sym : possiblyUnusedSymbols) {
            auto it = std::find_if(m_workers.begin(), m_workers.end(), [&sym](const auto worker) {
                const auto& refs = worker->parser().references();
                return refs.hasReference(sym) || refs.hasPublic(sym);
            });

            if (it == m_workers.end() && !m_symFileParser.isImport(sym))
                unusedSymbols.push_back(sym);
        }

        exitIfUndefinedSymbols("Unknown symbol(s) found: ", unusedSymbols);
    }
}

void InputConverter::reportUnreferencedItems(const AllowedChunkList& activeChunks) const
{
    struct Node {
        std::string name;
        bool procedure;
        std::unordered_set<std::string> references;
        size_t line = 0;
        size_t sourceIndex = SIZE_MAX;
    };

    std::vector<Node> nodes;
    std::unordered_map<std::string, size_t> nodeByName;
    std::unordered_map<std::string, std::string> definitionOwner;

    auto addNode = [&](const String& name, bool procedure) -> std::string {
        auto str = name.string();
        auto [it, inserted] = nodeByName.emplace(str, nodes.size());
        if (inserted)
            nodes.push_back({ str, procedure });
        definitionOwner[str] = str;
        return str;
    };

    for (size_t workerIndex = 0; workerIndex < m_workers.size(); workerIndex++) {
        if (!activeChunks[workerIndex])
            continue;

        std::string currentProc;
        std::vector<std::string> pendingLabels;
        for (const auto& item : m_workers[workerIndex]->parser().outputItems()) {
            switch (item.type()) {
            case OutputItem::kProc:
                currentProc = addNode(item.getItem<Proc>()->name(), true);
                for (const auto& label : pendingLabels)
                    definitionOwner[label] = currentProc;
                pendingLabels.clear();
                break;

            case OutputItem::kEndProc:
                currentProc.clear();
                break;

            case OutputItem::kLabel:
                {
                    auto label = item.getItem<Label>()->name().string();
                    if (!currentProc.empty())
                        definitionOwner[label] = currentProc;
                    else
                        pendingLabels.push_back(std::move(label));
                }
                break;

            case OutputItem::kDataItem:
                if (currentProc.empty()) {
                    auto data = item.getItem<DataItem>();
                    if (!data->name().empty()) {
                        if (data->size()) {
                            auto owner = addNode(data->name(), false);
                            for (const auto& label : pendingLabels)
                                definitionOwner[label] = owner;
                            pendingLabels.clear();
                        } else {
                            pendingLabels.push_back(data->name().string());
                        }
                    }
                }
                break;
            }
        }
    }

    auto normalizedSymbol = [](const String& symbol) {
        auto result = symbol.string();
        while (!result.empty() && (result.front() == '(' || result.front() == '['))
            result.erase(result.begin());
        auto separator = result.find_first_of(".+-)]");
        if (separator != std::string::npos)
            result.resize(separator);
        return result;
    };

    std::unordered_set<std::string> roots;
    auto addReference = [&](const std::string& owner, const String& symbol) {
        auto targetName = normalizedSymbol(symbol);
        auto target = definitionOwner.find(targetName);
        if (target == definitionOwner.end())
            return;

        if (owner.empty()) {
            roots.insert(target->second);
        } else {
            auto ownerNode = nodeByName.find(owner);
            assert(ownerNode != nodeByName.end());
            if (target->second != owner)
                nodes[ownerNode->second].references.insert(target->second);
        }
    };

    for (size_t workerIndex = 0; workerIndex < m_workers.size(); workerIndex++) {
        if (!activeChunks[workerIndex])
            continue;

        std::string currentProc;
        std::string currentData;
        for (const auto& item : m_workers[workerIndex]->parser().outputItems()) {
            switch (item.type()) {
            case OutputItem::kProc:
                currentProc = item.getItem<Proc>()->name().string();
                currentData.clear();
                break;

            case OutputItem::kEndProc:
                currentProc.clear();
                break;

            case OutputItem::kDataItem:
                if (currentProc.empty()) {
                    auto data = item.getItem<DataItem>();
                    if (!data->name().empty() && data->size())
                        currentData = data->name().string();

                    if (!currentData.empty()) {
                        auto element = data->initialElement();
                        for (size_t i = 0; i < data->numElements(); i++, element = element->next())
                            if ((element->type() & DataItem::kTypeMask) == DataItem::kLabel)
                                addReference(currentData, element->text());
                    }
                }
                break;

            case OutputItem::kInstruction:
                {
                    auto instruction = item.getItem<Instruction>();
                    for (const auto& operand : instruction->operands())
                        for (const auto& token : operand)
                            if (token.type() == Token::T_ID && !token.isRegister() && !token.isNumber())
                                addReference(currentProc, token.text());
                }
                break;
            }
        }
    }

    for (const auto& symbol : m_symFileParser.exports()) {
        auto owner = definitionOwner.find(symbol.string());
        if (owner != definitionOwner.end())
            roots.insert(owner->second);
    }

    std::unordered_set<std::string> reachable;
    std::vector<std::string> pending(roots.begin(), roots.end());
    while (!pending.empty()) {
        auto name = std::move(pending.back());
        pending.pop_back();
        if (!reachable.insert(name).second)
            continue;

        auto node = nodeByName.find(name);
        if (node != nodeByName.end())
            for (const auto& reference : nodes[node->second].references)
                pending.push_back(reference);
    }

    for (auto& node : nodes)
        node.line = findSymbolDefinitionLine(m_data.get(), m_dataLength,
            String(node.name.data(), node.name.length()));

    const auto sourceItems = getSourceItemNames(m_data.get(), m_dataLength);
    std::unordered_map<std::string, size_t> sourceItemIndices;
    for (size_t i = 0; i < sourceItems.size(); i++)
        sourceItemIndices.emplace(sourceItems[i], i);
    for (auto& node : nodes) {
        auto sourceItem = sourceItemIndices.find(node.name);
        if (sourceItem != sourceItemIndices.end())
            node.sourceIndex = sourceItem->second;
    }

    std::sort(nodes.begin(), nodes.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.line < rhs.line;
    });

    std::string report = "Unreferenced VM items:\n";
    size_t count = 0;
    for (size_t i = 0; i < nodes.size();) {
        if (reachable.count(nodes[i].name)) {
            i++;
            continue;
        }

        size_t end = i + 1;
        while (end < nodes.size() && !reachable.count(nodes[end].name) &&
            nodes[end].procedure == nodes[i].procedure && nodes[end - 1].sourceIndex != SIZE_MAX &&
            nodes[end].sourceIndex == nodes[end - 1].sourceIndex + 1)
            end++;

        const bool region = end - i > 1;
        if (region) {
            const auto nextSourceIndex = nodes[end - 1].sourceIndex + 1;
            const auto exclusiveEnd = nextSourceIndex < sourceItems.size() ? sourceItems[nextSourceIndex] : "@end";
            report += "  " + std::string(nodes[i].procedure ? "procedure" : "data") + " region: " +
                nodes[i].name + " - " + exclusiveEnd + " (end exclusive)\n";
        }

        for (; i < end; i++) {
            const auto& node = nodes[i];
            report += region ? "    " : "  ";
            report += std::string(node.procedure ? "procedure " : "data      ") + node.name;
            if (node.line)
                report += " -- " + std::string(m_inputPath) + ':' + std::to_string(node.line);
            report += '\n';
            count++;
        }
    }

    if (!count)
        report = "No unreferenced VM items found.\n";

    if (!*m_unreferencedReportPath) {
        std::cout << report;
    } else {
        auto file = fopen(m_unreferencedReportPath, "wb");
        if (!file)
            Util::exit("Unable to open unreferenced-items report file: %s", EXIT_FAILURE, m_unreferencedReportPath);
        fwrite(report.data(), 1, report.size(), file);
        fclose(file);
    }
}

void InputConverter::outputStructsAndDefines()
{
    m_outputWriter = OutputFactory::create(m_format, m_outputPath, m_workers.size() + 1, m_extraMemorySize,
        m_disableOptimizations, m_disableAlignmentChecks, m_symFileParser, m_structs, m_defines, References(),
        OutputItemStream(), m_dataBank);

    if (!m_outputWriter->output(OutputWriter::kStructs | OutputWriter::kDefines))
        Util::exit("Error writing output file!\n%s", EXIT_FAILURE,
            m_outputWriter->getOutputError().c_str());
}

void InputConverter::waitForWorkers()
{
    for (auto& future : m_futures)
        future.wait();
}

void InputConverter::error(const std::string& desc, size_t lineNo)
{
    Util::exit("%s(%d): %s.", EXIT_FAILURE, m_inputPath, lineNo, desc.c_str());
}
