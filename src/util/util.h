#pragma once

#include "swos.h"

#ifndef  sizeofarray
# define sizeofarray(a) (sizeof(a)/sizeof((a)[0]))
#endif

constexpr int kMaxPath = 256;

void sdlErrorExit(const char *format, ...);
void errorExit(const char *format, ...);

static inline bool guidEqual(const SDL_JoystickGUID& guid1, const SDL_JoystickGUID& guid2) {
    return !memcmp(&guid1, &guid2, sizeof(guid1));
}

struct TimeInfo {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    int msec;
};

uint64_t getMillisecondsSinceEpoch();
TimeInfo getCurrentTime();
std::string formatNumberWithCommas(int64_t num);
int numDigits(int num);

static inline int sgn(int num) {
    return num < 0 ? -1 : 1;
}

static inline char *stpcpy(char *dest, const char *src)
{
    while (*dest++ = *src++)
        ;

    return dest - 1;
}

struct TextInputScope {
    TextInputScope() { SDL_StartTextInput(); }
    ~TextInputScope() { SDL_StopTextInput(); }
};

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct ColorF {
    float r;
    float g;
    float b;
};

void save68kRegisters();
void restore68kRegisters();

template <typename F>
void invokeWithSaved68kRegisters(F f)
{
    save68kRegisters();
    f();
    restore68kRegisters();
}

unsigned hash(const char *str);
unsigned hash(const void *buffer, size_t length);
int getRandomInRange(int min, int max);
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
