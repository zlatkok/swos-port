#include "VerbatimOutput.h"

constexpr int kExpectedPrefixSize = 300;

VerbatimOutput::VerbatimOutput(const char *path, const SymbolFileParser& symFileParser, const StructStream& structs,
    const DefinesMap& defines, const OutputItemStream& outputItems)
:
    OutputWriter(path, symFileParser, structs, defines, References(), outputItems)
{
    m_outputPrefix.reserve(kExpectedPrefixSize);
}

void VerbatimOutput::setOutputPrefix(const std::string& prefix)
{
    m_outputPrefix = prefix;
    ensureNewLineEnd(m_outputPrefix);
}

void VerbatimOutput::setDisassemblyPrefix(const std::string& prefix)
{
    m_disassemblyPrefix = prefix;
    ensureNewLineEnd(m_disassemblyPrefix);
}

bool VerbatimOutput::output(OutputFlags flags, CToken *)
{
    if (!openOutputFile(flags))
        return false;

    if (!flags)
        return true;

    if (!(flags & (kStructs | kDefines)))
        out("include defs.inc", Util::kDoubleNewLine);

    if (flags & kStructs)
        outputStructs();

    if (flags & kDefines)
        outputDefines();

    if (flags & kDisassembly)
        outputDisassembly();

    out("end", Util::kNewLine);

    bool result = save();
    closeOutputFile();

    return result;
}

const char *VerbatimOutput::getDefsFilename() const
{
    return "defs.inc";
}

std::string VerbatimOutput::segmentDirective(const TokenRange& range) const
{
    std::string result;

    for (auto token = range.first; token != range.second; advance(token))
        result += token->string() + ' ';

    if (!result.empty())
        result.replace(result.end() - 1, result.end(), Util::kNewLine.begin(), Util::kNewLine.end());

    return result;
}

std::string VerbatimOutput::endSegmentDirective(const TokenRange& range) const
{
    return range.first->string() + " ends" + Util::kNewLineString();
}

void VerbatimOutput::outputStructs()
{
    for (auto& struc : m_structs)
        outputStruct(struc);
}

void VerbatimOutput::outputStruct(const Struct& struc)
{
    int column = outputComment(struc.leadingComments());
    column += out(struc.name(), struc.isUnion() ? " union" : " struc");
    outputComment(struc.lineComment(), column);

    for (auto& field : struc) {
        column = out(kIndent, field.name());
        if (!field.name().empty())
            column += out(' ');

        if (!field.type().empty()) {
            column += out(field.type(), ' ');
        } else {
            auto fieldSize = dataSizeSpecifier(field.elementSize());
            column += out(fieldSize, ' ');
        }

        if (!field.dup().empty())
            column += out(field.dup(), " dup(?)");
        else
            column += out('?');

        outputComment(field.comment(), column);
    }

    out(struc.name(), " ends", Util::kNewLine);
}

void VerbatimOutput::outputDefines()
{
    for (const auto& it : m_defines) {
        auto def = it.cargo;

        int column = outputComment(def->leadingComments());
        column += out(def->name(), " = ");

        if (def->isInverted())
            column += out(" not ");

        column += out(def->value());
        outputComment(def->comment(), column);
    }
}

void VerbatimOutput::outputDisassembly()
{
    for (const auto& item : m_outputItems) {
        int column = outputComment(item.leadingComments());
        switch (item.type()) {
        case OutputItem::kInstruction:
            column = outputInstruction(item.getItem<Instruction>());
            break;
        case OutputItem::kDataItem:
            column = outputDataItem(item.getItem<DataItem>());
            break;
        case OutputItem::kProc:
            column = outputProc(item.getItem<Proc>());
            break;
        case OutputItem::kEndProc:
            column = outputEndProc(item.getItem<EndProc>());
            break;
        case OutputItem::kLabel:
            column = outputLabel(item.getItem<Label>());
            break;
        case OutputItem::kStackVariable:
            column = outputStackVariable(item.getItem<StackVariable>());
            break;
        case OutputItem::kDirective:
            column = outputDirective(item.getItem<Directive>());
            break;
        case OutputItem::kSegment:
            column = outputSegment(item.getItem<Segment>());
            break;
        default:
            assert(false && "Unhandled output item type");
            break;
        }
        outputComment(item.comment(), column);
    }
}

int VerbatimOutput::outputInstruction(const Instruction *instruction)
{
    int column = 0;

    if (m_currentProc)
        column = out(kIndent, kIndent);

    if (!instruction->prefix().empty())
        column += out(instruction->prefix(), ' ');

    column += out(instruction->instructionText());

    if (instruction->hasOperands()) {
        auto spaces = &OutputWriter::kIndent[std::min<int>(instruction->instructionText().length(), 4) - 1];
        column += out(spaces);
    }

    const auto& operands = instruction->operands();

    for (int i = 0; ; i++) {
        const auto& operand = operands[i];

        for (auto it = operand.begin(); it != operand.end(); it = it->next()) {
            column += out(it->text());

            if (needsSpaceDelimiter(it, operand.end()))
                column += out(' ');
        }

        if (i == operands.size() - 1 || operands[i + 1].begin() == operands[i + 1].end())
            break;

        column += out(", ");
    }

    return column;
}

int VerbatimOutput::outputDataItem(const DataItem *item)
{
    int column = 0;

    if (!item->name().empty())
        column = out(item->name(), ' ');

    if (!item->structName().empty()) {
        column += out(item->structName(), ' ');
    } else {
        auto sizeSpecifier = dataSizeSpecifier(item->size());
        column += out(sizeSpecifier, ' ');
    }

    auto element = item->initialElement();
    bool isString = item->numElements() > 0 && element->type() == DataItem::kString;

    for (size_t i = 0; i < item->numElements(); i++, element = element->next()) {
        bool isOffset = (element->type() & DataItem::kIsOffsetFlag) != 0;

        if (isOffset)
            column += out("offset ");

        if (element->dup())
            column += out(element->dup(), " dup(", element->text(), ')');
        else
            column += out(element->text());

        if (isOffset && element->offset())
            column += out('+', element->offset());

        bool last = i == item->numElements() - 1;
        if (!last) {
            out(',');
            // another IDA formatting weirdness
            if (!isString)
                out(' ');
        }
    }

    return column;
}

int VerbatimOutput::outputProc(const Proc *proc)
{
    m_currentProc = proc;
    // assume everything is near
    return out(proc->name(), " proc near");
}

int VerbatimOutput::outputEndProc(const EndProc *endProc)
{
    return out(endProc->name(), " endp");
}

int VerbatimOutput::outputLabel(const Label *label)
{
    return out(label->name(), ':');
}

int VerbatimOutput::outputStackVariable(const StackVariable *var)
{
    return out(var->name(), "= ", var->sizeString(), " ptr ", var->offsetString());
}

int VerbatimOutput::outputDirective(const Directive *directive)
{
    int column = out(directive->directive());

    auto count = directive->paramCount();
    auto param = directive->begin();

    while (count--) {
        if (param->text() != ",")
            column += out(' ');
        column += out(param->text());
        param = param->next();
    }

    return column;
}

int VerbatimOutput::outputSegment(const Segment *segment)
{
    assert(segment && segment->count() > 1);

    int column = 0;
    auto param = segment->begin();

    out(param->text());
    param = param->next();

    for (size_t i = 1; i < segment->count(); i++) {
        // with `use32' MASM refuses to call procs in data segment from code segment
        if (param->text() != "use32")
            column += out(' ', param->text());
        param = param->next();
    }

    return column;
}

const char *VerbatimOutput::dataSizeSpecifier(size_t size)
{
    switch (size) {
    case 1: return "db";
    case 2: return "dw";
    case 4: return "dd";
    case 8: return "dq";
    default:
        assert(false);
        return nullptr;
    }
}

// try matching IDA's weird rules about whitespace delimiter placing in an instruction
bool VerbatimOutput::needsSpaceDelimiter(const Instruction::Operand *op, const Instruction::Operand *end)
{
    if (op->next() == end)
        return false;

    auto type = op->type();
    auto nextType = op->next()->type();

    return Token::isText(type) && Token::isText(nextType) || type != Token::T_ID && nextType == Token::T_LBRACKET;
}

void VerbatimOutput::ensureNewLineEnd(std::string& str)
{
    if (!str.empty() && !Util::endsWith(str, Util::kNewLine))
        str.append(std::begin(Util::kNewLine), std::end(Util::kNewLine));
}
