#pragma once

// included from OutputWriter declaration, pulled here to minimize polluting it

template <typename T, typename... Args>
int out(T t, const Args&... args)
{
    int len = out(t);
    return len + out(args...);
}

template<>
int out(const std::string str)
{
    assert(str.length() < 1'000);
    assert(m_outPtr + str.length() < m_outBuffer.get() + m_outBufferSize);

    if (!str.empty()) {
        memcpy(m_outPtr, str.data(), str.length());
        m_outPtr += str.length();
    }

    return str.length();
}


template<>
int out(String str)
{
    assert(str.length() < 160);
    assert(m_outPtr + str.length() < m_outBuffer.get() + m_outBufferSize);

    if (str.length()) {
        memcpy(m_outPtr, str.str(), str.length());
        m_outPtr += str.length();
    }

    return str.length();
}

template<>
int out(decltype(Util::kNewLine) newLine)
{
    for (auto c : newLine)
        out(c);

    return newLine.size();
}

template<>
int out(const char *str)
{
    assert(m_outPtr + strlen(str) < m_outBuffer.get() + m_outBufferSize);

    auto start = m_outPtr;

    while (*str)
        *m_outPtr++ = *str++;

    return m_outPtr - start;
}

template<>
int out(char *str)
{
    return out(const_cast<const char *>(str));
}

template<>
int out(char c)
{
    assert(m_outPtr + 1 < m_outBuffer.get() + m_outBufferSize);
    *m_outPtr++ = c;
    return 1;
}

template<>
int out(size_t num)
{
    auto start = m_outPtr;

    while (num) {
        assert(m_outPtr + 1 < m_outBuffer.get() + m_outBufferSize);
        auto res = std::div(num, 10);
        *m_outPtr++ = res.rem + '0';
        num = res.quot;
    }

    std::reverse(start, m_outPtr);
    return m_outPtr - start;
}

template<>
int out(int num)
{
    if (num < 0) {
        out('-');
        num = -num;
    }

    return out(static_cast<size_t>(num));
}
