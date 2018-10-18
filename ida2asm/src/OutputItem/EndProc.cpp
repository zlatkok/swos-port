#include "EndProc.h"
#include "Util.h"

EndProc::EndProc(CToken *name)
{
    assert(name && name->textLength);

    Util::assignSize(m_nameLength, name->textLength);
    memcpy(namePtr(), name->text(), name->textLength);
}

String EndProc::name() const
{
    return { namePtr(), m_nameLength };
}

char *EndProc::namePtr() const
{
    return (char *)(this + 1);
}

size_t EndProc::requiredSize(CToken *name)
{
    assert(name);
    return sizeof(EndProc) + name->textLength;
}
