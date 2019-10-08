#include "log.h"
#include "mockLog.h"
#include "unitTest.h"

static bool m_strictLogMode = true; // catch-all by default

void initLog() {}
void flushLog() {}
void finishLog() {}
std::string logPath() { return {}; }

void log(LogCategory category, const char *, ...)
{
    assertTrue(!m_strictLogMode || category == kInfo);
}

void logv(LogCategory category, const char *, va_list)
{
    assertTrue(!m_strictLogMode || category == kInfo);
}

bool setStrictLogMode(bool active)
{
    auto oldMode = m_strictLogMode;
    m_strictLogMode = active;
    return oldMode;
}
