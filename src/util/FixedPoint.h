#pragma once

// fixed point, 16.16, signed (sign bit in the whole part), fraction always positive
struct FixedPoint {
    FixedPoint() = default;
    constexpr FixedPoint(const FixedPoint& other) : m_value(other.m_value) {}
    constexpr FixedPoint(int value, bool raw = false) : m_value(raw ? value : value << 16) {}
    constexpr FixedPoint(unsigned value) : m_value(value) {}
    constexpr FixedPoint(int whole, int fraction) : m_value((whole << 16) | fraction) {}
    static FixedPoint fromFloat(float value) {
        float whole, fraction = std::modf(value, &whole);
        return FixedPoint(static_cast<int>(whole), static_cast<int>(fraction * 0x10000));
    }
    static constexpr FixedPoint fromRaw(int value) {
        return FixedPoint(value, true);
    }
    constexpr float asFloat() const {
        return static_cast<float>(m_value & 0xffff) / 0x10000 + (m_value >> 16);
    }
    constexpr FixedPoint& operator=(const FixedPoint& other) {
        m_value = other.m_value;
        return *this;
    }
    constexpr FixedPoint& operator=(int value) {
        m_value = value << 16;
        return *this;
    }
    constexpr void set(int whole, int fraction) {
        m_value = (whole << 16) | fraction;
    }
    constexpr void setWhole(int whole) {
        m_value = (whole << 16) | fraction();
    }
    constexpr void clearFraction() {
        m_value &= 0xffff0000;
    }
    constexpr int32_t raw() const {
        return m_value;
    }
    constexpr void setRaw(int32_t value) {
        m_value = value;
    }
    constexpr int16_t whole() const {
        return m_value >> 16;
    }
    constexpr uint16_t fraction() const {
        return m_value & 0xffff;
    }
    constexpr int rounded() const {
        return whole() + (fraction() > 0x8000);
    }
    constexpr int truncated() const {
        return whole() + (sgn() < 0 && fraction());
    }
    constexpr int sgn() const {
        return m_value < 0 ? -1 : 1;
    }
    bool nearlyEqual(const FixedPoint& other) const {
        return std::abs(other.m_value - m_value) < 0x100;
    }
    constexpr explicit operator bool() const {
        return m_value != 0;
    }
    constexpr bool operator==(const FixedPoint& other) const {
        return m_value == other.m_value;
    }
    constexpr bool operator!=(const FixedPoint& other) const {
        return !operator==(other);
    }
    constexpr bool operator<(const FixedPoint& other) const {
        return m_value < other.m_value;
    }
    constexpr bool operator<(int value) const {
        return whole() < value;
    }
    constexpr bool operator<=(const FixedPoint& other) const {
        return m_value <= other.m_value;
    }
    constexpr bool operator<=(int value) const {
        return !operator>(value);
    }
    constexpr bool operator>(const FixedPoint& other) const {
        return m_value > other.m_value;
    }
    constexpr bool operator>(int value) const {
        return whole() > value || whole() == value && fraction();
    }
    constexpr bool operator>(int16_t value) const {
        return operator>(static_cast<int>(value));
    }
    constexpr bool operator>=(const FixedPoint& other) const {
        return m_value >= other.m_value;
    }
    constexpr bool operator>=(int value) const {
        return whole() >= value;
    }
    constexpr FixedPoint operator+(const FixedPoint& other) const {
        return FixedPoint(m_value + other.m_value, true);
    }
    constexpr FixedPoint operator+(int value) const {
        return FixedPoint(m_value + (value << 16), true);
    }
    constexpr FixedPoint operator-() const {
        return FixedPoint(-m_value, true);
    }
    constexpr FixedPoint operator-(const FixedPoint& other) const {
        return FixedPoint(m_value - other.m_value, true);
    }
    constexpr FixedPoint operator-(int value) const {
        return FixedPoint(m_value - (value << 16), true);
    }
    constexpr FixedPoint operator/(int value) const {
        return FixedPoint(m_value / value, true);
    }
    constexpr FixedPoint& operator+=(int value) {
        m_value += value << 16;
        return *this;
    }
    constexpr FixedPoint& operator+=(const FixedPoint& other) {
        m_value += other.m_value;
        return *this;
    }
    constexpr FixedPoint& operator-=(int value) {
        m_value -= (value << 16);
        return *this;
    }
    constexpr FixedPoint& operator-=(const FixedPoint& other) {
        m_value -= other.m_value;
        return *this;
    }
    constexpr FixedPoint& operator>>=(int count) {
        m_value >>= count;
        return *this;
    }
    constexpr FixedPoint& operator/=(int value) {
        m_value /= value;
        return *this;
    }
    constexpr FixedPoint& operator|=(int value) {
        m_value |= value;
        return *this;
    }

private:
    struct RawTag {};

    constexpr FixedPoint(int32_t raw, RawTag) : m_value(raw) {}

    int32_t m_value;
};

constexpr FixedPoint operator+(int value, const FixedPoint& fixed)
{
    return FixedPoint(value) + fixed;
}
constexpr FixedPoint operator-(int value, const FixedPoint& fixed)
{
    return FixedPoint(value) - fixed;
}
constexpr bool operator<(int value, const FixedPoint& fixed)
{
    return FixedPoint(value) < fixed;
}
constexpr bool operator<=(int value, const FixedPoint& fixed)
{
    return FixedPoint(value) <= fixed;
}
constexpr bool operator>(int value, const FixedPoint& fixed)
{
    return FixedPoint(value) > fixed;
}
constexpr bool operator>=(int value, const FixedPoint& fixed)
{
    return FixedPoint(value) >= fixed;
}

constexpr FixedPoint operator""_fp(long double value)
{
    return FixedPoint(static_cast<int32_t>(value * 65536.0L), true);
}

constexpr FixedPoint operator""_fp(unsigned long long value)
{
    return FixedPoint(static_cast<int32_t>(value << 16), true);
}
