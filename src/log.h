#pragma once

enum LogCategory {
    kInfo,
    kWarning,
    kError,
};

struct LogItem {
    LogItem(LogCategory category, std::string text) : category(category), text(text) {}
    LogCategory category;
    std::string text;
};

#define logInfo(...) log(kInfo, __VA_ARGS__)
#define logWarn(...) log(kWarning, __VA_ARGS__)
#define logError(...) log(kError, __VA_ARGS__)

#ifndef NDEBUG
#define logDebug(...) logInfo(__VA_ARGS__)
#else
#define logDebug(...) ((void)0)
#endif

void initLog();
void flushLog();
void finishLog();
void log(LogCategory category, const char *format, ...);
void logv(LogCategory category, const char *format, va_list args);
std::string logPath();
