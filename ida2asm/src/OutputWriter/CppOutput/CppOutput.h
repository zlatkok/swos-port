#pragma once

#include "OutputWriter.h"
#include "StringSet.h"
#include "DataItem.h"
#include "IntermediateFormConverter.h"
#include "DataBank.h"
#include "X86InstructionWriter.h"

class CppOutput : public OutputWriter
{
public:
    CppOutput(const char *path, int index, int extraMemorySize, bool disableOptimizations, const SymbolFileParser& symFileParser,
        const StructStream& structs, const DefinesMap& defines, const References& references, const OutputItemStream& outputItems,
        const DataBank& dataBank);
    void setOutputPrefix(const std::string&) override {}
    void setCImportSymbols(const StringSet *syms) override;
    void setCExportSymbols(const StringList *syms) override;
    void setDisassemblyPrefix(const std::string& prefix) override {}
    bool output(OutputFlags flags, CToken *openingSegment = nullptr) override;
    const char *getDefsFilename() const override;
    std::string segmentDirective(const TokenRange&) const override { return {}; }
    std::string endSegmentDirective(const TokenRange&) const override { return {}; }

private:
    void outputStructs();
    void outputStruct(Struct& struc);

    bool outputExterns();

    void outputDefines();
    void outputCodeAndData(OutputFlags flags);

    void runFirstPass(OutputFlags flags);

    void outputInstruction(const InstructionNode& node);
    void outputProcStart(const InstructionNode& node);
    void outputEndProc(const InstructionNode& node);
    void outputLabel(const InstructionNode& node);
    void outputLabel(const String& label);
    void outputStackVariable(const InstructionNode& node);

    bool isFinalRetn(InstructionsIterator it);
    bool isLastLineEmpty() const;
    void outputOriginalInstructionComment(const Instruction *instruction, const char *start);
    void addCommentPadding(const char *start);
    size_t getLineLength(const char *start);
    void outputComment(const String& comment, bool endWithNewLine = true);

    static constexpr char kStartLabel[] = "_l_start";

    const StringSet *m_cImportSymbols = nullptr;
    const StringList *m_cExportSymbols = nullptr;

    bool m_insideProc = false;
    int m_index;
    int m_extendedMemorySize;

    const DataBank& m_dataBank;
    IntermediateFormConverter m_irConverter;
    StringSet m_inProcLabels;
    X86InstructionWriter m_x86Writer;

    std::vector<std::pair<const OutputItem *, const OutputItem *>> m_tableVarRanges;

    friend class X86InstructionWriter;
    friend class OpWriter;
};
