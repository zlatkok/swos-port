#include "Label.h"
#include "Util.h"

Label::Label(CToken *token)
{
    Util::assignSize(m_nameLength, token->textLength);

    m_nameLength = token->textLength;
    memcpy(namePtr(), token->text(), token->textLength);
}

size_t Label::requiredSize(CToken *token)
{
    assert(token);
    return sizeof(Label) + token->textLength;
}

String Label::name() const
{
    return String(namePtr(), m_nameLength - 1);
}

bool Label::isLocal() const
{
    return m_nameLength > 2 && namePtr()[0] == '@' && namePtr()[1] == '@';
}

char *Label::namePtr() const
{
    return (char *)(this + 1);
}
