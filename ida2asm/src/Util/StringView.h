#pragma once

#include "Util.h"

struct Token;
typedef const Token CToken;

class String {
public:
    String(CToken *token);
    String(const char *str, size_t length);
    String(const char *begin, const char *end);
    String(const std::pair<const char *, const char *>& range);
    template <size_t N>
    String(const char (&str)[N]) : m_str(str), m_length(N - 1) {}
    String();

    String(const String& rhs) = default;
    String(String&& rhs);

    String& operator=(const String& rhs) = default;
    String& operator=(String&& rhs);

    const char *str() const;
    size_t length() const;

    bool empty() const;
    bool contains(char c) const;
    bool contains(const String& str) const;
    int indexOf(char c) const;
    int indexOf(const String& str) const;
    bool startsWith(const String& str) const;
    bool startsWith(char c) const;
    bool endsWith(const String& str) const;
    bool endsWith(char c) const;
    String substr(int from, int len = -1) const;

    void clear();

    bool operator==(const String& rhs) const;
    bool operator==(char c) const;
    bool operator!=(const String& rhs) const { return !operator==(rhs); }
    bool operator!=(char c) const { return !operator==(c); }
    char operator[](size_t index) const;
    operator std::string() const { return { m_str,m_length }; }
    std::string string() const;
    char *c_str() const;    // caller needs to delete
    const char *end() const;

private:
    static void swap(String& lhs, String& rhs);
    const char *m_str = nullptr;
    size_t m_length = 0;
};

bool operator==(CToken *token, const String& str);
inline bool operator!=(CToken *token, const String& str) { return !operator==(token, str); }
bool operator==(const String& str, CToken *token);

namespace std {
    template <>
    struct hash<String> {
        std::size_t operator()(const String& str) const {
            return Util::hash(str.str(), str.length());
        }
    };
}
