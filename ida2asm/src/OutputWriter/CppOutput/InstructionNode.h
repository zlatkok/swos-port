#pragma once

#include "OutputItem/OutputItem.h"
#include "OperandInfo.h"

struct InstructionNode
{
    InstructionNode(OutputItem::Type type) : type(type) {}
    InstructionNode(OutputItem::Type type, const String& label, const String& leadingComments = String())
        : type(type), label(label), leadingComments(leadingComments) {}
    InstructionNode(const String& size, const String& name)
        : type(OutputItem::kStackVariable), stackVarSize(size), label(name) {}
    InstructionNode(OutputItem::Type type, Instruction *instruction, bool flowChange)
        : type(type), instruction(instruction), flowChange(flowChange) {}
    InstructionNode(const OutputItem *item) {
        assert(item);

        type = item->type();
        if (item->type() == OutputItem::kInstruction)
            instruction = item->getItem<Instruction>();
        deleted = false;
        next = item->next();
    }

    OutputItem::Type type;
    Instruction *instruction = nullptr;
    OutputItem *next = nullptr;
    union {
        String leadingComments;
        String stackVarSize;
    };
    unsigned numOperands = 0;
    bool deleted = false;
    union {
        std::array<OperandInfo, Instruction::kMaxOperands> opInfo;
        String label;
    };
    union {
        bool flowChange = false;
        bool needsStartLabel;
    };
    bool startOverJump = false;
    bool returnJump = false;

    bool suppressCarryFlag = false;
    bool suppressOverflowFlag = false;
    bool suppressSignFlag = false;
    bool suppressZeroFlag = false;
};

using Instructions = std::vector<InstructionNode>;
using InstructionsIterator = Instructions::const_iterator;
