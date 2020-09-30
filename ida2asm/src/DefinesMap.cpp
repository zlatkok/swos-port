#include "DefinesMap.h"

DefinesMap::Define::Define(const TokenList& leadingComments, CToken *comment, CToken *name, CToken *value, bool inverted)
{
    static_assert(std::numeric_limits<std::underlying_type<decltype(m_type)>::type>::max() ==
        std::numeric_limits<uint16_t>::max(), "Fix inverted flag");
    assert(name && value);

    m_value = value->parseInt();
    m_type = value->type;

    if (inverted) {
        m_type |= kInvertedFlag;
        m_value = ~m_value;
    }

    auto leadingCommentsLength = Token::flattenTokenList(leadingComments, leadingCommentsPtr());
    Util::assignSize(m_leadingCommentsLength, leadingCommentsLength);

    m_commentLength = 0;
    if (comment) {
        m_commentLength = comment->textLength;
        comment->copyText(lineCommentPtr());
    }

    m_nameLength = name->textLength;
    name->copyText(namePtr());

    m_valueLength = value->textLength;
    value->copyText(valuePtr());

    assert(requiredSize(leadingComments, comment, name, value, inverted) <= kMaxLength);
}

size_t DefinesMap::Define::requiredSize(const TokenList& leadingComments, CToken *comment, CToken *name, CToken *value, bool)
{
    assert(name && value);
    auto commentLength = comment ? comment->textLength : 0;
    return sizeof(Define) + Token::tokenListLength(leadingComments) + commentLength + name->textLength + value->textLength;
}

Token::Type DefinesMap::Define::type() const
{
    return m_type & ~kInvertedFlag;
}

bool DefinesMap::Define::isInverted() const
{
    return (m_type & kInvertedFlag) != 0;
}

String DefinesMap::Define::name() const
{
    return { namePtr(), m_nameLength };
}

String DefinesMap::Define::value() const
{
    return { valuePtr(), m_valueLength };
}

int DefinesMap::Define::intValue() const
{
    return m_value;
}

String DefinesMap::Define::leadingComments() const
{
    return { leadingCommentsPtr(), m_leadingCommentsLength };
}

String DefinesMap::Define::comment() const
{
    return { lineCommentPtr(), m_commentLength };
}

auto DefinesMap::Define::next() const -> const Define *
{
    return reinterpret_cast<Define *>((char *)this + sizeof(*this) + m_leadingCommentsLength +
        m_commentLength + m_nameLength + m_valueLength);
}

char *DefinesMap::Define::leadingCommentsPtr() const
{
    return (char *)(this + 1);
}

char *DefinesMap::Define::lineCommentPtr() const
{
    return leadingCommentsPtr() + m_leadingCommentsLength;
}

char *DefinesMap::Define::namePtr() const
{
    return lineCommentPtr() + m_commentLength;
}

char *DefinesMap::Define::valuePtr() const
{
    return namePtr() + m_nameLength;
}

DefinesMap::DefinesMap(size_t capacity) : m_defines(capacity)
{
}

void DefinesMap::add(const TokenList& leadingComments, CToken *comment, CToken *name, CToken *value, bool inverted)
{
    assert(name->isId() && value->category == Token::kNumber);
    m_defines.add(name->text(), name->textLength, name->hash, leadingComments, comment, name, value, inverted);
}

auto DefinesMap::get(const String& str) const -> const Define *
{
    return get(str, Util::hash(str.data(), str.length()));
}

auto DefinesMap::get(const String& str, Util::hash_t hash) const -> const Define *
{
    return m_defines.get(str.data(), str.length(), hash);
}

void DefinesMap::seal()
{
    m_defines.seal();
}

size_t DefinesMap::size() const
{
    return m_defines.size();
}

auto DefinesMap::begin() const -> StringMap<Define>::Iterator
{
    return m_defines.begin();
}

auto DefinesMap::end() const -> StringMap<Define>::Iterator
{
    return m_defines.end();
}

void DefinesMap::clear()
{
    m_defines.clear();
}
