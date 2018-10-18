#pragma once

#include "OutputWriter.h"
#include "SymbolFileParser.h"
#include "Struct.h"
#include "DefinesMap.h"
#include "References.h"
#include "OutputItem/OutputItem.h"
#include "OutputItem/Instruction.h"
#include "OutputItem/DataItem.h"
#include "OutputItem/Proc.h"
#include "OutputItem/Label.h"
#include "OutputItem/Segment.h"
#include "OutputItem/StackVariable.h"
#include "OutputItem/Directive.h"
#include "Util.h"

class MasmOutput : public OutputWriter
{
public:
    MasmOutput(const char *path, const SymbolFileParser& symFilePareser, const StructStream& structs, const DefinesMap& defines,
        const References& references, const OutputItemStream& outputItems);
    void setOutputPrefix(const std::string& prefix) override;
    void setCImportSymbols(const StringList *syms) override;
    void setCExportSymbols(const StringList *syms) override;
    void setDisassemblyPrefix(const std::string& prefix) override;
    bool output(OutputFlags flags, CToken *openingSegment = nullptr) override;
    std::string outputError() const override;
    std::string segmentDirective(const TokenRange& range) const override;
    std::string endSegmentDirective(const TokenRange& range) const override;

private:
    bool outputExterns();
    void outputPublics();

    void outputStructs();
    void outputStruct(Struct *struc);

    void outputCExternDefs();

    void outputDefines();
    void outputDisassembly();

    int outputInstruction(const Instruction *instruction);
    int outputDataItem(const DataItem *item);
    int outputProc(const Proc *proc);
    int outputEndProc(const EndProc *endProc, const String& comment, const OutputItem *next);
    int outputLabel(const Label *label);
    int outputStackVariable(const StackVariable *var);
    int outputDirective(const Directive *directive);
    int outputSegment(const Segment *segment);

    bool outputFixedStructAcces(const String& str, int& column, int opSize = -1);

    static const char *dataSizeSpecifier(size_t size);
    static bool needsSpaceDelimiter(const Instruction::Operand *op, const Instruction::Operand *end);
    static void ensureNewLineEnd(std::string& str);

    const SymbolFileParser& m_symFileParser;
    const StructStream& m_structs;
    const DefinesMap& m_defines;
    const References& m_references;
    const OutputItemStream& m_outputItems;

    const Proc *m_currentProc = nullptr;
    String m_previousSegment;
    String m_currentSegment;

    std::string m_outputPrefix;
    const StringList *m_cImportSymbols = nullptr;
    const StringList *m_cExportSymbols = nullptr;
    std::string m_disassemblyPrefix;

    std::string m_error;
};
