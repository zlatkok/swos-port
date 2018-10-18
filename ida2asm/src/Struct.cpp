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
    return false;   // should never happen
}

Struct::Struct(CToken *name, const TokenList& leadingComments, CToken *comment, bool isUnion, size_t size) : m_isUnion(isUnion)
{
    assert(name);
    assert(size == requiredSize(name, leadingComments, comment));

    Util::assignSize(m_size, size);

    Util::assignSize(m_nameLength, name->textLength);
    memcpy(namePtr(), name->text(), name->textLength);
    m_hash = name->hash;

    auto leadingCommentsLength = Token::flattenTokenList(leadingComments, leadingCommentsPtr());
    Util::assignSize(m_leadingCommentsLength, leadingCommentsLength);

    m_lineCommentLength = 0;
    if (comment) {
        Util::assignSize(m_lineCommentLength, comment->textLength);
        memcpy(lineCommentPtr(), comment->text(), comment->textLength);
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

bool Struct::resolved() const
{
    return (m_flags & kResolved) != 0;
}

void Struct::resolve()
{
    m_flags = static_cast<Flags>(m_flags | kResolved);
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
        memcpy(lastField->commentPtr() + lastField->m_commentLength, comment.str(), comment.length());
        Util::assertSize(lastField->m_commentLength, lastField->m_commentLength + comment.length());
        lastField->m_commentLength += static_cast<uint8_t>(comment.length());
        lastField->m_size += comment.length();
    } else {
        memcpy(lineCommentPtr() + m_lineCommentLength, comment.str(), comment.length());
        Util::assertSize(m_lineCommentLength, m_lineCommentLength + comment.length());
        m_lineCommentLength += static_cast<uint8_t>(comment.length());
    }

    Util::assertSize(m_size, m_size + comment.length());
    m_size += static_cast<uint16_t>(comment.length());
}

auto Struct::begin() const -> Util::Iterator<Field>
{
    return reinterpret_cast<Field *>(lineCommentPtr() + m_lineCommentLength);
}

auto Struct::end() const -> Util::Iterator<Field>
{
    return reinterpret_cast<Field *>((char *)this + m_size);
}

StructStream::StructStream() : m_structs(kStructBufferSize)
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

void StructStream::addStruct(CToken *name, const TokenList& leadingComments, CToken *lineComment, bool isUnion)
{
    assert(name);

    m_count++;
    auto size = Struct::requiredSize(name, leadingComments, lineComment);
    auto buf = m_structs.add(size);

    m_lastStruct = new (buf) Struct(name, leadingComments, lineComment, isUnion, size);
    m_lastField = nullptr;
}

void StructStream::addField(CToken *name, size_t fieldLength, CToken *comment, CToken *type, CToken *dup)
{
    assert(m_lastStruct);

    auto size = Struct::Field::requiredSize(name, comment, type, dup);
    auto buf = m_structs.add(size);
    m_lastField = new (buf) Struct::Field(name, fieldLength, comment, type, dup);
    m_lastStruct->increaseSize(size);
}

void StructStream::addComment(CToken *token)
{
    assert(m_lastStruct);
    assert(token && token->textLength && token->category == Token::Whitespace);

    m_structs.add(token->textLength);
    m_lastStruct->addComment(token, m_lastField);
}

void StructStream::disallowDup()
{
    assert(m_lastStruct);
    m_lastStruct->disallowDup();
}

String StructStream::lastStructName() const
{
    assert(m_lastStruct);
    return m_lastStruct->name();
}

Util::Iterator<Struct> StructStream::begin() const
{
    return reinterpret_cast<Struct *>(m_structs.begin());
}

Util::Iterator<Struct> StructStream::end() const
{
    return reinterpret_cast<Struct *>(m_structs.end());
}

Struct *StructStream::findStruct(const String& name) const
{
    for (auto struc : *this)
        if (struc->name() == name)
            return struc;

    return nullptr;
}
