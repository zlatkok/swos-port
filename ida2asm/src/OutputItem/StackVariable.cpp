#include "StackVariable.h"
#include "Util.h"

StackVariable::StackVariable(const String& name, CToken *size, CToken *offset)
{
    assert(offset && offset->textLength);
    assert(size && size->textLength);

    Util::assignSize(m_nameLength, name.length());
    memcpy(namePtr(), name.str(), name.length());

    Util::assignSize(m_offsetLength, offset->textLength);
    memcpy(offsetPtr(), offset->text(), offset->textLength);

    Util::assignSize(m_sizeLength, size->textLength);
    memcpy(sizePtr(), size->text(), size->textLength);

    Util::assignSize(m_offset, offset->parseInt());

    switch (size->type) {
    case Token::T_BYTE:
        m_size = 1;
        break;
    case Token::T_WORD:
        m_size = 2;
        break;
    case Token::T_DWORD:
        m_size = 4;
        break;
    case Token::T_QWORD:
        m_size = 8;
        break;
    case Token::T_ID:
        m_size = kStructVar;
        break;
    default:
        assert(false);
        break;
    }
}

size_t StackVariable::requiredSize(const String& name, CToken *size, CToken *offset)
{
    assert(size && offset && size->textLength && offset->textLength);

    return sizeof(StackVariable) + name.length() + size->textLength + offset->textLength;
}

String StackVariable::name() const
{
    return { namePtr(), m_nameLength };
}

String StackVariable::sizeString() const
{
    return { sizePtr(), m_sizeLength };
}

String StackVariable::offsetString() const
{
    return { offsetPtr(), m_offsetLength };
}

int StackVariable::offset() const
{
    return m_offset;
}

char *StackVariable::namePtr() const
{
    return (char *)(this + 1);
}

char *StackVariable::offsetPtr() const
{
    return namePtr() + m_nameLength;
}

char *StackVariable::sizePtr() const
{
    return offsetPtr() + m_offsetLength;
}
