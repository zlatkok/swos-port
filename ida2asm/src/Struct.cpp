#include "Struct.h"
#include "Util.h"

constexpr int kStructBufferSize = 30'000;

static bool structStrCmp(const String& s1, const String& s2)
{
    size_t i = 0, j = 0;

    for (; i < s1.length() && j < s2.length(); i++, j++) {
        auto c1 = s1[i];
        auto c2 = s2[j];

        while (c1 == '_') {
            if (++i >= s1.length())
                goto done;
            c1 = s1[i];
        }
        while (c2 == '_') {
            if (++j >= s1.length())
                goto done;
            c2 = s1[j];
        }

        if (c1 >= 'A' && c1 <= 'Z')
            c1 |= 0x20;
        if (c2 >= 'A' && c2 <= 'Z')
            c2 |= 0x20;

        if (c1 != c2)
            return false;
    }

done:
    if (i >= s1.length())
        return j >= s2.length();
    if (j >= s2.length())
        return i >= s1.length();

    assert(false);
    return false;   // should never be reached
}

Struct::Struct(CToken *name, const TokenList& leadingComments, CToken *comment, bool isUnion, size_t size) : m_isUnion(isUnion)
{
    assert(name);
    assert(size == requiredSize(name, leadingComments, comment));

    Util::assignSize(m_size, size);

    Util::assignSize(m_nameLength, name->textLength);
    name->copyText(namePtr());
    m_hash = name->hash;

    auto leadingCommentsLength = Token::flattenTokenList(leadingComments, leadingCommentsPtr());
    Util::assignSize(m_leadingCommentsLength, leadingCommentsLength);

    m_lineCommentLength = 0;
    if (comment) {
        Util::assignSize(m_lineCommentLength, comment->textLength);
        comment->copyText(lineCommentPtr());
    }
}

size_t Struct::requiredSize(CToken *name, const TokenList& leadingComments, CToken *comment)
{
    auto size = sizeof(Struct) + Token::tokenListLength(leadingComments);

    if (name)
        size += name->textLength;

    if (comment)
        size += comment->textLength;

    return size;
}

bool Struct::operator==(const String& str) const
{
    return structStrCmp({ namePtr(), m_nameLength }, str);
}

char *Struct::namePtr() const
{
    return (char *)(this + 1);
}

String Struct::name() const
{
    return { namePtr(), m_nameLength };
}

Util::hash_t Struct::hash() const
{
    return m_hash;
}

String Struct::leadingComments() const
{
    return { leadingCommentsPtr(), m_leadingCommentsLength };
}

String Struct::lineComment() const
{
    return { lineCommentPtr(), m_lineCommentLength };
}

bool Struct::isUnion() const
{
    return m_isUnion != 0;
}

bool Struct::dupNotAllowed() const
{
    return (m_flags & kCantBeDuped) != 0;
}

void Struct::disallowDup()
{
    m_flags = static_cast<Flags>(m_flags | kCantBeDuped);
}

char *Struct::leadingCommentsPtr() const
{
    return namePtr() + m_nameLength;
}

char *Struct::lineCommentPtr() const
{
    return leadingCommentsPtr() + m_leadingCommentsLength;
}

Struct *Struct::next() const
{
    return reinterpret_cast<Struct *>((char *)this + m_size);
}

void Struct::increaseSize(uint32_t increment)
{
    m_size += increment;
}

void Struct::addComment(const String& comment, Struct::Field *lastField)
{
    assert(!comment.empty());

    if (lastField) {
        comment.copy(lastField->commentPtr() + lastField->m_commentLength);
        Util::assertSize(lastField->m_commentLength, lastField->m_commentLength + comment.length());
        lastField->m_commentLength += static_cast<uint8_t>(comment.length());
        lastField->m_size += comment.length();
    } else {
        comment.copy(lineCommentPtr() + m_lineCommentLength);
        Util::assertSize(m_lineCommentLength, m_lineCommentLength + comment.length());
        m_lineCommentLength += static_cast<uint8_t>(comment.length());
    }

    Util::assertSize(m_size, m_size + comment.length());
    m_size += static_cast<uint16_t>(comment.length());
}

auto Struct::begin() const -> Iterator::Iterator<Field>
{
    return reinterpret_cast<Field *>(lineCommentPtr() + m_lineCommentLength);
}

auto Struct::end() const -> Iterator::Iterator<Field>
{
    return reinterpret_cast<Field *>((char *)this + m_size);
}

StructStream::StructStream()
    : m_structs(kStructBufferSize), m_structMap(kAverageBytesPerStruct * 50)
{
}

size_t StructStream::count() const
{
    return m_count;
}

size_t StructStream::size() const
{
    return m_structs.spaceUsed();
}

bool StructStream::empty() const
{
    return size() <= 0;
}

void StructStream::addStruct(CToken *name, const TokenList& leadingComments, CToken *lineComment, bool isUnion)
{
    assert(name);

    m_count++;
    auto size = Struct::requiredSize(name, leadingComments, lineComment);
    auto buf = m_structs.add(size);

    m_lastStruct = new (buf) Struct(name, leadingComments, lineComment, isUnion, size);
    m_lastField = nullptr;

    m_structMap.add(name, m_lastStruct, 0);
}

void StructStream::addField(CToken *name, size_t byteSize, CToken *comment, CToken *type, CToken *dup)
{
    assert(m_lastStruct);

    auto size = Struct::Field::requiredSize(name, comment, type, dup);
    auto buf = m_structs.add(size);
    m_lastField = new (buf) Struct::Field(name, byteSize, comment, type, dup);
    m_lastStruct->increaseSize(size);
}

void StructStream::addComment(CToken *token)
{
    assert(m_lastStruct);
    assert(token && token->textLength && token->category == Token::kWhitespace);

    m_structs.add(token->textLength);
    m_lastStruct->addComment(token, m_lastField);
}

void StructStream::disallowDup()
{
    assert(m_lastStruct);
    m_lastStruct->disallowDup();
}

void StructStream::seal()
{
    determineTraversalOrderAndStructSizes();
}

String StructStream::lastStructName() const
{
    assert(m_lastStruct);
    return m_lastStruct->name();
}

auto StructStream::begin() const -> Iterator
{
    return { m_structOrder };
}

auto StructStream::end() const -> Iterator
{
    return { m_structOrder + m_count };
}

Struct *StructStream::findStruct(const String& name) const
{
    auto structInfo = m_structMap.get(name);
    return structInfo ? structInfo->ptr : nullptr;
}

size_t StructStream::getStructSize(const String& name) const
{
    auto structInfo = m_structMap.get(name);
    assert(structInfo);
    return structInfo->size;
}

// determine struct traversal order such that structs that contain other structs come after them
// (so there are no forward references); along the way, calculate byte sizes as well
void StructStream::determineTraversalOrderAndStructSizes()
{
    auto begin = reinterpret_cast<Struct *>(m_structs.begin());
    auto end = reinterpret_cast<Struct *>(m_structs.end());

    constexpr auto kStructPtrSize = sizeof(Struct *);
    auto padding = (kStructPtrSize - m_structs.spaceUsed() % kStructPtrSize) % kStructPtrSize;
    auto pointersBuff = m_structs.add(m_count * kStructPtrSize + padding);

    m_structOrder = reinterpret_cast<Struct **>(pointersBuff + padding);
    m_currentStructPos = 0;

    m_structMap.seal();

    for (auto struc = begin; struc != end; struc = struc->next()) {
        auto info = m_structMap.get(struc->name(), struc->hash());
        if (!info->size)
            info->size = getStructSizeAndOrder(info);
    }
}

size_t StructStream::getStructSizeAndOrder(StructInfoHolder *struc)
{
    assert(struc->size == 0 && m_structOrder);

    size_t size = 0;

    for (const auto& field : *struc->ptr) {
        if (field.elementSize()) {
            size += field.byteSize();
        } else {
            auto nestedStruct = m_structMap.get(field.type());
            if (!nestedStruct->size)
                nestedStruct->size = getStructSizeAndOrder(nestedStruct);
            auto nestedSize = nestedStruct->size;
            if (field.dup())
                nestedSize *= field.dup().toInt();
            size += nestedSize;
        }
    }

    assert(m_currentStructPos < m_count);

    m_structOrder[m_currentStructPos++] = struc->ptr;
    return size;
}
