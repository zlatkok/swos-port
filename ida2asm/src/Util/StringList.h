#pragma once

#include "DynaArray.h"

class StringList
{
public:
    class Iterator
    {
    public:
        Iterator(const char *str) : m_str(str) {}
        // format: first byte is offset to the next string
        String operator*() const { return { m_str + 1, static_cast<unsigned>(*m_str) - 1 }; }
        bool operator!=(const Iterator& rhs) { return m_str != rhs.m_str; }
        Iterator operator++() { m_str += static_cast<unsigned>(*m_str); return *this; }
    private:
        const char *m_str;
    };

    StringList(size_t initialCapacity);
    StringList(const StringList& rhs);
    StringList& operator=(const StringList&) = delete;
    void add(const char *begin, const char *end);
    void add(const String& str);
    bool empty() const;
    const Iterator begin() const;
    const Iterator end() const;

private:
    DynaArray m_data;
};
