#include "DefinesMap.h"
#include "DataBank.h"
#include "VmDataState.h"

// This is a very simple system for tracking values contained in registers and memory locations. It will only
// track instructions that run linearly, and at any point which is the target of multiple instruction flows
// all the contained values will be reset.
// Instructions operating on constant values will be simulated. If values are unknown, registers/memory
// locations will be assigned "color" which changes whenever the value changes. This allows catching:
//   mov eax, ebx
//   mov ecx, ebx
//   mov eax, ecx   ; <-- gets eliminated (eax and ecx contain the same register (ebx) with the same "color")
//
// This works even if we don't know what ebx register contains.
// But in the case below there will be no elimination since the "color" is different:
//   mov eax, ebx
//   inc ebx        ; changes "color" of ebx
//   mov ecx, ebx
//   mov eax, ecx   ; nothing done
//
// Expressions on unknown values are tracked. However it's pretty simplistic. A + B will appear different
// to B + A. Still, it's adequate for our modest needs:
//   mov eax, ebx
//   mov ecx, ebx
//   inc eax
//   inc ecx
//   mov eax, ecx   ; <-- gets eliminated (same initial value and subsequent operations)
//

void VmDataState::startNewProc()
{
    reset();
    m_ignoredLabels.clear();
    m_stacks.clear();
    m_stack = &m_stacks["$"];
}

void VmDataState::reset()
{
    assert(m_regs.size() == kNumRegisters);

    for (size_t i = 0; i < m_regs.size(); i++)
        for (size_t j = 0; j < Instruction::kRegSize; j++)
            m_regs[i][j].resetAsRegister(static_cast<RegisterEnum>(i), j, m_color);

    m_color++;
    m_mem.clear();
    m_expressions.clear();
    m_expressionId = 0;
}

void VmDataState::processInstruction(InstructionNode& node)
{
    assert(node.instruction);

    if (node.instruction->isBranch())
        handleBranching(node);

    switch(node.instruction->type()) {
    case Token::T_NOP:
    case Token::T_CMP:
    case Token::T_TEST:
    case Token::T_STC:
    case Token::T_CLC:
    case Token::T_CLD:
    case Token::T_STI:
    case Token::T_CLI:
        // ignore, doesn't change regs/memory
        break;

    case Token::T_MOV:
    case Token::T_MOVSX:
    case Token::T_MOVZX:
        handleMov(node);
        break;

    case Token::T_XCHG:
        handleXchg(node);
        break;

    case Token::T_OR:
    case Token::T_XOR:
    case Token::T_AND:
    case Token::T_SUB:
    case Token::T_ADD:
    case Token::T_ROL:
    case Token::T_ROR:
    case Token::T_MUL:
    case Token::T_IMUL:
    case Token::T_DIV:
    case Token::T_IDIV:
    case Token::T_SHL:
    case Token::T_SHR:
    case Token::T_SAR:
    case Token::T_CBW:
    case Token::T_CWDE:
    case Token::T_CDQ:
    case Token::T_NEG:
    case Token::T_NOT:
    case Token::T_INC:
    case Token::T_DEC:
        handleStateChangeInstruction(node);
        break;

    case Token::T_PUSH:
    case Token::T_POP:
    case Token::T_PUSHF:
    case Token::T_PUSHA:
    case Token::T_POPA:
    case Token::T_POPF:
        handleStackOp(node);
        break;

    case Token::T_LODSB:
    case Token::T_LODSW:
    case Token::T_LODS:
    case Token::T_STOSB:
    case Token::T_STOSW:
    case Token::T_STOSD:
    case Token::T_MOVSB:
    case Token::T_MOVSW:
    case Token::T_MOVSD:
        handleBlockInstructions(node);
        break;

    case Token::T_LOOP:
        assignConstantToRegister(kEcx, 0);
        break;

    case Token::T_RCR:
    case Token::T_RCL:
        trashLocation(node.opInfo[0]);
        break;

    case Token::T_INT:
    case Token::T_CALL:
        reset();
        break;

    case Token::T_IN:
    case Token::T_OUT:
        // trash the damn things immediately
        node.deleted = true;
        break;
    }
}

void VmDataState::processLabel(InstructionNode& node)
{
    assert(node.type == OutputItem::kLabel);

    auto it = m_stacks.find(node.label);
    if (it != m_stacks.end())
        m_stack = &it->second;
    else
        m_ignoredLabels.insert(node.label);
}

void VmDataState::handleBranching(const InstructionNode& node)
{
    auto target = node.instruction->getBranchTarget();
    if (!target.empty() && m_ignoredLabels.find(target) == m_ignoredLabels.end())
        m_stacks[target] = *m_stack;
}

void VmDataState::handleMov(InstructionNode& node)
{
    auto [dst, src] = extractOperands(node);
    compareAssign(node, dst, src);
}

void VmDataState::handleXchg(InstructionNode& node)
{
    auto [op1, op2] = extractOperands(node);

    assert(op1.size() == op2.size());

    if (valuesEqual(op1, op2)) {
        node.deleted = true;
    } else {
        if (op1.isDynamicMem() || op2.isDynamicMem()) {
            trashLocation(op1);
            trashLocation(op2);
        } else {
            Register tmp;

            if (op1.type == OperandInfo::kReg) {
                assignRegisters({ tmp, 0, op1.base.size }, getRegister(op1));
            } else {
                assert(op1.type == OperandInfo::kFixedMem);
                assignMemoryToRegister({ tmp, 0, op1.memSize }, op1.address(), op1.memSize);
            }

            assignValue(op1, op2);

            if (op2.type == OperandInfo::kReg) {
                assignRegisters(getRegister(op2), { tmp, 0, op2.base.size });
            } else {
                assert(op2.type == OperandInfo::kFixedMem);
                assignRegisterToMemory(op2.address(), { tmp, 0, op2.memSize });
            }
        }
    }
}

void VmDataState::handleStateChangeInstruction(InstructionNode& node)
{
    auto [dst, src] = extractOperands(node);

    assert(node.instruction->numOperands() == 0 || dst.type == OperandInfo::kReg ||
        dst.type == OperandInfo::kFixedMem || dst.type == OperandInfo::kDynamicMem);

    auto type = node.instruction->type();

    if (node.instruction->type() == Token::T_IMUL) {
        handleImul(node);
    } else if (node.instruction->numOperands() < 2) {
        doOp(dst, type);
    } else {
        assert(dst.type == OperandInfo::kReg || dst.type == OperandInfo::kFixedMem || dst.type == OperandInfo::kDynamicMem);

        if (operandsEqual(dst, src)) {
            switch (type) {
            case Token::T_SUB:
            case Token::T_XOR:
                assignConstant(dst, 0);
                [[fallthrough]];
            case Token::T_OR:
            case Token::T_AND:
                // no-op
                return;
            }
        }

        auto dstValue = getConstantValue(dst);
        auto srcValue = getConstantValue(src);

        // normally these instructions would be dangerous to remove since they affect the flags,
        // but it's all right since SWOS never uses them after ADD/SUB
        if ((type == Token::T_ADD || type == Token::T_SUB) && srcValue && !*srcValue) {
            node.deleted = true;
            return;
        }

        if (dstValue && srcValue) {
            auto size = dst.size() ? dst.size() : src.size();
            auto result = doOp(*dstValue, *srcValue, size, node.instruction->type());
            assignConstant(dst, result);
        } else if (dst.type == OperandInfo::kReg || dst.type == OperandInfo::kFixedMem) {
            assignExpression(dst, type, src);
        } else if (dst.type == OperandInfo::kDynamicMem) {
            dynamicMemoryWrite();
        }
    }
}

void VmDataState::handleStackOp(const InstructionNode& node)
{
    static const RegisterEnum kPushOrder[] = { kEax, kEcx, kEdx, kEbx, kEsp, kEbp, kEsi, kEdi };
    auto type = node.instruction->type();

    if (type == Token::T_PUSHF) {
        m_stack->push_back({});
    } else if (type == Token::T_POPF) {
        assert(!m_stack->empty());
        m_stack->pop_back();
    } else if (type == Token::T_PUSHA) {
        for (auto reg : kPushOrder) {
            m_stack->push_back({});
            assignRegisters({ m_stack->back(), 0, 4 }, { m_regs[reg], 0, 4 });
        }
    } else if (type == Token::T_POPA) {
        for (auto it = std::crbegin(kPushOrder); it != std::crend(kPushOrder); ++it) {
            auto val = m_stack->back();
            m_stack->pop_back();
            assignRegisters({ m_regs[*it], 0, 4 }, { val, 0, 4 });
        }
    } else {
        auto dst = extractOperands(node).first;
        if (type == Token::T_PUSH) {
            m_stack->push_back({});
            if (dst.type == OperandInfo::kReg) {
                auto& reg = m_regs[dst.base.reg];
                assignRegisters({ m_stack->back().data(), 0, 4 }, { reg.data(), 0, 4 });
            } else if (dst.type == OperandInfo::kFixedMem) {
                assert(dst.memSize == 4 || dst.memSize == 2);
                assignMemoryToRegister({ m_stack->back().data(), 0, dst.memSize }, dst.address(), dst.memSize);
            } else if (dst.type == OperandInfo::kImmediate) {
                assignConstantToRegister({ m_stack->back().data(), 0, 4 }, *dst.displacement.num);
            } else {
                // no idea what it is, simply leave it as is
                assert(dst.type == OperandInfo::kDynamicMem);
                dynamicMemoryWrite();
            }
        } else {
            assert(dst.type != OperandInfo::kImmediate && m_stack);

            // there are some functions that continue into other functions, without retn
            // it would be too complicated to handle so just letting them be
            if (m_stack->empty())
                return;

            auto val = m_stack->back();
            m_stack->pop_back();

            if (dst.type == OperandInfo::kReg) {
                auto dstReg = m_regs[dst.base.reg];
                auto offset = dst.base.offset;
                auto size = dst.base.size;
                assert(size == 4 || size == 2);
                assignRegisters({ dstReg.data(), offset, size }, { val.data(), 0, size }, false);
            } else if (dst.type == OperandInfo::kFixedMem) {
                auto addr = *dst.displacement.num;
                auto size = dst.memSize;
                assignRegisterToMemory(addr, { val.data(), 0, size });
            } else if (dst.type == OperandInfo::kDynamicMem) {
                dynamicMemoryWrite();
            } else {
                assert(false);
            }
        }
    }
}

void VmDataState::handleBlockInstructions(const InstructionNode& node)
{
    auto type = node.instruction->type();
    bool isMovs = type == Token::T_MOVSB || type == Token::T_MOVSW || type == Token::T_MOVSD;
    bool isLods = type == Token::T_LODSB || type == Token::T_LODSW || type == Token::T_LODSD;
    bool isStos = type == Token::T_STOSB || type == Token::T_STOSW || type == Token::T_STOSD;

    assert(isMovs || isLods || isStos);

    auto size = 1;
    if (type == Token::T_MOVSW || type == Token::T_LODSW || type == Token::T_STOSW)
        size = 2;
    else if (type == Token::T_MOVSD || type == Token::T_LODSW || type == Token::T_LODSD)
        size = 4;

    auto len = 1;
    if (node.instruction->prefix() == "rep") {
        auto repLen = getRegisterConstant(kEcx);
        if (repLen) {
            len = *repLen;
            assignConstantToRegister(kEcx, 0);
        } else {
            if (!isStos)
                trashRegister(kEsi);
            if (!isLods)
                trashRegister(kEdi);
            if (isLods)
                trashRegister(kEax);
            assignConstantToRegister(kEcx, 0);
            return;
        }
    }

    if (isMovs)
        handleMovs(node, len * size);
    else if (isLods)
        handleLods(node, len, size);
    else
        handleStos(node, len, size);
}

void VmDataState::handleMovs(const InstructionNode& node, uint32_t len)
{
    auto dstAddress = getRegisterConstant(kEdi);
    if (dstAddress) {
        auto srcAddress = getRegisterConstant(kEsi);
        if (srcAddress) {
            ensureMemoryValues(*dstAddress, len);
            while (len--)
                m_mem[(*dstAddress)++] = m_mem[(*srcAddress)++];

            assignConstantToRegister(kEsi, *srcAddress);
            assignConstantToRegister(kEdi, *dstAddress);
        } else {
            trashMemory(*dstAddress, len);
            trashRegister(kEsi);
            assignConstantToRegister(kEdi, *dstAddress + len);
        }
    } else {
        trashRegister(kEsi);
        trashRegister(kEdi);
        dynamicMemoryWrite();
    }
}

void VmDataState::handleLods(const InstructionNode& node, uint32_t len, uint32_t size)
{
    RegSlice eax{ m_regs[kEax].data(), 0, size };

    auto addr = getRegisterConstant(kEsi);
    if (addr) {
        assignMemoryToRegister(eax, *addr + (len - 1) * size, size);
        assignConstantToRegister(kEsi, *addr + len * size);
    } else {
        trashRegister(kEsi);
        trashRegister(eax);
    }
}

void VmDataState::handleStos(const InstructionNode& node, uint32_t len, uint32_t size)
{
    auto dstAddress = getRegisterConstant(kEdi);
    if (dstAddress) {
        auto val = getRegisterConstant(kEax);
        if (val) {
            while (len--) {
                assignConstantToMemory(*dstAddress, size, *val);
                dstAddress = *dstAddress + size;
            }
        } else {
            while (len--) {
                assignRegisterToMemory(*dstAddress, { m_regs[kEax], 0, size });
                dstAddress = *dstAddress + size;
            }
        }
        assignConstantToRegister(kEdi, *dstAddress);
    } else {
        trashRegister(kEdi);
        dynamicMemoryWrite();
    }
}

uint32_t VmDataState::doOp(uint32_t dst, uint32_t src, size_t size, Token::Type type)
{
    switch (type) {
    case Token::T_ADD:
        return dst + src;
    case Token::T_SUB:
        return dst - src;
    case Token::T_OR:
        return dst | src;
    case Token::T_XOR:
        return dst ^ src;
    case Token::T_AND:
        return dst & src;
    case Token::T_ROL:
        return (dst << src) | (dst >> (8 * size - src));
    case Token::T_ROR:
        return (dst >> src) | (dst << (8 * size - src));
    case Token::T_IMUL:
    case Token::T_SHL:
        return dst << src;
    case Token::T_SHR:
        return dst >> src;
    case Token::T_SAR:
        assert(src < 32);
        switch (size) {
        case 1: return static_cast<int8_t>(dst) >> src;
        case 2: return static_cast<int16_t>(dst) >> src;
        case 4: return static_cast<int32_t>(dst) >> src;
        }
        [[fallthrough]];
    default:
        assert(false);
        return 0;
    }
}

void VmDataState::doOp(const OperandInfo& op, Token::Type type)
{
    switch (type) {
    case Token::T_CBW:
        {
            auto& eax = m_regs[kEax];
            eax[1] = eax[0];

            if (eax[0].isConst())
                eax[1].value = eax[0].value < 0 ? -1 : 0;
            else
                eax[1].isSignOf = true;
        }
        break;

    case Token::T_CWDE:
        {
            auto& eax = m_regs[kEax];
            eax[2] = eax[1];
            eax[3] = eax[1];

            if (eax[0].isConst() && eax[1].isConst()) {
                eax[2].value = eax[1].value < 0 ? -1 : 0;
                eax[3].value = eax[1].value < 0 ? -1 : 0;
            } else {
                eax[2].isSignOf = true;
                eax[3].isSignOf = true;
            }
        }
        break;

    case Token::T_CDQ:
        {
            auto& eax = m_regs[kEax];
            auto& edx = m_regs[kEdx];

            std::optional<int> constVal = getRegisterConstant({ eax, 0, 4 });
            if (constVal) {
                ByteValue val;
                val.value = *constVal < 0 ? -1 : 0;
                edx[0] = edx[1] = edx[2] = edx[3] = val;
            } else {
                edx[0] = edx[1] = edx[2] = edx[3] = eax[3];
                edx[0].isSignOf = true;
                edx[1].isSignOf = true;
                edx[2].isSignOf = true;
                edx[3].isSignOf = true;
            }
        }
        break;

    case Token::T_MUL:
        handleMul<uint64_t, uint32_t>(op, type);
        break;

    case Token::T_DIV:
        handleIdiv<uint64_t, uint32_t, uint16_t>(op, type);
        break;

    case Token::T_IDIV:
        handleIdiv<int64_t, int32_t, int16_t>(op, type);
        break;

    case Token::T_NEG:
    case Token::T_NOT:
    case Token::T_INC:
    case Token::T_DEC:
        {
            auto val = getConstantValue(op);
            if (val) {
                switch (type) {
                case Token::T_NEG:
                    val = -static_cast<int>(*val);
                    break;
                case Token::T_NOT:
                    val = ~*val;
                    break;
                case Token::T_INC:
                    ++*val;
                    break;
                case Token::T_DEC:
                    --*val;
                    break;
                }
                assignConstant(op, *val);
            } else if (op.type == OperandInfo::kDynamicMem) {
                dynamicMemoryWrite();
            } else {
                assignExpression(op, type, {});
            }
        }
        break;

    default:
        assert(false);
    }
}

template<typename T64, typename T32>
void VmDataState::handleMul(const OperandInfo& op, Token::Type type)
{
    assert(op.type == OperandInfo::kReg || op.type == OperandInfo::kFixedMem || op.type == OperandInfo::kDynamicMem);

    auto size = op.type == OperandInfo::kReg ? op.base.size : op.memSize;

    switch (size) {
    case 2:
    case 4:
        {
            std::optional<T64> val1 = getConstantValue(op);
            std::optional<T32> val2 = getRegisterConstant(kEax);

            if (val1 && val2) {
                val1 = *val1 * *val2;
                assignConstant(op, static_cast<uint32_t>(*val1));
                assignConstantToRegister({ m_regs[kEdx], 0, size }, static_cast<uint32_t>(*val1 >> (size * 8)));
            } else {
                trashRegister({ m_regs[kEax], 0, size });
                trashRegister({ m_regs[kEdx], 0, size });
            }
        }
        break;
    case 1:
        {
            RegSlice al{ m_regs[kEax], 0, 1 };
            RegSlice ax{ m_regs[kEax], 0, 2 };

            std::optional<T32> val1 = getConstantValue(op);
            std::optional<T32> val2 = getRegisterConstant(al);

            if (val1 && val2) {
                assignConstantToRegister(ax, *val1 * *val2);
            } else {
                trashRegister(ax);
            }
        }
        break;
    default:
        assert(false);
    }
}

void VmDataState::handleImul(InstructionNode& node)
{
    assert(node.instruction->type() == Token::T_IMUL);

    switch (node.instruction->numOperands()) {
    case 1:
        {
            auto op = extractOperands(node).first;
            handleMul<int64_t, int32_t>(op, node.instruction->type());
        }
        break;
    case 2:
        {
            auto [dst, src] = extractOperands(node);
            assert(dst.type == OperandInfo::kReg);

            std::optional<int64_t> val1 = getConstantValue(dst);
            std::optional<int32_t> val2 = getConstantValue(src);
            if (val1 && val2) {
                assignConstantToRegister(getRegister(dst), static_cast<uint32_t>(*val1 * *val2));
            } else {
                trashLocation(dst);
            }
        }
        break;
    case 3:
        {
            auto [dst, op1, op2] = node.opInfo;
            assert(dst.type == OperandInfo::kReg && op2.type == OperandInfo::kImmediate);

            std::optional<int64_t> val1 = getConstantValue(op1);
            int32_t val2 = *op2.displacement.num;

            if (val1)
                assignConstantToRegister(getRegister(dst), static_cast<uint32_t>(*val1 * val2));
            else
                trashLocation(dst);
        }
    }
}

template<typename T64, typename T32, typename T16>
void VmDataState::handleIdiv(const OperandInfo& op, Token::Type type)
{
    assert(op.type == OperandInfo::kReg || op.type == OperandInfo::kFixedMem || op.type == OperandInfo::kDynamicMem);

    auto size = op.type == OperandInfo::kReg ? op.base.size : op.memSize;

    switch (size) {
    case 2:
    case 4:
        {
            RegSlice eax{ m_regs[kEax], 0, size };
            RegSlice edx{ m_regs[kEdx], 0, size };

            auto eaxVal = getRegisterConstant(eax);
            auto edxVal = getRegisterConstant(edx);
            std::optional<T32> divisor = getConstantValue(op);

            if (divisor && eaxVal && edxVal) {
                T64 dividend = (*edxVal << (8 * size)) | *eaxVal;
                auto quotient = dividend / *divisor;
                auto remainder = dividend % *divisor;
                assignConstantToRegister(eax, static_cast<uint32_t>(quotient));
                assignConstantToRegister(edx, static_cast<uint32_t>(remainder));
            } else {
                trashLocation(op);
                trashRegister(eax);
                trashRegister(edx);
            }
        }
        break;
    case 1:
        RegSlice ax{ m_regs[kEax], 0, 2 };
        std::optional<T32> dividend = getConstantValue(op);
        std::optional<T32> divisor = getRegisterConstant(ax);

        if (dividend && divisor) {
            dividend = static_cast<T16>(*dividend);
            auto quotient = *dividend / *divisor;
            auto remainder = *dividend % *divisor;
            assignConstantToRegister({ m_regs[kEax], 0, 1 }, quotient);
            assignConstantToRegister({ m_regs[kEax], 1, 1 }, remainder);
        } else {
            trashRegister(ax);
        }
    }
}

std::tuple<OperandInfo, OperandInfo, OperandInfo> VmDataState::extractAllOperands(const InstructionNode& node)
{
    decltype(node.opInfo) result;

    for (size_t i = 0; i < node.instruction->numOperands(); i++) {
        auto& op = result[i];
        op = node.opInfo[i];

        if (op.type == OperandInfo::kDynamicMem)
            if (auto addr = tryExtractingDynamicAddress(op))
                op.setAsFixedMem(*addr, op.memSize);
    }

    return { result[0], result[1], result[2] };
}

std::pair<OperandInfo, OperandInfo> VmDataState::extractOperands(const InstructionNode & node)
{
    auto [op1, op2, op3] = extractAllOperands(node);
    return { op1, op2 };
}

void VmDataState::compareAssign(InstructionNode& node, const OperandInfo& dst, const OperandInfo& src)
{
    assert(dst.type == OperandInfo::kReg || dst.type == OperandInfo::kFixedMem || dst.type == OperandInfo::kDynamicMem);
    assert(node.instruction->type() != Token::T_MOVSX && node.instruction->type() != Token::T_MOVZX || dst.type == OperandInfo::kReg);

    if (valuesEqual(dst, src, node.instruction->type())) {
        node.deleted = true;
    } else {
        bool signExtend = node.instruction->type() == Token::T_MOVSX;
        assignValue(dst, src, signExtend);
    }
}

bool VmDataState::valuesEqual(const OperandInfo& op1, const OperandInfo& op2, Token::Type instruction)
{
    assert(instruction == Token::T_MOV || instruction == Token::T_MOVZX || instruction == Token::T_MOVSX);

    if (!valuesEqual(op1, op2))
        return false;

    if (instruction == Token::T_MOVZX || instruction == Token::T_MOVSX) {
        assert(op1.isRegister() && op1.size() > op2.size() && !op2.isConst());

        if (op2.isDynamicMem())
            return false;

        bool signExtend = instruction == Token::T_MOVSX;
        return registerWideningEqual(getRegister(op1), op2.size(), signExtend);
    }

    return true;
}

bool VmDataState::valuesEqual(const OperandInfo& op1, const OperandInfo& op2)
{
    if (op1.isDynamicMem() || op2.isDynamicMem())
        return false;

    if (op1.isConst()) {
        if (op2.isConst())
            return op1.displacement.num == op2.displacement.num;

        assert(op2.size() == 1 || op2.size() == 2 || op2.size() == 4);

        if (op2.isRegister()) {
            return compareRegisterAndConstant(getRegister(op2), *op1.displacement.num);
        } else {
            assert(op2.isFixedMem());
            return compareMemoryAndConstant(op2.address(), op2.memSize, *op1.displacement.num);
        }
    } else if (op1.isRegister()) {
        if (op2.isConst()) {
            return compareRegisterAndConstant(getRegister(op1), *op2.displacement.num);
        } else if (op2.isRegister()) {
            return compareRegisters(getRegister(op1), getRegister(op2));
        } else {
            assert(op2.isFixedMem());
            assert(op1.size() && op1.size() >= op2.size());
            ensureMemoryValues(op2.address(), op2.memSize);
            return compareRegisterAndMemory(getRegister(op1), op2.address(), op2.memSize);
        }
    } else {
        assert(op1.isFixedMem());
        if (op2.isConst()) {
            return compareMemoryAndConstant(op1.address(), op1.memSize, *op2.displacement.num);
        } else if (op2.isRegister()) {
            assert(op1.size() && op1.size() == op2.size());
            ensureMemoryValues(op1.address(), op1.memSize);
            return compareRegisterAndMemory(getRegister(op2), op1.address(), op1.memSize);
        } else {
            assert(false);
        }
    }

    return false;
}

bool VmDataState::registerWideningEqual(const RegSlice& reg, size_t size, bool signExtend)
{
    assert(reg.size >= size && reg.size <= Instruction::kRegSize);

    if (reg.size > size) {
        auto cmpVal = ByteValue::zero();
        if (signExtend) {
            cmpVal = reg.val[reg.offset + size - 1];
            cmpVal.isSignOf = true;
        }

        for (size_t i = size; i < reg.size; i++)
            if (reg.val[reg.offset + i] != cmpVal)
                return false;
    }

    return true;
}

bool VmDataState::operandsEqual(const OperandInfo& op1, const OperandInfo& op2) const
{
    if (op1.type != op2.type)
        return false;

    switch (op1.type) {
    case OperandInfo::kImmediate:
        return op1.displacement.num == op2.displacement.num;

    case OperandInfo::kReg:
        return op1.base == op2.base;

    case OperandInfo::kFixedMem:
        return op1.displacement.num == op2.displacement.num && op1.memSize == op2.memSize;

    case OperandInfo::kDynamicMem:
        return op1 == op2;

    default:
        assert(false);
        return false;
    }
}

std::optional<uint32_t> VmDataState::getConstantValue(const OperandInfo& op) const
{
    switch (op.type) {
    case OperandInfo::kImmediate:
        return op.displacement.num;

    case OperandInfo::kReg:
        return getRegisterConstant(getRegister(op));

    case OperandInfo::kFixedMem:
        return getMemoryConstant(op.address(), op.memSize);

    default:
        assert(op.type == OperandInfo::kDynamicMem);
        return {};
    }
}

void VmDataState::assignValue(const OperandInfo& dst, const OperandInfo& src, bool signExtend /* = false */)
{
    switch (dst.type) {
    case OperandInfo::kReg:
        if (src.isConst()) {
            assignConstantToRegister(getRegister(dst), *src.displacement.num);
        } else if (src.isRegister()) {
            assert(dst.base.size >= src.base.size);
            assignRegisters(getRegister(dst), getRegister(src), signExtend);
        } else if (src.isFixedMem()) {
            assert(dst.base.size >= src.memSize);
            assignMemoryToRegister(getRegister(dst), src.address(), src.memSize, signExtend);
        } else if (src.isDynamicMem()) {
            assert(dst.isRegister());
            assignDynamicMemToRegister(dst, src);
        } else {
            assert(false);
        }
        break;

    case OperandInfo::kFixedMem:
        if (src.isConst()) {
            assignConstantToMemory(dst.address(), dst.memSize, *src.displacement.num);
        } else if (src.isRegister()) {
            assert(dst.memSize == src.base.size);
            assignRegisterToMemory(dst.address(), getRegister(src));
        } else {
            assert(false);
        }
        break;

    case OperandInfo::kDynamicMem:
        dynamicMemoryWrite();
        break;

    default:
        assert(false);
    }
}

void VmDataState::assignDynamicMemToRegister(const OperandInfo& dst, const OperandInfo& src)
{
    // make sure to assign the expression first or it might get mangled, e.g.
    // mov ax, [esi+eax]
    assignExpression(dst, Token::T_MOV, src);

    const auto& reg = getRegister(dst);
    resetRegisterToUnknown(getRegister(dst));

    // make sure only the last one remains
    for (size_t i = 0; i < reg.size; i++) {
        auto& val = reg.val[reg.offset + i];
        if (val.expression.size() > 1) {
            auto expr = val.expression.back();
            val.expression.clear();
            val.expression.push_back(expr);
        }
    }
}

void VmDataState::assignConstant(const OperandInfo& dst, uint32_t value)
{
    switch (dst.type) {
    case OperandInfo::kReg:
        assignConstantToRegister(getRegister(dst), value);
        break;

    case OperandInfo::kFixedMem:
        assignConstantToMemory(dst.address(), dst.memSize, value);
        break;

    case OperandInfo::kDynamicMem:
        dynamicMemoryWrite();
        break;

    default:
        assert(false);
    }
}

void VmDataState::assignExpression(const OperandInfo& dst, Token::Type type, const OperandInfo& op)
{
    assert(dst.type == OperandInfo::kReg || dst.type == OperandInfo::kFixedMem);

    const auto& expr = createExpression(type, op);
    auto it = m_expressions.find(expr);

    size_t expressionId;
    if (it == m_expressions.end()) {
        expressionId = ++m_expressionId;
        m_expressions[expr] = expressionId;
    } else {
        expressionId = it->second;
    }

    if (dst.type == OperandInfo::kReg)
        assignExpressionToRegister(getRegister(dst), expressionId);
    else
        assignExpressionToMemory(*dst.displacement.num, dst.memSize, expressionId);
}

auto VmDataState::createExpression(Token::Type type, const OperandInfo& op) const -> Expression
{
    Expression expr{ type, op };

    if (!op.base.empty())
        assignRegisters({ expr.base, op.base.offset, op.base.size }, getRegister(op));

    if (!op.scale.empty())
        assignRegisters({ expr.scale, op.scale.offset, op.scale.size }, { m_regs[op.scale.reg], op.scale.offset, op.scale.size });

    return expr;
}

void VmDataState::trashLocation(const OperandInfo& dst)
{
    switch (dst.type) {
    case OperandInfo::kReg:
        trashRegister(getRegister(dst));
        break;

    case OperandInfo::kFixedMem:
        trashMemory(dst.address(), dst.memSize);
        break;

    case OperandInfo::kDynamicMem:
        dynamicMemoryWrite();
        break;

    default:
        assert(false);
    }
}

bool VmDataState::compareRegisters(const RegSlice& reg1, const RegSlice& reg2)
{
    assert(reg1.offset < Instruction::kRegSize && reg2.offset < Instruction::kRegSize &&
        reg1.size <= Instruction::kRegSize && reg2.size <= Instruction::kRegSize && reg1.size >= reg2.size);
    assert(reg1.size && reg2.size && reg1.offset + reg1.size <= Instruction::kRegSize && reg2.offset + reg2.size <= Instruction::kRegSize);

    if (reg1 == reg2)
        return true;

    for (size_t i = 0; i < reg2.size; i++)
        if (reg1.val[reg1.offset + i] != reg2.val[reg2.offset + i])
            return false;

    return true;
}

void VmDataState::assignRegisters(const RegSlice& dst, const RegSlice& src, bool signExtend /* = false */)
{
    assert(dst.offset < Instruction::kRegSize && src.offset < Instruction::kRegSize &&
        dst.size <= Instruction::kRegSize && src.size <= Instruction::kRegSize && dst.size >= src.size);
    assert(dst.size && src.size && dst.offset + dst.size <= Instruction::kRegSize && src.offset + src.size <= Instruction::kRegSize);

    for (size_t i = 0; i < src.size; i++)
        dst.val[dst.offset + i] = src.val[src.offset + i];

    widenRegister(dst, src.size, signExtend);
}

void VmDataState::widenRegister(const RegSlice& reg, size_t size, bool signExtend)
{
    if (reg.size > size) {
        const auto& assignVal = signExtend ? reg.val[reg.offset + size - 1] : ByteValue::zero();

        for (size_t i = size; i < reg.size; i++) {
            reg.val[reg.offset + i] = assignVal;
            if (signExtend)
                reg.val[reg.offset + i].isSignOf = true;
        }
    }
}

void VmDataState::ensureMemoryValues(int addr, size_t size)
{
    assert(size < 10'000);

    while (size--) {
        if (m_mem.find(addr) == m_mem.end())
            m_mem[addr].resetAsMem(addr);
        addr++;
    }
}

bool VmDataState::compareRegisterAndMemory(const RegSlice& reg, size_t address, size_t size) const
{
    assert(reg.size <= Instruction::kRegSize && size <= reg.size);

    for (size_t i = 0; i < size; i++) {
        auto it = m_mem.find(address);
        assert(it != m_mem.end());

        if (reg.val[reg.offset + i] != it->second)
            return false;
    }

    return true;
}

void VmDataState::assignMemoryToRegister(const RegSlice& reg, size_t addr, size_t size, bool signExtend /* = false */)
{
    ensureMemoryValues(addr, reg.size);

    for (size_t i = 0; i < reg.size; i++) {
        assert(m_mem.find(addr) != m_mem.end());
        reg.val[reg.offset + i] = m_mem[addr++];
    }

    widenRegister(reg, size, signExtend);
}

void VmDataState::assignRegisterToMemory(size_t addr, const RegSlice& reg)
{
    for (size_t i = 0; i < reg.size; i++)
        m_mem[addr++] = reg.val[reg.offset + i];
}

bool VmDataState::compareRegisterAndValue(const RegSlice& reg, const ByteValue& val)
{
    for (size_t i = 0; i < reg.size; i++)
        if (reg.val[reg.offset + i] != val)
            return false;

    return true;
}

void VmDataState::assignValueToMemory(size_t address, size_t size, const ByteValue& val)
{
    while (size--)
        m_mem[address++] = val;
}

bool VmDataState::compareRegisterAndConstant(const RegSlice& reg, size_t val)
{
    for (size_t i = 0; i < reg.size; i++) {
        const auto& byte = reg.val[reg.offset + i];
        if (!byte.isConst() || byte.value != (val & 0xff))
            return false;

        val >>= 8;
    }

    return true;
}

bool VmDataState::compareMemoryAndConstant(size_t addr, size_t size, uint32_t val) const
{
    while (size--) {
        auto it = m_mem.find(addr);
        if (it == m_mem.end())
            return false;

        if (!it->second.isConst())
            return false;

        if (it->second.unsignedValue() != (val & 0xff))
            return false;

        val >>= 8;
    }
    return false;
}

std::optional<uint32_t> VmDataState::getRegisterConstant(const RegSlice& reg)
{
    uint32_t result = 0;

    // little endian
    for (size_t i = 0; i < reg.size; i++) {
        const auto& val = reg.val[reg.offset + i];
        if (!val.isConst())
            return {};

        result |= val.unsignedValue() << (i * 8);
    }

    return result;
}

std::optional<uint32_t> VmDataState::getRegisterConstant(RegisterEnum reg) const
{
    return getRegisterConstant({ m_regs[reg], 0, 4 });
}

std::optional<uint32_t> VmDataState::getMemoryConstant(size_t address, size_t size) const
{
    uint32_t val = 0;

    while (size--) {
        auto it = m_mem.find(address);
        if (it == m_mem.end() || !it->second.isConst())
            return {};

        val = (val << 8) | it->second.unsignedValue();
    }

    return val;
}

void VmDataState::assignConstantToRegister(const RegSlice& reg, uint32_t val)
{
    // little endian
    for (size_t i = 0; i < reg.size; i++) {
        reg.val[reg.offset + i].resetAsConst(val & 0xff);
        val >>= 8;
    }
}

void VmDataState::assignConstantToRegister(RegisterEnum reg, uint32_t val)
{
    assignConstantToRegister({ m_regs[reg], 0, 4 }, val);
}

void VmDataState::assignConstantToMemory(size_t address, size_t size, uint32_t val)
{
    while (size--) {
        m_mem[address++].value = val & 0xff;
        val >>= 8;
    }
}

void VmDataState::assignExpressionToRegister(const RegSlice& reg, size_t expressionId)
{
    assert(expressionId < 256);

    for (uint8_t i = 0; i < reg.size; i++)
        reg.val[reg.offset + i].expression.emplace_back(i, static_cast<uint8_t>(expressionId));
}

void VmDataState::assignExpressionToMemory(size_t addr, size_t size, size_t expressionId)
{
    assert(expressionId < 256);

    ensureMemoryValues(addr, size);

    for (uint8_t i = 0; i < size; i++)
        m_mem[addr++].expression.emplace_back(i, static_cast<uint8_t>(expressionId));
}

auto VmDataState::getRegister(const OperandInfo& op) const -> const RegSlice
{
    return { m_regs[op.base.reg], op.base.offset, op.base.size };
}

auto VmDataState::getRegister(const OperandInfo& op) -> RegSlice
{
    return { m_regs[op.base.reg], op.base.offset, op.base.size };
}

std::optional<int> VmDataState::tryExtractingDynamicAddress(const OperandInfo& op) const
{
    assert(!op.base.empty());

    auto addr = getRegisterConstant(op.base.reg);
    if (!addr)
        return addr;

    if (!op.scale.empty()) {
        auto scale = getRegisterConstant(op.scale.reg);
        if (!scale)
            return scale;

        if (op.gotScaleFactor()) {
            assert(op.scaleFactor == 1 || op.scaleFactor == 2 || op.scaleFactor == 4 || op.scaleFactor == 8);
            scale = *scale * op.scaleFactor;
        }

        addr = *addr + *scale;
    }

    if (op.displacement.num)
        addr = *addr + *op.displacement.num;

    return addr;
}

void VmDataState::trashRegister(const RegSlice& reg)
{
    assert(reg.val >= m_regs[0].data() && reg.val <= m_regs[kNumRegisters - 1].data() &&
        reg.offset < Instruction::kRegSize && reg.offset + reg.size <= Instruction::kRegSize);

    auto regIndex = static_cast<RegisterEnum>((reinterpret_cast<char *>(reg.val) -
        reinterpret_cast<char *>(&m_regs[0])) / sizeof(m_regs[0]));

    for (size_t i = 0; i < reg.size; i++) {
        auto& val = reg.val[reg.offset + i];
        val.resetAsRegister(regIndex, reg.offset + i, m_color);
    }

    m_color++;
}

void VmDataState::trashRegister(RegisterEnum reg)
{
    trashRegister({ m_regs[reg], 0, 4 });
}

void VmDataState::resetRegisterToUnknown(const RegSlice& reg)
{
    for (size_t i = 0; i < reg.size; i++) {
        auto& val = reg.val[reg.offset + i];
        val.resetAsRegister(kNoReg, reg.offset + i, -1, true);
    }
}

void VmDataState::trashMemory(size_t address, size_t size)
{
    while (size--)
        m_mem[address].resetAsMem(address++, m_color);

    m_color++;
}

void VmDataState::dynamicMemoryWrite()
{
    // do nothing for now, test destroying register/memory state
}
