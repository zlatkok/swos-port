// This class converts all the instruction into the intermediate form which we can use to optimize them a bit
// (namely, to remove useless register reloading, which helps reduce bloated C converted instructions).

#include "RedundantInstructionRemover.h"
#include "OutputException.h"
#include "InstructionOperandsInfoExtractor.h"
#include "IntermediateFormConverter.h"

constexpr int kAverageBytesPerStruct = 905;

IntermediateFormConverter::IntermediateFormConverter(bool disableOptimizations, const StructStream& structs,
    const DefinesMap& defines, const References& references, const DataBank& dataBank)
:
    m_disableOptimizations(disableOptimizations), m_structs(structs), m_references(references), m_dataBank(dataBank),
    m_defines(defines), m_optimizer(defines, dataBank), m_structMap(structs.count() * kAverageBytesPerStruct)
{
    fillStructInfo(structs);

    m_instructions.reserve(26'000);
}

void IntermediateFormConverter::fillStructInfo(const StructStream& structs)
{
    for (const auto& struc : structs) {
        const auto& structName = struc.name();
        char buf[256];
        structName.copy(buf);
        buf[structName.length()] = '.';

        size_t offset = 0;

        for (const auto& field : struc) {
            const auto& fieldName = field.name();

            auto nameLen = structName.length() + fieldName.length() + 1;
            assert(nameLen < sizeof(buf));

            fieldName.copy(buf + structName.length() + 1);

            if (field.elementSize()) {
                auto size = field.elementSize();
                m_structMap.add<size_t, size_t>(buf, nameLen, offset, size);
                offset += field.byteSize();
            } else {
                auto nestedStruct = structs.findStruct(field.type());
                assert(nestedStruct);

                buf[nameLen++] = '.';
                size_t nestedStructOffset = 0;
                size_t unionSize = 0;

                for (const auto& field : *nestedStruct) {
                    const auto& fieldName = field.name();
                    assert(nameLen + fieldName.length() < sizeof(buf));

                    fieldName.copy(buf + nameLen);
                    m_structMap.add<size_t, size_t>(buf, nameLen + fieldName.length(),
                        offset + nestedStructOffset, field.byteSize());

                    if (nestedStruct->isUnion())
                        unionSize = std::max(unionSize, field.byteSize());
                    else
                        nestedStructOffset += field.byteSize();
                }

                m_structMap.add<size_t, size_t>(buf, nameLen - 1, offset, nestedStructOffset);
                auto dup = field.dup() ? field.dup().toInt() : 1;
                offset += (nestedStructOffset + unionSize) * dup;
            }
        }

        m_structMap.add(structName, offset, offset);
    }

    m_structMap.seal();
}

void IntermediateFormConverter::convertProc(const OutputItem *item, const OutputItem *end)
{
    assert(item && item->type() == OutputItem::kProc);

    auto instructionStartIndex = convertProcInstructions(item, end);
    const auto& labels = gatherLabels(instructionStartIndex + 1, item);
    markJumpTargetInstructions(instructionStartIndex, labels);
    assert(instructionStartIndex < m_instructions.size());

    if (!m_disableOptimizations)
        m_optimizer.markRedundantProcInstructions(m_instructions, instructionStartIndex, m_instructions.size() - 1);
}

void IntermediateFormConverter::optimizeFlags()
{
    if (!m_disableOptimizations)
        RedundantInstructionRemover::markRedundantFlags(m_instructions);
}

auto IntermediateFormConverter::instructions() const -> const Instructions&
{
    return m_instructions;
}

size_t IntermediateFormConverter::convertProcInstructions(const OutputItem *item, const OutputItem *end)
{
    assert(item && item->type() == OutputItem::kProc);

    size_t start = m_instructions.size();

    m_instructions.emplace_back(OutputItem::kProc, item->getItem<Proc>()->name(), item->leadingComments());

    Instruction *lastInstruction = nullptr;

    for (; item->type() != OutputItem::kEndProc; item = item->next()) {
        switch (item->type()) {
        case OutputItem::kInstruction:
            {
                auto instruction = item->getItem<Instruction>();

                // remove, since there are only two of them and are used in a wrong way
                if (instruction->type() == Token::T_JO)
                    continue;

                m_instructions.emplace_back(item);

                if (instruction->type() == Token::T_STD)
                    throw OutputException("STD instruction not supported");

                if (instruction->isBranch()) {
                    for (auto& op : m_instructions.back().opInfo)
                        op.reset();
                } else if (instruction->numOperands() > 0 &&
                    instruction->type() != Token::T_IN && instruction->type() != Token::T_OUT) {
                    InstructionOperandsInfoExtractor extractor(instruction, m_structs, m_structMap, m_defines, m_references, m_dataBank);
                    m_instructions.back().opInfo = extractor.getOperandInfo();
                }

                lastInstruction = instruction;
            }
            break;

        case OutputItem::kLabel:
            m_instructions.emplace_back(OutputItem::kLabel, item->getItem<Label>()->name());
            break;

        case OutputItem::kStackVariable:
            auto stackVar = item->getItem<StackVariable>();
            m_instructions.emplace_back(stackVar->sizeString(), stackVar->name());
            break;
        }
    }

    String fallThroughProc;

    if (lastInstruction && lastInstruction->type() != Token::T_RETN &&
        (lastInstruction->type() != Token::T_JMP || !lastInstruction->getBranchTarget())) {
        while (item != end && item->type() != OutputItem::kProc)
            item = item->next();
        if (item->type() == OutputItem::kProc)
            fallThroughProc = item->getItem<Proc>()->name();
    }

    m_instructions.emplace_back(OutputItem::kEndProc, fallThroughProc);

    return start;
}

auto IntermediateFormConverter::gatherLabels(size_t start, const OutputItem *item) -> LabelList
{
    LabelList labels;
    bool tieLabel = false;

    for (; item->type() != OutputItem::kEndProc; item = item->next()) {
        switch (item->type()) {
        case OutputItem::kInstruction:
            if (tieLabel) {
                assert(!labels.empty());
                labels.back().second = start;
                tieLabel = false;
            }
            start++;
            break;
        case OutputItem::kLabel:
            labels.emplace_back(item->getItem<Label>()->name(), -1);
            assert(!tieLabel);
            tieLabel = true;
            [[fallthrough]];
        case OutputItem::kStackVariable:
            start++;
            break;
        }
    }

    assert(!tieLabel && start == m_instructions.size() - 1 || start == m_instructions.size());

    return labels;
}

void IntermediateFormConverter::markJumpTargetInstructions(size_t start, const LabelList& labels)
{
    assert(m_instructions[start].type == OutputItem::kProc);
    auto& proc = m_instructions[start];

    for (; start < m_instructions.size(); start++) {
        auto instruction = m_instructions[start].instruction;
        if (instruction && instruction->isBranch() && instruction->numOperands() == 1) {
            auto target = instruction->getBranchTarget();

            if (target == proc.label) {
                m_instructions[start].startOverJump = true;
                proc.needsStartLabel = true;
            }

            auto targetInstruction = std::find_if(labels.begin(), labels.end(), [&target](const auto& item) {
                return item.first == target;
            });

            if (targetInstruction != labels.end()) {
                auto& target = m_instructions[targetInstruction->second];
                assert(target.instruction);
                target.flowChange = true;
                if (target.instruction->type() == Token::T_RETN)
                    m_instructions[start].returnJump = true;
            }
        }
    }
}
