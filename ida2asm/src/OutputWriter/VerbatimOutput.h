#pragma once

#include "OutputWriter.h"
#include "Struct.h"
#include "DefinesMap.h"
#include "OutputItem/OutputItem.h"
#include "OutputItem/Instruction.h"
#include "OutputItem/DataItem.h"
#include "OutputItem/Proc.h"
#include "OutputItem/Label.h"
#include "OutputItem/Segment.h"
#include "OutputItem/StackVariable.h"
#include "OutputItem/Directive.h"
#include "Util.h"

class VerbatimOutput : public OutputWriter
{
public:
    VerbatimOutput(const char *path, const StructStream& structs, const DefinesMap& defines, const OutputItemStream& outputItems);
    void setOutputPrefix(const std::string& prefix) override;
    void setDisassemblyPrefix(const std::string& prefix) override;
    bool output(OutputFlags flags, CToken *openingSegment = nullptr) override;
    std::string segmentDirective(const TokenRange& range) const override;
    std::string endSegmentDirective(const TokenRange& range) const override;

private:
    void outputStructs();
    void outputStruct(const Struct *struc);

    void outputDefines();
    void outputDisassembly();

    int outputInstruction(const Instruction *instruction);
    int outputDataItem(const DataItem *item);
    int outputProc(const Proc *proc);
    int outputEndProc(const EndProc *endProc);
    int outputLabel(const Label *label);
    int outputStackVariable(const StackVariable *var);
    int outputDirective(const Directive *directive);
    int outputSegment(const Segment *segment);

    static const char *dataSizeSpecifier(size_t size);
    static bool needsSpaceDelimiter(const Instruction::Operand *op, const Instruction::Operand *end);
    static void ensureNewLineEnd(std::string& str);

    const StructStream& m_structs;
    const DefinesMap& m_defines;
    const OutputItemStream& m_outputItems;

    const Proc *m_currentProc = nullptr;

    std::string m_outputPrefix;
    std::string m_disassemblyPrefix;
};
