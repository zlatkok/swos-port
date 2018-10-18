#include "String.h"
#include "Tokenizer.h"

String::String(CToken *token)
{
    if (token) {
        m_str = token->text();
        m_length = token->textLength;
    } else {
        m_str = nullptr;
        m_length = 0;
    }
}

String::String(const char *str, size_t length) : m_str(str), m_length(length)
{
}

String::String(const char *begin, const char *end) : m_str(begin), m_length(end - begin)
{
    assert(end >= begin);
}

String::String(const std::pair<const char *, const char *>& range) : String(range.first, range.second)
{
}

String::String()
{
}

String::String(String&& rhs)
{
    swap(*this, rhs);
}

String& String::operator=(String&& rhs)
{
    swap(*this, rhs);
    return *this;
}

const char *String::str() const
{
    return m_str;
}

size_t String::length() const
{
    return m_length;
}

bool String::empty() const
{
    return !m_length;
}

bool String::contains(char c) const
{
    return memchr(m_str, c, m_length) != nullptr;
}

bool String::contains(const String& str) const
{
    return indexOf(str) >= 0;
}

int String::indexOf(char c) const
{
    char *p = (char *)memchr(m_str, c, m_length);
    return p ? p - m_str : -1;
}

int String::indexOf(const String& str) const
{
    auto substr = std::search(m_str, m_str + m_length, str.m_str, str.m_str + str.m_length);
    return substr != m_str + m_length ? substr - m_str : -1;
}

bool String::startsWith(const String& str) const
{
    return m_length >= str.m_length && !memcmp(m_str, str.m_str, str.m_length);
}

bool String::startsWith(char c) const
{
    return m_length >= 1 && m_str[0] == c;
}

bool String::endsWith(const String& str) const
{
    return m_length >= str.m_length && !memcmp(m_str + m_length - str.m_length, str.m_str, str.m_length);
}

bool String::endsWith(char c) const
{
    return m_length > 0 && m_str[m_length - 1] == c;
}

String String::substr(int from, int len) const
{
    assert(from >= 0 && from < static_cast<int>(m_length) && (len < 0 || from + len < static_cast<int>(m_length)));

    return String(m_str + from, len < 0 ? m_length - from : len);
}

void String::clear()
{
    m_str = nullptr;
    m_length = 0;
}

bool String::operator==(const String& rhs) const
{
    return m_length == rhs.m_length && !memcmp(m_str, rhs.m_str, m_length);
}

bool String::operator==(char c) const
{
    return m_length == 1 && c == m_str[0];
}

char String::operator[](size_t index) const
{
    assert(index < m_length);
    return m_str[index];
}

std::string String::string() const
{
    return { m_str, m_length };
}

char *String::c_str() const
{
    auto str = new char[m_length + 1];
    memcpy(str, m_str, m_length);
    str[m_length] = '\0';
    return str;
}

const char *String::end() const
{
    return m_str + m_length;
}

void String::swap(String& lhs, String& rhs)
{
    std::swap(lhs.m_str, rhs.m_str);
    std::swap(lhs.m_length, rhs.m_length);
}

bool operator==(CToken *token, const String& str)
{
    return token->type <= Token::T_ID && token->textLength == str.length() && !memcmp(token->text(), str.str(), str.length());
}

bool operator==(const String& str, CToken *token)
{
    return token == str;
}
