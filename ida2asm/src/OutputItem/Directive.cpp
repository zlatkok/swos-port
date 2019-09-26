#include "Directive.h"
#include "Util.h"
#include "ParsingException.h"

Directive::Directive(CToken *begin, CToken *end)
{
    assert(begin && begin < end);
    auto directive = begin;
    begin = begin->next();

    assert(directive && directive->isText() && directive->textLength && (directive->text()[0] == '.' ||
        directive->type == Token::T_ALIGN || directive->type == Token::T_ASSUME));
    assert(begin <= end);

    if (directive->type == Token::T_ALIGN) {
        m_type = kAlign;
        assert(begin && begin < end);
    } else if (directive->type == Token::T_ASSUME) {
        m_type = kAssume;
        assert(begin && begin < end);
    } else {
        auto p = directive->text() + 1;

        if (directive->textLength == 4) {
            if (p[0] == '3' && p[1] == '8' && p[2] == '6')
                m_type = k386;
            else if (p[0] == '3' && p[1] == '8' && p[2] == '7')
                m_type = k387;
            else if (p[0] == '4' && p[1] == '8' && p[2] == '6')
                m_type = k486;
            else if (p[0] == '5' && p[1] == '8' && p[2] == '6')
                m_type = k586;
            else if (p[0] == '6' && p[1] == '8' && p[2] == '6')
                m_type = k686;
            else if (p[0] == 'm' && p[1] == 'm' && p[2] == 'x')
                m_type = kMMX;
            else if (p[0] == 'x' && p[1] == 'm' && p[2] == 'm')
                m_type = kXMM;
            else if (p[0] == 'k' && p[1] == '3' && p[2] == 'd')
                m_type = kK3D;
        } else if (directive->textLength == 5) {
            if (p[0] == '3' && p[1] == '8' && p[2] == '6' && p[3] == 'p')
                m_type = k386p;
            else if (p[0] == '4' && p[1] == '8' && p[2] == '6' && p[3] == 'p')
                m_type = k486p;
            else if (p[0] == '5' && p[1] == '8' && p[2] == '6' && p[3] == 'p')
                m_type = k586p;
            else if (p[0] == '6' && p[1] == '8' && p[2] == '6' && p[3] == 'p')
                m_type = k686p;
        } else if (directive->textLength == 6 && *directive == ".model") {
            assert(begin && begin < end);
            if (*begin == "flat")
                m_type = kModelFlat;
        }
    }

    if (m_type == kNone) {
        assert(false && "unknown directive");
        throw ParsingException("invalid directive: \"" + directive->string() + '"', directive);
    }

    Util::assignSize(m_directiveLenght, directive->textLength);
    memcpy(directivePtr(), directive->text(), directive->textLength);

    m_numParams = 0;
    auto param = paramsPtr();

    while (begin != end) {
        new (param) Param(begin);
        param += Param::requiredSize(begin);
        begin = begin->next();
        m_numParams++;
    }
}

size_t Directive::requiredSize(CToken *begin, CToken *end)
{
    assert(begin && begin < end);
    auto directive = begin;

    begin = begin->next();
    assert(begin <= end);

    size_t paramsLength = 0;

    while (begin != end) {
        paramsLength += Param::requiredSize(begin);
        begin = begin->next();
    }

    return sizeof(Directive) + directive->textLength + paramsLength;
}

String Directive::directive() const
{
    return { directivePtr(), m_directiveLenght };
}

auto Directive::type() const -> DirectiveType
{
    return m_type;
}

size_t Directive::paramCount() const
{
    return m_numParams;
}

auto Directive::begin() const -> const Param *
{
    return paramsPtr();
}

char *Directive::directivePtr() const
{
    return (char *)(this + 1);
}

auto Directive::paramsPtr() const -> Param *
{
    return reinterpret_cast<Param *>(directivePtr() + m_directiveLenght);
}

Directive::Param::Param(CToken *token)
{
    assert(token && token->textLength);

    Util::assignSize(m_length, token->textLength);
    memcpy(textPtr(), token->text(), token->textLength);
}

size_t Directive::Param::requiredSize(CToken *token)
{
    assert(token && token->textLength);
    return sizeof(Param) + token->textLength;
}

String Directive::Param::text() const
{
    return { textPtr(), m_length };
}

auto Directive::Param::next() const -> const Param *
{
    return reinterpret_cast<Param *>(textPtr() + m_length);
}

char *Directive::Param::textPtr() const
{
    return (char *)(this + 1);
}
