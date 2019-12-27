#pragma once

class PascalString
{
public:
    PascalString(const char *str, size_t len) {
        Util::assignSize(m_length, len);
        memcpy(data(), str, len);
    }
    PascalString(const String& str) : PascalString(str.data(), str.length()) {}
    PascalString(size_t initialSize) {
        Util::assignSize(m_length, initialSize);
    }
    PascalString(const PascalString&) = delete;

    static size_t requiredSize(const char *, size_t stringLength) {
        return sizeof(PascalString) + stringLength;
    }
    static size_t requiredSize(const String& str) {
        return PascalString::requiredSize(str.data(), str.length());
    }
    static size_t requiredSize(const PascalString& str) {
        return PascalString::requiredSize(str.data(), str.length());
    }

    void copy(char *dest) const {
        memcpy(dest, data(), m_length);
        dest[m_length] = '\0';
    }

    std::string string() const {
        return { data(), m_length };
    }

    size_t length() const { return m_length; }
    char *data() const { return (char *)(this + 1); }
    bool empty() const { return m_length == 0; }

private:
    uint8_t m_length;
};
