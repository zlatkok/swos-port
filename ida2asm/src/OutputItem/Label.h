#pragma once

#include "Tokenizer.h"

#pragma pack(push, 1)
class Label
{
public:
    Label(CToken *token);
    static size_t requiredSize(CToken *token);

    String name() const;
    bool isLocal() const;

private:
    char *namePtr() const;

    uint8_t m_nameLength;
};
#pragma pack(pop)
