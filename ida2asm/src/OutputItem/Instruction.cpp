#include "Instruction.h"
#include "Util.h"

Instruction::Instruction(CToken *prefix, CToken *instructionToken, const OperandSizes& opSizes,
    const OperandTypes& opTypes, const OperandTokens& opTokens)
    : m_operands(opTypes), m_operandSizes(opSizes)
{
    assert(instructionToken && instructionToken->textLength);
    assert(instructionToken->category == Token::Instruction);

    m_type = instructionToken->type;
    m_instructionType = instructionToken->instructionType;

    Util::assignSize(m_instructionTextLength, instructionToken->textLength);
    instructionToken->copyText(instructionTextPtr());

    m_prefixLength = 0;
    if (prefix) {
        Util::assignSize(m_prefixLength, prefix->textLength);
        prefix->copyText(prefixPtr());
    }

    m_operandDataSizes[0] = copyTokens(reinterpret_cast<char *>(op1Data()), opTokens[0], opTokens[1]);
    m_operandDataSizes[1] = copyTokens(reinterpret_cast<char *>(op2Data()), opTokens[2], opTokens[3]);
    m_operandDataSizes[2] = copyTokens(reinterpret_cast<char *>(op3Data()), opTokens[4], opTokens[5]);
}

size_t Instruction::requiredSize(CToken *prefix, CToken *instructionToken, const OperandTokens& opTokens)
{
    assert(instructionToken);

    size_t opSize = 0;
    for (size_t i = 0; i < opTokens.size() - 1; i += 2)
        for (const auto *token = opTokens[i]; token != opTokens[i + 1]; token = token->next())
            opSize += Operand::requiredSize(token);

    auto prefixLength = prefix ? prefix->textLength : 0;

    assert(opSize <= kMaxLen);
    return sizeof(Instruction) + instructionToken->textLength + prefixLength + opSize;
}

String Instruction::instructionText() const
{
    return { instructionTextPtr(), m_instructionTextLength };
}

String Instruction::prefix() const
{
    return { prefixPtr(), m_prefixLength };
}

Token::Type Instruction::type() const
{
    return m_type;
}

bool Instruction::isBranch() const
{
    return m_instructionType == Token::BranchInstruction;
}

bool Instruction::isShiftRotate() const
{
    return m_instructionType == Token::ShiftRotateInstruction;
}

auto Instruction::operandSizes() const -> OperandSizes
{
    return m_operandSizes;
}

auto Instruction::operandTypes() const -> OperandTypes
{
    return m_operands;
}

size_t Instruction::numOperands() const
{
    if (m_operandDataSizes[2] > 0)
        return 3;

    if (m_operandDataSizes[1] > 0)
        return 2;

    if (m_operandDataSizes[0] > 0)
        return 1;

    return 0;
}

bool Instruction::hasOperands() const
{
    return m_operandDataSizes[0] > 0;
}

auto Instruction::operands() const -> const Operands
{
    return { OperandHolder{ op1Data(), op2Data(), }, OperandHolder{ op2Data(), op3Data(), },
        OperandHolder{ op3Data(), reinterpret_cast<Operand *>(reinterpret_cast<char *>(op3Data()) + m_operandDataSizes[2]), }};
}

uint8_t Instruction::copyTokens(char *dest, CToken *begin, CToken *end)
{
    assert(begin <= end);

    size_t totalSize = 0;
    for (auto token = begin; token != end; token = token->next()) {
        new (dest) Operand(token);
        auto operandSize = Operand::requiredSize(token);
        dest += operandSize;
        totalSize += operandSize;
    }

    assert(totalSize <= kMaxLen);
    return static_cast<uint8_t>(totalSize);
}

char *Instruction::instructionTextPtr() const
{
    return (char *)(this + 1);
}

char *Instruction::prefixPtr() const
{
    return instructionTextPtr() + m_instructionTextLength;
}

auto Instruction::op1Data() const -> Operand *
{
    return reinterpret_cast<Operand *>(prefixPtr() + m_prefixLength);
}

auto Instruction::op2Data() const -> Operand *
{
    return reinterpret_cast<Operand *>(reinterpret_cast<char *>(op1Data()) + m_operandDataSizes[0]);
}

auto Instruction::op3Data() const -> Operand *
{
    return reinterpret_cast<Operand *>(reinterpret_cast<char *>(op2Data()) + m_operandDataSizes[1]);
}
