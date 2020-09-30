#pragma once

#include "PascalString.h"
#include "StringMap.h"
#include "DataItem.h"

class DefinesMap;
class OutputItemStream;
class OutputItem;
class StructStream;
class Instruction;
class SymbolFileParser;

class DataBank
{
public:
    enum VarType : uint8_t {
        kInt, kConstant, kString, kOffset, kStruct,
    };

    struct Var
    {
        Var() {}
        String name;
        VarType type;
        uint8_t alignment = 0;
        unsigned size = 0;
        unsigned dup = 1;
        unsigned offset = 0;
        String exportedDecl;
        String exportedBaseType;
        int exportedArraySize;
        int exportedSize;
        union {
            int intValue;
            String stringValue;
            String structName;
            String offsetVar;
        };
        unsigned additionalOffset = 0;  // dd offset aTspvhcfstpvhcf + 5Bh
        String structField;             // dd offset advertIngameSprites.teamNumber + 6Eh
        String originalValue;
        String leadingComment;
        String comment;
    };

    struct OffsetReference
    {
        OffsetReference(Var *var, bool imported) : var(var), imported(imported) {}
        OffsetReference(const OffsetReference& other) = default;
        static size_t requiredSize(Var *, bool) { return sizeof(OffsetReference); }
        Var *var;
        int index;
        uint8_t imported = false;
    };
    using OffsetMap = StringMap<OffsetReference>;

    using VarList = std::vector<Var>;
    using StructVarsMap = StringMap<PascalString>;
    using VarData = std::tuple<VarList, StructVarsMap, OffsetMap>;

    DataBank(const SymbolFileParser& symFileParser);

    static VarData processRegion(const OutputItemStream& items, const StructStream& structs, const DefinesMap& defines);

    void addVariables(VarData&& vars);
    void consolidateVariables(const StructStream& structs);

    static size_t zeroRegionSize();
    size_t memoryByteSize() const;
    size_t numProcPointers() const;
    size_t getVarOffset(const String& varName) const;
    bool isVariable(const String& name) const;
    int getProcOffset(const String& varName) const;
    const PascalString *structNameFromVar(const String& varName) const;

    void traverseVars(std::function<bool(const Var& var, const Var *next)> f) const;
    void traverseVars(std::function<void(const Var& var)> f) const;
    void traverseProcs(std::function<void(const String& procName, bool imported, int index)> f) const;

private:
    static void addVariable(const OutputItem& item, const DataItem *dataItem, const StructStream& structs,
        const DefinesMap& defines, VarList& varList);
    static void addPotentialStructVar(const DataItem *item, const String& comment,
        const StructVarsMap& firstMembersMap, StructVarsMap& structVars);
    static void handleOffsetValue(Var& var, const DataItem::Element *element);
    static void addPotentialFunctionPointers(VarList& vars, OffsetMap& offsetReferences);
    static void addPotentialFunctionPointer(const Instruction *instruction, OffsetMap& offsetReferences);
    static StructVarsMap getFirstMembersStructMap(const StructStream& structs);
    void addStructVariables(const StructVarsMap& localStructVars);
    void addOffsetVariables(const OffsetMap& offsetVarsMap);
    void consolidateOffsetVariables();
    size_t extractVarAlignment(Var& var, size_t address, const StructStream& structs);
    int getElementSize(const String& type);
    void filterPotentialProcOffsetList();
    void fillExportedProcs();
    void fillProcIndicesAndFixupVars();

    const SymbolFileParser& m_symFileParser;

    std::vector<VarList> m_vars;
    StructVarsMap m_globalStructVarMap;
    OffsetMap m_globalOffsetMap;
    std::vector<std::pair<String, OffsetReference>> m_procs;
    OffsetMap m_procToIndex;

    size_t m_memoryByteSize;
    size_t m_numProcPointers;

    struct VarHolder {
        VarHolder(size_t offset) : offset(offset) {}
        static size_t requiredSize(size_t) { return sizeof(VarHolder); }
        size_t offset;
    };
    StringMap<VarHolder> m_varToAddress;
};
