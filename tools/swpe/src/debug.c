#include <stdarg.h>
#include "debug.h"
#include "file.h"

/* controls file flushing when writing debug messages to log */
/*#define FLUSH*/

#ifdef DEBUG

HANDLE hlogfile;

/* OpenLogFile

   Opens log file and writes starting message.
*/
void OpenLogFile()
{
    extern const char prog_name[], version[];
    hlogfile = FCreate("swdbg.log", FF_WRITE | FF_SHARE_READ);
    if (hlogfile == INVALID_HANDLE_VALUE) {
        MessageBox(NULL, "Can't open log file", "Debug system", MB_ICONERROR | MB_TASKMODAL);
        ExitProcess(1);
    }
    WriteToLog((">>> Log file started..."));
    WriteToLog(("[ %s %s ]", prog_name, version));
}


/* CloseLogFile

   Writes ending message and closes log file.
*/
void CloseLogFile()
{
    WriteToLog((">>> Log file ended..."));
    CloseHandle(hlogfile);
}


/* WriteToLogFunc

   str - printf format string
   ... - additional parameters for format string

   Prints printf-format style string to log file.
*/
void WriteToLogFunc(char *str, ...)
{
    ulong tmp;
    uchar buf[2048];
    va_list va;

    va_start(va, str);
    wvsprintf(buf, str, va);
    va_end(va);

    OutputDebugStringA(buf);
    OutputDebugStringA("\n");

    WriteFile(hlogfile, buf, strlen(buf), &tmp, 0);
    WriteFile(hlogfile, "\n", 1, &tmp, 0);
#ifdef FLUSH
    FlushFileBuffers(hlogfile);
#endif
}

#endif /* DEBUG */