#pragma once

#include "StringView.h"

constexpr int kNumAmigaRegisters = 15;

inline int amigaRegisterToIndex(const String& str)
{
    if (str.length() == 2) {
        auto text = str.str();

        if (text[0] == 'D') {
            if (text[1] >= '0' && text[1] <= '7')
                return text[1] - '0';
        } else if (text[0] == 'A') {
            if (text[1] >= '0' && text[1] <= '6')
                return text[1] - '0' + 8;
        }
    }

    return -1;
}


inline String indexToAmigaRegister(int index)
{
    static const char kAmigaRegisterNames[] = "D0D1D2D3D4D5D6D7A0A1A2A3A4A5A6";
    static_assert(sizeof(kAmigaRegisterNames) == 2 * kNumAmigaRegisters + 1, "kAmigaRegisterNames has invalid size");

    assert(index >= 0 && index < kNumAmigaRegisters);
    return String(&kAmigaRegisterNames[2 * index], 2);
}
