#pragma once

// alignment-safe fetch & store routines for integer types
template<typename T, std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 2, int> = 0>
T fetch(const T *t)
{
    auto p = reinterpret_cast<const uint8_t *>(t);
    auto address = reinterpret_cast<uintptr_t>(t);

    switch (address % sizeof(T)) {
    case 0:
        return *t;
    case 1:
        return p[0] | (p[1] << 8);
    default:
        assert(false);
        return 0;
    }
}

template<typename T, std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 4, int> = 0>
T fetch(const T *t)
{
    auto p = reinterpret_cast<const uint8_t *>(t);
    auto address = reinterpret_cast<uintptr_t>(t);

    switch (address % sizeof(T)) {
    case 0:
        return *t;
    case 1:
        return (*reinterpret_cast<const uint32_t *>(p - 1) >> 8) | (p[3] << 24);
    case 2:
        return *reinterpret_cast<const uint16_t *>(p) | (*reinterpret_cast<const uint16_t *>(p + 2) << 16);
    case 3:
        return p[0] | (*reinterpret_cast<const uint32_t *>(p + 1) << 8);
    default:
        assert(false);
        return 0;
    }
}

template<typename T, std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 8, int> = 0>
T fetch(const T *t)
{
    auto p = reinterpret_cast<const uint8_t *>(t);
    auto address = reinterpret_cast<uintptr_t>(t);

    switch (address % sizeof(T)) {
    case 0:
        return *t;
    case 1:
        return (*reinterpret_cast<const uint64_t *>(p - 1) >> 8) | (static_cast<uint64_t>(p[7]) << 56);
    case 2:
        return (*reinterpret_cast<const uint64_t *>(p - 2) >> 16) | (static_cast<uint64_t>(*reinterpret_cast<const uint16_t *>(p + 6)) << 48);
    case 3:
        return (*reinterpret_cast<const uint64_t *>(p - 3) >> 24) | (static_cast<uint64_t>(*reinterpret_cast<const uint32_t *>(p + 5)) << 40);
    case 4:
        return *reinterpret_cast<const uint32_t *>(p) | (static_cast<uint64_t>(*reinterpret_cast<const uint32_t *>(p + 4)) << 32);
    case 5:
        return (*reinterpret_cast<const uint32_t *>(p - 1) >> 8) | (*reinterpret_cast<const uint64_t *>(p + 3) << 24);
    case 6:
        return *reinterpret_cast<const uint16_t *>(p) | (*reinterpret_cast<const uint64_t *>(p + 2) << 16);
    case 7:
        return p[0] | (*reinterpret_cast<const uint64_t *>(p + 1) << 8);
    default:
        assert(false);
        return 0;
    }
}

template<typename T1, typename T2, std::enable_if_t<!std::is_integral<T1>::value && (sizeof(T1) == 4 || sizeof(T1) == 8), int> = 0>
T1 fetch(const T2 *t)
{
    using Uint = typename std::conditional<sizeof(T1) == 8, uint64_t, uint32_t>::type;
    union U1 {
        U1() {}
        Uint temp;
        T1 result;
    } u;
    u.temp = fetch<Uint>((Uint *)t);
    return u.result;
}

template<typename T, std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 2, int> = 0>
void store(T *t, int value)
{
    auto p = reinterpret_cast<uint8_t *>(t);
    auto address = reinterpret_cast<uintptr_t>(t);

    switch (address % sizeof(T)) {
    case 0:
        *t = value;
        break;
    case 1:
        p[0] = value & 0xff;
        p[1] = static_cast<uint8_t>(value >> 8);
        break;
    default:
        assert(false);
    }
}

template<typename T, std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 4, int> = 0>
void store(T *t, int value)
{
    auto p = reinterpret_cast<uint8_t *>(t);
    auto address = reinterpret_cast<uintptr_t>(t);

    switch (address % sizeof(T)) {
    case 0:
        *t = value;
        break;
    case 1:
        *reinterpret_cast<uint32_t *>(p - 1) = p[-1] | (value << 8);
        p[3] = value >> 24;
        break;
    case 2:
        *reinterpret_cast<uint16_t *>(p) = value & 0xffff;
        *reinterpret_cast<uint16_t *>(p + 2) = value >> 16;
        break;
    case 3:
        p[0] = value & 0xff;
        *reinterpret_cast<uint32_t *>(p + 1) = (p[4] << 24) | (value >> 8);
        break;
    default:
        assert(false);
    }
}

template<typename T, std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 8, int> = 0>
void store(T *t, T value)
{
    auto p = reinterpret_cast<uint8_t *>(t);
    auto address = reinterpret_cast<uintptr_t>(t);

    switch (address % sizeof(T)) {
    case 0:
        *t = value;
        break;
    case 1:
        *reinterpret_cast<uint64_t *>(p - 1) = p[-1] | (value << 8);
        p[7] = value >> 56;
        break;
    case 2:
        *reinterpret_cast<uint64_t *>(p - 2) = *reinterpret_cast<uint16_t *>(p - 2) | (value << 16);
        *reinterpret_cast<uint16_t *>(p + 6) = value >> 48;
        break;
    case 3:
        *reinterpret_cast<uint64_t *>(p - 3) = (value << 24) | (*reinterpret_cast<uint32_t *>(p - 3) & 0xffffff);
        *reinterpret_cast<uint32_t *>(p + 5) = (value >> 40) | (p[8] << 24);
        break;
    case 4:
        *reinterpret_cast<uint32_t *>(p) = value & 0xffffffff;
        *reinterpret_cast<uint32_t *>(p + 4) = value >> 32;
        break;
    case 5:
        *reinterpret_cast<uint32_t *>(p - 1) = p[-1] | ((value << 8) & 0xffffffff);
        *reinterpret_cast<uint64_t *>(p + 3) = (value >> 24) | (((static_cast<uint64_t>(*reinterpret_cast<uint32_t *>(p + 7))) & 0xffffff00) << 32);
        break;
    case 6:
        *reinterpret_cast<uint16_t *>(p) = value & 0xffff;
        *reinterpret_cast<uint64_t *>(p + 2) = (value >> 16) | (static_cast<uint64_t>(*reinterpret_cast<uint16_t *>(p + 8)) << 48);
        break;
    case 7:
        p[0] = value & 0xff;
        *reinterpret_cast<uint64_t *>(p + 1) = (value >> 8) | (static_cast<uint64_t>(p[8]) << 56);
        break;
    default:
        assert(false);
    }
}
