#pragma once

#include "Tokenizer.h"
#include "Util.h"
#include "StringView.h"
#include "Iterator.h"

#pragma pack(push, 1)
class Instruction
{
public:
    enum OperandType : uint8_t {
        kNone = 0,
        kRegister = 1,
        kVariable = 2,
        kLabel = 4,
        kLiteral = 8,
        kOffset = 16,
        kIndirect = 32,
        kPointer = 64,
        kShort = 128,   // for jump instructions
    };

    static constexpr int kMaxOperands = 3;

    using OperandTokens = std::array<CToken *, kMaxOperands * 2>;
    using OperandSizes = std::array<uint8_t, kMaxOperands>;
    using OperandTypes = std::array<OperandType, kMaxOperands>;

    Instruction(CToken *prefix, CToken *instructionToken, const OperandSizes& opSizes,
        const OperandTypes& opTypes, const OperandTokens& opTokens);
    static size_t requiredSize(CToken *prefix, CToken *instructionToken, const OperandTokens& opTokens);

    String instructionText() const;
    String prefix() const;
    Token::Type type() const;
    bool isBranch() const;
    OperandSizes operandSizes() const;
    OperandTypes operandTypes() const;
    size_t numOperands() const;
    bool hasOperands() const;

    class Operand
    {
    public:
        Operand(CToken *token) {
            assert(token && token->textLength);
            Util::assignSize(m_length, token->textLength);
            m_type = token->type;
            memcpy(textPtr(), token->text(), token->textLength);
        }
        static size_t requiredSize(CToken *token) {
            assert(token);
            return sizeof(Operand) + token->textLength;
        }
        String text() const {
            return { textPtr(), m_length };
        }
        Token::Type type() const {
            return m_type;
        }
        const Operand *next() const {
            return reinterpret_cast<Operand *>(textPtr() + m_length);
        }
    private:
        char *textPtr() const {
            return (char *)(this + 1);
        }
        Token::Type m_type;
        uint8_t m_length;
        // followed by operand text
    };

    class OperandHolder
    {
    public:
        OperandHolder(const Operand *begin, const Operand *end) : m_begin(begin), m_end(end) {}
        const Util::Iterator<const Operand> begin() const { return m_begin; }
        const Util::Iterator<const Operand> end() const { return m_end; }
    private:
        const Operand *m_begin;
        const Operand *m_end;
    };

    using Operands = std::array<const OperandHolder, kMaxOperands>;
    const Operands operands() const;

private:
    uint8_t copyTokens(char *dest, CToken *begin, CToken *end);
    char *instructionTextPtr() const;
    char *prefixPtr() const;
    Operand *op1Data() const;
    Operand *op2Data() const;
    Operand *op3Data() const;

    Token::Type m_prefixType = Token::T_NONE;
    Token::Type m_type;
    OperandTypes m_operands;
    OperandSizes m_operandSizes;
    OperandSizes m_operandDataSizes;
    uint8_t m_instructionTextLength;
    uint8_t m_prefixLength;
    uint8_t m_isBranch;
    // followed by instruction text, prefix, then by operands, each one is a byte length, two byte enum, then a string

    static constexpr size_t kMaxLen = std::numeric_limits<decltype(m_operandDataSizes)::value_type>::max();
};
#pragma pack(pop)

ENABLE_FLAGS(Instruction::OperandType)
