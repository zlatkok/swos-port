#include "InstructionNode.h"
#include "CppOutput/CppOutput.h"
#include "OpWriter.h"

#define out(...) (m_outputWriter.out(__VA_ARGS__))
#define kIndent (m_outputWriter.kIndent)
#define kNewLine (Util::kNewLine)

OpWriter::OpWriter(const InstructionNode& node, CppOutput& outputWriter, int flags /* = 0 */)
:
    m_node(node), m_outputWriter(outputWriter),
    m_dst(m_node.opInfo[0], m_src, "dst"), m_src(m_node.opInfo[1], m_dst, "src"), m_flags(flags),
    m_startPtr(outputWriter.getOutputPtr())
{
    auto numOperands = node.instruction->numOperands();

    assert(numOperands < 1 || m_dst.opInfo.size() || m_src.opInfo.size());
    assert(numOperands < 2 || !(flags & kSigned) || m_dst.opInfo.size() != m_src.opInfo.size() || node.instruction->type() == Token::T_IMUL);

    if (numOperands > 0) {
        m_dst.setSize();
        if (numOperands > 1)
            m_src.setSize();

        m_dstTypeInfo = getTypeInfo(m_dst.size);

        if (m_dst.fetchMemory = m_dst.opInfo.needsMemoryFetch())
            m_dst.constantAddress = m_dst.opInfo.isFixedMem() ? *m_dst.opInfo.displacement.num : -1;

        if (numOperands > 1 && (m_src.fetchMemory = m_src.opInfo.needsMemoryFetch()))
            m_src.constantAddress = m_src.opInfo.isFixedMem() ? *m_src.opInfo.displacement.num : -1;
    }

    if (m_flags & kExtraScope) {
        out('{', kNewLine);
        m_indentLevel++;
        indent();
    }

    if (node.instruction->numOperands())
        std::swap(m_src.varName, m_dst.varName);
}

OpWriter::~OpWriter()
{
    auto p = m_outputWriter.getOutputPtr() - 1;
    auto len = m_outputWriter.outputLength();
    assert(len);

    if (m_aborted) {
        m_outputWriter.setOutputPtr(m_startPtr);
    } else {
        if (m_indentLevel > 1) {
            while (len && *p != '\n')
                len--, p--;
            m_outputWriter.setOutputPtr(p + 1 + !len);

            do {
                m_indentLevel--;
                indent();
                out('}');
                if (m_indentLevel < 2)
                    break;
                out(kNewLine);
            } while (true);
        } else {
            while (len && Util::isSpace(*p))
                len--, p--;
            m_outputWriter.setOutputPtr(p + 1 + !len);
        }
    }
}

void OpWriter::indent() const
{
    for (int i = 0; i < m_indentLevel; i++)
        out(kIndent);
}

void OpWriter::increaseIndentLevel()
{
    m_indentLevel++;
}

void OpWriter::decreaseIndentLevel()
{
    m_indentLevel--;
}

void OpWriter::startNewLine(bool semicolon /* = true */, int indentChange /* = 0 */)
{
    if (semicolon && m_outputWriter.lastOutputChar() != '{')
        out(';');

    out(kNewLine);
    m_indentLevel += indentChange;
    indent();
}

void OpWriter::discardOutput()
{
    m_aborted = true;
}

bool OpWriter::isDestMemory() const
{
    return m_dst.fetchMemory;
}

void OpWriter::writeSrcToDestMemory()
{
    assert(m_dst.fetchMemory);

    writeToMemory(m_dst, kSourceValue);
}

void OpWriter::outputDest(ValueCategory category /* = kAuto */)
{
    outputOp(m_dst, category == kAuto ? kLvalue : category);
}

void OpWriter::outputSrc(ValueCategory category /* = kAuto */)
{
    outputOp(m_src, category == kAuto ? kRvalue : category);
}

void OpWriter::outputThirdOp()
{
    assert(m_node.instruction->numOperands() == 3);

    OpInfo src2(m_node.opInfo[2], m_dst, "src2");
    outputOp(src2, kRvalue);
}

bool OpWriter::twoOperandsSameRegister() const
{
    return m_node.instruction->numOperands() == 2 && m_dst.opInfo.isRegister() && m_dst.opInfo == m_src.opInfo;
}

void OpWriter::outputSrcVar(ValueCategory category /* = kAuto */)
{
    if (m_srcVarDeclared)
        out(m_src.varName);
    else
        outputSrc(category);
}

void OpWriter::outputDestVar(ValueCategory category /* = kAuto */)
{
    if (m_dstVarDeclared)
        out(m_dst.varName);
    else
        outputDest(category);
}

void OpWriter::setDestVar(const char *destVar)
{
    m_dstVarDeclared = true;
    m_dst.varName = destVar;
}

size_t OpWriter::srcSize() const
{
    return m_src.size;
}

size_t OpWriter::destSize() const
{
    return m_dst.size;
}

std::optional<int> OpWriter::srcConstValue() const
{
    return m_src.opInfo.displacement.num;
}

int OpWriter::bitCount() const
{
    return m_dstTypeInfo.bitCount;
}

const char *OpWriter::destType() const
{
    return m_flags & kSigned ? m_dstTypeInfo.signedType : m_dstTypeInfo.unsignedType;
}

const char *OpWriter::signedType() const
{
    return m_dstTypeInfo.signedType;
}

const char *OpWriter::nextLargerSignedType() const
{
    return m_dstTypeInfo.nextLargerSignedType;
}

const char *OpWriter::unsignedType() const
{
    return m_dstTypeInfo.unsignedType;
}

const char *OpWriter::signMask() const
{
    return m_dstTypeInfo.signMask;
}

const char *OpWriter::signedMin() const
{
    return m_dstTypeInfo.signedMin;
}

const char *OpWriter::signedMax() const
{
    return m_dstTypeInfo.signedMax;
}

void OpWriter::fetchSrcFromMemoryIfNeeded(ValueCategory category /* = kAuto */)
{
    fetchFromMemoryIfNeeded(m_src, category == kAuto ? kRvalue : category);
}

void OpWriter::fetchDestFromMemoryIfNeeded(ValueCategory category /* = kAuto */)
{
    fetchFromMemoryIfNeeded(m_dst, category == kAuto ? kRvalue : category);
}

void OpWriter::writeBackDestToMemoryIfNeeded()
{
    if (m_dstVarDeclared) {
        if (m_dst.opInfo.needsMemoryFetch()) {
            writeToMemory(m_dst, kDestVar);
        } else {
            // only used to support pop small, such as pop small [word ptr D0]:
            //   int32_t val = stack[stackTop++]
            //   *(word *)&D0 = val
            outputDest();
            out(" = ");
            outputDestVar();
        }
        startNewLine();
    }
}

void OpWriter::writeBackSrcToMemoryIfNeeded()
{
    if (m_srcVarDeclared) {
        writeToMemory(m_src, kDestVar);
        startNewLine();
    }
}

void OpWriter::clearCarryAndOverflowFlags()
{
    if (!m_node.suppressCarryFlag)
        setFlag("carry", [this] { out("false"); });

    if (!m_node.suppressOverflowFlag)
        setFlag("overflow", [this] { out("false"); });
}

void OpWriter::setCarryFlag(std::function<void()> f)
{
    if (!m_node.suppressCarryFlag)
        setFlag("carry", f);
}

void OpWriter::setCarryFlag(const char *val)
{
    if (!m_node.suppressCarryFlag)
        setFlag("carry", val);
}

void OpWriter::setCarryFlag(bool val)
{
    if (!m_node.suppressCarryFlag)
        setFlag("carry", val);
}

void OpWriter::setOverflowFlag(std::function<void()> f)
{
    if (!m_node.suppressOverflowFlag)
        setFlag("overflow", f);
}

void OpWriter::setOverflowFlag(const char *val)
{
    if (!m_node.suppressOverflowFlag)
        setFlag("overflow", val);
}

void OpWriter::setSignFlag(std::function<void()> f)
{
    if (!m_node.suppressSignFlag)
        setFlag("sign", f);
}

void OpWriter::setSignFlag(const char *val)
{
    if (!m_node.suppressSignFlag)
        setFlag("sign", val);
}

void OpWriter::setZeroFlag(std::function<void()> f)
{
    if (!m_node.suppressZeroFlag)
        setFlag("zero", f);
}

void OpWriter::setZeroFlag(const char *val)
{
    if (!m_node.suppressZeroFlag)
        setFlag("zero", val);
}

void OpWriter::outputOp(const OpInfo& op, ValueCategory category)
{
    assert(category != kAuto);

    if (handleLocalVariable(op.opInfo))
        return;

    if (op.fetchMemory) {
        if (op.constantAddress > 0) {
            readConstantMemory(op);
        } else {
            if (category == kRvalue) {
                if (op.size == 1)
                    out(m_flags & kSigned ? "(int8_t)" : "(byte)");
                else if (op.size == 2)
                    out(m_flags & kSigned ? "(int16_t)" : "(word)");
            }
            out(category == kRvalue ? "read" : "write");
            out("Memory(");
            outputOpValue(op, category);
            out(", ");
            out(op.size);
            if (category == kLvalue) {
                out(", ");
                outputOpValue(op.otherOp, category);
            }
            out(')');
        }
    } else {
        outputOpValue(op, category);
    }
}

void OpWriter::fetchFromMemoryIfNeeded(const OpInfo& op, ValueCategory category)
{
    if (op.fetchMemory) {
        m_dstVarDeclared = true;

        if (m_indentLevel < 2) {
            m_flags |= kExtraScope;
            out('{', kNewLine);
            m_indentLevel++;
            indent();
        }

        out(getType(op), ' ', op.varName, " = ");
        outputDest(category);
        startNewLine();
    }
}

void OpWriter::writeToMemory(const OpInfo& op, DestMemoryData source)
{
    assert(op.fetchMemory);

    if (op.constantAddress > 0)
        writeToConstantAddress(op, source);
    else
        writeToDynamicAddress(op, source);
}

void OpWriter::writeToConstantAddress(const OpInfo& op, DestMemoryData source)
{
    assert(op.constantAddress > 0);

    auto outputData = [source, this] {
        source == kSourceValue ? outputSrcValue(kRvalue) : outputDestVar();
    };
    auto isConstZero = [source, this] {
        return source == kSourceValue ? m_src.isConstZero() : false;
    };

    const auto& typeInfo = getTypeInfo(op.size);

    if (op.size == 1) {
        outMemAccess(op.constantAddress);
        out(" = ");
        outputData();
    } else {
        switch (op.constantAddress % op.size) {
        case 0:
            outMemAccess(op.constantAddress, op.size);
            out(" = ");
            outputData();
            break;

        case 1:
            if (op.size == 2) {
                outMemAccess(op.constantAddress);
                if (isConstZero()) {
                    out(" = 0");
                } else {
                    out(" = ");
                    outputData();
                    out(" & 0xff");
                }

                startNewLine();

                outMemAccess(op.constantAddress + 1);
                if (isConstZero()) {
                    out(" = 0");
                } else {
                    out(" = ");
                    outputData();
                    out(" >> 8");
                }
            } else {
                outMemAccess(op.constantAddress - 1, 4);
                if (isConstZero()) {
                    out(" = ");
                } else {
                    out(" = (");
                    outputData();
                    out(" << 8) | ");
                }
                outMemAccess(op.constantAddress - 1);

                startNewLine();

                outMemAccess(op.constantAddress + 3);
                if (isConstZero()) {
                    out(" = 0");
                } else {
                    out(" = ");
                    outputData();
                    out(" >> 24");
                }
            }
            break;

        case 2:
            outMemAccess(op.constantAddress, 2);
            if (isConstZero()) {
                out(" = 0");
            } else {
                out(" = ");
                outputData();
                out(" & 0xffff");
            }

            startNewLine();

            outMemAccess(op.constantAddress + 2, 2);
            if (isConstZero()) {
                out(" = 0");
            } else {
                out(" = ");
                outputData();
                out(" >> 16");
            }

            break;

        case 3:
            outMemAccess(op.constantAddress);
            if (isConstZero()) {
                out(" = 0");
            } else {
                out(" = ");
                outputData();
                out(" & 0xff");
            }

            startNewLine();

            outMemAccess(op.constantAddress + 1, 4);
            if (isConstZero()) {
                out(" = ");
                outMemAccess(op.constantAddress + 3);
                out(" << 24");
            } else {
                out(" = (");
                outputData();
                out(" >> 8) | (");
                outMemAccess(op.constantAddress + 3);
                out(" << 24)");
            }

            break;
        }
    }
}

void OpWriter::writeToDynamicAddress(const OpInfo& op, DestMemoryData source)
{
    assert(op.fetchMemory && op.constantAddress < 0);

    auto outputData = [source, this] {
        source == kSourceValue ? outputSrcValue(kRvalue) : outputDestVar();
    };

    out("writeMemory(");
    outputDestValue(kRvalue);
    out(", ");
    out(op.size);
    out(", ");
    outputData();
    out(")");
}

void OpWriter::readConstantMemory(const OpInfo& op) const
{
    assert(op.constantAddress > 0 &&
        static_cast<size_t>(op.constantAddress) >= DataBank::zeroRegionSize() &&
        static_cast<size_t>(op.constantAddress) < m_outputWriter.m_dataBank.memoryByteSize());

    auto otherOpSize = op.otherOp.size ? op.otherOp.size : op.size;
    auto size = std::min(op.size, otherOpSize);

    const auto& typeInfo = getTypeInfo(size);

    if (size == 1) {
        outMemAccess(op.constantAddress);
    } else {
        switch (op.constantAddress % size) {
        case 0:
            outMemAccess(op.constantAddress, size);
            break;

        case 1:
            if (size == 2) {
                outMemAccess(op.constantAddress);
                out(" | (");
                outMemAccess(op.constantAddress + 1);
                out(" << 8)");
            } else {
                out('(');
                outMemAccess(op.constantAddress - 1, 4);
                out(" >> 8) | (");
                outMemAccess(op.constantAddress + 3);
                out(" << 24)");
            }
            break;

        case 2:
            assert(size == 4);
            outMemAccess(op.constantAddress, 2);
            out(" | ");
            outMemAccess(op.constantAddress + 2, 2);
            out(" << 16");
            break;

        case 3:
            assert(size == 4);
            outMemAccess(op.constantAddress);
            out(" | (");
            outMemAccess(op.constantAddress + 1, 4);
            out(" << 8)");
            break;

        default:
            assert(false);
        }
    }
}

const char *OpWriter::getType(const OpInfo& op) const
{
    return m_flags & kSigned ? getTypeInfo(op.size).signedType : getTypeInfo(op.size).unsignedType;
}

void OpWriter::outputDestValue(ValueCategory category)
{
    outputOpValue(m_dst, category);
}

void OpWriter::outputSrcValue(ValueCategory category)
{
    outputOpValue(m_src, category);
}

void OpWriter::outputOpValue(const OpInfo& op, ValueCategory category)
{
    if (handleLocalVariable(op.opInfo))
        return;

    bool needsParen = false;

    if (!op.opInfo.base.empty()) {
        needsParen = outputOpValue(op.opInfo.base, op, category);

        if (!op.opInfo.scale.empty() && !op.opInfo.scale.isConstZero()) {
            out(" + ");
            outputOpValue(op.opInfo.scale, op, category);

            if (op.opInfo.gotScaleFactor())
                out(" * ", op.opInfo.scaleFactor);
        }

        if (op.opInfo.displacement.num && *op.opInfo.displacement.num)
            out(" + ");
    }

    if (op.opInfo.displacement.num && (op.opInfo.base.empty() || *op.opInfo.displacement.num)) {
        const auto& str = op.opInfo.displacement.str;
        if (str.length() == 3 && str[0] == '\'' && str[2] == '\'')
            out(op.opInfo.displacement.str);
        else
            out(*op.opInfo.displacement.num);
    }

    if (needsParen)
        out(')');
}

bool OpWriter::outputOpValue(const OperandInfo::Component& comp, const OpInfo& op, ValueCategory category) const
{
    if (op.opInfo.type == OperandInfo::kUnknown)
        return false;

    assert(category != kAuto && !comp.val.str.empty());

    bool needsParen = false;

    if (comp.val.num) {
        out(*comp.val.num);
    } else {
        const auto& str = comp.str();
        if (!comp.empty()) {
            if (comp.reg >= kAmigaRegsStart) {
                // want to cast:
                //   mov word ptr D6, ax   -> *(word *)&D6 = ax;
                //   mov word ptr D6+2, dx -> *(word *)((byte *)&D6 + 2) = dx;
                //   mov dx, word ptr D0+2 -> dx = *(word *)((byte *)&D0 + 2);
                //   imul ebx              -> int64_t res = (int64_t)eax * (int32_t)ebx
                //
                // dont't want to cast:
                //   mov ax, word ptr D6   -> ax = D6
                //   mov eax, D0           -> eax = D0

                bool needsCast = ((m_flags & kSigned) && !op.fetchMemory) ||
                    (op.size != 4 &&
                    (category == kRvalue && comp.offset ||
                    category == kLvalue || !op.otherOp.opInfo.isRegister() ||
                    op.size != op.otherOp.size));

                if (needsCast) {
                    out("*(", getType(op), " *)");
                    if (comp.offset) {
                        out("((byte *)");
                        needsParen = true;
                    }
                    out('&');
                }
            } else if ((category != kLvalue && (m_flags & kSigned)) && !op.fetchMemory) {
                out("(", getType(op), ")");
            }
            out(str);
        } else if (auto def = m_outputWriter.m_defines.get(str)) {
            out(def->value());
        } else {
            int procOffset = m_outputWriter.m_dataBank.getProcOffset(str);
            if (procOffset != -1) {
                out(procOffset);
            } else {
                assert(!str.empty());
                auto offset = m_outputWriter.m_dataBank.getVarOffset(str);
                out(offset);
            }
        }
    }

    return needsParen;
}

bool OpWriter::handleLocalVariable(const OperandInfo& op)
{
    if (op.base.reg == kEsp) {
        assert(!op.scale.val.str.empty());

        auto var = &op.scale.val;
        if (op.scale.val.str.empty()) {
            assert(!op.displacement.str.empty());
            var = &op.displacement;
        }

        out(var->str);
        startNewLine();
        return true;
    }

    return false;
}

void OpWriter::setFlag(const char *flag, std::function<void()> f)
{
    outFlagAssignment(flag);
    f();
    startNewLine();
}

void OpWriter::setFlag(const char *flag, const char *val)
{
    outFlagAssignment(flag);
    out(val, " != 0");
    startNewLine();
}

void OpWriter::setFlag(const char *flag, bool val)
{
    outFlagAssignment(flag);
    out(val ? "true" : "false");
    startNewLine();
}

void OpWriter::outFlagAssignment(const char *flag) const
{
    out("flags.");
    out(flag);
    out(" = ");
}

void OpWriter::outMemAccess(size_t address, size_t size /* = 1 */) const
{
    assert(address % size == 0);

    switch (size) {
    case 4:
        out("g_memDword[");
        address /= 4;
        break;
    case 2: out("g_memWord[");
        address /= 2;
        break;
    default:
        out("g_memByte[");
        assert(size == 1);
    }

    out(address, ']');
}
