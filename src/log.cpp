#include "log.h"
#include "util.h"

#ifdef __ANDROID__
constexpr const char * const kLogTag = "swos";
#else
# include "file.h"
# include <sys/stat.h>

constexpr char kLogFilename[] = "swos.log";
constexpr char kOldLogFilename[] = "swos.log.old";
static FILE *m_logFile;
static std::string m_logPath;

static bool fileExists(const char *path)
{
    struct stat buffer;
    return stat(path, &buffer) == 0;
}
#endif

void initLog()
{
#ifndef __ANDROID__
    auto dir = rootDir();
    m_logPath = dir + kLogFilename;

    if (fileExists(m_logPath.c_str())) {
        auto oldLog = dir + kOldLogFilename;
        remove(oldLog.c_str());
        rename(m_logPath.c_str(), oldLog.c_str());
    }

    m_logFile = fopen(m_logPath.c_str(), "w");

    if (!m_logFile)
        errorExit("Could not open log file %s for writing", kLogFilename);

    logInfo("Log started");
    if (!dir.empty())
        logInfo("Root directory set to `%s'", dir.c_str());
#endif
}

void flushLog()
{
#ifndef __ANDROID__
    if (m_logFile)
        fflush(m_logFile);
#endif
}

void finishLog()
{
#ifndef __ANDROID__
    logInfo("Log ended.");
    if (m_logFile)
        fclose(m_logFile);
    m_logFile = nullptr;
#endif
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
#else
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

    if (len >= 0 && buf[len - 1] != '\n' && len <= sizeof(buf) - 2) {
        buf[len++] = '\n';
        buf[len] = '\0';
    }

    if (!fwrite(buf, len, 1, m_logFile))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Writing to log file failed");

    std::cout << buf;
#endif
}

std::string logPath()
{
#ifndef __ANDROID__
    return m_logPath;
#else
    return {};
#endif
}
