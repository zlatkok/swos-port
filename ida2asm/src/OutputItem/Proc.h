#pragma once

#include "Tokenizer.h"
#include "StringView.h"

#pragma pack(push, 1)
class Proc
{
public:
    Proc(CToken *name);
    static size_t requiredSize(CToken *name);

    String name() const;

private:
    char *namePtr() const;

    uint8_t m_nameLength;
    uint8_t m_numStackVars = 0;
    // followed by arguments & local variables
};
#pragma pack(pop)
