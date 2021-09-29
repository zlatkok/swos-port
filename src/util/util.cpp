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

static_assert(sizeof(uint64_t) >= sizeof(time_t), "Timestamp type too big");

uint64_t getMillisecondsSinceEpoch()
{
    using namespace std::chrono;

    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

TimeInfo getCurrentTime()
{
    using namespace std::chrono;

    auto timeSinceEpochMs = getMillisecondsSinceEpoch();

    time_t time = timeSinceEpochMs / 1000;
    auto tm = std::localtime(&time);

    return { 1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
        tm->tm_min, tm->tm_sec, static_cast<int>(timeSinceEpochMs % 1000) };
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
        auto quoteRem = std::lldiv(num, 10);
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

void formatDoubleNoTrailingZeros(double value, char *buf, int bufLen, int digits)
{
    int len = snprintf(buf, bufLen, "%.*f", digits, value);

    auto p = buf + len - 1;

    while (*p == '0')
        *p-- = '\0';

    if (*p == '.')
        *p = '\0';

    assert(p > buf);
}

int numDigits(int num)
{
    assert(num >= 0);

    if (num < 10)
        return 1;
    else if (num < 100)
        return 2;
    else if (num < 1'000)
        return 3;
    else if (num < 10'000)
        return 4;
    else {
        int numDigits = 4;
        num /= 10'000;

        do {
            numDigits++;
            num /= 10;
        } while (num);

        return numDigits;
    }
}

constexpr int kMaxRegStorageCapacity = 10;
static std::array<SwosVM::RegisterSet68k, kMaxRegStorageCapacity> m_savedRegisters;
static int m_regStorageIndex;

void save68kRegisters()
{
    if (m_regStorageIndex >= static_cast<int>(m_savedRegisters.size()))
        errorExit("Capacity for saving 68k registers exceeded (%d spots)", m_savedRegisters.size());

    SwosVM::store68kRegistersTo(m_savedRegisters[m_regStorageIndex++]);
}

void restore68kRegisters()
{
    if (m_regStorageIndex <= 0)
        errorExit("Saved 68k registers stack underflow");

    SwosVM::load68kRegistersFrom(m_savedRegisters[--m_regStorageIndex]);
}

constexpr size_t kInitialHashValue = 1021;

static size_t updateHash(size_t hash, char c, size_t index)
{
    hash += index + c;
    hash = _rotl(hash, 1);
    return hash ^ index + c;
}

unsigned hash(const char *str)
{
    size_t hash = kInitialHashValue;
    size_t index = 0;

    while (*str++)
        hash = updateHash(hash, str[-1], index++);

    return hash;
}

unsigned hash(const void *buffer, size_t length)
{
    size_t hash = kInitialHashValue;
    auto p = reinterpret_cast<const char *>(buffer);

    for (size_t i = 0; i < length; i++, p++)
        hash = updateHash(hash, *p, i);

    return hash;
}

int getRandomInRange(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_mt);
}

void beep()
{
#ifdef _WIN32
    ::PlaySound(TEXT("SystemExclamation"), nullptr, SND_ALIAS | SND_ASYNC);
#else
    // not working on Android, need something better, maybe some JNI calls
    putchar('\a');
#endif
}

// in:
//      D0 -  word to convert
//      A1 -> buffer
// out:
//      A1 -> points to terminating zero in the buffer
//
void SWOS::Int2Ascii()
{
    int num = D0.asInt16();
    auto dest = A1.asPtr();

    if (num < 0) {
        num = -num;
        *dest++ = '-';
    } else if (!num) {
        dest[0] = '0';
        dest[1] = '\0';
        A1++;
        return;
    }

    auto end = dest;
    while (num) {
        auto quotRem = std::div(num, 10);
        num = quotRem.quot;
        *end++ = quotRem.rem + '0';
    }

    *end = '\0';
    A1 = end;

    for (end--; dest < end; dest++, end--)
        std::swap(*dest, *end);
}

bool isDebuggerPresent()
{
#ifdef _WIN32
    return ::IsDebuggerPresent() != 0;
#else
    return false;
#endif
}

#ifdef DEBUG
void debugBreakIfDebugged()
{
    if (isDebuggerPresent())
        debugBreak();
}
#endif
