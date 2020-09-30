#pragma once

class StructStream;
class DefinesMap;
class References;
class DataBank;

#include "OperandInfo.h"
#include "Instruction.h"
#include "StructMap.h"

// parse instruction and provide necessary information for further processing
class InstructionOperandsInfoExtractor
{
public:
    InstructionOperandsInfoExtractor(const Instruction *instruction, const StructStream& structs,
        const StructMap& structMap, const DefinesMap& defines, const References& references, const DataBank& dataBank);
    std::array<OperandInfo, Instruction::kMaxOperands> getOperandInfo() const;

private:
    // info about the instruction operand only needed during parsing phase
    class RawOperandInfo
    {
    public:
        RawOperandInfo(const Instruction *instruction, size_t index, const StructStream& structs,
            const StructMap& structMap, const References& references, const DataBank& dataBank);

        struct OpComponent
        {
            bool empty() const {
                return str.empty();
            }
            bool hasString() const {
                return !empty() && type == Token::T_ID;
            }
            bool isNumber() const {
                return !empty() && Token::isNumber(type);
            }
            void clear() {
                str.clear();
            }
            void assign(const String& str, Token::Type type) {
                this->str = str;
                this->type = type;
                if (Token::isNumber(type))
                    num = str.toInt();
            }

            String str;
            Token::Type type = Token::T_EOF;
            int num;
        };

        bool needsMemoryFetch() const;
        bool got68kRegister() const;
        int getConstantAddress(const DataBank& dataBank) const;
        int getMemoryOffset(const DataBank& dataBank) const;
        std::optional<int> getConstValue() const;

        OpComponent& base() { return m_base; }
        const OpComponent& base() const { return m_base; }
        OpComponent& scale() { return m_scale; }
        const OpComponent& scale() const { return m_scale; }
        OpComponent& displacement() { return m_displacement; }
        const OpComponent& displacement() const { return m_displacement; }

        int scaleFactor() const { return m_scaleFactor; }
        size_t size() const { return m_size; }
        void setSize(size_t size) { m_size = size; }
        bool address() const { return m_address; }
        bool pointer() const { return m_pointer; }
        bool dereference() const { return m_dereference; }
        void setDereference(bool dereference) { m_dereference = dereference; }
        bool structField() const { return m_structField; }
        bool straightReg() const { return m_straightReg; }

    private:
        void parseInstructionOperand(size_t index, const Instruction *instruction);
        void assignComponent(Iterator::Iterator<const Instruction::Operand>& op,
            const Instruction::OperandHolder& params, Token::Type type, bool forceSearch = false);
        void handleScaleExpression(const Instruction::Operand *op);
        OpComponent expandOperand(Iterator::Iterator<const Instruction::Operand>& op, Token::Type type,
            bool forceSearch = false);
        bool expandDirectStructField(OpComponent& result, Iterator::Iterator<const Instruction::Operand>& op);
        bool expandVariableStructField(OpComponent& result, Iterator::Iterator<const Instruction::Operand>& op);
        static size_t gatherNumericOffsets(Iterator::Iterator<const Instruction::Operand>& op);
        static String removeSegmentPrefix(const String& str);
        void markMemoryAccessForVariables();
        bool gotScale() const;
        void setScale(int scale);

        const StructStream& m_structs;
        const StructMap& m_structMap;
        const References& m_references;
        const DataBank& m_dataBank;

        OpComponent m_base;
        OpComponent m_scale;
        OpComponent m_displacement;
        size_t m_size = 0;
        int m_scaleFactor = -1;
        bool m_address = false;
        bool m_pointer = false;
        bool m_dereference = false;
        bool m_structField = false;
        bool m_straightReg = false;
        bool m_hasReg = false;
        bool m_forceAddress = false;
    };

    void convertProcPointers();
    void fixPushSmall();
    void ensureSizeInformation();
    size_t getStructVariableSize(const String& structName, const String& fieldName) const;
    void overrideOrPropagateStructVarSize();
    void fillComponents(OperandInfo& dst, const RawOperandInfo& op, size_t opSize, bool needsMemoryFetch) const;
    static std::tuple<RegisterEnum, size_t, size_t> tokenToRegister(const RawOperandInfo::OpComponent& val);
    std::optional<int> getOffset(const RawOperandInfo& op) const;
    int extractConstantValue(const RawOperandInfo::OpComponent& val) const;

    const Instruction *m_instruction = nullptr;
    const StructMap& m_structMap;
    const DefinesMap& m_defines;
    const References& m_references;
    const DataBank& m_dataBank;

    RawOperandInfo m_op1;
    RawOperandInfo m_op2;
    RawOperandInfo m_op3;
};
