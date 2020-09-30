#pragma once

class DynaArray
{
public:
    DynaArray(size_t initialReserved);
    DynaArray(const DynaArray& rhs);
    DynaArray(DynaArray&& rhs) noexcept;
    DynaArray& operator=(const DynaArray& rhs);
    DynaArray& operator=(DynaArray&& rhs) noexcept;

    char *add(size_t size);
    size_t spaceUsed() const;
    size_t spaceLeft() const;
    char *begin() const;
    char *end() const;
    void reset(size_t newSize);
    void clear();

private:
    void copy(const DynaArray& rhs);
    void move(DynaArray&& rhs);

    std::unique_ptr<char[]> m_data;
    size_t m_reserved = 0;
    size_t m_used = 0;
};
