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

#include "assert.h"
#include "swos.h"
#include "swossym.h"
#include "log.h"

#ifdef SWOS_VM
# include "vm.h"
# define __declspec(naked)
#endif

using namespace std::string_literals;

#ifndef _WIN64
# define PTR32
static_assert(sizeof(void *) == 4, "Define pointer size");
#endif
