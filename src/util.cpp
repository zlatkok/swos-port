#include "util.h"
#include "log.h"
#include <dirent.h>
#include <ctime>
#include <chrono>
#include <random>

static std::random_device m_randomDevice;
static std::mt19937 m_mt(m_randomDevice());

void sdlErrorExit(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    logv(kError, format, args);
    va_end(args);

    logError("SDL reported: %s", SDL_GetError());

    debugBreakIfDebugged();
    std::exit(EXIT_FAILURE);
}

void errorExit(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    logv(kError, format, args);
    va_end(args);

#ifdef DEBUG
    debugBreak();
#endif

    std::exit(EXIT_FAILURE);
}

TimeInfo getCurrentTime()
{
    using namespace std::chrono;

    auto timeSinceEpochMs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    time_t time = timeSinceEpochMs / 1000;
    auto tm = std::localtime(&time);

    return { 1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, timeSinceEpochMs % 1000 };
}

std::string formatNumberWithCommas(int64_t num)
{
    std::string result;

    bool neg = false;
    if (num < 0) {
        neg = true;
        num = -num;
    }

    int i = 0;
    do {
        auto quoteRem = std::div(num, 10ll);
        num = quoteRem.quot;
        if (i++ == 3) {
            result += ',';
            i = 1;
        }
        result += static_cast<char>(quoteRem.rem) + static_cast<int64_t>('0');
    } while (num);

    if (neg)
        result += '-';

    std::reverse(result.begin(), result.end());

    return result;
}

void toUpper(char *str)
{
    while (*str++)
        str[-1] = toupper(str[-1]);
}

constexpr int kMaxRegStorageCapacity = 10;
static std::array<char[k68kRegisterTotalSize], kMaxRegStorageCapacity> m_savedRegisters;
static int m_regStorageIndex;

void save68kRegisters()
{
    if (m_regStorageIndex >= static_cast<int>(m_savedRegisters.size()))
        errorExit("Capacity for saving 68k registers exceeded (%d spots)", m_savedRegisters.size());

    memcpy(m_savedRegisters[m_regStorageIndex++], &D0, k68kRegisterTotalSize);
}

void restore68kRegisters()
{
    if (m_regStorageIndex <= 0)
        errorExit("Saved 68k registers stack underflow");

    memcpy(&D0, m_savedRegisters[--m_regStorageIndex], k68kRegisterTotalSize);
}

constexpr size_t kInitialHashValue = 1021;

size_t hash(const void *buffer, size_t length)
{
    size_t hash = kInitialHashValue;
    auto p = reinterpret_cast<const char *>(buffer);

    for (size_t i = 0; i < length; i++, p++) {
        hash += i + *p;
        hash = _rotl(hash, 1);
        hash ^= i + *p;
    }

    return hash;
}

int getRandomInRange(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_mt);
}

__declspec(naked) int setZeroFlagAndD0FromAl()
{
    __asm {
        and  eax, 0xff
        test eax, eax
        mov  D0, eax
        jz   done

        or eax, eax

done:
        retn
    }
}

bool isMatchRunning()
{
    return screenWidth == kGameScreenWidth;
}

void beep()
{
    ::PlaySound(TEXT("SystemExclamation"), nullptr, SND_ALIAS | SND_ASYNC);
}

bool isDebuggerPresent()
{
    return ::IsDebuggerPresent() != 0;
}

#ifdef DEBUG
void debugBreakIfDebugged()
{
    if (isDebuggerPresent())
        debugBreak();
}
#endif
