#pragma once

#include "swos.h"

#define sizeofarray(x) (sizeof(x)/sizeof(x[0]))

constexpr int kMaxPath = 256;

void sdlErrorExit(const char *format, ...);
void errorExit(const char *format, ...);

struct TimeInfo {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    int msec;
};

TimeInfo getCurrentTime();
std::string formatNumberWithCommas(int64_t num);
void toUpper(char *str);

void save68kRegisters();
void restore68kRegisters();

size_t hash(const void *buffer, size_t length);
int getRandomInRange(int min, int max);

int setZeroFlagAndD0FromAl();

bool isMatchRunning();

void beep();

bool isDebuggerPresent();

#ifdef DEBUG
void debugBreakIfDebugged();
#else
#define debugBreakIfDebugged() ((void)0)
#endif

inline bool hiBitSet(dword d) {
    return (d & 0x80000000) != 0;
}
inline void clearHiBit(dword& d) {
    d &= 0x7fffffff;
}
inline word loWord(dword d) {
    return d & 0xffff;
}
inline word hiWord(dword d) {
    return d >> 16;
}

// preserve registers VC++ doesn't expect to change between calls
#define SAFE_INVOKE(proc) \
{                   \
    __asm push ebx  \
    __asm push esi  \
    __asm push edi  \
    __asm push ebp  \
    __asm call proc \
    __asm pop  ebp  \
    __asm pop  edi  \
    __asm pop  esi  \
    __asm pop  ebx  \
}

#ifdef _MSC_VER
#define suppressConstQualifierOnFunctionWarning() __pragma(warning(suppress:4180))
#else
#error Define pragma to suppress warning when using const with function pointer for your compiler, if needed
#endif
