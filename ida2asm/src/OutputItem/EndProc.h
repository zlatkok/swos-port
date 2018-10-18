#pragma once

#include "Tokenizer.h"
#include "StringView.h"

#pragma pack(push, 1)
class EndProc
{
public:
    EndProc(CToken *name);
    static size_t requiredSize(CToken *name);

    String name() const;
private:
    char *namePtr() const;

    uint8_t m_nameLength;
};
#pragma pack(pop)
