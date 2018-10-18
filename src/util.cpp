#include "util.h"
#include "log.h"
#include <ctime>
#include <chrono>

void sdlErrorExit(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    logv(kError, format, args);
    va_end(args);

    logError("SDL reported: %s", SDL_GetError());

    std::exit(EXIT_FAILURE);
}

void errorExit(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    logv(kError, format, args);
    va_end(args);

#ifndef NDEBUG
    __debugbreak();
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

    std::reverse(result.begin(), result.end());;

    return result;
}

static char savedRegisters[k68kRegisterTotalSize];

void save68kRegisters()
{
    memcpy(savedRegisters, &D0, k68kRegisterTotalSize);
}

void restore68kRegisters()
{
    memcpy(&D0, savedRegisters, k68kRegisterTotalSize);
}
