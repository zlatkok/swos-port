#pragma once

#include "Tokenizer.h"
#include "StringView.h"

#pragma pack(push, 1)
class DataItem
{
public:
    DataItem(CToken *name, CToken *structName, uint8_t baseSize);
    static size_t requiredSize(CToken *name, CToken *structName);
    static size_t requiredElementSize(CToken *token, int offset);
    void addElement(char *buf, CToken *token, bool isOffset, int offset, size_t dup);

    String name() const;
    String structName() const;
    size_t size() const;
    size_t numElements() const;

    enum ElementType : uint8_t {
        kNone,
        kHex,
        kDec,
        kBin,
        kLabel,
        kString,
        kStructInit,
        kUninitialized,
        kHasAddressOffset = 64,
        kIsOffsetFlag = 128,
    };

    class Element
    {
    public:
        Element(CToken *token, bool isOffset, int offset, size_t dup);
        const Element *next() const;
        String text() const;
        size_t dup() const;
        ElementType type() const;
        int offset() const;
        void increaseDup();

    private:
        char *textPtr() const;
        int *offsetPtr() const;

        int value = 0;
        uint32_t m_dup;
        ElementType m_type;
        uint8_t m_textLength;
        // followed by optional text, and optional address offset
    };

    const Element *begin() const;
    Element *begin();

private:
    char *namePtr() const;
    char *structNamePtr() const;

    uint8_t m_nameLength;
    uint8_t m_structNameLength;
    uint8_t m_baseSize;
    uint8_t m_numElements = 0;
    // followed by name
    // followed by struct name if structure type
    // followed by n elements
};
#pragma pack(pop)

ENABLE_FLAGS(DataItem::ElementType)
