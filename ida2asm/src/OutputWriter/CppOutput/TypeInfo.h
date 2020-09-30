#pragma once

struct TypeInfo
{
    const char *unsignedType;
    const char *signedType;
    const char *nextLargerSignedType;
    int bitCount;
    const char *signMask;
    const char *signedMin;
    const char *signedMax;
};

inline TypeInfo getTypeInfo(size_t size)
{
    size = size ? size : 4;

    switch (size) {
    case 1: return { "byte", "int8_t", "int16_t", 8, "0x80", "INT8_MIN", "INT8_MAX" };
    case 2: return { "word", "int16_t", "int32_t", 16, "0x8000", "INT16_MIN", "INT16_MAX" };
    case 4: return { "dword", "int32_t", "int64_t", 32, "0x80000000", "INT32_MIN", "INT32_MAX" };
    case 8: return { "uint64_t", "int64_t", nullptr, 64, "0x8000000000000000", "INT64_MIN", "INT64_MAX" };
    default: assert(false); return {};
    };
}
