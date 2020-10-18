#pragma once

#include "swos.h"

#ifndef  sizeofarray
# define sizeofarray(a) (sizeof(a)/sizeof((a)[0]))
#endif

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

template <typename F>
void invokeWithSaved68kRegisters(F f)
{
    save68kRegisters();
    f();
    restore68kRegisters();
}

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

#ifdef SWOS_VM
// we're safe by default ;)
# define SAFE_INVOKE(proc) (proc)()
#else
// preserve registers VC++ doesn't expect to change between calls
# define SAFE_INVOKE(proc) \
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
#endif

// this had to be added since they banned inline assembly in lambda in VS2019
static inline void safeInvokeWithSaved68kRegisters(void (*f)())
{
    save68kRegisters();
    SAFE_INVOKE(f);
    restore68kRegisters();
}

#ifdef _MSC_VER
#define suppressConstQualifierOnFunctionWarning() __pragma(warning(suppress:4180))
#endif
