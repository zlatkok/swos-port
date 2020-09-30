#pragma once

#include "AmigaRegs.h"
#include "InstructionNode.h"
#include "RegisterEnum.h"
#include "OperandInfo.h"

class VmDataState
{
public:
    VmDataState() {};
    void startNewProc();
    void reset();
    void processInstruction(InstructionNode& node);
    void processLabel(InstructionNode& node);

private:
    void handleBranching(const InstructionNode& node);
    void handleMov(InstructionNode& node);
    void handleXchg(InstructionNode& node);
    void handleStateChangeInstruction(InstructionNode& node);
    void handleStackOp(const InstructionNode& node);
    void handleBlockInstructions(const InstructionNode& node);
    void handleMovs(const InstructionNode& node, uint32_t len);
    void handleLods(const InstructionNode& node, uint32_t len, uint32_t size);
    void handleStos(const InstructionNode& node, uint32_t len, uint32_t size);

    // this can represent any byte value a register or a memory address can hold
    struct ByteValue
    {
        RegisterEnum reg = kNoReg;
        int color = -1;

        union {
            int8_t value = -1;
            int8_t offset;
        };

        uint8_t isSignOf = 0;

        struct ExpressionEntry {
            ExpressionEntry(int8_t offset, int8_t id) : offset(offset), id(id) {}
            bool operator==(const ExpressionEntry& other) const { return offset == other.offset && id == other.id; }
            int8_t offset = -1;
            uint8_t id = 0;
        };
        std::vector<ExpressionEntry> expression;

        static const ByteValue zero() {
            return { kNoReg, 0, -1, 0 };
        }

        ByteValue() {}
        ByteValue(RegisterEnum reg, int8_t value, int color, uint8_t isSignOf)
            : reg(reg), value(value), color(color), isSignOf(isSignOf) {}
        ByteValue(const ByteValue& other) {
            assign(other);
        }
        ByteValue operator=(const ByteValue& other) {
            assign(other);
            return *this;
        }
        void assign(const ByteValue& other) {
            reg = other.reg;
            offset = other.offset;
            color = other.color;
            isSignOf = other.isSignOf;
            expression = other.expression;
        }
        bool operator==(const ByteValue& other) const {
            return reg == other.reg && value == other.value && color == other.color &&
                isSignOf == other.isSignOf && expression == other.expression;
        }
        bool operator!=(const ByteValue& other) const {
            return !operator==(other);
        }
        bool isConst() const {
            return reg == kNoReg && color == -1 && !isSignOf && expression.empty();
        }
        uint8_t unsignedValue() const {
            return value;
        }
        void resetAsRegister(RegisterEnum reg, int offset, int color, bool saveExpressions = false) {
            this->reg = reg;
            this->offset = offset;
            this->color = color;
            isSignOf = 0;
            if (!saveExpressions)
                expression.clear();
        }
        void resetAsMem(int addr, int color = 0) {
            value = addr;
            this->color = color;
        }
        void resetAsConst(int8_t value) {
            this->value = value;
            reg = kNoReg;
            color = -1;
            expression.clear();
        }
        size_t hash() const {
            size_t res = (1873 * reg) ^ (1987 * value) ^ color ^ (521 * isSignOf);

            for (const auto& expr : expression)
                res ^= (487 * expr.offset) ^ (1759 * expr.id);

            return res;
        }
    };

    class Register
    {
        std::array<ByteValue, Instruction::kRegSize> m_data;

    public:
        Register() {}
        bool operator==(const Register& other) const {
            return m_data == other.m_data;
        }
        ByteValue& operator[](size_t index) {
            assert(index < m_data.size());
            return m_data[index];
        }
        const ByteValue& operator[](size_t index) const {
            assert(index < m_data.size());
            return m_data[index];
        }
        ByteValue *data() {
            return &m_data[0];
        }
        const ByteValue *data() const {
            return &m_data[0];
        }
    };

    struct Expression
    {
        Token::Type op;
        OperandInfo valType;
        Register base;
        Register scale;

        bool operator==(const Expression& other) const {
            return op == other.op && valType == other.valType && base == other.base && scale == other.scale;
        }
    };

    struct ExpressionHasher
    {
        size_t operator()(const Expression& expr) const {
            size_t res = (expr.op * 1327) ^ expr.valType.hash();

            if (!expr.valType.base.empty())
                res ^= hash(expr.base, expr.valType.base);

            if (!expr.valType.scale.empty())
                res ^= hash(expr.scale, expr.valType.scale);

            return res;
        }
        size_t hash(const Register& reg, const OperandInfo::Component& op) const {
            size_t res = 0;

            for (size_t i = 0; i < op.size; i++)
                res ^= reg[i].hash();

            return res;
        }
    };

    static uint32_t doOp(uint32_t dst, uint32_t src, size_t size, Token::Type type);
    void doOp(const OperandInfo& op, Token::Type type);
    template<typename T64, typename T32>
    void handleMul(const OperandInfo& op, Token::Type type);
    void handleImul(InstructionNode& node);
    template<typename T64, typename T32, typename T16>
    void handleIdiv(const OperandInfo& op, Token::Type type);

    std::tuple<OperandInfo, OperandInfo, OperandInfo> extractAllOperands(const InstructionNode& node);
    std::pair<OperandInfo, OperandInfo> extractOperands(const InstructionNode& node);
    void compareAssign(InstructionNode& node, const OperandInfo& dst, const OperandInfo& src);

    struct RegSlice
    {
        RegSlice(ByteValue *val, size_t offset, size_t size)
            : val(val), offset(offset), size(size) {}
        RegSlice(const Register& reg, size_t offset, size_t size)
            : val(const_cast<Register&>(reg).data()), offset(offset), size(size) {}

        ByteValue *val;
        size_t offset;
        size_t size;

        bool operator==(const RegSlice& other) const {
            return val == other.val && offset == other.offset && size == other.size;
        }
    };

    bool valuesEqual(const OperandInfo& op1, const OperandInfo& op2, Token::Type instruction);
    bool valuesEqual(const OperandInfo& op1, const OperandInfo& op2);
    static bool registerWideningEqual(const RegSlice& reg, size_t size, bool signExtend);
    bool operandsEqual(const OperandInfo& op1, const OperandInfo& op2) const;
    std::optional<uint32_t> getConstantValue(const OperandInfo& op) const;
    void assignValue(const OperandInfo& dst, const OperandInfo& src, bool signExtend = false);
    void assignDynamicMemToRegister(const OperandInfo& dst, const OperandInfo& src);
    void assignConstant(const OperandInfo& dst, uint32_t value);
    void assignExpression(const OperandInfo& dst, Token::Type type, const OperandInfo& op);
    Expression createExpression(Token::Type type, const OperandInfo& op) const;
    void trashLocation(const OperandInfo& dst);

    static bool compareRegisters(const RegSlice& reg1, const RegSlice& reg2);
    static void assignRegisters(const RegSlice& dst, const RegSlice& src, bool signExtend = false);
    static void widenRegister(const RegSlice& reg, size_t size, bool signExtend);
    void ensureMemoryValues(int addr, size_t size);
    bool compareRegisterAndMemory(const RegSlice& reg, size_t address, size_t size) const;
    void assignMemoryToRegister(const RegSlice& reg, size_t addr, size_t size, bool signExtend = false);
    void assignRegisterToMemory(size_t addr, const RegSlice& reg);
    static bool compareRegisterAndValue(const RegSlice& reg, const ByteValue& val);
    void assignValueToMemory(size_t address, size_t size, const ByteValue& val);
    static bool compareRegisterAndConstant(const RegSlice& reg, size_t val);
    bool compareMemoryAndConstant(size_t addr, size_t size, uint32_t val) const;
    static std::optional<uint32_t> getRegisterConstant(const RegSlice& reg);
    std::optional<uint32_t> getRegisterConstant(RegisterEnum reg) const;
    std::optional<uint32_t> getMemoryConstant(size_t address, size_t size) const;
    void assignConstantToRegister(const RegSlice& reg, uint32_t val);
    void assignConstantToRegister(RegisterEnum reg, uint32_t val);
    void assignConstantToMemory(size_t address, size_t size, uint32_t val);
    void assignExpressionToRegister(const RegSlice& reg, size_t expressionId);
    void assignExpressionToMemory(size_t addr, size_t size, size_t expressionId);
    const RegSlice getRegister(const OperandInfo& op) const;
    RegSlice getRegister(const OperandInfo& op);

    std::optional<int> tryExtractingDynamicAddress(const OperandInfo& op) const;
    void trashRegister(const RegSlice& reg);
    void trashRegister(RegisterEnum reg);
    void resetRegisterToUnknown(const RegSlice& reg);
    void trashMemory(size_t address, size_t size);
    void dynamicMemoryWrite();

    std::array<Register, kNumAmigaRegisters + kNumIntelRegisters> m_regs;
    std::unordered_map<int, ByteValue> m_mem;

    int m_color = 0;
    std::unordered_map<Expression, size_t, ExpressionHasher> m_expressions;
    size_t m_expressionId = 0;

    using Stack = std::vector<Register>;
    std::unordered_map<String, Stack> m_stacks;
    std::unordered_set<String> m_ignoredLabels;
    Stack *m_stack = nullptr;
};
