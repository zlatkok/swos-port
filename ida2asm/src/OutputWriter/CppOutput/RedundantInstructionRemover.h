#pragma once

#include "InstructionNode.h"
#include "OrphanedAssignmentChainRemover.h"

class DefinesMap;
class DataBank;

class RedundantInstructionRemover
{
public:
    RedundantInstructionRemover(const DefinesMap& defines, const DataBank& dataBank);
    void markRedundantProcInstructions(Instructions& nodes, size_t start, size_t end);
    static void markRedundantFlags(Instructions& nodes);

private:
    enum FlagEffect {
        kIgnore,
        kRead,
        kWrite,
        kReadAndWrite,
    };

    struct InstructionFlagEffects
    {
        FlagEffect carry;
        FlagEffect overflow;
        FlagEffect sign;
        FlagEffect zero;
    };

    static InstructionFlagEffects getFlagEffect(Token::Type instructionType);

    struct InstructionFlags
    {
        // pointer to instruction that last wrote the flag and is a candidate to have flag setting removed
        InstructionNode *lastCarrySet = nullptr;
        InstructionNode *lastOverflowSet = nullptr;
        InstructionNode *lastSignSet = nullptr;
        InstructionNode *lastZeroSet = nullptr;
        bool overflowTested = false;

        void resetLastSetInstructions();
        void resetTestedInfo();
        void updateTestedInfo(InstructionNode& node);
        void updateLastWrite(InstructionNode& node);
    };

    static void removeUnusedFlags(Instructions& nodes, size_t start, const InstructionFlags& flags);

    const DefinesMap& m_defines;
    const DataBank& m_dataBank;

    OrphanedAssignmentChainRemover m_orphanRemover;
};
