#pragma once

#include "StringMap.h"
#include "Tokenizer.h"

#pragma pack(push, 1)
class DefinesMap
{
public:
    class Define
    {
    public:
        static constexpr int kInvertedFlag = 0x8000;

        Define(const TokenList& leadingComments, CToken *comment, CToken *name, CToken *value, bool inverted);
        static size_t requiredSize(const TokenList& leadingComments, CToken *comment, CToken *name, CToken *value, bool);
        Token::Type type() const;
        bool isInverted() const;
        String name() const;
        String value() const;
        String leadingComments() const;
        String comment() const;
        const Define *next() const;

    private:
        char *leadingCommentsPtr() const;
        char *lineCommentPtr() const;
        char *namePtr() const;
        char *valuePtr() const;

        int m_value;
        Token::Type m_type;
        uint8_t m_leadingCommentsLength;
        uint8_t m_commentLength;
        uint8_t m_nameLength;
        uint8_t m_valueLength;
        // followed by: leading comments, line comment, name string, value string

        static constexpr size_t kMaxLength = std::numeric_limits<decltype(m_nameLength)>::max();
    };

    DefinesMap(size_t capacity);
    void add(const TokenList& leadingComments, CToken *comment, CToken *name, CToken *value, bool inverted);
    const Define *get(const String& str);
    const Define *get(const String& str, Util::hash_t hash);
    void seal();
    size_t size() const;
    StringMap<Define>::Iterator begin() const;
    StringMap<Define>::Iterator end() const;

private:
    StringMap<Define> m_defines;
};
#pragma pack(pop)
