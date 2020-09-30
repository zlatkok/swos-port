#pragma once

#include "TypeInfo.h"
#include "InstructionNode.h"

class CppOutput;

class OpWriter
{
public:
    enum Flags {
        kSigned = 1,
        kExtraScope = 2,
    };

    OpWriter(const InstructionNode& node, CppOutput& outputWriter, int flags = 0);
    ~OpWriter();

    void indent() const;
    void increaseIndentLevel();
    void decreaseIndentLevel();
    void startNewLine(bool semicolon = true, int indentChange = 0);
    void discardOutput();

    bool isDestMemory() const;
    void writeSrcToDestMemory();

    enum ValueCategory { kAuto, kLvalue, kRvalue, };
    void outputDest(ValueCategory category = kAuto);
    void outputSrc(ValueCategory category = kAuto);
    void outputThirdOp();

    void outputSrcVar(ValueCategory category = kAuto);
    void outputDestVar(ValueCategory category = kAuto);
    void setDestVar(const char *destVar);
    size_t srcSize() const;
    size_t destSize() const;
    std::optional<int> srcConstValue() const;
    int bitCount() const;
    const char *destType() const;
    const char *signedType() const;
    const char *nextLargerSignedType() const;
    const char *unsignedType() const;
    const char *signMask() const;
    const char *signedMin() const;
    const char *signedMax() const;

    void fetchSrcFromMemoryIfNeeded(ValueCategory category = kAuto);
    void fetchDestFromMemoryIfNeeded(ValueCategory category = kAuto);
    void writeBackDestToMemoryIfNeeded();
    void writeBackSrcToMemoryIfNeeded();

    void clearCarryAndOverflowFlags();
    void setCarryFlag(std::function<void()> f);
    void setCarryFlag(const char *val);
    void setCarryFlag(bool val);
    void setOverflowFlag(std::function<void()> f);
    void setOverflowFlag(const char *val);
    void setSignFlag(std::function<void()> f);
    void setSignFlag(const char *val);
    void setZeroFlag(std::function<void()> f);
    void setZeroFlag(const char *val);

private:
    struct OpInfo
    {
        OpInfo(const OperandInfo& opInfo, const OpInfo& otherOp, const char *varName)
            : opInfo(opInfo), otherOp(otherOp), varName(varName)
        {
        }
        void setSize() {
            if (opInfo.type != OperandInfo::kUnknown)
                size = opInfo.size();
        }
        bool isConstZero() const {
            return opInfo.isConstZero();
        }
        const OperandInfo& opInfo;
        const OpInfo& otherOp;
        const char *varName;
        size_t size;
        bool fetchMemory = false;
        int constantAddress = false;
    };

    enum DestMemoryData { kSourceValue, kDestVar, };
    void outputOp(const OpInfo& op, ValueCategory category);
    void fetchFromMemoryIfNeeded(const OpInfo& op, ValueCategory category);
    void writeToMemory(const OpInfo& op, DestMemoryData source);
    void writeToConstantAddress(const OpInfo& op, DestMemoryData source);
    void writeToDynamicAddress(const OpInfo& op, DestMemoryData source);
    void readConstantMemory(const OpInfo& op) const;
    const char *getType(const OpInfo& op) const;
    void outputDestValue(ValueCategory category);
    void outputSrcValue(ValueCategory category);
    void outputOpValue(const OpInfo& op, ValueCategory category);
    bool outputOpValue(const OperandInfo::Component& val, const OpInfo& op, ValueCategory category) const;
    bool handleLocalVariable(const OperandInfo& op);
    void setFlag(const char *flag, std::function<void()> f);
    void setFlag(const char *flag, const char *val);
    void setFlag(const char *flag, bool val);
    void outFlagAssignment(const char *flag) const;
    void outMemAccess(size_t address, size_t size = 1) const;

    const InstructionNode& m_node;
    CppOutput& m_outputWriter;

    OpInfo m_dst;
    OpInfo m_src;
    TypeInfo m_dstTypeInfo;
    int m_flags;
    int m_indentLevel = 1;
    bool m_aborted = false;
    char *m_startPtr;

    bool m_dstVarDeclared = false;
    bool m_srcVarDeclared = false;
};
