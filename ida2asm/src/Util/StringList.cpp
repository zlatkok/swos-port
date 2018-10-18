#include "StringList.h"

StringList::StringList(size_t initialCapacity): m_data(initialCapacity)
{
}

void StringList::add(const char *begin, const char *end)
{
    auto len = end - begin;
    auto buf = m_data.add(len + 1);
    Util::assignSize(*buf, len + 1);
    memcpy(buf + 1, begin, len);
}

void StringList::add(const String& str)
{
    add(str.str(), str.end());
}

bool StringList::empty() const
{
    return m_data.spaceUsed() == 0;
}

auto StringList::begin() const -> const Iterator
{
    return m_data.begin();
}

auto StringList::end() const -> const Iterator
{
    return m_data.end();
}
