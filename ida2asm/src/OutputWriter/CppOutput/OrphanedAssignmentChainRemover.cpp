#include "InstructionNode.h"
#include "OrphanedAssignmentChainRemover.h"

// Tracks contents of each byte of registers and known memory locations. Uses reference counting on
// instructions and creates chains of assignments and operations on them, so that the whole connected
// sequences of associated instructions could be removed when detected as unnecessary.

OrphanedAssignmentChainRemover::OrphanedAssignmentChainRemover()
{
    m_nodeLinks.reserve(512);
    m_nodes.reserve(128);
}

void OrphanedAssignmentChainRemover::reset()
{
    for (auto& reg : m_regs)
        reg.fill(-1);

    m_mem.clear();
    m_nodes.clear();
    m_nodeLinks.clear();
}

void OrphanedAssignmentChainRemover::startNewProc()
{
    reset();
}

void OrphanedAssignmentChainRemover::processInstruction(InstructionNode& node)
{
    if (node.deleted)
        return;

    unlinkMemoryReferences(node);

    const auto& dst = node.opInfo[0];
    const auto& src = node.opInfo[1];

    switch (node.instruction->type()) {
    case Token::T_NOP:
    case Token::T_STC:
    case Token::T_CLC:
    case Token::T_CLD:
    case Token::T_CLI:
    case Token::T_STI:
    case Token::T_RETN:
    case Token::T_PUSHF:
    case Token::T_POPF:
    case Token::T_PUSHA:
        // ignore
        break;

    case Token::T_MOV:
    case Token::T_MOVZX:
    case Token::T_MOVSX:
        if (dst == src) {
            node.deleted = true;
        } else {
            decreaseDestinationReferenceCount(node);
            unlinkDestinationChain(node);
            appendSourceNodesToDestination(node);
            addDestinationLink(node);
        }
        break;

    case Token::T_XCHG:
        handleXchg(node);
        break;

    case Token::T_IMUL:
        if (node.instruction->numOperands() == 1) {
            handleMul(node);
            break;
        } else if (node.instruction->numOperands() == 3) {
            handleImulThreeOperands(node);
            break;
        }
        [[fallthrough]];

    case Token::T_OR:
    case Token::T_XOR:
    case Token::T_AND:
    case Token::T_SUB:
    case Token::T_ADD:
        if (dst != src) {
            appendSourceNodesToDestination(node);
        } else if (node.instruction->type() == Token::T_XOR || node.instruction->type() == Token::T_SUB) {
            decreaseDestinationReferenceCount(node);
            unlinkDestinationChain(node);
        }
        addDestinationLink(node);
        break;

    case Token::T_SHL:
    case Token::T_SHR:
    case Token::T_SAR:
    case Token::T_ROL:
    case Token::T_ROR:
        handleShiftRotate(node);
        break;

    case Token::T_MOVSB:
    case Token::T_MOVSW:
    case Token::T_MOVSD:
        unlinkChain(kEsi);
        unlinkChain(kEdi);
        if (node.instruction->prefix() == "rep")
            unlinkChain(kEcx);
        break;

    case Token::T_LODSB:
    case Token::T_LODSW:
    case Token::T_LODSD:
        {
            auto size = blockInstructionSize(node.instruction->type());
            decreaseReferenceCount(kEax, 0, size);
            unlinkChain(kEax, 0, size);
            unlinkChain(kEsi);
            if (node.instruction->prefix() == "rep")
                unlinkChain(kEcx);
        }
        break;

    case Token::T_STOSB:
    case Token::T_STOSW:
    case Token::T_STOSD:
        unlinkChain(kEax, 0, blockInstructionSize(node.instruction->type()));
        unlinkChain(kEdi);
        if (node.instruction->prefix() == "rep")
            unlinkChain(kEcx);
        break;

    case Token::T_SETZ:
        decreaseDestinationReferenceCount(node);
        unlinkDestinationChain(node);
        [[fallthrough]];

    case Token::T_INC:
    case Token::T_DEC:
    case Token::T_NEG:
    case Token::T_NOT:
        addDestinationLink(node);
        break;

    case Token::T_POP:
        decreaseDestinationReferenceCount(node);
        unlinkDestinationChain(node);
        break;

    case Token::T_CBW:
    case Token::T_CWD:
    case Token::T_CWDE:
    case Token::T_CDQ:
        handleRegisterWidening(node);
        break;

    case Token::T_DIV:
    case Token::T_IDIV:
        handleDiv(node);
        break;

    case Token::T_MUL:
        handleMul(node);
        break;

    case Token::T_CMP:
    case Token::T_TEST:
    case Token::T_PUSH:
    case Token::T_RCL:
    case Token::T_RCR:
        unlinkChains(node);
        break;

    case Token::T_INT:
    case Token::T_CALL:
    case Token::T_JMP:
    case Token::T_JG:
    case Token::T_JGE:
    case Token::T_JL:
    case Token::T_JZ:
    case Token::T_JNZ:
    case Token::T_JS:
    case Token::T_JNS:
    case Token::T_JA:
    case Token::T_JB:
    case Token::T_JNB:
    case Token::T_JBE:
    case Token::T_JO:
    case Token::T_JNO:
    case Token::T_JLE:
    case Token::T_LOOP:
        reset();
        break;

    case Token::T_IN:
    case Token::T_OUT:
        // this should've been removed already
        assert(false);
        node.deleted = true;
        break;

    case Token::T_POPA:
        for (auto reg : { kEax, kEcx, kEdx, kEbx, kEsp, kEbp, kEsi, kEdi }) {
            decreaseReferenceCount(reg);
            unlinkChain(reg);
        }
        break;

    default:
        assert(false);
    }
}

void OrphanedAssignmentChainRemover::unlinkMemoryReferences(const InstructionNode& node)
{
    for (size_t i = 0; i < node.instruction->numOperands(); i++) {
        const auto& op = node.opInfo[i];
        if (op.isDynamicMem()) {
            if (!op.base.empty())
                unlinkChain(op.base.reg, op.base.offset, op.base.size);
            if (!op.scale.empty())
                unlinkChain(op.scale.reg, op.scale.offset, op.scale.size);
        }
    }

    if (node.instruction->numOperands() >= 2 && node.opInfo[0].isDynamicMem() && node.opInfo[1].isRegister())
        unlinkChain(node.opInfo[1]);
}

void OrphanedAssignmentChainRemover::handleXchg(InstructionNode& node)
{
    assert(node.instruction->type() == Token::T_XCHG);

    const auto& dst = node.opInfo[0];
    const auto& src = node.opInfo[1];

    assert(dst.size() == src.size());

    auto dstNodes = getOperandNodes(dst);
    auto srcNodes = getOperandNodes(src);

    size_t i;
    for (i = 0; i < dst.size(); i++) {
        if (dstNodes[i] < 0 || srcNodes[i] < 0)
            break;

        const auto& dstNode = m_nodeLinks[dstNodes[i]];
        const auto& srcNode = m_nodeLinks[srcNodes[i]];

        const auto& nodeRef = m_nodes[dstNode.nodeRef];

        if (nodeRef.node->instruction->type() != Token::T_XCHG)
            break;
        if (nodeRef.node->opInfo[0].isDynamicMem() || nodeRef.node->opInfo[1].isDynamicMem())
            break;
        if (dstNode.nodeRef != srcNode.nodeRef || nodeRef.node->deleted || nodeRef.refCount > 2 * static_cast<int>(dst.size()))
            break;
    }

    if (i > 0) {
        auto nodeRef = m_nodeLinks[dstNodes[0]].nodeRef;
        m_nodes[nodeRef].node->deleted = true;

        if (i == dst.size())
            node.deleted = true;
    } else {
        swapRoots(node);
        addLinks(node);
    }
}

void OrphanedAssignmentChainRemover::handleRegisterWidening(InstructionNode& node)
{
    auto type = node.instruction->type();

    assert(type == Token::T_CBW || type == Token::T_CWD || type == Token::T_CWDE || type == Token::T_CDQ);

    switch (type) {
    case Token::T_CBW:
        decreaseReferenceCount(m_regs[kEax][1]);
        unlinkChain(m_regs[kEax][1]);

        appendRegisterNodes(m_regs[kEax], 1, m_regs[kEax], 0, 1);
        addLink(kEax, 1, 2, node);
        break;
    case Token::T_CWDE:
        decreaseReferenceCount(kEax, 2, 2);
        unlinkChain(kEax, 2, 2);

        appendNodes(m_regs[kEax][2], m_regs[kEax][0]);
        appendNodes(m_regs[kEax][2], m_regs[kEax][1]);
        appendNodes(m_regs[kEax][3], m_regs[kEax][0]);
        appendNodes(m_regs[kEax][3], m_regs[kEax][1]);

        addLink(kEax, 2, 2, node);
        break;
    case Token::T_CWD:
        decreaseReferenceCount(kEdx, 0, 2);
        unlinkChain(kEdx, 0, 2);

        appendNodes(m_regs[kEdx][0], m_regs[kEax][0]);
        appendNodes(m_regs[kEdx][0], m_regs[kEax][1]);
        appendNodes(m_regs[kEdx][1], m_regs[kEax][0]);
        appendNodes(m_regs[kEdx][1], m_regs[kEax][1]);

        addLink(kEdx, 0, 2, node);
        break;
    case Token::T_CDQ:
        decreaseReferenceCount(kEdx);
        unlinkChain(kEdx);

        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                appendNodes(m_regs[kEdx][i], m_regs[kEax][j]);

        addLink(kEdx, 0, 4, node);
        break;
    }
}

void OrphanedAssignmentChainRemover::handleMul(InstructionNode& node)
{
    assert((node.instruction->type() == Token::T_MUL || node.instruction->type() == Token::T_IMUL) &&
        node.instruction->numOperands() == 1);

    const auto& src = node.opInfo[0];
    assert(src.type != OperandInfo::kImmediate);

    bool gotSrcRef = src.type == OperandInfo::kReg && src.base.reg != kEax || src.type == OperandInfo::kFixedMem;

    if (src.size() == 1) {
        decreaseReferenceCount(kEax, 1, 1);
        unlinkChain(kEax, 1, 1);
        addLink(kEax, 0, 2, node);
        appendNodes(m_regs[kEax][1], m_regs[kEax][0]);
        if (gotSrcRef) {
            auto srcReg = getOperandNodes(src);
            appendNodes(m_regs[kEax][0], srcReg[0]);
            appendNodes(m_regs[kEax][1], srcReg[0]);
        }
    } else {
        decreaseReferenceCount(kEdx, 0, src.size());
        unlinkChain(kEdx, 0, src.size());

        addLink(kEdx, 0, src.size(), node);
        addLink(kEax, 0, src.size(), node);

        for (size_t i = 0; i < src.size(); i++)
            for (size_t j = 0; j < src.size(); j++)
                appendNodes(m_regs[kEdx][j], m_regs[kEax][i]);

        if (gotSrcRef) {
            auto srcReg = getOperandNodes(src);
            for (size_t i = 0; i < src.size(); i++) {
                for (size_t j = 0; j < src.size(); j++) {
                    appendNodes(m_regs[kEdx][j], srcReg[i]);
                    appendNodes(m_regs[kEax][j], srcReg[i]);
                }
            }
        }
    }
}

void OrphanedAssignmentChainRemover::handleImulThreeOperands(InstructionNode& node)
{
    assert(node.instruction->type() == Token::T_IMUL && node.instruction->numOperands() == 3);

    const auto& dst = node.opInfo[0];
    const auto& src = node.opInfo[1];

    assert(src.type != OperandInfo::kImmediate && dst.type != OperandInfo::kImmediate && node.opInfo[2].type == OperandInfo::kImmediate);
    assert(src.size() == dst.size());

    decreaseDestinationReferenceCount(node);
    unlinkChain(dst);

    auto dstReg = getOperandNodes(dst);
    auto srcReg = getOperandNodes(src);

    if (src != dst) {
        for (size_t i = 0; i < src.size(); i++)
            for (size_t j = 0; j < dst.size(); j++)
                appendNodes(dstReg[i], srcReg[j]);
    }

    addLink(dst, node);
}

void OrphanedAssignmentChainRemover::handleDiv(InstructionNode& node)
{
    assert((node.instruction->type() == Token::T_DIV || node.instruction->type() == Token::T_IDIV) &&
        node.instruction->numOperands() == 1);

    const auto& src = node.opInfo[0];
    bool gotSrcRef = src.type == OperandInfo::kReg && src.base.reg != kEax || src.type == OperandInfo::kFixedMem;

    if (src.size() == 1) {
        appendNodes(m_regs[kEax][1], m_regs[kEax][0]);
        appendNodes(m_regs[kEax][0], m_regs[kEax][1]);
        addLink(kEax, 0, 1, node);
        addLink(kEax, 1, 1, node);
        if (gotSrcRef) {
            auto srcReg = getOperandNodes(src);
            appendNodes(m_regs[kEax][0], srcReg[0]);
            appendNodes(m_regs[kEax][1], srcReg[0]);
        }
    } else {
        for (size_t i = 0; i < src.size(); i++) {
            for (size_t j = 0; j < src.size(); j++) {
                appendNodes(m_regs[kEdx][j], m_regs[kEax][i]);
                appendNodes(m_regs[kEax][j], m_regs[kEdx][i]);
            }
        }

        if (gotSrcRef) {
            auto srcReg = getOperandNodes(src);
            for (size_t i = 0; i < src.size(); i++) {
                for (size_t j = 0; j < src.size(); j++) {
                    appendNodes(m_regs[kEdx][j], srcReg[i]);
                    appendNodes(m_regs[kEax][j], srcReg[i]);
                }
            }
        }

        for (size_t i = 0; i < src.size(); i++) {
            addLink(kEdx, i, 1, node);
            addLink(kEax, i, 1, node);
        }
    }
}

void OrphanedAssignmentChainRemover::handleShiftRotate(InstructionNode& node)
{
    assert(node.instruction && (node.instruction->type() == Token::T_SHL || node.instruction->type() == Token::T_SHR ||
        node.instruction->type() == Token::T_SAR || node.instruction->type() == Token::T_ROL ||
        node.instruction->type() == Token::T_ROR));

    const auto& dst = node.opInfo[0];
    const auto& src = node.opInfo[1];

    assert(dst.type != OperandInfo::kImmediate);

    if (src.type != OperandInfo::kImmediate || src.constValue() % 8) {
        appendSourceNodesToDestination(node);
        addDestinationLink(node);
        return;
    }

    auto count = src.constValue() / 8 % dst.size();
    auto size = dst.size();

    switch (node.instruction->type()) {
    case Token::T_SHR:
    case Token::T_SAR:
        if (dst.type == OperandInfo::kReg) {
            decreaseReferenceCount(dst.base.reg, dst.base.offset, count);
            memmove(&m_regs[dst.base.reg][dst.base.offset], &m_regs[dst.base.reg][dst.base.offset + count], (size - count) * kRegNodeSize);
            unlinkChain(dst.base.reg, dst.base.offset + size - count, count);
        } else if (dst.type == OperandInfo::kFixedMem) {
            decreaseReferenceCountMem(dst.address(), count);
            for (auto i = size - count; i != 0; i--)
                m_mem[dst.address() + i] = m_mem[dst.address() + count + i];
            for (size_t i = 0; i < count; i++)
                unlinkChain(m_mem[dst.address() + size - i - 1]);
        }
        break;
    case Token::T_SHL:
        if (dst.type == OperandInfo::kReg) {
            decreaseReferenceCount(dst.base.reg, dst.base.offset + size - count, count);
            memmove(&m_regs[dst.base.reg][dst.base.offset + count], &m_regs[dst.base.reg][dst.base.offset], (size - count) * kRegNodeSize);
            unlinkChain(dst.base.reg, dst.base.offset, count);
        } else if (dst.type == OperandInfo::kFixedMem) {
            decreaseReferenceCountMem(dst.address() + size - count, count);
            for (auto i = 0; i < count; i--)
                m_mem[dst.address() + count + i] = m_mem[dst.address() + i];
            for (size_t i = 0; i < count; i++)
                unlinkChain(m_mem[dst.address() + i]);
        }
        break;
    case Token::T_ROR:
        if (dst.type == OperandInfo::kReg) {
            while (count--)
                for (size_t i = 0; i < size - 1; i--)
                    std::swap(m_regs[dst.base.reg][dst.base.offset + i], m_regs[dst.base.reg][dst.base.offset + i + 1]);
        } else if (dst.type == OperandInfo::kFixedMem) {
            while (count--)
                for (size_t i = 0; i < size - 1; i--)
                    std::swap(m_mem[dst.address() + i], m_mem[dst.address() + i + 1]);
        }
        break;
    case Token::T_ROL:
        if (dst.type == OperandInfo::kReg) {
            while (count--)
                for (size_t i = size - 1; i != 0; i--)
                    std::swap(m_regs[dst.base.reg][dst.base.offset + i], m_regs[dst.base.reg][dst.base.offset + i - 1]);
        } else if (dst.type == OperandInfo::kFixedMem) {
            while (count--)
                for (size_t i = size - 1; i != 0; i--)
                    std::swap(m_mem[dst.address() + i], m_mem[dst.address() + i - 1]);
        }
        break;
    }
}

void OrphanedAssignmentChainRemover::addDestinationLink(InstructionNode& node)
{
    assert(node.instruction->numOperands() > 0);

    const auto& dst = node.opInfo[0];
    addLink(dst, node);
}

void OrphanedAssignmentChainRemover::addLinks(InstructionNode& node)
{
    assert(node.instruction->numOperands() > 1);

    const auto& dst = node.opInfo[0];
    const auto& src = node.opInfo[1];

    addLink(dst, node);
    addLink(src, node);
}

void OrphanedAssignmentChainRemover::addLink(const OperandInfo& op, InstructionNode& node)
{
    switch (op.type) {
    case OperandInfo::kReg:
        addLink(op.base, node);
        break;

    case OperandInfo::kFixedMem:
        addLink(op.address(), op.memSize, node);
        break;
    }
}

void OrphanedAssignmentChainRemover::addLink(const OperandInfo::Component& reg, InstructionNode& node)
{
    addLink(reg.reg, reg.offset, reg.size, node);
}

void OrphanedAssignmentChainRemover::addLink(RegisterEnum reg, size_t offset, size_t size, InstructionNode& node)
{
    addLink(size, node, [this, reg, offset](size_t i) {
        return std::ref(m_regs[reg][offset + i]);
    });
}

void OrphanedAssignmentChainRemover::addLink(size_t address, size_t size, InstructionNode& node)
{
    addLink(size, node, [this, address](size_t i) {
        return std::ref(m_mem.try_emplace(address + i, -1).first->second);
    });
}

inline void OrphanedAssignmentChainRemover::addLink(size_t size, InstructionNode& node, std::function<int&(size_t)> getRoot)
{
    for (size_t i = 0; i < size; i++) {
        auto& root = getRoot(i);
        root = addLink(node, root);
    }
}

void OrphanedAssignmentChainRemover::swapRoots(InstructionNode& node)
{
    assert(node.instruction->numOperands() > 1);

    const auto *dst = &node.opInfo[0];
    const auto *src = &node.opInfo[1];

    assert(dst->size() == src->size());

    if (*dst == *src) {
        node.deleted = true;
        return;
    }

    if (dst->type == OperandInfo::kReg && src->type == OperandInfo::kReg) {
        for (size_t i = 0; i < dst->size(); i++)
            std::swap(m_regs[dst->base.reg][dst->base.offset + i], m_regs[src->base.reg][src->base.offset + i]);
    } else if (dst->type == OperandInfo::kReg && src->type == OperandInfo::kFixedMem ||
        dst->type == OperandInfo::kFixedMem && src->type == OperandInfo::kReg) {
        if (dst->type == OperandInfo::kFixedMem && src->type == OperandInfo::kReg)
            std::swap(dst, src);

        assert(dst->base.size == src->memSize);

        for (size_t i = 0; i < dst->size(); i++) {
            assert(m_mem.find(src->address() + i) != m_mem.end());
            std::swap(m_regs[dst->base.reg][dst->base.offset + i], m_mem[src->address() + i]);
        }
    }
}

void OrphanedAssignmentChainRemover::increaseReferenceCountMem(size_t address, size_t size, int count /* = 1 */)
{
    assert(size <= Instruction::kRegSize);

    for (size_t i = 0; i < size; i++) {
        auto it = m_mem.find(address + i);
        if (it != m_mem.end())
            increaseReferenceCount(it->second, count);
    }
}

void OrphanedAssignmentChainRemover::increaseReferenceCount(int nodeLinkIndex, int count /* = 1 */)
{
    while (nodeLinkIndex >= 0) {
        auto& nodeLink = m_nodeLinks[nodeLinkIndex];

        assert(nodeLink.nodeRef >= 0);
        auto& nodeRef = m_nodes[nodeLink.nodeRef];

        if (!nodeRef.node->deleted) {
            nodeRef.refCount += count;
            assert(nodeRef.refCount >= 0);

            if (!nodeRef.refCount)
                nodeRef.node->deleted = true;
        }

        nodeLinkIndex = nodeLink.next;
    }
}

void OrphanedAssignmentChainRemover::appendSourceNodesToDestination(const InstructionNode& node)
{
    assert(node.instruction->numOperands() > 1);

    const auto& dst = node.opInfo[0];
    const auto& src = node.opInfo[1];

    assert([&]() {
        auto isMismatchOperandSizeInstruction = [](Token::Type type) {
            return type == Token::T_MOVZX || type == Token::T_MOVSX || type == Token::T_SHL ||
                type == Token::T_RCR || type == Token::T_ROL || type == Token::T_SAR;
        };
        auto type = node.instruction->type();
        return src.type != OperandInfo::kReg && src.type != OperandInfo::kFixedMem ||
            isMismatchOperandSizeInstruction(type) && dst.size() > src.size() ||
            dst.size() == src.size();
    }());

    Register srcReg;
    size_t size;

    switch (src.type) {
    case OperandInfo::kReg:
        size = src.base.size;
        memcpy(srcReg.data(), &m_regs[src.base.reg][src.base.offset], size * kRegNodeSize);
        break;

    case OperandInfo::kFixedMem:
        srcReg = memoryToRegister(src.address(), src.memSize);
        size = src.memSize;
        break;

    default:
        return;
    }

    switch (dst.type) {
    case OperandInfo::kReg:
        appendRegisterNodes(m_regs[dst.base.reg], dst.base.offset, srcReg, 0, size);
        if (dst.base.size > size)
            appendRegisterNodes(m_regs[dst.base.reg], dst.base.offset + size, srcReg, 0, size);
        break;

    case OperandInfo::kFixedMem:
        for (size_t i = 0; i < dst.memSize; i++) {
            auto address = dst.address() + i;
            auto it = m_mem.try_emplace(address, -1).first;
            appendNodes(it->second, srcReg[i]);
        }
    }
}

void OrphanedAssignmentChainRemover::unlinkChains(const InstructionNode& node)
{
    assert(node.instruction->numOperands() > 0);

    for (size_t i = 0; i < node.instruction->numOperands(); i++) {
        const auto& op = node.opInfo[i];
        unlinkChain(op);
    }
}

void OrphanedAssignmentChainRemover::unlinkDestinationChain(const InstructionNode& node)
{
    assert(node.instruction->numOperands() > 0);

    const auto& dst = node.opInfo[0];
    unlinkChain(dst);
}

void OrphanedAssignmentChainRemover::unlinkChain(const OperandInfo& op)
{
    switch (op.type) {
    case OperandInfo::kReg:
        for (size_t i = 0; i < op.base.size; i++)
            unlinkChain(m_regs[op.base.reg][op.base.offset + i]);
        break;

    case OperandInfo::kFixedMem:
        for (size_t i = 0; i < op.memSize; i++)
            unlinkChain(m_mem[op.address() + i]);
        break;
    }
}

void OrphanedAssignmentChainRemover::unlinkChain(RegisterEnum reg)
{
    m_regs[reg].fill(-1);
}

void OrphanedAssignmentChainRemover::unlinkChain(RegisterEnum reg, size_t offset, size_t size)
{
    for (size_t i = 0; i < size; i++)
        m_regs[reg][offset + i] = -1;
}

void OrphanedAssignmentChainRemover::unlinkChain(int& root)
{
    root = -1;
}

void OrphanedAssignmentChainRemover::decreaseDestinationReferenceCount(InstructionNode& node)
{
    assert(node.instruction->numOperands() > 0);

    const auto& dst = node.opInfo[0];

    switch (dst.type) {
    case OperandInfo::kReg:
        decreaseReferenceCount(dst.base);
        break;
    case OperandInfo::kFixedMem:
        decreaseReferenceCountMem(dst.address(), dst.memSize);
        break;
    }
}

void OrphanedAssignmentChainRemover::decreaseReferenceCount(const OperandInfo::Component& reg)
{
    for (size_t i = 0; i < reg.size; i++)
        decreaseReferenceCount(m_regs[reg.reg][reg.offset + i]);
}

void OrphanedAssignmentChainRemover::decreaseReferenceCount(RegisterEnum reg)
{
    decreaseReferenceCount(reg, 0, Instruction::kRegSize);
}

void OrphanedAssignmentChainRemover::decreaseReferenceCount(RegisterEnum reg, size_t offset, size_t size)
{
    for (size_t i = 0; i < size; i++)
        decreaseReferenceCount(m_regs[reg][offset + i]);
}

void OrphanedAssignmentChainRemover::decreaseReferenceCountMem(size_t address, int size, int count /* = 1 */)
{
    increaseReferenceCountMem(address, size, -count);
}

void OrphanedAssignmentChainRemover::decreaseReferenceCount(int nodeLinkIndex, int count /* = 1 */)
{
    increaseReferenceCount(nodeLinkIndex, -count);
}

int OrphanedAssignmentChainRemover::addLink(InstructionNode& node, int next /* = -1 */)
{
    int nodeRef = getNodeRef(node);
    m_nodeLinks.emplace_back(nodeRef, next);
    return m_nodeLinks.size() - 1;
}

int OrphanedAssignmentChainRemover::addLink(int nodeRef, int next /* = -1 */)
{
    m_nodeLinks.emplace_back(nodeRef, next);
    m_nodes[nodeRef].refCount++;
    return m_nodeLinks.size() - 1;
}

int OrphanedAssignmentChainRemover::getNodeRef(InstructionNode& node)
{
    auto it = std::find(m_nodes.begin(), m_nodes.end(), &node);

    if (it == m_nodes.end()) {
        m_nodes.emplace_back(&node);
        return m_nodes.size() - 1;
    } else {
        it->refCount++;
        return it - m_nodes.begin();
    }
}

auto OrphanedAssignmentChainRemover::getOperandNodes(const OperandInfo& op) const -> Register
{
    assert(op.type == OperandInfo::kReg || op.type == OperandInfo::kFixedMem);

    Register result;

    switch (op.type) {
    case OperandInfo::kReg:
        memcpy(result.data(), &m_regs[op.base.reg][op.base.offset], op.base.size * kRegNodeSize);
        break;

    case OperandInfo::kFixedMem:
        result = memoryToRegister(op.address(), op.memSize);
        break;
    }

    return result;
}

void OrphanedAssignmentChainRemover::appendRegisterNodes(RegisterEnum reg, size_t offset, size_t size, const Register& src)
{
    appendRegisterNodes(m_regs[reg], offset, src, 0, size);
}

void OrphanedAssignmentChainRemover::appendRegisterNodes(Register& dstReg, size_t dstOffset,
    const Register& srcReg, size_t srcOffset, size_t size)
{
    assert(size == 1 || size == 2 || size == 4);
    assert(dstOffset + size <= Instruction::kRegSize && srcOffset + size <= Instruction::kRegSize);

    for (size_t i = 0; i < size; i++) {
        auto& dst = dstReg[dstOffset + i];
        auto src = srcReg[srcOffset + i];

        appendNodes(dst, src);
    }
}

void OrphanedAssignmentChainRemover::appendNodes(int& dst, int src)
{
    while (src >= 0) {
        auto srcLink = m_nodeLinks[src];
        dst = addLink(srcLink.nodeRef, dst);
        src = srcLink.next;
    }
}

auto OrphanedAssignmentChainRemover::memoryToRegister(size_t address, size_t size) const -> Register
{
    Register reg;

    for (size_t i = 0; i < size; i++) {
        auto nodeLinkIndex = -1;

        auto it = m_mem.find(address);
        if (it != m_mem.end())
            nodeLinkIndex = it->second;

        reg[i] = nodeLinkIndex;
    }

    return reg;
}

size_t OrphanedAssignmentChainRemover::blockInstructionSize(Token::Type type)
{
    switch (type) {
    case Token::T_LODSB: return 1;
    case Token::T_LODSW: return 2;
    case Token::T_LODSD: return 4;
    case Token::T_STOSB: return 1;
    case Token::T_STOSW: return 2;
    case Token::T_STOSD: return 4;
    default:
        assert(false);
        return 0;
    }
}
