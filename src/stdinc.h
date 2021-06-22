#pragma once

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

#ifdef __ANDROID__
# include <SDL_system.h>
#endif

// prevent name clash of WriteFile that comes from SWOS and a Win32 API function
#ifdef _WIN32
# define WriteFile Win32WriteFile
#endif

#include <SimpleIni.h>

#ifdef _WIN32
# undef WriteFile
#endif

#include <SDL2/SDL.h>
#include <SDL_mixer.h>
#include <SDL2_Image/SDL_image.h>

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

#ifndef _WIN64
# define PTR32
static_assert(sizeof(void *) == 4, "Define pointer size");
#endif

#define strncpy_s(strDest, strSource, size) strncpy_s(strDest, size, strSource, _TRUNCATE)

#ifndef SWOS_TEST
# define sprintf_s(...) static_assert(false, "sprintf_s detected, use snprintf instead!")
#endif
