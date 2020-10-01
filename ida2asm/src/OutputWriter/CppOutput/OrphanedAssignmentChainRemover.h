#pragma once

#include "RegisterEnum.h"
#include "Instruction.h"

struct InstructionNode;

class OrphanedAssignmentChainRemover
{
public:
    OrphanedAssignmentChainRemover();
    void reset();
    void startNewProc();
    void processInstruction(InstructionNode& node);

private:
    struct NodeReference
    {
        NodeReference(InstructionNode *node) : node(node) {}
        InstructionNode *node;
        int refCount = 1;

        bool operator==(const NodeReference& other) const { return node == other.node; }
    };

    struct NodeLink
    {
        NodeLink(int nodeRef, int next = -1) : nodeRef(nodeRef), next(next) {}
        int nodeRef;
        int next = -1;
    };

    void unlinkMemoryReferences(const InstructionNode& node);
    void handleXchg(InstructionNode& node);
    void handleRegisterWidening(InstructionNode& node);
    void handleMul(InstructionNode& node);
    void handleImulThreeOperands(InstructionNode& node);
    void handleDiv(InstructionNode& node);
    void handleShiftRotate(InstructionNode& node);
    void addDestinationLink(InstructionNode& node);
    void addLinks(InstructionNode& node);
    void addLink(const OperandInfo& op, InstructionNode& node);
    void addLink(const OperandInfo::Component& reg, InstructionNode& node);
    void addLink(RegisterEnum reg, size_t offset, size_t size, InstructionNode& node);
    void addLink(size_t address, size_t size, InstructionNode& node);
    void addLink(size_t size, InstructionNode& node, std::function<int&(size_t)> getRoot);
    void swapRoots(InstructionNode& node);
    void increaseReferenceCountMem(size_t address, size_t size, int nodeIndex = 1);
    void increaseReferenceCount(int nodeLinkIndex, int count = 1);
    void appendSourceNodesToDestination(const InstructionNode& node);
    void unlinkChains(const InstructionNode& node);
    void unlinkDestinationChain(const InstructionNode& node);
    void unlinkChain(const OperandInfo& op);
    void unlinkChain(RegisterEnum reg);
    void unlinkChain(RegisterEnum reg, size_t offset, size_t size);
    void unlinkChain(int& root);
    void decreaseDestinationReferenceCount(InstructionNode& node);
    void decreaseReferenceCount(const OperandInfo::Component& reg);
    void decreaseReferenceCount(RegisterEnum reg);
    void decreaseReferenceCount(RegisterEnum reg, size_t offset, size_t size);
    void decreaseReferenceCountMem(size_t address, int size, int count = 1);
    void decreaseReferenceCount(int nodeLinkIndex, int count = 1);

    std::vector<NodeLink> m_nodeLinks;
    std::vector<NodeReference> m_nodes;

    using Register = std::array<int, Instruction::kRegSize>;
    std::array<Register, kNumRegisters> m_regs;
    std::unordered_map<size_t, int> m_mem;
    static constexpr int kRegNodeSize = sizeof(m_regs[0][0]);


    int addLink(InstructionNode& node, int next = -1);
    int addLink(int nodeRef, int next = -1);
    int getNodeRef(InstructionNode& node);
    Register getOperandNodes(const OperandInfo& op) const;
    void appendRegisterNodes(RegisterEnum reg, size_t offset, size_t size, const Register& src);
    void appendRegisterNodes(Register& dstReg, size_t dstOffset, const Register& srcReg, size_t srcOffset, size_t size);
    void appendNodes(int& dst, int src);
    Register memoryToRegister(size_t address, size_t size) const;
    static size_t blockInstructionSize(Token::Type type);
};
