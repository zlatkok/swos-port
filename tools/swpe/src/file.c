#include "file.h"
#include "debug.h"

#ifdef DEBUG
/* debug support for open files checking */
typedef struct OpenFileDescriptor {
    HANDLE h;
    const char *fname;
    const char *file;
    int line;
} OpenFileDescriptor;

static OpenFileDescriptor m_files[500];
static int m_trackFiles;
static int m_totalOpenFiles;
static int m_maxFilesOpen;
static int m_openFilesIndex;

int getNumOpenFiles()
{
    return m_totalOpenFiles;
}

int getMaxOpenFiles()
{
    return m_maxFilesOpen;
}

void logOpenFiles()
{
    for (int i = 0; i < m_openFilesIndex; i++) {
        if (m_files[i].h == ERR_HANDLE)
            continue;
        WriteToLog(("%s(%d): Open file with handle %#x", m_files[i].file, m_files[i].line, m_files[i].h));
    }
}

static void RecordFile(HANDLE h, const char *fname, const char *file, const int line)
{
    if (m_trackFiles && h != ERR_HANDLE) {
        m_files[m_openFilesIndex].h = h;
        m_files[m_openFilesIndex].fname = fname;
        m_files[m_openFilesIndex].file = file;
        m_files[m_openFilesIndex++].line = line;
        m_totalOpenFiles++;
        m_maxFilesOpen = max(m_maxFilesOpen, m_totalOpenFiles);
    }
}

void SetTrackFiles(bool state)
{
    m_trackFiles = state;
}
#endif

static void TranslateFlags(const uint flags, uint *access, uint *share_mode, uint *attribs)
{
    *access = 0;
    if (flags & FF_READ)
        *access |= GENERIC_READ;
    if (flags & FF_WRITE)
        *access |= GENERIC_WRITE;
    *share_mode = 0;
    if (flags & FF_SHARE_READ)
        *share_mode |= FILE_SHARE_READ;
    if (flags & FF_SHARE_WRITE)
        *share_mode |= FILE_SHARE_WRITE;
    *attribs = 0;
    if (flags & FF_SEQ_SCAN)
        *attribs |= FILE_FLAG_SEQUENTIAL_SCAN;
}

#ifdef DEBUG
HANDLE FOpenDbg(const char *fname, const uint flags, const char *file, const int line)
#else
HANDLE FOpen(const char *fname, const uint flags)
#endif
{
    HANDLE h;
    uint access, share_mode, attribs, creation = OPEN_EXISTING;
    TranslateFlags(flags, &access, &share_mode, &attribs);
    if (flags & FF_OPEN_ALWAYS)
        creation = OPEN_ALWAYS;
    h = CreateFile(fname, access, share_mode, NULL, creation, attribs, NULL);
#if DEBUG
    RecordFile(h, fname, file, line);
#endif
    return h;
}


/* FCreate

   fname -> name of file to create
   flags -  control creation of file
   debug only:
   file -> source file name that called us
   line -  line number of request in file
*/
#ifdef DEBUG
HANDLE FCreateDbg(const char *fname, const uint flags, const char *file, const int line)
#else
HANDLE FCreate(const char *fname, const uint flags)
#endif
{
    HANDLE h;
    uint access, share_mode, attribs, creation = CREATE_ALWAYS;
    TranslateFlags(flags, &access, &share_mode, &attribs);
    if (flags & FF_OPEN_ALWAYS)
        creation = OPEN_ALWAYS;
    h = CreateFile(fname, access, share_mode, NULL, creation, attribs, NULL);
#if DEBUG
    RecordFile(h, fname, file, line);
#endif
    return h;
}

#ifdef DEBUG
/* FCloseDbg

   h    -  file handle to close
   file -> source file name that called us
   line -  line number of request in file

   Close file and check if the handle is valid. If it's not, issue a warning
   into log file.
*/
void FCloseDbg(HANDLE h, const char *file, const int line)
{
    int i;
    CloseHandle(h);
    if (!m_trackFiles)
        return;
    for (i = 0; i < m_openFilesIndex && m_files[i].h != h; i++);
    if (i >= m_openFilesIndex) {
        WriteToLog(("*** %s(%d): closing invalid file handle [%#x]", file, line, h));
        return;
    }
    m_totalOpenFiles--;
    m_files[i].h = ERR_HANDLE;
    if (i < m_openFilesIndex - 1)
        return;
    /* compact list */
    do {
        m_openFilesIndex--;
    } while (i > 0 && m_files[--i].h == ERR_HANDLE);
}
#endif
