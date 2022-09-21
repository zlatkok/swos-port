#pragma once

// fixed point, 16.16, signed (sign bit in the whole part), fraction always positive
struct FixedPoint {
    FixedPoint() = default;
    FixedPoint(const FixedPoint& other) : m_value(other.m_value) {}
    constexpr FixedPoint(int value, bool raw = false) : m_value(raw ? value : value << 16) {}
    constexpr FixedPoint(unsigned value) : m_value(value) {}
    constexpr FixedPoint(int whole, int fraction) : m_value((whole << 16) | fraction) {}
    static FixedPoint fromFloat(float value) {
        float whole, fraction = std::modf(value, &whole);
        return FixedPoint(static_cast<int>(whole), static_cast<int>(fraction * 0x10000));
    }
    float asFloat() const {
        return *this;
    }
    FixedPoint& operator=(const FixedPoint& other) {
        m_value = other.m_value;
        return *this;
    }
    FixedPoint& operator=(int value) {
        m_value = value << 16;
        return *this;
    }
    void set(int whole, int fraction) {
        m_value = (whole << 16) | fraction;
    }
    int32_t raw() const {
        return m_value;
    }
    void setRaw(int32_t value) {
        m_value = value;
    }
    int16_t whole() const {
        return m_value >> 16;
    }
    uint16_t fraction() const {
        return m_value & 0xffff;
    }
    int rounded() const {
        return whole() + (fraction() > 0x8000);
    }
    int truncated() const {
        return whole() + (sgn() < 0 && fraction());
    }
    int sgn() const {
        return m_value < 0 ? -1 : 1;
    }
    operator float() const {
        return static_cast<float>(m_value & 0xffff) / 0x10000 + (m_value >> 16);
    }
    explicit operator bool() const {
        return m_value != 0;
    }
    bool operator<(const FixedPoint& other) const {
        return m_value < other.m_value;
    }
    bool operator<(int value) const {
        return whole() < value;
    }
    bool operator<=(int value) const {
        return !operator>(value);
    }
    bool operator>(const FixedPoint& other) const {
        return m_value > other.m_value;
    }
    bool operator>(int value) const {
        return whole() > value || whole() == value && fraction();
    }
    bool operator>(int16_t value) const {
        return operator>(static_cast<int>(value));
    }
    bool operator>=(int value) const {
        return whole() >= value;
    }
    FixedPoint operator+(const FixedPoint& other) const {
        return FixedPoint(m_value + other.m_value, true);
    }
    FixedPoint operator+(int value) const {
        return FixedPoint(m_value + (value << 16), true);
    }
    FixedPoint operator-() const {
        return FixedPoint(-m_value, true);
    }
    FixedPoint operator-(const FixedPoint& other) const {
        return FixedPoint(m_value - other.m_value, true);
    }
    FixedPoint operator-(int value) const {
        return FixedPoint(m_value - (value << 16), true);
    }
    FixedPoint operator/(int value) const {
        return FixedPoint(m_value / value, true);
    }
    FixedPoint& operator+=(int value) {
        m_value += value << 16;
        return *this;
    }
    FixedPoint& operator+=(const FixedPoint& other) {
        m_value += other.m_value;
        return *this;
    }
    FixedPoint& operator-=(int value) {
        m_value -= (value << 16);
        return *this;
    }
    FixedPoint& operator-=(const FixedPoint& other) {
        m_value -= other.m_value;
        return *this;
    }
    FixedPoint& operator>>=(int count) {
        m_value >>= count;
        return *this;
    }

private:
    int32_t m_value;
};

static inline FixedPoint operator+(int value, const FixedPoint& fixed)
{
    return FixedPoint(value) + fixed;
}
static inline FixedPoint operator-(int value, const FixedPoint& fixed)
{
    return FixedPoint(value) - fixed;
}
