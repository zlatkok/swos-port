#pragma once

#include "StringView.h"
#include "Tokenizer.h"

#pragma pack(push, 1)
class StackVariable
{
public:
    StackVariable(const String& name, CToken *size, CToken *offset);
    static size_t requiredSize(const String& name, CToken *size, CToken *offset);

    String name() const;
    String sizeString() const;
    String offsetString() const;
    int offset() const;

private:
    static constexpr uint8_t kStructVar = 0xff;

    char *namePtr() const;
    char *offsetPtr() const;
    char *sizePtr() const;

    uint8_t m_nameLength;
    uint8_t m_offsetLength;
    uint8_t m_sizeLength;
    uint8_t m_size;
    int8_t m_offset;
    // followed by: name string, offset string, size/struct string
};
#pragma pack(pop)
