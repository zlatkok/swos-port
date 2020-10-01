#include "DataBank.h"
#include "OutputItem/OutputItem.h"
#include "OutputItem/DataItem.h"
#include "AmigaRegs.h"
#include "Struct.h"
#include "SymbolFileParser.h"
#include "DefinesMap.h"
#include "OutputException.h"

constexpr int kStructVarsCapacity = 5'000;
constexpr int kGlobalOffsetMapCapacity = 368'000;

DataBank::DataBank(const SymbolFileParser& symFileParser)
:
    m_symFileParser(symFileParser), m_globalStructVarMap(kStructVarsCapacity),
    m_globalOffsetMap(kGlobalOffsetMapCapacity), m_procToIndex(kGlobalOffsetMapCapacity / 12), m_varToAddress(0)
{
}

auto DataBank::processRegion(const OutputItemStream& items, const StructStream& structs, const DefinesMap& defines) -> VarData
{
    VarList varList;
    StructVarsMap structVars(kStructVarsCapacity);
    OffsetMap offsetReferences(kGlobalOffsetMapCapacity);

    const auto& firstMembersMap = getFirstMembersStructMap(structs);

    for (const auto& item : items) {
        if (item.type() == OutputItem::kDataItem) {
            auto dataItem = item.getItem<DataItem>();

            addVariable(item, dataItem, structs, defines, varList);
            addPotentialStructVar(dataItem, item.comment(), firstMembersMap, structVars);
        } else if (item.type() == OutputItem::kInstruction) {
            auto instruction = item.getItem<Instruction>();
            addPotentialFunctionPointer(instruction, offsetReferences);
        }
    }

    // add offset references only once the vector is stable
    addPotentialFunctionPointers(varList, offsetReferences);
    offsetReferences.seal();

    structVars.seal();
    assert(!structVars.hasDuplicates());

    return { std::move(varList), std::move(structVars), std::move(offsetReferences) };
}

void DataBank::addVariables(VarData&& vars)
{
    auto [ varsList, structVarsMap, offsetVarsMap ] = std::move(vars);
    m_vars.push_back(std::move(varsList));
    addStructVariables(structVarsMap);
    addOffsetVariables(offsetVarsMap);
}

// Constructs a single structure to hold all variables, and calculates their offsets.
void DataBank::consolidateVariables(const StructStream& structs)
{
    auto numVariables = std::accumulate(m_vars.begin(), m_vars.end(), size_t(0), [](size_t sum, const auto& varList) {
        return sum + varList.size();
    });
    auto varMapSizeEstimate = numVariables * 40;

    m_varToAddress.reset(varMapSizeEstimate);

    m_memoryByteSize = zeroRegionSize();

    for (auto& varList : m_vars) {
        for (auto& var : varList) {
            if (amigaRegisterToIndex(var.name) < 0) {
                if (var.name) {
                    m_memoryByteSize += extractVarAlignment(var, m_memoryByteSize, structs);
                    var.offset = m_memoryByteSize;
                    m_varToAddress.add(var.name, m_memoryByteSize);

                    assert(!var.exportedDecl || var.type == kString || var.offset % (var.type == kStruct ? 4 : std::min<int>(var.size, 4)) == 0);
                }

                m_memoryByteSize += var.dup * var.size;
            }
        }
    }

    m_varToAddress.seal();

    m_globalStructVarMap.seal();
    assert(!m_globalStructVarMap.hasDuplicates());

    consolidateOffsetVariables();
}

// this area will be used to catch null pointer access (keep it pointer-size aligned)
size_t DataBank::zeroRegionSize()
{
    return 256;
}

size_t DataBank::memoryByteSize() const
{
    return m_memoryByteSize;
}

size_t DataBank::numProcPointers() const
{
    return m_numProcPointers;
}

size_t DataBank::getVarOffset(const String& varName) const
{
    auto addr = m_varToAddress.get(varName);
    if (!addr)
        throw OutputException("variable `" + varName.string() + "' missing, check for invalid ranges");

    return addr->offset;
}

bool DataBank::isVariable(const String& name) const
{
    return m_varToAddress.get(name) != nullptr;
}

int DataBank::getProcOffset(const String& varName) const
{
    auto varInfo = m_procToIndex.get(varName);

    if (!varInfo)
        return -1;

    assert(!varInfo->var || (varInfo->var->type == kInt && varInfo->var->intValue == varInfo->index));

    return varInfo->index;
}

const PascalString *DataBank::structNameFromVar(const String& varName) const
{
    return m_globalStructVarMap.get(varName);
}

void DataBank::traverseVars(std::function<bool(const Var& var, const Var *next)> f) const
{
    for (const auto& varList : m_vars) {
        if (!varList.empty()) {
            for (size_t i = 0; i < varList.size() - 1; i++)
                if (f(varList[i], &varList[i + 1]))
                    i++;

            f(varList.back(), nullptr);
        }
    }
}

void DataBank::traverseVars(std::function<void(const Var& var)> f) const
{
    for (const auto& varList : m_vars) {
        if (!varList.empty()) {
            for (const auto& var : varList)
                f(var);
        }
    }
}

void DataBank::traverseProcs(std::function<void(const String& procName, bool imported, int index)> f) const
{
    for (const auto& procVar : m_procToIndex)
        f(procVar.text, procVar.cargo->imported, procVar.cargo->index);
}

void DataBank::addVariable(const OutputItem& item, const DataItem *dataItem, const StructStream& structs,
    const DefinesMap& defines, VarList& varList)
{
    assert(dataItem);

    auto element = dataItem->initialElement();

    for (size_t i = 0; i < dataItem->numElements(); i++, element = element->next()) {
        Var var;

        if (i == 0) {
            var.name = dataItem->name();
            if (amigaRegisterToIndex(var.name) >= 0)
                continue;

            if (!dataItem->structName().empty()) {
                var.type = kStruct;
                var.size = structs.getStructSize(dataItem->structName());
                var.structName = dataItem->structName();
            }

            var.leadingComment = item.leadingComments();
            var.comment = item.comment();
        }

        switch (element->type()) {
            case DataItem::kLabel:
                if (element->isOffset()) {
                    assert(dataItem->size() == 4);
                    handleOffsetValue(var, element);
                } else {
                    if (element->text() == "<0>")
                        break;
                    var.type = kInt;
                    var.size = dataItem->size();
                    auto def = defines.get(element->text());
                    var.intValue = def->intValue();
                }
                break;

            case DataItem::kHex:
            case DataItem::kDec:
            case DataItem::kBin:
                var.type = kInt;
                var.size = dataItem->size();
                var.intValue = element->value();
                break;

            case DataItem::kString:
                var.type = kString;
                var.size = element->text().length() - 2;
                var.stringValue = element->text();
                break;

            case DataItem::kStructInit:
                // should already be handled
                assert(element->value() == 0);
                break;

            case DataItem::kUninitialized:
                var.type = kInt;
                var.size = dataItem->size();
                var.intValue = 0;
                break;

            default:
                assert(false);
        }

        var.originalValue = element->text();
        if (element->dup())
            var.dup = element->dup();

        varList.push_back(var);
    }
}

void DataBank::addPotentialStructVar(const DataItem *item, const String& comment,
    const StructVarsMap& firstMembersMap, StructVarsMap& structVars)
{
    if (!item->name().empty()) {
        if (!item->structName().empty()) {
            assert(item->numElements() > 0);
            auto element = item->initialElement();
            structVars.add(item->name(), item->structName().data(), item->structName().length());
        } else {
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
                        structVars.add(item->name(), structName->data(), structName->length());
            }
        }
    }
}

void DataBank::handleOffsetValue(Var& var, const DataItem::Element *element)
{
    var.type = kOffset;
    var.size = 4;

    auto dotIndex = element->text().indexOf('.');
    if (dotIndex >= 0) {
        var.offsetVar = element->text().substr(0, dotIndex);
        var.structField = element->text().substr(dotIndex + 1);
    } else {
        var.offsetVar = element->text();
    }

    if (element->isOffset())
        var.additionalOffset = element->offset();
}

void DataBank::addPotentialFunctionPointers(VarList& vars, OffsetMap& offsetReferences)
{
    for (auto& var : vars)
        if (var.type == kOffset)
            offsetReferences.add(var.offsetVar, &var, false);
}

void DataBank::addPotentialFunctionPointer(const Instruction *instruction, OffsetMap& offsetReferences)
{
    if (instruction->numOperands() == 2) {
        const auto& operands = instruction->operands();
        const auto& params = operands[1];

        auto op = params.begin();
        if (op != params.end() && op->type() == Token::T_OFFSET) {
            op = op->next();
            assert(op != params.end());
            if (op != params.end() && op->next() == params.end() && op->type() == Token::T_ID && !op->text().contains('.'))
                offsetReferences.add(op->text(), nullptr, false);
        }
    }
}

StringMap<PascalString> DataBank::getFirstMembersStructMap(const StructStream& structs)
{
    constexpr int kFirstMemmbersMapCapacity = 1'600;
    // these introduce ambiguity as multiple structures have them as first field
    static const String kIgnoredFields[] = { "headerSize", "driver", "teamId" };

    StringMap<PascalString> firstMembers(kFirstMemmbersMapCapacity);

    for (const auto& struc : structs) {
        auto firstField = struc.begin();

        auto it = std::find(std::begin(kIgnoredFields), std::end(kIgnoredFields), firstField->name());
        if (it != std::end(kIgnoredFields))
            continue;

        firstMembers.add(firstField->name(), struc.name());
    }

    firstMembers.seal();
    assert(!firstMembers.hasDuplicates());

    return firstMembers;
}

void DataBank::addStructVariables(const StructVarsMap& localStructVars)
{
    for (const auto& node : localStructVars)
        m_globalStructVarMap.add(node.text, node.cargo->data(), node.cargo->length());
}

void DataBank::addOffsetVariables(const OffsetMap& offsetVarsMap)
{
    for (const auto& node : offsetVarsMap)
        m_globalOffsetMap.add(node.text, node.cargo->var, m_symFileParser.isImport(node.text));
}

void DataBank::consolidateOffsetVariables()
{
    filterPotentialProcOffsetList();
    fillExportedProcs();
    fillProcIndicesAndFixupVars();
}

size_t DataBank::extractVarAlignment(Var& var, size_t address, const StructStream& structs)
{
    assert(!var.alignment);

    auto [exportedDecl, baseType, arraySize] = m_symFileParser.exportedDeclaration(var.name);
    if (exportedDecl) {
        var.exportedDecl = exportedDecl;
        var.exportedBaseType = baseType;
        var.exportedArraySize = arraySize;
        var.exportedSize = var.size;

        if (var.type != kString) {
            int alignment = var.type == kStruct ? 4 : var.size;

            // we need the real type to determine alignment
            if (var.exportedBaseType) {
                var.exportedSize = getElementSize(var.exportedBaseType);
                if (var.exportedSize < 0) {
                    if (!structs.findStruct(var.exportedBaseType))
                        Util::exit("Unknown structure encountered: `%.*s', variable: `%.*s'", EXIT_FAILURE,
                            var.exportedBaseType.length(), var.exportedBaseType.data(), var.name.length(), var.name.data());
                    alignment = 4;  // it's a known struct
                } else if (var.size != var.exportedSize) {
                    alignment = var.exportedSize;
                    if (var.exportedArraySize > 1) {
                        var.size = var.exportedSize;
                        var.dup = var.exportedArraySize;
                    } else if (var.dup > 1 && var.exportedSize > static_cast<int>(var.size)) {
                        assert(var.dup % var.exportedSize == 0);
                        var.size = var.exportedSize;
                        var.dup /= var.exportedSize;
                    }
                    alignment = std::min(alignment, 4);
                }
            }

            assert(alignment <= 4);

            if (auto misalignment = address % alignment) {
                Util::assignSize(var.alignment, alignment - misalignment);
                assert(var.alignment < 4 && (address + var.alignment) % alignment == 0);
            }
        }
    }

    return var.alignment;
}

int DataBank::getElementSize(const String& type)
{
    if (type.contains('*'))
        return 4;
    else if (type == "byte" || type == "char" || type == "const char")
        return 1;
    else if (type == "word" || type == "int16_t")
        return 2;
    else if (type == "dword")
        return 4;

    return m_symFileParser.getTypeSize(type);
}

void DataBank::filterPotentialProcOffsetList()
{
    m_globalOffsetMap.seal();

    for (auto& offsetVar : m_globalOffsetMap) {
        bool isData = m_varToAddress.get(offsetVar.text);
        if (!isData)
            m_procs.emplace_back(offsetVar.text, *offsetVar.cargo);
    }
}

void DataBank::fillExportedProcs()
{
    // must encompass exported procs as well, since we don't know how they will be used
    m_symFileParser.traverseExportedProcs([this](const auto& name) {
        OffsetReference ofs(nullptr, false);
        m_procs.emplace_back(name, ofs);
    });
}

void DataBank::fillProcIndicesAndFixupVars()
{
    std::sort(m_procs.begin(), m_procs.end(), [](const auto& proc1, const auto& proc2) {
        return proc1.first < proc2.first;
    });

    String lastVar;
    Util::hash_t lastHash = 0;
    int procIndex = -1;     // start with -2 since -1 is used as nullptr in 68k code

    for (auto& proc : m_procs) {
        const auto& varName = proc.first;
        auto hash = Util::hash(varName.data(), varName.length());

        if (hash != lastHash || varName != lastVar) {
            procIndex--;    // track proc index by negative numbers to make it more distinct
            lastHash = hash;
            lastVar = varName;

            const auto& cargo = proc.second;
            auto destCargo = m_procToIndex.add(varName.data(), varName.length(), hash, cargo.var, cargo.imported);
            destCargo->index = procIndex;
        }

        assert(procIndex < -1);
        proc.second.index = procIndex;

        if (proc.second.var) {
            assert(varName == proc.second.var->offsetVar);
            proc.second.var->type = kInt;
            proc.second.var->intValue = procIndex;
        }
    }

    assert(m_procToIndex.count() == -1 - procIndex);
    m_numProcPointers = m_procToIndex.count();

    m_procToIndex.seal();
}
