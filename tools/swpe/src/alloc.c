#include "alloc.h"
#include "util.h"
#include "file.h"
#include "debug.h"
#include <assert.h>

#ifdef DEBUG
/* memory leak checking debug support */
typedef struct MemBlock {
    void *p;
    int size;
    const char *file;
    int line;
} MemBlock;

static MemBlock m_blocks[20'000];
static MemBlock m_untrackedBlocks[2'000];
static int m_currentBlock;
static int m_currentUntrackedBlock;
static int m_trackAlloc;
static int m_totalMem;
static int m_totalRequests;
static int m_maxMemory;
static int m_maxRequests;
#endif

/* omalloc

   size - size of requested memory block
   debug only:
   file -> filename that requested memory
   line -  line number of request in file

   This is ordinary malloc routine, returning pointer to memory block of
   consecutive size bytes, or NULL if the request could not be satisfied.
*/
#ifdef DEBUG
void *omallocDbg(uint size, const char *file, const int line)
#else
void *omalloc(uint size)
#endif
{
    void *p = GlobalAlloc(GMEM_FIXED, size);
#ifdef DEBUG
    if (p) {
        MemBlock *blocks = m_trackAlloc ? m_blocks : m_untrackedBlocks;
        int *currentBlock = m_trackAlloc ? &m_currentBlock : &m_currentUntrackedBlock;
        blocks[*currentBlock].p = p;
        blocks[*currentBlock].file = file;
        blocks[*currentBlock].line = line;
        blocks[(*currentBlock)++].size = size;
        if (m_trackAlloc) {
            m_totalMem += size;
            m_totalRequests++;
            m_maxMemory = max(m_maxMemory, m_totalMem);
            m_maxRequests = max(m_maxRequests, m_totalRequests);
        }
        memset(p, 0xfe, size);
    }
#endif
    return p;
}

/* xmalloc

   size - size of requested memory block
   debug only:
   file -> filename that requested memory
   line -  line number of request in file

   Routine behavior is same as omalloc, except that it will never return NULL
   pointer - in that case, user will be notified of lack of memory and the
   program will be terminated. Will try several times, after waiting certain
   amount of time, is system can't fulfill the request.
*/
#ifdef DEBUG
void *xmallocDbg(uint size, const char *file, const int line)
#else
void *xmalloc(uint size)
#endif
{
    void *p;
    extern Global_info g;
    int i;
    for (i = 0; i < 10; i++) {
#ifdef DEBUG
        if (p = omallocDbg(size, file, line))
#else
        if (p = omalloc(size))
#endif
            break;
        Sleep(8 * i);
    }
    if (p) {
        return p;
    } else {
        WriteToLog(("xmalloc(): Memory allocation failure. Terminating program."));
        g.crashed = TRUE;
        ShowWindow(g.hWnd, SW_HIDE);
        MessageBox(g.hWnd, "Out of memory!", "Error", MB_OK);
#ifdef DEBUG
        DebugBreak();
#endif
        ExitProcess(1);
        return NULL; /* ach... */
    }
}

#ifdef DEBUG
void SetTrackAlloc(bool state)
{
    m_trackAlloc = state;
}

void ShowMemoryAndFilesUsage()
{
    char buf[256];
    uint mb_flags;

    if (!m_totalMem && !getNumOpenFiles())
        return;

    mb_flags = MB_OK | (m_totalMem || getNumOpenFiles() ? MB_ICONERROR : 0);
    wsprintf(buf, "Memory requests left: %d\nMemory not freed: %d bytes\n"
        "Maximum memory: %d bytes\nMaximum requests: %d\n"
        "Files open: %d\nMaximum files open: %d", m_totalRequests,
        m_totalMem, m_maxMemory, m_maxRequests, getNumOpenFiles(), getMaxOpenFiles());

    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
    MessageBox(NULL, buf, "Memory/Files report", mb_flags);

    if (m_totalMem)
        WriteToLog(("*** Memory leak report: ***"));
    for (int i = 0; i < m_currentBlock; i++) {
        if (!m_blocks[i].p)
            continue;
        WriteToLog(("%s(%d): %d bytes [%#x]", m_blocks[i].file, m_blocks[i].line, m_blocks[i].size, m_blocks[i].p));
    }
    if (getNumOpenFiles())
        WriteToLog(("*** Open files report: ***"));
    logOpenFiles();
}

void xfreeDbg(void *p, char *file, int line)
{
    if (!p)
        return;

    int i;

    for (i = 0; i < m_currentBlock && m_blocks[i].p != p; i++);
    if (i >= m_currentBlock) {
        for (i = 0; i < m_currentUntrackedBlock && m_untrackedBlocks[i].p != p; i++);
        if (i >= m_currentUntrackedBlock) {
            WriteToLog(("*** %s(%d): freeing invalid pointer [%#x]", file, line, p));
            assert(false);
        } else {
            memset(p, 0xce, m_untrackedBlocks[i].size);
            m_untrackedBlocks[i].p = nullptr;
            m_untrackedBlocks[i].size = 0;
        }
        GlobalFree(p);
        return;
    }

    memset(p, 0xbf, m_blocks[i].size);

    m_totalRequests--;
    m_totalMem -= m_blocks[i].size;
    m_blocks[i].p = NULL;
    m_blocks[i].size = 0;

    if (i < m_currentBlock - 1)
        return;

    /* compact the list */
    do {
        m_currentBlock--;
    } while (i > 0 && !m_blocks[--i].p);

    GlobalFree(p);
}
#endif
