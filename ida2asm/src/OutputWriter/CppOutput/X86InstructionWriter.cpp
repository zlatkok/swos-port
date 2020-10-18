#include "CppOutput.h"
#include "OpWriter.h"
#include "TypeInfo.h"
#include "x86InstructionWriter.h"

#define out(...) (m_outputWriter.out(__VA_ARGS__))
#define kIndent (m_outputWriter.kIndent)
#define kNewLine (Util::kNewLine)
#define kDoubleNewLine (Util::kDoubleNewLine)

X86InstructionWriter::X86InstructionWriter(CppOutput *outputWriter, const DataBank& dataBank,
    const SymbolFileParser& symFileParser, const StringSet& inProcLabels)
:
    m_outputWriter(*outputWriter), m_dataBank(dataBank), m_symFileParser(symFileParser), m_inProcLabels(inProcLabels)
{
    assert(outputWriter);
}

void X86InstructionWriter::outputInstruction(const InstructionNode& node)
{
    if (node.deleted)
        return;

    switch (node.instruction->type()) {
    case Token::T_NOP:
        // ignore
        break;
    case Token::T_MOV:
    case Token::T_MOVZX:
    case Token::T_MOVSX:
        outputMov(node);
        break;
    case Token::T_MOVSB:
    case Token::T_MOVSW:
    case Token::T_MOVSD:
        outputMovs(node);
        break;
    case Token::T_LODSB:
    case Token::T_LODSW:
    case Token::T_LODS:
        outputLods(node);
        break;
    case Token::T_STOSB:
    case Token::T_STOSW:
    case Token::T_STOSD:
        outputStos(node);
        break;
    case Token::T_CALL:
        outputCall(node);
        break;
    case Token::T_PUSH:
    case Token::T_POP:
        outputPushPop(node);
        break;
    case Token::T_CBW:
        outputCbw(node);
        break;
    case Token::T_CWD:
        outputCwd(node);
        break;
    case Token::T_CWDE:
        outputCwde(node);
        break;
    case Token::T_CDQ:
        outputCdq(node);
        break;
    case Token::T_RETN:
        out("return");
        break;
    case Token::T_JMP:
        outputJmp(node);
        break;
    case Token::T_JG:
        outputJg(node);
        break;
    case Token::T_JGE:
        outputJge(node);
        break;
    case Token::T_JL:
        outputJl(node);
        break;
    case Token::T_JZ:
        outputJz(node);
        break;
    case Token::T_JNZ:
        outputJnz(node);
        break;
    case Token::T_JS:
        outputJs(node);
        break;
    case Token::T_JNS:
        outputJns(node);
        break;
    case Token::T_JA:
        outputJa(node);
        break;
    case Token::T_JB:
        outputJb(node);
        break;
    case Token::T_JNB:
        outputJnb(node);
        break;
    case Token::T_JBE:
        outputJbe(node);
        break;
    case Token::T_JO:
        outputJo(node);
        break;
    case Token::T_JNO:
        outputJno(node);
        break;
    case Token::T_JLE:
        outputJle(node);
        break;
    case Token::T_OR:
    case Token::T_XOR:
        outputOrXor(node);
        break;
    case Token::T_CMP:
        outputCmpSubAdd(node, false, false);
        break;
    case Token::T_INC:
        outputIncDec(node, true);
        break;
    case Token::T_DEC:
        outputIncDec(node, false);
        break;
    case Token::T_AND:
        outputAndTest(node, true);
        break;
    case Token::T_XCHG:
        outputXchg(node);
        break;
    case Token::T_SUB:
        outputCmpSubAdd(node, true, false);
        break;
    case Token::T_ADD:
        outputCmpSubAdd(node, true, true);
        break;
    case Token::T_MUL:
        outputMul(node);
        break;
    case Token::T_IMUL:
        outputImul(node);
        break;
    case Token::T_DIV:
    case Token::T_IDIV:
        outputDiv(node);
        break;
    case Token::T_TEST:
        outputAndTest(node, false);
        break;
    case Token::T_SHL:
    case Token::T_SHR:
    case Token::T_SAR:
        outputShift(node);
        break;
    case Token::T_RCR:
    case Token::T_RCL:
    case Token::T_ROL:
    case Token::T_ROR:
        outputRotate(node);
        break;
    case Token::T_INT:
    case Token::T_IN:
    case Token::T_OUT:
    case Token::T_CLI:
    case Token::T_STI:
        outputIntInOut(node);
        break;
    case Token::T_NEG:
        outputNeg(node);
        break;
    case Token::T_NOT:
        outputNot(node);
        break;
    case Token::T_LOOP:
        outputLoop(node);
        break;
    case Token::T_PUSHF:
        outputPushf(node);
        break;
    case Token::T_POPF:
        outputPopf(node);
        break;
    case Token::T_PUSHA:
        outputPusha(node);
        break;
    case Token::T_POPA:
        outputPopa(node);
        break;
    case Token::T_STC:
        outputStc(node);
        break;
    case Token::T_CLC:
        outputClc(node);
        break;
    case Token::T_CLD:
        outputCld(node);
        break;
    case Token::T_SETZ:
        outputSetz(node);
        break;
    default:
        assert(false);
        out("// ", node.instruction->instructionText(), " unimplemented!");
    }
}

void X86InstructionWriter::outputFunctionCall(const String& target)
{
    if (m_symFileParser.isImport(target))
        out("SWOS::");
    out(target, "()");
}

void X86InstructionWriter::outputToken(const String& op, Token::Type type, bool isInstruction /* = true */)
{
    int startOffset = 0;

    switch (type) {
    case Token::T_HEX:
        if (op.startsWith('-')) {
            out('-');
            startOffset = 1;
        }
        out("0x", op.substr(startOffset).withoutLast());
        break;

    case Token::T_BIN:
        {
            int num = 0;

            for (size_t i = 0; i < op.length() - 1; i++)
                num = num * 2 + op[i] - '0';

            out(num);
        }
        break;

    case Token::T_STRING:
        if (isInstruction) {
            out(op);
        } else {
            out('"');
            for (size_t i = 1; i < op.length() - 1; i++) {
                if (op[i] == '\\')
                    out('\\');
                out(op[i]);
            }
            out('"');
        }
        break;

    default:
        out(op);
        break;
    }
}

void X86InstructionWriter::outputMov(const InstructionNode& node)
{
    auto instruction = node.instruction;
    assert(instruction && node.instruction->numOperands() == 2);

    const auto& dst = node.opInfo[0];
    const auto& src = node.opInfo[1];

    assert(!dst.isConst());
    assert(!src.size() || !dst.size() || (instruction->type() == Token::T_MOV ? !src.size() || dst.size() == src.size() :
        dst.size() != src.size() && dst.size() && src.size()));

    int flags = instruction->type() == Token::T_MOVSX ? OpWriter::kSigned : 0;
    OpWriter op(node, m_outputWriter, flags);

    if (op.isDestMemory()) {
        op.writeSrcToDestMemory();
    } else {
        op.outputDest();
        out(" = ");
        op.outputSrc();
    }
}

void X86InstructionWriter::outputMovs(const InstructionNode& node)
{
    bool rep = node.instruction->prefix() == "rep";
    assert(rep);

    bool isMovsb = node.instruction->type() == Token::T_MOVSB;

    if (isMovsb && !rep) {
        out("assert(esi >= kMemStartOfs && esi <= kMemSize && edi >= kMemStartOfs && edi <= kMemSize);", kNewLine, kIndent);
        out("g_memByte[edi++] = g_memByte[esi++]");
    } else {
        auto size = "ecx";
        switch (node.instruction->type()) {
        case Token::T_MOVSW: size = "2"; break;
        case Token::T_MOVSD: size = "4"; break;
        }
        auto ecxMul = !isMovsb && rep ? "ecx * " : "";

        out("assert(esi >= kMemStartOfs && esi + ", ecxMul, size, " <= kMemSize && "
            "edi >= kMemStartOfs && edi + ", ecxMul, size, " <= kMemSize);", kNewLine, kIndent);

        out("memcpy(&g_memByte[edi], &g_memByte[esi], ", ecxMul, size, ");", kNewLine, kIndent);
        out("esi += ", ecxMul, size, ';', kNewLine, kIndent);
        out("edi += ", ecxMul, size);
        if (rep)
            out(';', kNewLine, kIndent, "ecx = 0");
    }
}

void X86InstructionWriter::outputStos(const InstructionNode& node)
{
    auto instruction = node.instruction;
    assert(instruction->prefix().empty() || instruction->prefix() == "rep");

    bool rep = instruction->prefix() == "rep";
    auto type = instruction->type();

    out("assert(!isExternalPointer(edi));", kNewLine, kIndent);

    if (type == Token::T_STOSB) {
        auto len = rep ? "ecx" : "1";
        out("assert(edi >= kMemStartOfs && edi + ", len, " <= kMemSize);", kNewLine, kIndent);
        if (rep) {
            out("memset(&g_memByte[edi], al, ecx);", kNewLine, kIndent);
            out("edi += ecx;", kNewLine, kIndent);
            out("ecx = 0");
        } else {
            out("g_memByte[edi++] = al");
        }
    } else {
        auto reg = "ax";
        auto size = "2";

        if (type == Token::T_STOSD) {
            reg = "eax";
            size = "4";
        }

        if (!rep) {
            out("writeMemory(edi, ", size, ", ", reg, ");", kNewLine, kIndent);
            out("edi += ", size);
        } else {
            out("assert(edi >= kMemStartOfs && edi + ", size, " * ecx <= kMemSize);", kNewLine, kIndent);
            out("for (; ecx; ecx--)", kNewLine, kIndent);
            out(kIndent, "for (int i = 0; i < ", size, "; i++)", kNewLine, kIndent);
            out(kIndent, kIndent, "g_memByte[edi++] = ", reg, " >> (i * 8)");
        }
    }
}

void X86InstructionWriter::outputLods(const InstructionNode& node)
{
    auto instruction = node.instruction;

    assert(instruction->numOperands() == 0 && instruction->prefix().empty());

    out("assert(!isExternalPointer(esi));", kNewLine, kIndent);

    switch (instruction->type()) {
    case Token::T_LODSB:
        out("al = g_memByte[esi++]");
        break;
    case Token::T_LODSW:
        out("ax = (word)readMemory(esi, 2);", kNewLine, kIndent);
        out("esi += 2");
        break;
    case Token::T_LODS:
        out("eax = readMemory(esi, 4);", kNewLine, kIndent);
        out("esi += 4");
        break;
    default:
        assert(false);
    }
}

void X86InstructionWriter::outputCall(const InstructionNode& node)
{
    auto instruction = node.instruction;

    assert(instruction->numOperands() == 1);

    const auto& operands = instruction->operands();
    const auto& operand = instruction->operands()[0];
    const auto target = operand.begin();
    const auto& text = target->text();

    if (!text.startsWith('$')) {
        assert(target->next() == operand.end());
        outputFunctionInvoke(text);
    }
}

void X86InstructionWriter::outputPushPop(const InstructionNode& node)
{
    auto instruction = node.instruction;

    assert(instruction->type() == Token::T_PUSH || instruction->type() == Token::T_POP);
    assert(instruction->numOperands() == 1);

    OpWriter op(node, m_outputWriter);

    const auto& dst = node.opInfo[0];

    if (instruction->type() == Token::T_POP && (dst.size() == 2 || dst.needsMemoryFetch())) {
        out('{');
        op.startNewLine(false, +1);
        out("int32_t val = stack[stackTop++]");
        op.startNewLine();
        op.setDestVar("val");
        op.writeBackDestToMemoryIfNeeded();
    } else {
        out(instruction->type() == Token::T_PUSH ? "push(" : "pop(");
        op.outputDest(OpWriter::kRvalue);
        out(')');
    }
}

void X86InstructionWriter::outputCbw(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 0);

    out("ah = (int8_t)al < 0 ? -1 : 0");
}

void X86InstructionWriter::outputCwd(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 0);

    out("dx = (int16_t)ax < 0 ? -1 : 0");
}

void X86InstructionWriter::outputCwde(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 0);

    out("eax.hi16 = (int16_t)ax < 0 ? -1 : 0");
}

void X86InstructionWriter::outputCdq(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 0);

    out("edx = (int32_t)eax < 0 ? -1 : 0");
}

void X86InstructionWriter::outputOrXor(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 2 && (node.instruction->type() == Token::T_XOR || node.instruction->type() == Token::T_OR));

    OpWriter op(node, m_outputWriter);
    bool eliminateOr = false;

    if (node.instruction->type() == Token::T_OR && op.twoOperandsSameRegister()) {
        if (node.suppressZeroFlag && node.suppressSignFlag) {
            if (node.suppressCarryFlag)
                op.discardOutput();
            else
                op.clearCarryAndOverflowFlags();
            return;
        }
        eliminateOr = true;
    }

    if (!eliminateOr) {
        op.fetchDestFromMemoryIfNeeded();
        op.outputDestVar();

        switch (node.instruction->type()) {
        case Token::T_OR: out(" |= "); break;
        case Token::T_XOR: out(" ^= "); break;
        default: assert(false);
        }

        op.outputSrc(); op.startNewLine();
        op.writeBackDestToMemoryIfNeeded();
    }

    op.clearCarryAndOverflowFlags();
    op.setSignFlag([&] {
        out('('); op.outputDestVar(); out(" & "); out(op.signMask()); out(") != 0");
    });
    op.setZeroFlag([&] {
        op.outputDestVar(); out(" == 0");
    });
}

void X86InstructionWriter::outputCmpSubAdd(const InstructionNode& node, bool commitResult, bool add)
{
    assert(node.instruction->numOperands() == 2);

    OpWriter op(node, m_outputWriter, OpWriter::kExtraScope);
    op.fetchDestFromMemoryIfNeeded();

    out(op.signedType(), " dstSigned = ");
    op.outputDestVar();
    op.startNewLine();

    out(op.signedType(), " srcSigned = ");
    op.outputSrc();
    op.startNewLine();

    out(op.unsignedType(), " res = dstSigned ", add ? '+' : '-', " srcSigned");
    op.startNewLine();

    op.setOverflowFlag([&] {
        out("dstSigned < 0 ? "
            "srcSigned > dstSigned - ", op.signedMin(), " : "
            "srcSigned < dstSigned - ", op.signedMax());
    });
    op.setCarryFlag([&] {
        out('('); out(op.unsignedType()); out(")(dstSigned ", add ? '+' : '-',
            " srcSigned) ", add ? '<' : '>', " (", op.unsignedType(), ")dstSigned");
    });
    op.setSignFlag([&] {
        out("(res & "); out(op.signMask()); out(") != 0");
    });
    op.setZeroFlag("res == 0");

    if (commitResult) {
        op.outputDestVar();
        out(" = res");
        op.startNewLine();

        op.writeBackDestToMemoryIfNeeded();
    }
}

void X86InstructionWriter::outputIncDec(const InstructionNode& node, bool increment)
{
    assert(node.instruction->numOperands() == 1);

    OpWriter op(node, m_outputWriter, OpWriter::kSigned);

    op.fetchDestFromMemoryIfNeeded();
    out('('); op.outputDestVar(); out(')'); out(increment ? "++" : "--"); op.startNewLine();

    op.setOverflowFlag([&] {
        out('(', op.signedType(), ")("); op.outputDestVar(); out(')'); out(" == ", increment ? op.signedMax() : op.signedMin());
    });
    op.setSignFlag([&] {
        out('('); op.outputDestVar(); out(" & ", op.signMask(), ") != 0");
    });
    op.setZeroFlag([&] {
        op.outputDestVar(); out(" == 0");
    });

    op.writeBackDestToMemoryIfNeeded();
}

void X86InstructionWriter::outputAndTest(const InstructionNode& node, bool commitResult)
{
    assert(node.instruction->numOperands() == 2);

    OpWriter op(node, m_outputWriter, OpWriter::kExtraScope);
    op.fetchDestFromMemoryIfNeeded();

    out(op.unsignedType(), " res = "); op.outputDestVar(); out(" & "); op.outputSrc();
    op.startNewLine();

    if (commitResult) {
        op.outputDestVar();
        out(" = res");
        op.startNewLine();

        op.writeBackDestToMemoryIfNeeded();
    }

    op.clearCarryAndOverflowFlags();
    op.setSignFlag([&] {
        out("(res & "); out(op.signMask()); out(") != 0");
    });
    op.setZeroFlag("res == 0");
}

void X86InstructionWriter::outputXchg(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 2);

    OpWriter op(node, m_outputWriter, OpWriter::kExtraScope);

    op.fetchDestFromMemoryIfNeeded();
    op.fetchSrcFromMemoryIfNeeded();

    out(op.unsignedType(), " tmp = "); op.outputSrcVar(OpWriter::kLvalue); op.startNewLine();
    op.outputSrcVar(OpWriter::kLvalue); out(" = "); op.outputDestVar(OpWriter::kRvalue); op.startNewLine();
    op.outputDestVar(); out(" = tmp"); op.startNewLine();

    op.writeBackDestToMemoryIfNeeded();
    op.writeBackSrcToMemoryIfNeeded();
}

void X86InstructionWriter::outputMul(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 1 && node.opInfo[0].size());

    OpWriter op(node, m_outputWriter);
    op.fetchDestFromMemoryIfNeeded(OpWriter::kRvalue);

    switch (op.destSize()) {
    case 1:
        out("ax = al * "); op.outputDestVar(); op.startNewLine();
        op.setCarryFlag("ah != 0");
        op.setOverflowFlag("ah != 0");
        break;
    case 2:
        out("tmp = ax * "); op.outputDestVar(); op.startNewLine();
        out("ax = tmp.lo16"); op.startNewLine();
        out("dx = tmp.hi16"); op.startNewLine();
        op.setCarryFlag("dx != 0");
        op.setOverflowFlag("dx != 0");
        break;
    case 4:
        out('{'); op.startNewLine(false, +1);
        out("uint64_t res = eax * "); op.outputDestVar(); op.startNewLine();
        out("eax = res & 0xffffffff"); op.startNewLine();
        out("edx = res >> 32"); op.startNewLine();
        op.setCarryFlag("edx != 0");
        op.setOverflowFlag("edx != 0");
        break;
    default:
        assert(false);
        break;
    }
}

void X86InstructionWriter::outputImul(const InstructionNode& node)
{
    auto instruction = node.instruction;
    assert(instruction->numOperands() > 0);

    OpWriter op(node, m_outputWriter, OpWriter::kSigned | OpWriter::kExtraScope);

    if (instruction->numOperands() == 1) {
        op.fetchDestFromMemoryIfNeeded(OpWriter::kRvalue);

        switch (op.destSize()) {
        case 1:
            out("ax = (int8_t)al * "); op.outputDestVar(OpWriter::kRvalue); op.startNewLine();
            op.setCarryFlag("(int8_t)ax != (int8_t)al");
            op.setOverflowFlag("(int8_t)ax != (int8_t)al");
            break;
        case 2:
            out("int32_t res = (int16_t)ax * "); op.outputDestVar(OpWriter::kRvalue); op.startNewLine();
            out("ax = res & 0xffff"); op.startNewLine();
            out("dx = res >> 16"); op.startNewLine();
            op.setCarryFlag("res != (int16_t)ax");
            break;
        case 4:
            out("int64_t res = (int64_t)eax * "); op.outputDestVar(OpWriter::kRvalue); op.startNewLine();
            out("eax = res & 0xffffffff"); op.startNewLine();
            out("edx = res >> 32"); op.startNewLine();
            op.setCarryFlag("res != (int32_t)eax");
            op.setOverflowFlag("(int32_t)eax");
            break;
        default:
            assert(false);
            break;
        }
    } else {
        op.fetchDestFromMemoryIfNeeded();
        op.fetchSrcFromMemoryIfNeeded();

        assert(op.destSize() == op.srcSize() && (op.destSize() == 2 || op.destSize() == 4));

        if (instruction->numOperands() == 3) {
            out(op.nextLargerSignedType(), " tmp = "); op.outputDestVar(); out(" * "); op.outputSrcVar(); op.startNewLine();
        } else {
            assert(instruction->numOperands() == 2);

            const auto& src2 = node.opInfo[2];

            out(op.signedType(), " src2 = "); op.outputThirdOp(); op.startNewLine();
            out(op.nextLargerSignedType(), " tmp = "); op.outputSrcVar(); out(" * src2"); op.startNewLine();
        }

        op.outputDestVar(); out(" = tmp"); op.startNewLine();
        op.writeBackDestToMemoryIfNeeded();

        // can't collapse since optimizer might eliminate flag assignments
        op.setCarryFlag([&] {
            op.outputDestVar(); out(" != tmp");
        });
        op.setOverflowFlag([&] {
            op.outputDestVar(); out(" != tmp");
        });
    }
}

void X86InstructionWriter::outputDiv(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 1);

    bool signedDiv = node.instruction->type() == Token::T_IDIV;
    auto flags = OpWriter::kExtraScope | (signedDiv ? OpWriter::kSigned : 0);

    OpWriter op(node, m_outputWriter, flags);
    op.fetchDestFromMemoryIfNeeded();

    // we will ignore overflow and division by zero
    switch (op.destSize()) {
    case 1:
        if (signedDiv) {
            out("int8_t quot = (int16_t)ax / "); op.outputDestVar(OpWriter::kRvalue); op.startNewLine();
            out("int8_t rem = (int16_t)ax % "); op.outputDestVar(OpWriter::kRvalue); op.startNewLine();
        } else {
            out("byte quot = ax / "); op.outputDestVar(OpWriter::kRvalue); op.startNewLine();
            out("byte rem = ax % "); op.outputDestVar(OpWriter::kRvalue); op.startNewLine();
        }

        out("al = quot"); op.startNewLine();
        out("ah = rem"); op.startNewLine();
        break;
    case 2:
        if (signedDiv) {
            out("int32_t dividend = ((int16_t)dx << 16) | (int16_t)ax"); op.startNewLine();
            out("int16_t quot = (int16_t)(dividend / "); op.outputDestVar(OpWriter::kRvalue); out(')'); op.startNewLine();
            out("int16_t rem = (int16_t)(dividend % "); op.outputDestVar(OpWriter::kRvalue); out(')'); op.startNewLine();
        } else {
            out("dword dividend = (dx << 16) | ax"); op.startNewLine();
            out("word quot = (word)(dividend / "); op.outputDestVar(OpWriter::kRvalue); out(')'); op.startNewLine();
            out("word rem = (word)(dividend % "); op.outputDestVar(OpWriter::kRvalue); out(')'); op.startNewLine();
        }

        out("ax = quot"); op.startNewLine();
        out("dx = rem"); op.startNewLine();
        break;
    case 4:
        if (signedDiv) {
            out("int64_t dividend = ((int64_t)edx << 32) | (int32_t)eax"); op.startNewLine();
            out("int32_t quot = (int32_t)(dividend / "); op.outputDestVar(OpWriter::kRvalue); out(')'); op.startNewLine();
            out("int32_t rem = (int32_t)(dividend % "); op.outputDestVar(OpWriter::kRvalue); out(')'); op.startNewLine();
        } else {
            out("uint64_t dividend = ((uint64_t)edx << 32) | eax"); op.startNewLine();
            out("dword quot = (dword)(dividend / "); op.outputDest(OpWriter::kRvalue); out(')'); op.startNewLine();
            out("dword rem = (dword)(dividend % "); op.outputDestVar(OpWriter::kRvalue); out(')'); op.startNewLine();
        }

        out("eax = quot"); op.startNewLine();
        out("edx = rem"); op.startNewLine();
        break;
    default:
        assert(false);
        break;
    }
}

void X86InstructionWriter::outputShift(const InstructionNode& node)
{
    auto instruction = node.instruction;
    assert(instruction->numOperands() == 2);

    bool signedShift = instruction->type() == Token::T_SAR;
    auto flags = OpWriter::kExtraScope | (signedShift ? OpWriter::kSigned : 0);

    OpWriter op(node, m_outputWriter, flags);

    auto srcConst = op.srcConstValue();

    if (!srcConst) {
        out("byte shiftCount = ");
        op.outputSrc(); out(" & 0x1f"); op.startNewLine();
        out("if (shiftCount) {");
        op.startNewLine(true, +1);
    } else if (!(*srcConst & 0x1f)) {
        op.discardOutput();
        return;
    }

    op.fetchDestFromMemoryIfNeeded();
    out(op.destType(), " res = "); op.outputDestVar(OpWriter::kRvalue); out(' ');

    switch (instruction->type()) {
    case Token::T_SHL:
        out("<<");
        break;
    case Token::T_SHR:
    case Token::T_SAR:
        out(">>");
        break;
    default:
        assert(false);
        break;
    }

    out(' ');
    if (srcConst)
        out(*srcConst & 0x1f);
    else
        out("shiftCount");
    op.startNewLine();

    if (!srcConst && !node.suppressCarryFlag) {
        out("if (shiftCount <= ", op.bitCount(), ')'); op.startNewLine(false, +1);
    }

    if (!srcConst || *srcConst <= 16) {
        op.setCarryFlag([&] {
            out("((", op.unsignedType(), ')'); op.outputDestVar(); out(" >> ");
            if (!srcConst) {
                out("(", op.bitCount(), " - shiftCount)");
                op.decreaseIndentLevel();
            } else {
                out(op.bitCount() - *srcConst);
            }
            out(") & 1");
        });
        if (!srcConst && !node.suppressCarryFlag) {
            out("else"); op.startNewLine(false, +1);
            op.setCarryFlag([&] {
                out("0");
                op.decreaseIndentLevel();
            });
        }
    } else if (*srcConst > 16) {
        op.setCarryFlag("0");
    }

    if (!srcConst && !node.suppressOverflowFlag) {
        out("if (shiftCount == 1)"); op.startNewLine(false, +1);
    }

    if (!srcConst || *srcConst == 1) {
        op.setOverflowFlag([&] {
            out("((("); op.outputDestVar(); out(" >> ", op.bitCount() - 1, ") & 1)");
            if (instruction->type() == Token::T_SHL) {
                out(" ^ (("); op.outputDestVar(); out(" >> ", op.bitCount() - 2, ") & 1)");
            }
            out(") != 0");
            if (!srcConst)
                op.decreaseIndentLevel();
        });
    }

    op.outputDestVar(); out(" = res"); op.startNewLine();
    op.writeBackDestToMemoryIfNeeded();

    op.setSignFlag([&] {
        out("(res & ", op.signMask(), ") != 0");
    });
    op.setZeroFlag("res == 0");
}

void X86InstructionWriter::outputRotate(const InstructionNode& node)
{
    auto instruction = node.instruction;

    assert(instruction->numOperands() == 2);

    OpWriter op(node, m_outputWriter, OpWriter::kExtraScope);
    op.fetchDestFromMemoryIfNeeded();

    auto type = instruction->type();

    bool carryRotation = type == Token::T_RCL || type == Token::T_RCR;
    int mod = op.destSize() * 8 + (carryRotation != 0);

    auto srcConst = op.srcConstValue();
    size_t rotationCount;

    if (!srcConst) {
        out("byte rotationCount = (("); op.outputDestVar(OpWriter::kRvalue); out(") & 0x1f) % ", mod);
        op.startNewLine();
    } else {
        rotationCount = (*srcConst & 0x1f) % mod;
        if (!rotationCount) {
            op.discardOutput();
            return;
        }
    }

    if (!srcConst) {
        out("if (rotationCount) {");
        op.startNewLine(false, +1);
    }

    switch (instruction->type()) {
    case Token::T_RCL:
    case Token::T_ROL:
        if (!node.suppressCarryFlag) {
            out("dword newCarry = ("); op.outputDestVar(OpWriter::kRvalue); out(" >> ");
            if (srcConst)
                out(op.bitCount() - rotationCount);
            else
                out('(', op.bitCount(), " - rotationCount)");
            out(") & 1"); op.startNewLine();
        }

        op.outputDestVar(); out(" = ("); op.outputDestVar(OpWriter::kRvalue); out(" << ");

        if (srcConst)
            out(rotationCount);
        else
            out("rotationCount");

        out(")");
        if (type != Token::T_RCL || !srcConst || rotationCount > 1) {
            out(" | (");
            if (type == Token::T_RCL)
                out('(');
            out('('); out(op.unsignedType()); out(')'); op.outputDestVar(); out(" >> ");

            if (srcConst) {
                auto shiftBy = op.bitCount() - rotationCount;
                out(shiftBy + (type == Token::T_RCL));
            } else {
                out('(', op.bitCount(), " - rotationCount");
            }
        }

        if (type == Token::T_RCL) {
            if (!srcConst)
                out(" + 1");
            else if (rotationCount > 1)
                out(") << 1)");
            out(" | flags.carry");
        } else {
            out(')');
            if (!srcConst)
                out(')');
        }

        op.startNewLine();

        op.setCarryFlag("newCarry");

        if (!node.suppressOverflowFlag && (!srcConst || rotationCount == 1)) {
            if (!srcConst) {
                out("if (rotationCount == 1)"); op.startNewLine(false, +1);
            }
            op.setOverflowFlag([&] {
                out("(("); op.outputDestVar(OpWriter::kRvalue); out(" & ", op.signMask(), ") != 0) ^ flags.carry");
                if (!srcConst)
                    op.decreaseIndentLevel();
            });
        }
        break;

    case Token::T_RCR:
    case Token::T_ROR:
        if (!node.suppressOverflowFlag && (!srcConst || rotationCount == 1)) {
            if (!srcConst) {
                out("if (rotationCount == 1)"); op.startNewLine(false, +1);
            }
            op.setOverflowFlag([&] {
                out("(("); op.outputDestVar(OpWriter::kRvalue); out(" & ", op.signMask(), ") != 0) ^ flags.carry");
                if (!srcConst)
                    op.decreaseIndentLevel();
            });
        }

        if (!node.suppressCarryFlag) {
            out("int newCarry = ("); op.outputDestVar(OpWriter::kRvalue); out(" >> ");
            if (srcConst)
                out(rotationCount - 1);
            else
                out("(rotationCount - 1)");
            out(") & 1"); op.startNewLine();
        }

        op.outputDestVar(); out(" = ((", op.unsignedType(), ')'); op.outputDestVar(OpWriter::kRvalue); out(" >> ");
        if (srcConst)
            out(rotationCount);
        else
            out("rotationCount");
        out(") | ");

        if (type == Token::T_RCR) {
            out("(flags.carry << ");
            if (srcConst)
                out(op.bitCount() - rotationCount);
            else
                out('(', op.bitCount(), " - rotationCount)");

            out(')');
            if (!srcConst || rotationCount > 1) {
                out(" | (((", op.unsignedType(), ')'); op.outputDestVar(OpWriter::kRvalue); out(" >> 1) << ");
                if (srcConst)
                    out(op.bitCount() - rotationCount + 1);
                else
                    out('(', op.bitCount(), " - rotationCount + 1)");
                out(')');
            }
            op.startNewLine();
        } else {
            out('('); op.outputDestVar(OpWriter::kRvalue); out(" << ");
            if (srcConst)
                out(op.bitCount() - rotationCount, ')');
            else
                out('(', op.bitCount(), " - rotationCount)");
            op.startNewLine();
        }
        op.setCarryFlag("newCarry");
        break;

    default:
        assert(false);
    }
}

void X86InstructionWriter::outputIntInOut(const InstructionNode& node)
{
    // TODO: add some warnings about these instructions?
    if (node.instruction->type() == Token::T_INT) {
        assert(node.instruction->numOperands() == 1);
        assert(node.opInfo[0].displacement.num);

        if (node.opInfo[0].constValue() == 3)
            out("debugBreak()");
        //else
        //    out("#pragma message(\"Naughty INT instruction found\")");
    } else if (node.instruction->type() == Token::T_STI) {
        OpWriter op(node, m_outputWriter);
        op.setCarryFlag(true);
    }
}

void X86InstructionWriter::outputNeg(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 1);

    OpWriter op(node, m_outputWriter, OpWriter::kSigned);
    op.fetchDestFromMemoryIfNeeded();

    op.setCarryFlag([&] {
        op.outputDestVar(); out(" != 0");
    });
    op.setOverflowFlag([&] {
        op.outputDestVar(), out(" == ", op.signedMin());
    });

    op.outputDestVar(); out(" = -"); op.outputDestVar(); op.startNewLine();

    op.setSignFlag([&] {
        out('('); op.outputDestVar(); out(" & ", op.signMask(), ") != 0");
    });
    op.setZeroFlag([&] {
        op.outputDestVar(); out(" == 0");
    });

    op.writeBackDestToMemoryIfNeeded();
}

void X86InstructionWriter::outputNot(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 1);

    OpWriter op(node, m_outputWriter);
    op.fetchDestFromMemoryIfNeeded();

    op.outputDestVar(); out(" = ~("); op.outputDestVar(OpWriter::kRvalue); out(')'); op.startNewLine();
}

void X86InstructionWriter::outputLoop(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 1);
    assert(node.instruction->prefix().empty());

    outputConditionalJump(node, "--ecx");
}

void X86InstructionWriter::outputPushf(const InstructionNode& node)
{
    out("stack[--stackTop] = "
        "flags.carry | "
        "(flags.zero << 1) | "
        "(flags.sign << 2) | "
        "(flags.overflow << 3)");
}

void X86InstructionWriter::outputPopf(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 0);

    out("pop(tmp);", kNewLine);
    out(kIndent, "flags.carry = tmp & 1;", kNewLine);
    out(kIndent, "flags.zero = (tmp >> 2) & 1;", kNewLine);
    out(kIndent, "flags.sign = (tmp >> 3) & 1;", kNewLine);
    out(kIndent, "flags.overflow = (tmp >> 4) & 1");
}

void X86InstructionWriter::outputPusha(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 0);

    out("tmp = stackTop;", kNewLine);
    out(kIndent, "push(eax);", kNewLine);
    out(kIndent, "push(ecx);", kNewLine);
    out(kIndent, "push(edx);", kNewLine);
    out(kIndent, "push(ebx);", kNewLine);
    out(kIndent, "push(tmp);", kNewLine);
    out(kIndent, "push(ebp);", kNewLine);
    out(kIndent, "push(esi);", kNewLine);
    out(kIndent, "push(edi)");
}

void X86InstructionWriter::outputPopa(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 0);

    out("pop(edi);", kNewLine);
    out(kIndent, "pop(esi);", kNewLine);
    out(kIndent, "pop(ebp);", kNewLine);
    out(kIndent, "stackTop++;", kNewLine);
    out(kIndent, "pop(ebx);", kNewLine);
    out(kIndent, "pop(edx);", kNewLine);
    out(kIndent, "pop(ecx);", kNewLine);
    out(kIndent, "pop(eax)");
}

void X86InstructionWriter::outputStc(const InstructionNode& node)
{
    OpWriter op(node, m_outputWriter);
    op.setCarryFlag(true);
}

void X86InstructionWriter::outputClc(const InstructionNode& node)
{
    OpWriter op(node, m_outputWriter);
    op.setCarryFlag(false);
}

void X86InstructionWriter::outputCld(const InstructionNode& node)
{
    // nop
}

void X86InstructionWriter::outputSetz(const InstructionNode& node)
{
    assert(node.instruction->numOperands() == 1);

    OpWriter op(node, m_outputWriter);
    op.fetchDestFromMemoryIfNeeded();
    op.outputDestVar(); out(" = flags.zero != 0"); op.startNewLine();
    op.writeBackDestToMemoryIfNeeded();
}

void X86InstructionWriter::outputConditionalJump(const InstructionNode& node, const char *condition /* = nullptr */)
{
    auto instruction = node.instruction;

    assert(instruction && instruction->isBranch() && instruction->numOperands() == 1);

    auto target = instruction->operands()[0].begin();

    if (target->type() == Token::T_SHORT)
        target = target->next();

    if (!target->text().startsWith('$')) {
        assert(target->next() == instruction->operands()[0].end());

        const auto& label = target->text();
        bool localLabel = label.startsWith('@');

        if (condition)
            out("if (", condition, ")", kNewLine, kIndent, kIndent);

        if (node.startOverJump) {
            out("goto ", CppOutput::kStartLabel);
        } else if (localLabel || m_inProcLabels.present(target->text())) {
            if (target->text() == "return" || node.returnJump) {
                out("return");
            } else {
                out("goto ");
                m_outputWriter.outputLabel(label);
            }
        } else {
            if (isRetnNext(node)) {
                outputFunctionInvoke(label);
            } else {
                out("{ ");
                outputFunctionInvoke(label);
                out("; return; }");
            }
        }
    }
}

void X86InstructionWriter::outputJmp(const InstructionNode& node)
{
    outputConditionalJump(node);
}

void X86InstructionWriter::outputJg(const InstructionNode& node)
{
    outputConditionalJump(node, "!flags.zero && flags.sign == flags.overflow");
}

void X86InstructionWriter::outputJge(const InstructionNode& node)
{
    outputConditionalJump(node, "flags.sign == flags.overflow");
}

void X86InstructionWriter::outputJl(const InstructionNode& node)
{
    outputConditionalJump(node, "flags.sign != flags.overflow");
}

void X86InstructionWriter::outputJz(const InstructionNode& node)
{
    outputConditionalJump(node, "flags.zero");
}

void X86InstructionWriter::outputJnz(const InstructionNode& node)
{
    outputConditionalJump(node, "!flags.zero");
}

void X86InstructionWriter::outputJs(const InstructionNode& node)
{
    outputConditionalJump(node, "flags.sign");
}

void X86InstructionWriter::outputJns(const InstructionNode& node)
{
    outputConditionalJump(node, "!flags.sign");
}

void X86InstructionWriter::outputJb(const InstructionNode& node)
{
    outputConditionalJump(node, "flags.carry");
}

void X86InstructionWriter::outputJa(const InstructionNode& node)
{
    outputConditionalJump(node, "!flags.carry && !flags.zero");
}

void X86InstructionWriter::outputJnb(const InstructionNode& node)
{
    outputConditionalJump(node, "!flags.carry");
}

void X86InstructionWriter::outputJbe(const InstructionNode& node)
{
    outputConditionalJump(node, "flags.carry || flags.zero");
}

void X86InstructionWriter::outputJle(const InstructionNode& node)
{
    outputConditionalJump(node, "flags.zero || flags.sign != flags.overflow");
}

void X86InstructionWriter::outputJo(const InstructionNode& node)
{
    outputConditionalJump(node, "flags.overflow");
}

void X86InstructionWriter::outputJno(const InstructionNode& node)
{
    outputConditionalJump(node, "!flags.overflow");
}

void X86InstructionWriter::outputFunctionInvoke(const String& target)
{
    auto isCallReg = [&]() {
        static const char *kRegs[] = { "ax", "bx", "cx", "dx", "si", "di", "bp" };

        if (target.length() != 3 || !target.startsWith('e'))
            return false;

        return std::find_if(std::begin(kRegs), std::end(kRegs), [&](const char *suffix) {
            return target[1] == suffix[0] && target[2] == suffix[1];
        }) != std::end(kRegs);
    };

    auto regIndex = amigaRegisterToIndex(target);
    auto isVar = m_dataBank.isVariable(target);
    if (target.startsWith('-') || regIndex >= 0 || isCallReg() || isVar) {
        out("invokeProc(");
        if (isVar) {
            auto offset = m_dataBank.getVarOffset(target);
            out("g_memDword[", offset / 4, ']');
        } else {
            out(target);
        }
        out(')');
    } else {
        outputFunctionCall(target);
    }
}

bool X86InstructionWriter::isRetnNext(const InstructionNode& node)
{
    auto next = node.next;

    while (next->type() != OutputItem::kEndProc && next->type() != OutputItem::kInstruction)
        next = next->next();

    if (next->type() != OutputItem::kEndProc) {
        auto instruction = next->getItem<Instruction>();
        if (instruction->type() == Token::T_RETN)
            return true;
    } else {
        return true;
    }

    return false;
}
