#include "DynaArray.h"

DynaArray::DynaArray(size_t initialReserved)
{
    m_reserved = initialReserved;
    if (initialReserved)
        m_data.reset(new char[initialReserved]);
}

DynaArray::DynaArray(const DynaArray& rhs)
{
    copy(rhs);
}

DynaArray::DynaArray(DynaArray&& rhs) noexcept
{
    move(std::move(rhs));
}

DynaArray& DynaArray::operator=(const DynaArray& rhs)
{
    copy(rhs);
    return *this;
}

DynaArray& DynaArray::operator=(DynaArray&& rhs) noexcept
{
    move(std::move(rhs));
    return *this;
}

char *DynaArray::add(size_t size)
{
    assert(m_used <= m_reserved);

    if (!size)
        return m_data.get();

    if (m_used + size > m_reserved) {
        assert(false);
        std::cerr << "Performance warning: reallocation!\n";
        m_reserved += std::max(std::max(m_reserved, size), static_cast<size_t>(100 * 1024));
        auto newData = new char[m_reserved];
        memcpy(newData, m_data.get(), m_used);
        m_data.reset(newData);
    }

    auto p = m_data.get() + m_used;
    m_used += size;
    return p;
}

size_t DynaArray::spaceUsed() const
{
    return m_used;
}

size_t DynaArray::spaceLeft() const
{
    return m_reserved - m_used;
}

char *DynaArray::begin() const
{
    return m_data.get();
}

char *DynaArray::end() const
{
    return m_data.get() + m_used;
}

void DynaArray::reset(size_t newSize)
{
    m_reserved = newSize;
    m_data.reset(new char[m_reserved]);
}

void DynaArray::clear()
{
    m_used = 0;
}

void DynaArray::copy(const DynaArray& rhs)
{
    if (this != &rhs) {
        m_reserved = rhs.m_reserved;
        m_used = rhs.m_used;
        m_data.reset(new char[m_used]);
        memcpy(m_data.get(), rhs.m_data.get(), m_used);
    }
}

void DynaArray::move(DynaArray&& rhs)
{
    m_data = std::move(rhs.m_data);
    m_reserved = std::exchange(rhs.m_reserved, 0);
    m_used = std::exchange(rhs.m_used, 0);
}
