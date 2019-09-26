#pragma once

#include "Tokenizer.h"
#include "StringView.h"

#pragma pack(push, 1)
class Directive
{
public:
    enum DirectiveType : uint8_t {
        kNone, k386, k486, k586, k686, k386p, k486p, k586p, k686p, k387, kK3D, kMMX, kXMM, kModelFlat, kAlign, kAssume,
    };

    Directive(CToken *begin, CToken *end);
    static size_t requiredSize(CToken *begin, CToken *end);

    String directive() const;
    DirectiveType type() const;
    size_t paramCount() const;

    class Param
    {
    public:
        Param(CToken *token);
        static size_t requiredSize(CToken *token);

        String text() const;
        const Param *next() const;

    private:
        char *textPtr() const;

        uint8_t m_length;
    };

    const Param *begin() const;

private:
    char *directivePtr() const;
    Param *paramsPtr() const;

    uint8_t m_directiveLenght;
    uint8_t m_numParams;
    DirectiveType m_type = kNone;
    // followed by directive text, params (length then text)
};
#pragma pack(pop)
