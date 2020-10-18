#include <jni.h>
#include <errno.h>

#include <unistd.h>
#include <sys/resource.h>

#include <android/log.h>

#include <cstdint>
#include <iostream>
#include <cstdarg>
#include <algorithm>
#include <string>
#include <numeric>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <thread>
#include <array>
#include <vector>
#include <functional>
#include <condition_variable>
#include <sys/stat.h>

#include <SimpleIni.h>

#include <SDL2/SDL.h>
#include <SDL_mixer.h>

#include "assert.h"
#include "swos.h"
#include "swossym.h"
#include "log.h"

#ifdef SWOS_VM
# include "vm.h"
# define __declspec(naked)
#endif

using namespace std::string_literals;

#ifdef __arm__
# define PTR32
static_assert(sizeof(void *) == 4, "Define pointer size");
#endif

#define _stricmp strcasecmp

template <size_t size>
int sprintf_s(char(&buffer)[size], const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vsnprintf(buffer, size, format, ap);
    va_end(ap);
    return result;
}

template <size_t size> size_t strcpy_s(char (&dst)[size], const char *src)
{
    return strlcpy(dst, src, size);
}

#define _TRUNCATE ((size_t)-1)
#define strncpy_s(dst, dmax, src, slen) strlcpy(dst, src, dmax)

static inline char *_itoa(int val, char *buf, int)
{
    sprintf(buf, "%d", val);
    return buf;
}

template <size_t size> char *itoa_s(int val, char (&buf)[size], int)
{
    snprintf(buf, size, "%d", val);
    return buf;
}

#define _itoa_s itoa_s

static inline unsigned int _rotl(unsigned int val, int shift)
{
    assert(shift > 0 && shift < 32);
    return (val << shift) | (val >> (32 - shift));
}
