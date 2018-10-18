#include "OutputItem.h"
#include "DataItem.h"
#include "Util.h"

constexpr int kOutputItemsBufferSize = 12'000'000;

OutputItem::OutputItem(Type type, size_t size, const TokenList& leadingComments, CToken *comment)
    : m_type(type), m_lineCommentLength(0)
{
    Util::assignSize(m_size, size);

    auto leadingCommentsSize = Token::flattenTokenList(leadingComments, leadingCommentsPtr());
    Util::assignSize(m_leadingCommentsLength, leadingCommentsSize);

    if (comment) {
        Util::assignSize(m_lineCommentLength, comment->textLength);
        memcpy(commentPtr(), comment->text(), comment->textLength);
    }
}

size_t OutputItem::requiredSize(const TokenList& leadingComments, CToken *comment)
{
    auto size = sizeof(OutputItem) + Token::tokenListLength(leadingComments);

    if (comment)
        size += comment->textLength;

    return size;
}

OutputItem::Type OutputItem::type() const
{
    return m_type;
}

String OutputItem::leadingComments() const
{
    return { leadingCommentsPtr(), m_leadingCommentsLength };
}

String OutputItem::comment() const
{
    return { commentPtr(), m_lineCommentLength };
}

size_t OutputItem::size() const
{
    return m_size;
}

void OutputItem::increaseSize(size_t size)
{
    Util::assertSize(m_size, m_size + size);
    m_size += static_cast<uint16_t>(size);
}

OutputItem *OutputItem::next() const
{
    return reinterpret_cast<OutputItem *>((char *)this + m_size);
}

char *OutputItem::leadingCommentsPtr() const
{
    return (char *)(this + 1);
}

char *OutputItem::commentPtr() const
{
    return leadingCommentsPtr() + m_leadingCommentsLength;
}

void *OutputItem::tail() const
{
    return commentPtr() + m_lineCommentLength;
}

OutputItemStream::OutputItemStream() : m_items(kOutputItemsBufferSize)
{
}

size_t OutputItemStream::size() const
{
    return m_items.spaceUsed();
}

void OutputItemStream::addInstruction(const TokenList& leadingComments, CToken *comment, CToken *prefix,
    CToken *insnToken, const Instruction::OperandSizes& opSizes, const Instruction::OperandTypes& opTypes,
    const Instruction::OperandTokens& opTokens)
{
    auto size = Instruction::requiredSize(prefix, insnToken, opTokens);
    addOutputItem<Instruction, OutputItem::kInstruction>(leadingComments, comment, size, prefix,
        insnToken, opSizes, opTypes, opTokens);
}

void OutputItemStream::addProc(const TokenList& leadingComments, CToken *comment, CToken *name)
{
    auto size = Proc::requiredSize(name);
    addOutputItem<Proc, OutputItem::kProc>(leadingComments, comment, size, name);
}

void OutputItemStream::addEndProc(const TokenList& leadingComments, CToken *comment, CToken *name)
{
    auto size = EndProc::requiredSize(name);
    addOutputItem<EndProc, OutputItem::kEndProc>(leadingComments, comment, size, name);
}

void OutputItemStream::addStackVariable(const TokenList& leadingComments, CToken *comment,
    const String& name, CToken *size, CToken *offset)
{
    auto varSize = StackVariable::requiredSize(name, size, offset);
    addOutputItem<StackVariable, OutputItem::kStackVariable>(leadingComments, comment, varSize, name, size, offset);
}

void OutputItemStream::addDataItem(const TokenList& leadingComments, CToken *comment, CToken *name,
    CToken *structName, uint8_t baseSize)
{
    auto size = DataItem::requiredSize(name, structName);
    addOutputItem<DataItem, OutputItem::kDataItem>(leadingComments, comment, size, name, structName, baseSize);
}

void OutputItemStream::addDataElement(CToken *token, bool isOffset, int offset, size_t dup)
{
    assert(m_lastItem && m_lastItem->type() == OutputItem::kDataItem);

    auto size = DataItem::requiredElementSize(token, offset);
    auto buf = m_items.add(size);

    m_lastItem->increaseSize(size);

    assert(m_lastItem->type() == OutputItem::kDataItem);
    auto lastDataItem = m_lastItem->getItem<DataItem>();

    lastDataItem->addElement(buf, token, isOffset, offset, dup);
}

void OutputItemStream::addLabel(const TokenList& leadingComments, CToken *comment, CToken *token)
{
    assert(token);

    auto size = Label::requiredSize(token);
    addOutputItem<Label, OutputItem::kLabel>(leadingComments, comment, size, token);
}

void OutputItemStream::addDirective(const TokenList& leadingComments, CToken *comment, CToken *begin, CToken *end)
{
    assert(begin && begin < end);

    auto size = Directive::requiredSize(begin, end);
    addOutputItem<Directive, OutputItem::kDirective>(leadingComments, comment, size, begin, end);
}

void OutputItemStream::addSegment(const TokenList& leadingComments, CToken *comment, CToken *begin, CToken *end)
{
    assert(begin && end && end > begin);

    auto size = Segment::requiredSize(begin, end);
    addOutputItem<Segment, OutputItem::kSegment>(leadingComments, comment, size, begin, end);
}

void OutputItemStream::addTrailingComments(const TokenList& comments)
{
    auto bufferSize = OutputItem::requiredSize(comments, nullptr);
    auto buffer = m_items.add(bufferSize);

    m_lastItem = new (buffer) OutputItem(OutputItem::kComment, bufferSize, comments, nullptr);
}

DataItem *OutputItemStream::lastDataItem() const
{
    assert(m_lastItem->type() == OutputItem::kDataItem);
    return m_lastItem->getItem<DataItem>();
}

const Util::Iterator<const OutputItem> OutputItemStream::begin() const
{
    return reinterpret_cast<OutputItem *>(m_items.begin());
}

const Util::Iterator<const OutputItem> OutputItemStream::end() const
{
    return reinterpret_cast<OutputItem *>(m_items.end());
}

template<typename T, OutputItem::Type type, typename... Args>
void OutputItemStream::addOutputItem(const TokenList& leadingComments, CToken *comment, size_t itemSize, Args... args)
{
    auto headerSize = OutputItem::requiredSize(leadingComments, comment);

    auto bufferSize = headerSize + itemSize;
    auto buffer = m_items.add(bufferSize);

    m_lastItem = new (buffer) OutputItem(type, bufferSize, leadingComments, comment);
    auto item = new (buffer + headerSize) T(args...);
}
