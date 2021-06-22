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
#include <SDL_image.h>

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
#error "Big endian not supported!"
#endif

constexpr int kRedMask = 0xff;
constexpr int kGreenMask = 0xff00;
constexpr int kBlueMask = 0xff0000;
constexpr int kAlphaMask = 0xff000000;

#include "assert.h"
#include "swos.h"
#include "swossym.h"
#include "log.h"

#include "vm.h"
#define __declspec(naked)

using namespace std::string_literals;

#ifdef __arm__
# define PTR32
static_assert(sizeof(void *) == 4, "Define pointer size");
#endif

#define _stricmp strcasecmp

template <size_t size> size_t strcpy_s(char (&dst)[size], const char *src)
{
    return strlcpy(dst, src, size);
}

#define _TRUNCATE ((size_t)-1)
#define strncpy_s(dst, src, dmax) strlcpy(dst, src, dmax)

template <size_t size> char *itoa_s(int val, char (&buf)[size], int)
{
    snprintf(buf, size, "%d", val);
    return buf;
}

#define stricmp strcasecmp
#define _itoa_s itoa_s

static inline unsigned int _rotl(unsigned int val, int shift)
{
    assert(shift > 0 && shift < 32);
    return (val << shift) | (val >> (32 - shift));
}
