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

    void assign(const char *str, size_t length);
    void assign(const char *begin, const char *end);

    String(const String& rhs) = default;
    String(String&& rhs) noexcept;

    String& operator=(const String& rhs) = default;
    String& operator=(String&& rhs) noexcept;

    const char *data() const;
    size_t length() const;

    void copy(char *buf) const;
    bool writeToFile(FILE *f) const;

    bool empty() const;
    bool contains(char c) const;
    bool contains(const String& str) const;
    int indexOf(char c, size_t start = 0) const;
    int indexOf(const String& str, size_t start = 0) const;
    bool startsWith(const char *str) const;
    bool startsWith(const String& str) const;
    bool startsWith(char c) const;
    bool endsWith(const String& str) const;
    bool endsWith(char c) const;
    char first() const;
    char last() const;
    char secondLast() const;
    String substr(int from, int len = -1) const;
    String withoutLast() const;
    String trimmed() const;
    int toInt() const;

    void removeLast();
    void clear();

    bool isOneOf(const std::initializer_list<const char *>& strings) const {
        for (const auto str : strings) {
            if (*this == str)
                return true;
        }
        return false;
    }

    bool operator==(const String& rhs) const;
    bool operator==(char c) const;
    bool operator==(const char *str) const;
    bool operator!=(const String& rhs) const { return !operator==(rhs); }
    bool operator!=(char c) const { return !operator==(c); }
    bool operator!=(const char *str) const { return !operator==(str); }
    bool operator<(const String& rhs) const;
    const char& operator[](size_t index) const;
    explicit operator bool() const;
    operator std::string() const { return { m_str, m_length }; }
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
    template<>
    struct hash<String> {
        std::size_t operator()(const String& str) const {
            return Util::hash(str.data(), str.length());
        }
    };
}
