#include "DataItem.h"
#include "Util.h"

DataItem::DataItem(CToken *name, CToken *structName, uint8_t baseSize) : m_baseSize(baseSize)
{
    m_nameLength = 0;
    if (name) {
        Util::assignSize(m_nameLength, name->textLength);
        name->copyText(namePtr());
    }

    m_structNameLength = 0;
    if (structName) {
        Util::assignSize(m_structNameLength, structName->textLength);
        structName->copyText(structNamePtr());
    }
}

size_t DataItem::requiredSize(CToken *name, CToken *structName)
{
    auto nameLen = name ? name->textLength : 0;
    auto structNameLen = structName ? structName->textLength : 0;
    return sizeof(DataItem) + nameLen + structNameLen;
}

size_t DataItem::requiredElementSize(CToken *token, int offset)
{
    assert(token);
    return sizeof(Element) + token->textLength + (offset ? sizeof(Util::hash_t) : 0);
}

void DataItem::addElement(char *buf, CToken *token, bool isOffset, int offset, size_t dup)
{
    assert(m_numElements < std::numeric_limits<decltype(m_numElements)>::max());

    auto elem = new (buf) Element(token, isOffset, offset, dup);
    m_numElements++;
}

bool DataItem::contiguous() const
{
    return (m_numElements & kContiguousDataFlag) != 0;
}

void DataItem::setContiguous()
{
    m_numElements |= kContiguousDataFlag;
}

String DataItem::name() const
{
    return { namePtr(), m_nameLength };
}

size_t DataItem::size() const
{
    return m_baseSize;
}

String DataItem::structName() const
{
    return { structNamePtr(), m_structNameLength };
}

size_t DataItem::numElements() const
{
    return m_numElements & ~kContiguousDataFlag;
}

auto DataItem::initialElement() const -> const Element *
{
    return const_cast<DataItem *>(this)->initialElement();
}

auto DataItem::initialElement() -> Element *
{
    return reinterpret_cast<Element *>(structNamePtr() + m_structNameLength);
}

char *DataItem::namePtr() const
{
    return (char *)(this + 1);
}

char *DataItem::structNamePtr() const
{
    return namePtr() + m_nameLength;
}

DataItem::Element::Element(CToken *token, bool isOffset, int offset, size_t dup) : m_type(kNone)
{
    assert(token);
    assert(token->category == Token::kId || token->category == Token::kNumber || token->category == Token::kDup);

    Util::assignSize(m_dup, dup);

    switch (token->type) {
    case Token::T_HEX:
    case Token::T_NUM:
    case Token::T_BIN:
        m_value = token->parseInt();
        if (token->type == Token::T_HEX)
            m_type = kHex;
        else if (token->type == Token::T_NUM)
            m_type = kDec;
        else
            m_type = kBin;

        m_type |= kIsNumber;
        break;

    case Token::T_STRING:
        m_type = kString;
        break;

    case Token::T_ID:
        m_type = kLabel;
        break;

    case Token::T_DUP_QMARK:
        m_type = kUninitialized;
        break;

    case Token::T_DUP_STRUCT_INIT:
        m_type = kStructInit;
        break;

    default:
        assert(false);
    }

    if (isOffset)
        m_type |= kIsOffsetFlag;

    Util::assignSize(m_textLength, token->textLength);
    token->copyText(textPtr());

    if (offset) {
        m_type |= kHasAddressOffset;
        *offsetPtr() = offset;
    }
}

auto DataItem::Element::next() const -> Element *
{
    return reinterpret_cast<Element *>((char *)(this + 1) + m_textLength);
}

String DataItem::Element::text() const
{
    return { textPtr(), m_textLength };
}

int DataItem::Element::value() const
{
    return m_value;
}

bool DataItem::Element::isNumber() const
{
    return (m_type & kIsNumber) != 0;
}

size_t DataItem::Element::dup() const
{
    return m_dup;
}

auto DataItem::Element::type() const -> ElementType
{
    return m_type & kTypeMask;
}

bool DataItem::Element::isOffset() const
{
    return (m_type & kIsOffsetFlag) != 0;
}

int DataItem::Element::offset() const
{
    return m_type & kHasAddressOffset ? *offsetPtr() : 0;
}

void DataItem::Element::increaseDup()
{
    m_dup++;
}

char *DataItem::Element::textPtr() const
{
    return (char *)(this + 1);
}

int *DataItem::Element::offsetPtr() const
{
    return reinterpret_cast<int *>(textPtr() + m_textLength);
}
