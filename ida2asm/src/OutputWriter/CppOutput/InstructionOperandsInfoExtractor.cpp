#include "InstructionOperandsInfoExtractor.h"
#include "Instruction.h"
#include "AmigaRegs.h"
#include "DataBank.h"
#include "DefinesMap.h"
#include "Struct.h"
#include "References.h"
#include "OutputException.h"

constexpr int kAverageBytesPerStruct = 750;

InstructionOperandsInfoExtractor::InstructionOperandsInfoExtractor(const Instruction *instruction, const StructStream& structs,
    const StructMap& structMap, const DefinesMap& defines, const References& references, const DataBank& dataBank)
:
    m_instruction(instruction), m_structMap(structMap), m_defines(defines), m_references(references), m_dataBank(dataBank),
    m_op1(instruction, 0, structs, structMap, references, dataBank),
    m_op2(instruction, 1, structs, structMap, references, dataBank),
    m_op3(instruction, 2, structs, structMap, references, dataBank)
{
    convertProcPointers();
    fixPushSmall();
    ensureSizeInformation();

    assert(m_op1.size() | m_op2.size() | m_op3.size());
    assert(m_instruction->type() == Token::T_MOVZX || m_instruction->type() == Token::T_SHL ||
        m_instruction->type() == Token::T_RCR || m_instruction->type() == Token::T_RCL ||
        m_instruction->type() == Token::T_ROL || m_instruction->type() == Token::T_ROR ||
        m_instruction->type() == Token::T_SAR ||
        (m_op1.size() + m_op2.size() + m_op3.size()) % ((m_op1.size() > 0) + (m_op2.size() > 0) + (m_op3.size() > 0)) == 0);
    assert(m_instruction->numOperands() < 1 || !(m_op1.pointer() && m_op1.address()));
    assert(m_instruction->numOperands() < 2 || !(m_op2.pointer() && m_op2.address()));
}

// cook the operand info
auto InstructionOperandsInfoExtractor::getOperandInfo() const -> std::array<OperandInfo, Instruction::kMaxOperands>
{
    assert(m_op1.size() | m_op2.size() | m_op3.size());

    std::array<OperandInfo, Instruction::kMaxOperands> result;

    const RawOperandInfo *kOpInfo[] = { &m_op1, &m_op2, &m_op3 };

    for (size_t i = 0; i < std::min<size_t>(m_instruction->numOperands(), 2); i++) {
        const auto& op = *kOpInfo[i];
        const auto& otherOp = *kOpInfo[i ^ 1];
        auto& cookedOp = result[i];

        auto opSize = op.size() ? op.size() : otherOp.size();
        cookedOp.type = OperandInfo::kUnknown;

        bool needsMemoryFetch = op.needsMemoryFetch();

        // mov word ptr savedSprites.lineQuads[ebx], ax -> base: ebx, scale: /, disp: savedSprites + offsetOf(lineQuads)
        // [esi+MenuEntry.entryType1], ENTRY_FRAME_AND_BACK_COLOR -> base: esi, scale: /, disp: 28
        fillComponents(cookedOp, op, opSize, needsMemoryFetch);

        cookedOp.scaleFactor = op.scaleFactor();

        if (needsMemoryFetch) {
            assert(opSize);
            cookedOp.memSize = static_cast<uint8_t>(opSize);

            auto addr = op.getConstantAddress(m_dataBank);
            if (addr >= 0) {
                cookedOp.displacement.num = addr;
                cookedOp.type = OperandInfo::kFixedMem;

                auto size = op.size();
                if (!size)
                    size = m_instruction->type() == Token::T_MOV ? otherOp.size() : otherOp.size() / 2;

                cookedOp.memSize = static_cast<uint8_t>(size);
            } else {
                cookedOp.type = OperandInfo::kDynamicMem;
                cookedOp.displacement.num = getOffset(op);
            }
        } else if (cookedOp.type == OperandInfo::kUnknown) {
            auto val = extractConstantValue(op.base());

            if (op.scale().isNumber())
                val += op.scale().num;

            assert(op.displacement().empty());

            cookedOp.displacement.num = val;
            cookedOp.displacement.str = op.base().str;
            cookedOp.type = OperandInfo::kImmediate;
        } else {
            assert(cookedOp.type == OperandInfo::kReg);
        }

        assert(cookedOp.type != OperandInfo::kUnknown);
    }

    if (m_instruction->numOperands() == 3) {
        // 3rd operand of imul instruction, must be immediate
        auto constValue = m_op3.getConstValue();
        assert(constValue);
        result[2].setAsConstant(*constValue);
    }

    return result;
}

void InstructionOperandsInfoExtractor::convertProcPointers()
{
    assert(m_instruction && (m_instruction->type() != Token::T_MOV || !m_op2.base().empty()));

    bool moveOffsetInstruction = m_instruction->type() == Token::T_MOV && m_op2.address() &&
        m_op2.base().type == Token::T_ID && m_op2.scale().empty() && m_op2.displacement().empty();

    if (moveOffsetInstruction && !m_op2.base().str.contains('.')) {
        int offset = m_dataBank.getProcOffset(m_op2.base().str);
        if (offset != -1) {
            m_op2.base().type = Token::T_NUM;
            m_op2.base().num = offset;
        }
    }
}

void InstructionOperandsInfoExtractor::fixPushSmall()
{
    // get rid of 'push small', we'll keep it for the pop
    if (m_instruction->type() == Token::T_PUSH)
        m_op1.setSize(4);
}

void InstructionOperandsInfoExtractor::ensureSizeInformation()
{
    assert(!m_instruction->isShiftRotate() || m_instruction->numOperands() == 2);

    auto numKnownSizeOperands = (m_op1.size() > 0) + (m_op2.size() > 0) + (m_op3.size() > 0);
    bool doThoroughSearch = m_instruction->isShiftRotate() && !m_op1.size() || numKnownSizeOperands == 0;

    // only do more thorough search if simpler methods have failed
    if (doThoroughSearch) {
        // resolve tough cases like: mov dword ptr savedSprites.lineQuads[ebx], 0
        for (auto opPtr : { &m_op1, &m_op2 }) {
            auto& op = *opPtr;
            if (op.base().hasString() || op.scale().hasString()) {
                const auto& idField = op.base().hasString() ? op.base().str : op.scale().str;

                auto regIndex = amigaRegisterToIndex(idField);
                if (regIndex >= 0) {
                    op.setSize(4);
                    break;
                } else {
                    auto id = idField;

                    auto dotIndex = idField.indexOf('.');
                    if (dotIndex >= 0)
                        id = id.substr(0, dotIndex);

                    auto type = m_references.getType(id);

                    switch (type.first) {
                    case References::kByte:
                        op.setSize(1);
                        return;
                    case References::kWord:
                        op.setSize(2);
                        return;
                    case References::kDword:
                        op.setSize(4);
                        return;
                    case References::kTbyte:
                        op.setSize(10);
                        return;
                    case References::kUser:
                        {
                            assert(dotIndex >= 0);

                            auto fieldName = idField.substr(dotIndex + 1);
                            op.setSize(getStructVariableSize(type.second, fieldName));
                            return;
                        }
                    case References::kIgnore:
                        return;
                    default:
                        assert(false);
                        throw OutputException("undefined reference: " + id.string());
                    }
                }
            }
        }
    }

    if (m_instruction->type() == Token::T_INT)
        m_op1.setSize(1);
    else
        overrideOrPropagateStructVarSize();
}

size_t InstructionOperandsInfoExtractor::getStructVariableSize(const String& structName, const String& fieldName) const
{
    assert(!structName.empty() && !fieldName.empty());

    char structNameAndField[256];
    assert(structName.length() + fieldName.length() + 1 < sizeof(structNameAndField));

    structName.copy(structNameAndField);
    structNameAndField[structName.length()] = '.';
    fieldName.copy(structNameAndField + structName.length() + 1);

    size_t len = structName.length() + fieldName.length() + 1;
    auto offsetAndSize = m_structMap.get(structNameAndField, len);

    assert(offsetAndSize);
    return offsetAndSize->size();
}

void InstructionOperandsInfoExtractor::overrideOrPropagateStructVarSize()
{
    if (m_instruction->numOperands() > 1) {
        if (m_instruction->type() == Token::T_MOVZX || m_instruction->type() == Token::T_MOVSX) {
            if (!m_op2.size()) {
                assert(m_op1.size() > 1);
                m_op2.setSize(m_op1.size() / 2);
            }
        } else {
            if (m_op1.size() != m_op2.size() || !m_op1.size()) {
                RawOperandInfo *opInfo[2] = { &m_op1, &m_op2 };
                for (int i = 0; i < 2; i++) {
                    auto otherOp = i ^ 1;
                    if (opInfo[i]->structField()) {
                        if (opInfo[otherOp]->straightReg()) {
                            opInfo[i]->setSize(opInfo[otherOp]->size());
                            break;
                        } else if (opInfo[otherOp]->base().type == Token::T_NUM) {
                            opInfo[otherOp]->setSize(opInfo[i]->size());
                            break;
                        }
                    } else if (!opInfo[i]->size() && opInfo[i]->dereference() && !opInfo[i]->pointer() && opInfo[otherOp]->straightReg()) {
                        opInfo[i]->setSize(opInfo[otherOp]->size());
                    }
                }
            }
        }
    }
}

void InstructionOperandsInfoExtractor::fillComponents(OperandInfo& dst, const RawOperandInfo& op,
    size_t opSize, bool needsMemoryFetch) const
{
    for (auto opVal : { op.base(), op.scale(), op.displacement() }) {
        if (!opVal.empty()) {
            auto [reg, regOffset, size] = tokenToRegister(opVal);

            if (reg != kNoReg) {
                assert(!opVal.isNumber());
                assert(dst.base.str().empty() || dst.scale.str().empty());

                dst.type = OperandInfo::kReg;

                if (dst.base.str().empty())
                    dst.base.val.str = opVal.str;
                else
                    dst.scale.val.str = opVal.str;

                if (dst.base.empty()) {
                    dst.base.reg = reg;
                    if (reg >= kAmigaRegsStart) {
                        assert(opSize <= Instruction::kRegSize);
                        dst.base.size = static_cast<uint8_t>(opSize);

                        auto offset = getOffset(op);
                        assert(offset < Instruction::kRegSize);

                        if (offset)
                            dst.base.offset = *offset;
                    } else {
                        assert(size <= Instruction::kRegSize && regOffset < Instruction::kRegSize);
                        dst.base.size = static_cast<uint8_t>(size);
                        dst.base.offset = static_cast<uint8_t>(regOffset);
                    }
                } else if (dst.scale.empty()) {
                    assert(reg < kAmigaRegsStart);
                    assert(needsMemoryFetch);
                    dst.scale.reg = reg;
                    dst.scale.offset = static_cast<uint8_t>(regOffset);
                    dst.scale.size = static_cast<uint8_t>(size);
                } else {
                    assert(false);
                }
            } else {
                for (auto dstVal : { &dst.displacement, &dst.scale.val, &dst.base.val }) {
                    if (dstVal->str.empty()) {
                        dstVal->str = opVal.str;
                        if (opVal.isNumber())
                            dstVal->num = opVal.num;
                        break;
                    }
                }
            }
        }
    }
}

std::tuple<RegisterEnum, size_t, size_t> InstructionOperandsInfoExtractor::tokenToRegister(const RawOperandInfo::OpComponent& val)
{
    switch (val.type) {
    case Token::T_EAX: return { kEax, 0, 4 };
    case Token::T_EBX: return { kEbx, 0, 4 };
    case Token::T_ECX: return { kEcx, 0, 4 };
    case Token::T_EDX: return { kEdx, 0, 4 };
    case Token::T_ESI: return { kEsi, 0, 4 };
    case Token::T_EDI: return { kEdi, 0, 4 };
    case Token::T_ESP: return { kEsp, 0, 4 };
    case Token::T_EBP: return { kEbp, 0, 4 };
    case Token::T_AX: return { kEax, 0, 2 };
    case Token::T_BX: return { kEbx, 0, 2 };
    case Token::T_CX: return { kEcx, 0, 2 };
    case Token::T_DX: return { kEdx, 0, 2 };
    case Token::T_SI: return { kEsi, 0, 2 };
    case Token::T_DI: return { kEdi, 0, 2 };
    case Token::T_BP: return { kEbp, 0, 2 };
    case Token::T_SP: return { kEsp, 0, 2 };
    case Token::T_AL: return { kEax, 0, 1 };
    case Token::T_AH: return { kEax, 1, 1 };
    case Token::T_BL: return { kEbx, 0, 1 };
    case Token::T_BH: return { kEbx, 1, 1 };
    case Token::T_CL: return { kEcx, 0, 1 };
    case Token::T_CH: return { kEcx, 1, 1 };
    case Token::T_DL: return { kEdx, 0, 1 };
    case Token::T_DH: return { kEdx, 1, 1 };
    case Token::T_ID:
        {
            auto regIndex = amigaRegisterToIndex(val.str);
            if (regIndex >= 0)
                return { static_cast<RegisterEnum>(kAmigaRegsStart + regIndex), 0, 0 };
        }
        // fall-through
    default:
        return { kNoReg, 0, 0 };
    }
}

std::optional<int> InstructionOperandsInfoExtractor::getOffset(const RawOperandInfo& op) const
{
    std::optional<int> offset;

    if (!op.base().empty() && op.base().type == Token::T_ESP)
        return offset;

    for (const auto& val : { op.base(), op.scale(), op.displacement() }) {
        bool isId = val.type == Token::T_ID && amigaRegisterToIndex(val.str) < 0;
        if (isId || val.type == Token::T_STRING)
            offset = offset.value_or(0) + extractConstantValue(val);
        else if (val.isNumber())
            offset = offset.value_or(0) + val.num;
    }

    return offset;
}

int InstructionOperandsInfoExtractor::extractConstantValue(const RawOperandInfo::OpComponent& val) const
{
    assert(!val.empty());

    if (val.isNumber()) {
        return val.num;
    } else if (val.type == Token::T_STRING) {
        // little endian
        if (val.str.length() == 6) {
            return val.str[4] | (val.str[3] << 8) | (val.str[2] << 16) | (val.str[1] << 24);
        } else if (val.str.length() == 5) {
            return val.str[3] | (val.str[2] << 8) | (val.str[1] << 16);
        } else {
            assert(val.str.length() == 3);
            return val.str[1];
        }
    } else if (auto def = m_defines.get(val.str)) {
        return def->value().toInt();
    } else {
        int procOffset = m_dataBank.getProcOffset(val.str);
        if (procOffset != -1) {
            return procOffset;
        } else {
            auto offset = m_dataBank.getVarOffset(val.str);
            return offset;
        }
    }
}

InstructionOperandsInfoExtractor::RawOperandInfo::RawOperandInfo(const Instruction *instruction, size_t index,
    const StructStream& structs, const StructMap& structMap, const References& references, const DataBank& dataBank)
:
    m_structs(structs), m_structMap(structMap), m_references(references), m_dataBank(dataBank)
{
    if (index >= instruction->numOperands())
        return;

    parseInstructionOperand(index, instruction);
}

void InstructionOperandsInfoExtractor::RawOperandInfo::parseInstructionOperand(size_t index, const Instruction *instruction)
{
    m_size = instruction->operandSizes()[index];
    const auto& operands = instruction->operands();
    const auto& params = operands[index];

    for (auto op = params.begin(); op != params.end(); op = op->next()) {
        assert(op < params.end());
        auto type = op->type();

        switch (type) {
        case Token::T_OFFSET:
            m_address = true;
            assert(m_base.empty());
            break;

        case Token::T_LBRACKET:
            m_dereference = true;
            break;

        case Token::T_BYTE:
        case Token::T_WORD:
        case Token::T_DWORD:
            m_pointer = true;
            break;

        case Token::T_PTR:
        case Token::T_RBRACKET:
        case Token::T_LPAREN:
        case Token::T_RPAREN:
        case Token::T_PLUS:
            // skip over
            break;

        case Token::T_MULT:
            op = op->next();
            handleScaleExpression(op);

            assert(op->type() == Token::T_NUM);
            assert(op->next() == params.end() || op->next()->type() != Token::T_PLUS);
            break;

        default:
            bool forceSearch = false;
            if (type == Token::T_SIZE) {
                op = op->next();
                assert(op->type() == Token::T_ID);
                forceSearch = true;
                type = Token::T_ID;
            }

            assignComponent(op, params, type, forceSearch);

            if (op->isRegister())
                m_hasReg = true;

            break;
        }
    }

    if (m_straightReg && (m_address || m_dereference || m_pointer || !m_displacement.empty() || !m_scale.empty() || gotScale()))
        m_straightReg = false;

    markMemoryAccessForVariables();
}

void InstructionOperandsInfoExtractor::RawOperandInfo::assignComponent(Iterator::Iterator<const Instruction::Operand>& op,
    const Instruction::OperandHolder& params, Token::Type type, bool forceSearch)
{
    if (type == Token::T_ID || type == Token::T_STRING || op->isRegister() || op->isNumber()) {
        if (m_base.empty()) {
            m_base = expandOperand(op, type, forceSearch);
            if (op->isRegister())
                m_straightReg = true;
            if (type == Token::T_ID && op != params.end() && op->next()->type() == Token::T_LBRACKET)
                m_forceAddress = true;
        } else if (m_scale.empty()) {
            m_scale = expandOperand(op, type, forceSearch);
            if (op->isRegister() && m_base.type == Token::T_ID) {
                // mov dl, menuItemColorTable[edx]
                std::swap(m_base, m_scale);
            }
        } else if (m_displacement.empty()) {
            m_displacement = expandOperand(op, type, forceSearch);
        } else {
            assert(false);
        }
    }
}

bool InstructionOperandsInfoExtractor::RawOperandInfo::needsMemoryFetch() const
{
    // 68k registers will be handled specially; mov dx, word ptr D0+2
    return !got68kRegister() && (m_pointer || !m_scale.empty() || m_dereference) && !m_address;
}

bool InstructionOperandsInfoExtractor::RawOperandInfo::got68kRegister() const
{
    if (!m_base.empty() && m_base.type == Token::T_ID && amigaRegisterToIndex(m_base.str) >= 0)
        return true;

    if (!m_scale.empty() && m_scale.type == Token::T_ID && amigaRegisterToIndex(m_scale.str) >= 0)
        return true;

    if (!m_displacement.empty() && m_displacement.type == Token::T_ID && amigaRegisterToIndex(m_displacement.str) >= 0)
        return true;

    return false;
}

int InstructionOperandsInfoExtractor::RawOperandInfo::getConstantAddress(const DataBank& dataBank) const
{
    if (m_hasReg || amigaRegisterToIndex(m_base.str) >= 0)
        return -1;

    return getMemoryOffset(dataBank);
}

int InstructionOperandsInfoExtractor::RawOperandInfo::getMemoryOffset(const DataBank& dataBank) const
{
    assert(!m_base.empty());

    int memOffset;

    if (m_base.type == Token::T_ID) {
        memOffset = dataBank.getVarOffset(m_base.str);
    } else {
        assert(m_base.isNumber());
        memOffset = m_base.str.toInt();
    }

    if (!m_scale.empty()) {
        assert(m_scale.isNumber());
        memOffset += m_scale.num;
    }

    if (!m_displacement.empty()) {
        assert(m_displacement.isNumber());
        memOffset += m_displacement.num;
    }

    return memOffset;
}

std::optional<int> InstructionOperandsInfoExtractor::RawOperandInfo::getConstValue() const
{
    std::optional<int> result;

    for (const auto& op : { m_base, m_scale, m_displacement }) {
        if (!op.isNumber())
            return result;

        result = op.num + result.value_or(0);
    }

    return result;
}

void InstructionOperandsInfoExtractor::RawOperandInfo::handleScaleExpression(const Instruction::Operand *op)
{
    // the function isn't really handling expressions, it's only so named in advance :P
    // must handle special cases: add edi, 24 * 16 * 16 / add ebx, 44 * 4
    const auto& opText = op->text();

    if (m_base.isNumber() && op->isNumber()) {
        m_base.num *= op->text().toInt();
    } else if (!gotScale()) {
        assert(!m_base.empty() && !m_scale.empty());
        assert(!opText.contains('*'));

        setScale(opText.toInt());
    } else {
        assert(false);
    }
}

auto InstructionOperandsInfoExtractor::RawOperandInfo::expandOperand(Iterator::Iterator<const Instruction::Operand>& op,
    Token::Type type, bool forceSearch /* = false */) -> OpComponent
{
    if (op->type() == Token::T_ID && (forceSearch || op->text().contains('.'))) {
        OpComponent opPart;
        if (expandDirectStructField(opPart, op) || expandVariableStructField(opPart, op))
            return opPart;
    }

    int num = Token::isNumber(type) ? op->text().toInt() : -1;
    return { removeSegmentPrefix(op->text()), type, num };
}

bool InstructionOperandsInfoExtractor::RawOperandInfo::expandDirectStructField(OpComponent& result,
    Iterator::Iterator<const Instruction::Operand>& op)
{
    if (auto fieldData = m_structMap.get(op->text())) {
        // directly referencing a struct field: mov ax, [esi+Sprite.shirtNumber]
        if (!m_size && !m_pointer && m_dereference)
            m_size = std::min<int>(fieldData->size(), 4);

        int offset = fieldData->offset();
        offset += gatherNumericOffsets(op);

        if (m_dereference)
            m_structField = true;

        result = { op->text(), Token::T_NUM, offset };
        return true;
    }

    return false;
}

bool InstructionOperandsInfoExtractor::RawOperandInfo::expandVariableStructField(OpComponent& result,
    Iterator::Iterator<const Instruction::Operand>& op)
{
    int dotIndex = op->text().indexOf('.');
    if (dotIndex >= 0) {
        // handle non-declared struct var accessed with dot: mov goal1TopSprite.saveSprite, 0
        const auto& var = op->text().substr(0, dotIndex);

        if (auto structName = m_dataBank.structNameFromVar(var)) {
            auto struc = m_structs.findStruct(structName->string());
            const auto& field = op->text().substr(dotIndex);

            // make it into a variable access + fixed offset
            char buf[256];
            struc->name().copy(buf);
            field.copy(buf + struc->name().length());

            auto len = struc->name().length() + field.length();
            assert(len <= sizeof(buf));

            auto qualifiedFieldName = String(buf, len);
            auto fieldData = m_structMap.get(qualifiedFieldName);
            assert(fieldData);

            int offset = fieldData->offset();
            offset += gatherNumericOffsets(op);

            if (m_scale.empty()) {
                m_scale = { field, Token::T_NUM, offset };
            } else if (m_displacement.empty()) {
                m_displacement = { field, Token::T_NUM, offset };
            } else {
                assert(false);
            }

            m_structField = true;
            if (!m_address) {
                m_dereference = true;
                m_size = fieldData->size();
            } else {
                // mov A6, offset editTacticsCurrentTactics.someTable
                m_size = 4;
            }

            result = { var, Token::T_ID, -1 };
            return true;
        }
    }

    return false;
}

size_t InstructionOperandsInfoExtractor::RawOperandInfo::gatherNumericOffsets(Iterator::Iterator<const Instruction::Operand>& op)
{
    size_t offset = 0;

    // there should always be an operand before the end
    while (op->next()->type() == Token::T_PLUS) {
        // mov al, [esi+ebx+(PlayerFile.positionAndFace+4Ch)]
        op = op->next()->next();
        assert(op->isNumber());
        offset += op->text().toInt();
    }

    return offset;
}

String InstructionOperandsInfoExtractor::RawOperandInfo::removeSegmentPrefix(const String& str)
{
    // remove things like: cmp cs:int8Executing, 0
    // it shouldn't really show up anymore, but just to be on the safe side
    if (str.length() > 3 && str[2] == ':' && str[1] == 's')
        return str.substr(3);

    return str;
}

void InstructionOperandsInfoExtractor::RawOperandInfo::markMemoryAccessForVariables()
{
    if (!m_address && !m_dereference) {
        for (const auto& val : { m_base, m_scale, m_displacement }) {
            if (val.hasString() && amigaRegisterToIndex(val.str) < 0 &&
                m_references.getType(val.str).first != References::kIgnore)
                m_dereference = true;
        }
    }
}

bool InstructionOperandsInfoExtractor::RawOperandInfo::gotScale() const
{
    return m_scaleFactor > 0;
}

void InstructionOperandsInfoExtractor::RawOperandInfo::setScale(int scale)
{
    m_scaleFactor = scale;
}

