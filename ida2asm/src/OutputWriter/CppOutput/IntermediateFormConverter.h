#pragma once

#include "InstructionNode.h"
#include "OutputItem/OutputItem.h"
#include "References.h"
#include "StructMap.h"
#include "Struct.h"
#include "DataBank.h"
#include "RedundantInstructionRemover.h"

class IntermediateFormConverter
{
public:
    IntermediateFormConverter(bool disableOptimizations, const StructStream& structs, const DefinesMap& defines,
        const References& references, const DataBank& dataBank);
    void convertProc(const OutputItem *item, const OutputItem *end);
    void optimizeFlags();
    const Instructions& instructions() const;

private:
    using LabelList = std::vector<std::pair<const String, size_t>>;

    void fillStructInfo(const StructStream& structs);
    size_t convertProcInstructions(const OutputItem *item, const OutputItem *end);

    LabelList gatherLabels(size_t start, const OutputItem *item);
    void markJumpTargetInstructions(size_t start, const LabelList& labels);

    bool m_disableOptimizations;

    const StructStream& m_structs;
    const References& m_references;
    const DataBank& m_dataBank;
    const DefinesMap& m_defines;

    Instructions m_instructions;
    RedundantInstructionRemover m_optimizer;

    StructMap m_structMap;
};
