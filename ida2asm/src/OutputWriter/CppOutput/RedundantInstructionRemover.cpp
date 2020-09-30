#include "DefinesMap.h"
#include "DataBank.h"
#include "VmDataState.h"
#include "RedundantInstructionRemover.h"
#include "OrphanedAssignmentChainRemover.h"

RedundantInstructionRemover::RedundantInstructionRemover(const DefinesMap& defines, const DataBank& dataBank)
    : m_defines(defines), m_dataBank(dataBank)
{
}

// Remove redundant MOV instructions by tracking registry and memory contents
void RedundantInstructionRemover::markRedundantProcInstructions(Instructions& nodes, size_t start, size_t end)
{
    VmDataState state;
    OrphanedAssignmentChainRemover orphanRemover;

    for (; start < end; start++) {
        auto& node = nodes[start];

        if (node.type == OutputItem::kProc) {
            state.startNewProc();
            orphanRemover.startNewProc();
        } else if (node.type == OutputItem::kLabel) {
            state.processLabel(node);
        } else if (node.instruction) {
            if (node.flowChange) {
                state.reset();
                orphanRemover.reset();
            }

            state.processInstruction(node);
            orphanRemover.processInstruction(node);
        } else if (node.type == OutputItem::kEndProc) {
            assert(start == end - 1);
        }
    }
}

void RedundantInstructionRemover::markRedundantFlags(Instructions& nodes)
{
    InstructionFlags flagsState;
    int procStart = -1;

    for (size_t i = 0; i < nodes.size(); i++) {
        auto& node = nodes[i];

        if (node.deleted)
            continue;

        if (node.type == OutputItem::kProc || node.instruction && node.instruction->isBranch()) {
            flagsState.resetLastSetInstructions();
            if (node.type == OutputItem::kProc) {
                procStart = i;
                flagsState.resetTestedInfo();
            } else {
                flagsState.updateLastWrite(node);
                if (node.instruction->isBranch())
                    flagsState.updateTestedInfo(node);
            }
        } else if (node.type == OutputItem::kEndProc) {
            assert(procStart >= 0);
            removeUnusedFlags(nodes, procStart, flagsState);
        } else if (node.instruction) {
            flagsState.updateLastWrite(node);
        }
    }

}

// The plan was to remove calculation of any flag inside the function that doesn't get tested. Unfortunatelly there
// are function that use flags as return values (carry, sign and zero) so the only one left was overflow. It is rarely
// used so there might be some savings still.
void RedundantInstructionRemover::removeUnusedFlags(Instructions& nodes, size_t start, const InstructionFlags& flags)
{
    if (!flags.overflowTested)
        for (auto node = &nodes[start]; node->type != OutputItem::kEndProc; node = &nodes[++start])
            node->suppressOverflowFlag = true;
}

auto RedundantInstructionRemover::getFlagEffect(Token::Type instructionType) -> InstructionFlagEffects
{
    switch (instructionType) {
    case Token::T_NOP:
    case Token::T_MOV:
    case Token::T_MOVSX:
    case Token::T_MOVZX:
    case Token::T_MOVSB:
    case Token::T_MOVSW:
    case Token::T_MOVSD:
    case Token::T_LODSB:
    case Token::T_LODSW:
    case Token::T_LODS:
    case Token::T_STOSB:
    case Token::T_STOSW:
    case Token::T_STOSD:
    case Token::T_PUSH:
    case Token::T_POP:
    case Token::T_CBW:
    case Token::T_CWD:
    case Token::T_CWDE:
    case Token::T_CDQ:
    case Token::T_XCHG:
    case Token::T_IN:
    case Token::T_OUT:
    case Token::T_STI:
    case Token::T_CLI:
    case Token::T_NOT:
    case Token::T_LOOP:
    case Token::T_PUSHF:
    case Token::T_PUSHA:
    case Token::T_POPA:
    case Token::T_CLD:
        return { kIgnore, kIgnore, kIgnore, kIgnore };
    case Token::T_OR:
    case Token::T_XOR:
    case Token::T_CMP:
    case Token::T_AND:
    case Token::T_SUB:
    case Token::T_ADD:
    case Token::T_MUL:
    case Token::T_IMUL:
    case Token::T_DIV:
    case Token::T_IDIV:
    case Token::T_TEST:
    case Token::T_SHL:
    case Token::T_SHR:
    case Token::T_SAR:
    case Token::T_INT:
    case Token::T_NEG:
    case Token::T_POPF:
    case Token::T_CALL:
    case Token::T_JMP:
    case Token::T_RETN:
        return { kWrite, kWrite, kWrite, kWrite };
    case Token::T_JG:
    case Token::T_JLE:
        return { kIgnore, kRead, kRead, kRead };
    case Token::T_JGE:
    case Token::T_JL:
        return { kIgnore, kRead, kRead, kIgnore };
    case Token::T_JZ:
    case Token::T_JNZ:
    case Token::T_SETZ:
        return { kIgnore, kIgnore, kIgnore, kRead };
    case Token::T_JS:
    case Token::T_JNS:
        return { kIgnore, kIgnore, kRead, kIgnore };
    case Token::T_JA:
    case Token::T_JBE:
        return { kRead, kIgnore, kIgnore, kRead };
    case Token::T_JB:
    case Token::T_JNB:
        return { kRead, kIgnore, kIgnore, kIgnore };
    case Token::T_JO:
    case Token::T_JNO:
        return { kIgnore, kRead, kIgnore, kIgnore };
    case Token::T_INC:
    case Token::T_DEC:
        return { kIgnore, kWrite, kWrite, kWrite };
    case Token::T_RCR:
    case Token::T_RCL:
        return { kReadAndWrite, kWrite, kIgnore, kIgnore };
    case Token::T_ROL:
    case Token::T_ROR:
        return { kWrite, kWrite, kIgnore, kIgnore };
    case Token::T_STC:
    case Token::T_CLC:
        return { kWrite, kIgnore, kIgnore, kIgnore };
    default:
        assert(false);
        return {};
    }
}

void RedundantInstructionRemover::InstructionFlags::resetLastSetInstructions()
{
    lastCarrySet = nullptr;
    lastOverflowSet = nullptr;
    lastSignSet = nullptr;
    lastZeroSet = nullptr;
}

void RedundantInstructionRemover::InstructionFlags::resetTestedInfo()
{
    overflowTested = false;
}

void RedundantInstructionRemover::InstructionFlags::updateTestedInfo(InstructionNode& node)
{
    const auto& flagsAffected = getFlagEffect(node.instruction->type());

    if (flagsAffected.overflow == kRead)
        overflowTested = true;
}

void RedundantInstructionRemover::InstructionFlags::updateLastWrite(InstructionNode& node)
{
    const auto& flagsAffected = getFlagEffect(node.instruction->type());

    const std::tuple<const FlagEffect&, InstructionNode **, bool InstructionNode::*> kFlagInfo[] = {
        { flagsAffected.carry, &lastCarrySet, &InstructionNode::suppressCarryFlag },
        { flagsAffected.overflow, &lastOverflowSet, &InstructionNode::suppressOverflowFlag },
        { flagsAffected.sign, &lastSignSet, &InstructionNode::suppressSignFlag },
        { flagsAffected.zero, &lastZeroSet, &InstructionNode::suppressZeroFlag },
    };

    for (auto& flag : kFlagInfo) {
        auto [flagAccess, lastWriteInstructionPtr, suppressFlag] = flag;

        if (flagAccess == kRead || flagAccess == kReadAndWrite) {
            *lastWriteInstructionPtr = nullptr;
            if (flagAccess == kReadAndWrite)
                *lastWriteInstructionPtr = &node;
        } else if (flagAccess == kWrite) {
            if (*lastWriteInstructionPtr)
                (*lastWriteInstructionPtr)->*suppressFlag = true;
            *lastWriteInstructionPtr = &node;
        }
    }
}
