#include "Proc.h"

Proc::Proc(CToken *name)
{
    assert(name);

    m_nameLength = name->textLength;
    memcpy(this->namePtr(), name->text(), name->textLength);
}

size_t Proc::requiredSize(CToken *name)
{
    assert(name);
    return sizeof(Proc) + name->textLength;
}

String Proc::name() const
{
    return { namePtr(), m_nameLength };
}

char *Proc::namePtr() const
{
    return (char *)(this + 1);
}
