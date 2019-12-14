#pragma once

#include "OutputWriter.h"
#include "StringSet.h"
#include "DataItem.h"

class COutput : public OutputWriter
{
public:
    COutput(const char *path, const SymbolFileParser& symFileParser, const StructStream& structs, const DefinesMap& defines,
        const References& references, const OutputItemStream& outputItems);
    void setOutputPrefix(const std::string&) override {}
    void setCImportSymbols(const StringList *syms) override;
    void setCExportSymbols(const StringList *syms) override;
    void setDisassemblyPrefix(const std::string& prefix) override {}
    bool output(OutputFlags flags, CToken *openingSegment = nullptr) override;
    const char *getDefsFilename() const override;
    std::string segmentDirective(const TokenRange&) const override { return {}; }
    std::string endSegmentDirective(const TokenRange&) const override { return {}; }

private:
    bool outputFile(const char *filename, const char *contents, size_t size);
    bool outputVmFiles();

    void outputStructs();
    void outputStruct(Struct& struc);

    bool outputExterns();

    void outputDefines();
    void outputCodeAndData();

    void outputForwardDeclarationsAndTagLabels();

    void outputInstruction(const Instruction *instruction);
    void outputDataItem(const DataItem *item, const OutputItem *nextItem, bool onlyDeclare);
    void outputItem(const DataItem::Element *element, bool zeroTerminate);
    void outputProc(const Proc *proc);
    void outputEndProc(const EndProc *endProc, const String& comment, const OutputItem *next);
    void outputLabel(const Label *label);
    void outputStackVariable(const StackVariable *var);

    void outputMov(const Instruction *instruction);
    void outputMovs(const Instruction *instruction);
    void outputStos(const Instruction *instruction);
    void outputLods(const Instruction *instruction);
    void outputCall(const Instruction *instruction);
    void outputPushPop(const Instruction *instruction);
    void outputCbw(const Instruction *instruction);
    void outputCwde(const Instruction *instruction);
    void outputCdq(const Instruction *instruction);
    void outputOrXor(const Instruction *instruction);
    void outputCmpSubAdd(const Instruction *instruction, bool commitResult, bool add);
    void outputIncDec(const Instruction *instruction, bool increment);
    void outputAndTest(const Instruction *instruction, bool commitResult);
    void outputXchg(const Instruction *instruction);
    void outputMul(const Instruction *instruction);
    void outputImul(const Instruction *instruction);
    void outputDiv(const Instruction *instruction);
    void outputShift(const Instruction *instruction);
    void outputRotate(const Instruction *instruction);
    void outputIntInOut(const Instruction *instruction);
    void outputNeg(const Instruction *instruction);
    void outputNot(const Instruction *instruction);
    void outputLoop(const Instruction *instruction);
    void outputPushf(const Instruction *instruction);
    void outputPopf(const Instruction *instruction);
    void outputPusha(const Instruction *instruction);
    void outputPopa(const Instruction *instruction);
    void outputStc(const Instruction *instruction);
    void outputClc(const Instruction *instruction);
    void outputCld(const Instruction *instruction);
    void outputSetz(const Instruction *instruction);

    void outputConditionalJump(const Instruction *instruction, const char *condition = nullptr);
    void outputJmp(const Instruction *instruction);
    void outputJg(const Instruction *instruction);
    void outputJge(const Instruction *instruction);
    void outputJl(const Instruction *instruction);
    void outputJz(const Instruction *instruction);
    void outputJnz(const Instruction *instruction);
    void outputJs(const Instruction *instruction);
    void outputJns(const Instruction *instruction);
    void outputJb(const Instruction *instruction);
    void outputJa(const Instruction *instruction);
    void outputJnb(const Instruction *instruction);
    void outputJbe(const Instruction *instruction);
    void outputJle(const Instruction *instruction);
    void outputJo(const Instruction *instruction);
    void outputJno(const Instruction *instruction);

    using OutputIterator = Iterator::Iterator<const OutputItem>;
    OutputIterator checkForSwitchTable(OutputIterator it, const Instruction *instruction);
    OutputIterator checkForSwitchTable(OutputIterator it, const DataItem *dataItem);

    void fillStructInfo();
    bool isFinalRetn(const OutputItem& item);
    bool isLastLineEmpty() const;
    void outputOriginalInstructionComment(const Instruction *instruction, const char *start);
    void addCommentPadding(const char *start);
    size_t getLineLength(const char *start);
    void outputComment(const String& comment, bool endWithNewLine = true);
    void outputFunctionInvoke(const String& target);
    size_t getDataItemArraySize(const DataItem *item);

    struct OperandInfo {
        String base;
        String offset;
        String displacement;
        String scale;
        bool address = false;
        bool pointer = false;
        size_t size = 0;
        Token::Type baseType;
        Token::Type offsetType;
        Token::Type displacementType;
    };
    using InstructionOperandsInfo = std::array<OperandInfo, 3>;

    void ensureFunction();
    void outputInstructionArgument(const OperandInfo& op, bool assignAddress = false, size_t defaultSize = 0);
    void outputToken(const String& op, Token::Type type, bool isInstruction = true);
    void outputLabel(const String& label);
    InstructionOperandsInfo getOperandsInfo(const Instruction *instruction) const;
    void ensureSizeInformation(const Instruction *instruction, InstructionOperandsInfo& opInfo) const;
    size_t getStructVariableSize(const String& structName, const String& fieldName) const;
    String expandStructField(Iterator::Iterator<const Instruction::Operand>& op, OperandInfo& opInfo,
        char *dest, bool forceSearch = false) const;
    bool isSpriteVar(const String& str);

    struct TypeInfo {
        const char *unsignedType;
        const char *signedType;
        const char *nextLargerSignedType;
        int bitCount;
        const char *signMask;
        const char *signedMin;
        const char *signedMax;
    };
    TypeInfo getTypeInfo(size_t size);

    const StringList *m_cImportSymbols = nullptr;
    const StringList *m_cExportSymbols = nullptr;

    bool m_insideProc = false;
    int m_autoGeneratedProcIndex = 0;
    int m_dataIndex = 0;

    class SizeAndOffsetHolder {
        uint8_t m_offset;
        uint8_t m_size;
    public:
        SizeAndOffsetHolder(size_t offset, size_t size) {
            Util::assignSize(m_offset, offset);
            Util::assignSize(m_size, size);
        }
        static size_t requiredSize(size_t, size_t) { return sizeof(SizeAndOffsetHolder); }
        uint8_t offset() const { return m_offset; }
        uint8_t size() const { return m_size; }
    };

    StringMap<SizeAndOffsetHolder> m_structMap;
    StringSet m_inProcLabels;

    static const std::vector<String> m_spriteStructVars;
};
