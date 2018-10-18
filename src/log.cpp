#include "log.h"
#include "file.h"
#include "util.h"
#include <sys/stat.h>

constexpr char kLogFilename[] = "swos.log";
constexpr char kOldLogFilename[] = "swos.log.old";
static FILE *m_logFile;
static std::string m_logPath;

static bool fileExists(const char *path)
{
    struct stat buffer;
    return stat(path, &buffer) == 0;
}

void initLog()
{
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
}

void flushLog()
{
    if (m_logFile)
        fflush(m_logFile);
}

void finishLog()
{
    logInfo("Log ended.");
    if (m_logFile)
        fclose(m_logFile);
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

    auto len = sprintf_s(buf, "[%d-%02d-%02d %02d:%02d:%02d.%03d] %s ", tm.year, tm.month, tm.day, tm.hour, tm.min, tm.sec, tm.msec, catStr);

    len += vsprintf_s(buf + len, sizeof(buf) - len, format, args);

    if (buf[len - 1] != '\n' && len <= sizeof(buf) - 2) {
        buf[len++] = '\n';
        buf[len] = '\0';
    }

    if (!fwrite(buf, len, 1, m_logFile))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Writing to log file failed");

    std::cout << buf;
}

std::string logPath()
{
    return m_logPath;
}
