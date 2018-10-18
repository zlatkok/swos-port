#pragma once

class DynaArray
{
public:
    DynaArray(size_t initialReserved);
    char *add(size_t size);
    size_t spaceUsed() const;
    size_t spaceLeft() const;
    char *begin() const;
    char *end() const;
private:
    std::unique_ptr<char[]> m_data;
    size_t m_reserved = 0;
    size_t m_used = 0;
};
