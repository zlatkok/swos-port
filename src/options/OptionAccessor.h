#pragma once

template<typename T>
struct OptionAccessor
{
    T (*get)();
    void (*set)(T);
    const char *section;
    const char *key;
    T defaultValue;
};
