#pragma once

#include "InstructionNode.h"

class CppOutput;

class X86InstructionWriter
{
public:
    X86InstructionWriter(CppOutput *outputWriter, const DataBank& dataBank, const SymbolFileParser& symFileParser, const StringSet& inProcLabels);
    void outputInstruction(const InstructionNode& node);
    void outputFunctionCall(const String& target);

private:
    void outputToken(const String& op, Token::Type type, bool isInstruction = true);
    void outputMov(const InstructionNode& node);
    void outputMovs(const InstructionNode& node);
    void outputStos(const InstructionNode& node);
    void outputLods(const InstructionNode& node);
    void outputCall(const InstructionNode& node);
    void outputPushPop(const InstructionNode& node);
    void outputCbw(const InstructionNode& node);
    void outputCwd(const InstructionNode& node);
    void outputCwde(const InstructionNode& node);
    void outputCdq(const InstructionNode& node);
    void outputOrXor(const InstructionNode& node);
    void outputCmpSubAdd(const InstructionNode& node, bool commitResult, bool add);
    void outputIncDec(const InstructionNode& node, bool increment);
    void outputAndTest(const InstructionNode& node, bool commitResult);
    void outputXchg(const InstructionNode& node);
    void outputMul(const InstructionNode& node);
    void outputImul(const InstructionNode& node);
    void outputDiv(const InstructionNode& node);
    void outputShift(const InstructionNode& node);
    void outputRotate(const InstructionNode& node);
    void outputIntInOut(const InstructionNode& node);
    void outputNeg(const InstructionNode& node);
    void outputNot(const InstructionNode& node);
    void outputLoop(const InstructionNode& node);
    void outputPushf(const InstructionNode& node);
    void outputPopf(const InstructionNode& node);
    void outputPusha(const InstructionNode& node);
    void outputPopa(const InstructionNode& node);
    void outputStc(const InstructionNode& node);
    void outputClc(const InstructionNode& node);
    void outputCld(const InstructionNode& node);
    void outputSetz(const InstructionNode& node);

    void outputConditionalJump(const InstructionNode& node, const char *condition = nullptr);
    void outputJmp(const InstructionNode& node);
    void outputJg(const InstructionNode& node);
    void outputJge(const InstructionNode& node);
    void outputJl(const InstructionNode& node);
    void outputJz(const InstructionNode& node);
    void outputJnz(const InstructionNode& node);
    void outputJs(const InstructionNode& node);
    void outputJns(const InstructionNode& node);
    void outputJb(const InstructionNode& node);
    void outputJa(const InstructionNode& node);
    void outputJnb(const InstructionNode& node);
    void outputJbe(const InstructionNode& node);
    void outputJle(const InstructionNode& node);
    void outputJo(const InstructionNode& node);
    void outputJno(const InstructionNode& node);

    void outputFunctionInvoke(const String& target);
    void outputLabel(const String& label);
    static bool isRetnNext(const InstructionNode& node);

    CppOutput& m_outputWriter;
    const DataBank& m_dataBank;
    const SymbolFileParser& m_symFileParser;
    const StringSet& m_inProcLabels;
};
