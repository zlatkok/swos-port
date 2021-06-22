#include "log.h"
#include "util.h"
# include "file.h"
# include <sys/stat.h>

#ifdef __ANDROID__
constexpr const char * const kLogTag = "swos";
#endif

constexpr char kLogFilename[] = "swos.log";
constexpr char kOldLogFilename[] = "swos.log.old";
static SDL_RWops *m_logFile;
static std::string m_logPath;

void initLog()
{
    auto dir = rootDir();
    m_logPath = dir + kLogFilename;

    if (fileExists(m_logPath.c_str())) {
        auto oldLog = dir + kOldLogFilename;
        remove(oldLog.c_str());
        rename(m_logPath.c_str(), oldLog.c_str());
    }

    m_logFile = SDL_RWFromFile(m_logPath.c_str(), "w");

    if (!m_logFile)
        errorExit("Could not open log file %s for writing", kLogFilename);

    logInfo("Log started");
    if (!dir.empty())
        logInfo("Root directory set to `%s'", dir.c_str());
}

void finishLog()
{
    logInfo("Log ended.");
    if (m_logFile)
        SDL_RWclose(m_logFile);
    m_logFile = nullptr;
}

void log(LogCategory category, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    logv(category, format, args);
    va_end(args);
}

void logv(LogCategory category, const char *format, va_list args)
{
#ifdef __ANDROID__
    auto prio = ANDROID_LOG_INFO;
    if (category == kError)
        prio = ANDROID_LOG_ERROR;
    else if (category == kWarning)
        prio = ANDROID_LOG_WARN;
    else
        assert(category == kInfo);

    __android_log_vprint(prio, kLogTag, format, args);
#endif
    if (!m_logFile)
        return;

    auto catStr = "";
    switch (category) {
    case kInfo: catStr = "[INFO]"; break;
    case kWarning: catStr = "[WARNING]"; break;
    case kError: catStr = "[ERROR]"; break;
    default: assert(false);
    }

    auto tm = getCurrentTime();
    char buf[16 * 1024];

    auto len = snprintf(buf, sizeof(buf), "[%d-%02d-%02d %02d:%02d:%02d.%03d] %s ", tm.year, tm.month, tm.day, tm.hour, tm.min, tm.sec, tm.msec, catStr);

    len += vsnprintf(buf + len, sizeof(buf) - len - 1, format, args);

    if (len >= 1 && buf[len - 1] != '\n' && len <= sizeof(buf) - 2) {
        buf[len++] = '\n';
        buf[len] = '\0';
    }

    if (!SDL_RWwrite(m_logFile, buf, len, 1))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Writing to log file failed");

#ifndef __ANDROID__
    std::cout << buf;
#endif
}

std::string logPath()
{
    return m_logPath;
}
