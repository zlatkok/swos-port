#include "debug.h"
#include "pattern.h"
#include "pitch.h"
#include "pitchUndo.h"
#include "printstr.h"
#include "draw.h"
#include "alloc.h"
#include "file.h"
#include "util.h"
#include <limits.h>

static bool InitPatterns();
static void InitPatternMode(uint old_mode);
static bool PatternsKeyProc(uint code, uint data);
static byte *PatternsDraw(byte *pbits, const uint pitch);
static bool MovePattern(int new_pat);
static bool GotoPattern(int new_pat);
static void insertAllPatterns();
static void insertNewPatternAtEnd();
static bool expandPatterns(PitchPatterns *patterns, int numPatterns);
static int getNumberOfConsecutivePatternFilesInDirectory();

extern uchar gamepal[768];
extern PitchInfo pt;

static schar zoomf = 1; /* zoom factor */

/* help for pattern mode */
static char kPatternsHelp[] = {
    "arrows right/left\t- next/previous pitch\n"
    "arrows up/down\t\t- next/previous tile\n"
    "home/end\t\t\t- first/last tile\n"
    "page up/down\t\t- \r10 tiles forward/\nbackward\n"
    "space/backspace\t\t- change pitch type\n"
    "F2\t\t\t\t\t- save tile to bitmap\n"
    "shift + F2\t\t\t- save all tiles\n"
    "g\t\t\t\t\t- go to tile\n"
    "c\t\t\t\t\t- change tile number\n"
    "h\t\t\t\t\t- highlight tile\n"
    "delete\t\t\t\t- delete tile\n"
    "insert\t\t\t\t- \rreplace current\n\rtile with\ncorresponding bitmap\n"
    "shift + insert\t\t- \rinsert all tiles in\ngame directory\n"
    "control + insert\t\t- \rinsert new empty\ntile at the end\n"
    "control + z\t\t\t- undo\n"
    "control + y\t\t\t- redo\n"
};

const Mode PatternMode = {
    InitPatterns,
    InitPatternMode,
    Empty,
    PatternsKeyProc,
    nullptr,
    PatternsDraw,
    PatternsGetPalette,
    kPatternsHelp,
    "TILE MODE",
    VK_F8,
    0
};

Patterns_info pat;
PitchPatterns g_patterns[MAX_PITCH];    // information about patterns in every pitch

static void InsertPattern(uint pitch_no, uint pattern_no);
static bool SavePattern(uint pitch_no, uint pattern_no);
static void DeleteCurrentPattern();

/* strings for pitch types */
char *g_pitchTypes[] = {
    "frozen",
    "muddy",
    "wet",
    "soft",
    "normal",
    "dry",
    "hard"
};

/* shared by InitPatterns() and SavePattern() */
static char m_pitchFilename[] = "pitchx.dat";

/* InitPatterns

   Read everything needed to display patterns into memory. Also check second
   hash on copyright string.
*/
bool InitPatterns()
{
    for (int i = 0; i < MAX_PITCH; i++) {
        char buf[512];
        if (!LoadPitch(i, buf)) {
            SetErrorMsg(buf);
            return false;
        }
    }

    /* decrypt second pass - multiply */
    for (char *p = acopyright; p < acopyright + COPYRIGHT_LEN; p++)
        *p *= 61;

    pt.pal = xmalloc(sizeof(gamepal));
    memcpy(pt.pal, &gamepal, sizeof(gamepal));

    /* check HASH2 */
    int i = 0;
    uint h = 0;
    uchar *p = acopyright;
    while (i < COPYRIGHT_LEN) {
        h += *p * i * i;
        i++;
        p++;
    }

#ifdef DEBUG
    if (h != HASH2)
        SetErrorMsg("HASH2 mismatch, check COPYRIGHT_LEN");
#endif
    return h == HASH2;
}

/* InitPatternsMode

   Called every time when switching to pattern mode.
*/
void InitPatternMode(uint old_mode)
{
    SetPitchPalette(pt.pitchType);
    // just in case pitch mode messed with patterns
    pat.curPattern = max(pat.curPattern, 0);
    pat.curPattern = min(pat.curPattern, g_patterns[pat.curPitch].numPatterns - 1);
}

void CopyPattern(char *from, char *where, uint pitch)
{
    for (int i = 0; i < PATTERN_LENGTH; i++) {
        memcpy(where, from, PATTERN_LENGTH);
        where += pitch;
        from += PATTERN_LENGTH;
    }
}

static void deletePatternInternal(int pitchNo, int patternNo, bool logUndo)
{
    PitchPatterns *patterns = &g_patterns[pitchNo];

    if (logUndo)
        logPatternDeletionForUndo(pitchNo, patternNo, patterns, g.changedFlags);

    char *data = patterns->data;
    byte *currentPatternData = (byte *)data + PATTERN_BYTE_SIZE * patternNo;

    if (patternNo < patterns->numPatterns - 1)
        memmove(&patterns->used[patternNo], &patterns->used[patternNo + 1],
            (patterns->numPatterns - patternNo - 1) * sizeof(patterns->used[0]));

    memmove(currentPatternData, currentPatternData + PATTERN_BYTE_SIZE,
        (patterns->numPatterns-- - patternNo - 1) * PATTERN_BYTE_SIZE);
    g.changedFlags |= PATTERNS_CHANGED | PITCH_CHANGED;
    patterns->changed = TRUE;

    uint *ptrs = patterns->ptrs;
    /* see if any cell is referencing this pattern and change it to zero */
    for (int i = 0; i < patterns->numPtrs; i++) {
        ptrs[i] -= (uint)data;

        /* anything referencing deleted pattern goes to zero */
        if (ptrs[i] / PATTERN_BYTE_SIZE == patternNo)
            ptrs[i] = 0;

        /* following patterns will be shifted */
        if (ptrs[i] / PATTERN_BYTE_SIZE > (uint)patternNo)
            ptrs[i] -= PATTERN_BYTE_SIZE;

        ptrs[i] += (uint)data;
    }

    if (pat.curPattern > patternNo)
        pat.curPattern--;
}

void deletePattern(int pitchNo, int patternNo)
{
    deletePatternInternal(pitchNo, patternNo, true);
}

void deletePatternWithoutUndo(int pitchNo, int patternNo)
{
    deletePatternInternal(pitchNo, patternNo, false);
}

void showPattern(int pitchNo, int patternNo)
{
    assert(pitchNo >= 0 && pitchNo < MAX_PITCH && patternNo >= 0 &&
        patternNo < (int)g_patterns[pitchNo].numPatterns);
    pat.curPitch = pitchNo;
    pat.curPattern = patternNo;
}

void DrawPattern(char *where, uint pitch)
{
    char *from;
    pat.curPitch = min(pat.curPitch, MAX_PITCH - 1);
    pat.curPitch = max(pat.curPitch, 0);
    pat.curPattern = max(pat.curPattern, 0);
    pat.curPattern = min(pat.curPattern, g_patterns[pat.curPitch].numPatterns - 1);
    from = (char*)g_patterns[pat.curPitch].data + PATTERN_BYTE_SIZE * pat.curPattern;
    CopyPattern(from, where + (HEIGHT / 2 - 8) * pitch + WIDTH / 2 - 8, pitch);
}

/* PatternsKeyProc

   code - virtual key code
   data - key data

   Handle keys for patterns mode.
*/
bool PatternsKeyProc(uint code, uint data)
{
    switch (code) {
    case VK_RIGHT:
        pat.curPitch++;
        break;
    case VK_HOME:
        pat.curPattern = 0;
        break;
    case VK_END:
        pat.curPattern = g_patterns[pat.curPitch].numPatterns;
        break;
    case VK_LEFT:
        pat.curPitch--;
        break;
    case VK_DOWN:
        pat.curPattern--;
        break;
    case VK_UP:
        pat.curPattern++;
        break;
    case VK_NEXT:
        pat.curPattern += 10;
        break;
    case VK_PRIOR:
        pat.curPattern -= 10;
        break;
    case VK_ADD:
        zoomf++;
        zoomf = min(zoomf, MAX_ZOOM);
        break;
    case VK_SUBTRACT:
        zoomf--;
        zoomf = max(1, zoomf);
        break;
    case VK_SPACE:
        pt.pitchType++;
        pt.pitchType = min(pt.pitchType, MAX_PITCH_TYPES - 1);
        SetPitchPalette(pt.pitchType);
        break;
    case VK_BACK:
        pt.pitchType--;
        pt.pitchType = max(pt.pitchType, 0);
        SetPitchPalette(pt.pitchType);
        break;
    case VK_F2:
        if (isShiftDown()) {
            bool success = saveIndexFile(pat.curPitch);
            if (success) {
                for (int i = 0; i < g_patterns[pat.curPitch].numPatterns; i++) {
                    if (!SavePattern(pat.curPitch, i)) {
                        success = false;
                        break;
                    }
                }
            }
            PrintWarning(success ? "ALL TILES SAVED" : "SAVE FAILED", WARNING_INTERVAL, ALIGN_CENTER);
        } else if (!SavePattern(pat.curPitch, pat.curPattern)) {
            PrintWarning("SAVE FAILED", WARNING_INTERVAL, ALIGN_CENTER);
        }
        break;
    case VK_INSERT:
        if (isShiftDown())
            insertAllPatterns();
        else if (isControlDown())
            insertNewPatternAtEnd();
        else
            InsertPattern(pat.curPitch, pat.curPattern);
        break;
    case VK_DELETE:
        YNPrompt("Delete tile", DeleteCurrentPattern);
        break;
    case 'C':
        InputNumber("Change tile number to: ", 3, MovePattern);
        break;
    case 'G':
        InputNumber("Go to tile: ", 3, GotoPattern);
        break;
    case 'H':
        if (getHighlightedPattern(pat.curPitch) == pat.curPattern)
            setHighlightedPattern(pat.curPitch, -1);
        else
            setHighlightedPattern(pat.curPitch, pat.curPattern);
        markPitchForRedraw();
        UpdateScreen();
        break;
    case 'Z':
    case 'Y':
        undoRedoPitchOperation(pat.curPitch, code == 'Z');
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

/* PatternsDraw

   pbits  - pointer to bitmap bits
   pitch  - distance between lines

   Draw routine for pattern mode.
*/
static byte *PatternsDraw(byte *pbits, const uint pitch)
{
    for (int i = 0; i < HEIGHT; i++)
        memset(pbits + i * pitch, 3, WIDTH);

    DrawPattern(pbits, pitch);
    Zoom(pbits, zoomf, pitch);

    char refCountBuf[256];
    int refCount = g_patterns[pat.curPitch].used[pat.curPattern];
    if (refCount > 0)
        int2str(refCount, refCountBuf);
    else
        strcpy(refCountBuf, "**UNUSED**");
    char buf[256];
    wsprintf(buf, "PITCH %d - TILE %d (%s)", pat.curPitch + 1, pat.curPattern, refCountBuf);
    PrintString(buf, 0, 0, pbits, pitch, TRUE, -1, NO_ALIGNMENT);

    if (getHighlightedPattern(pat.curPitch) == pat.curPattern)
        PrintString("(HIGHLIGHTED)", 0, 0, pbits, pitch, false, -1, ALIGN_CENTERX | ALIGN_BOTTOM);

    return pbits;
}

/* PatternsGetPalette

   Returns valid palette for pattern mode, and is also shared by pitch mode.
*/
byte *PatternsGetPalette()
{
    return pt.pal;
}

bool LoadPitch(int pitchNo, char *errorBuf)
{
    m_pitchFilename[5] = pitchNo + 1 + '0';
    strcpy(&m_pitchFilename[7], "dat");

    HANDLE hfile = FOpen(m_pitchFilename, FF_READ | FF_SHARE_READ);
    if (hfile == ERR_HANDLE) {
        wsprintf(errorBuf, "Can't open %s", m_pitchFilename);
        return false;
    }

    PitchPatterns p;
    uint fsize = GetFileSize(hfile, 0);
    p.numPtrs = fsize / 4;
    p.ptrs = xmalloc(fsize);

    uint size;
    if (!ReadFile(hfile, p.ptrs, fsize, &size, 0) || size != fsize) {
        wsprintf(errorBuf, "Can't read file %s", m_pitchFilename);
        FClose(hfile);
        xfree(p.ptrs);
        return false;
    }

    FClose(hfile);
    strcpy(&m_pitchFilename[7], "blk");

    if ((hfile = FOpen(m_pitchFilename, FF_READ | FF_SHARE_READ)) == ERR_HANDLE) {
        wsprintf(errorBuf, "Can't open %s", m_pitchFilename);
        xfree(p.ptrs);
        return false;
    }

    fsize = GetFileSize(hfile, 0);
    p.numPatterns = fsize / PATTERN_BYTE_SIZE;
    p.data = xmalloc(fsize);

    int usedSize = p.numPatterns * sizeof(*p.used);
    p.used = xmalloc(usedSize);
    memset(p.used, 0, usedSize);

    if (!ReadFile(hfile, p.data, fsize, &size, 0)) {
        wsprintf(errorBuf, "Can't read file %s", m_pitchFilename);
        FClose(hfile);
        xfree(p.ptrs);
        xfree(p.data);
        xfree(p.used);
        return false;
    }

    FClose(hfile);

    p.changed = false;

    for (int i = 0; i < p.numPtrs; i++) {
        int patternIndex = (uint)p.ptrs[i] / PATTERN_BYTE_SIZE;
        p.used[patternIndex]++;
        (uint)p.ptrs[i] += (uint)p.data;
    }

    PitchPatterns *patterns= &g_patterns[pitchNo];
    xfree(patterns->ptrs);
    xfree(patterns->data);
    xfree(patterns->used);
    *patterns = p;

    return true;
}

bool saveIndexFile(int pitchNo)
{
    bool success = true;
    char buf[64];
    wsprintf(buf, "pitch%d.txt", pitchNo + 1);

    HANDLE hFile = FCreate(buf, FF_WRITE | FF_SEQ_SCAN);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    for (int i = 0; i < kVirtualPitchPatternsY; i++) {
        for (int j = 0; j < kVirtualPitchPatternsX; j++) {
            int pattern = getPatternNoAtIndex(&g_patterns[pitchNo], i * kVirtualPitchPatternsX + j);

            char buf[32];
            int2str(pattern, buf);
            int len = strlen(buf);

            if (j < kVirtualPitchPatternsX - 1) {
                buf[len++] = ' ';
            } else {
                buf[len++] = '\r';
                buf[len++] = '\n';
            }
            DWORD tmp;
            if (!WriteFile(hFile, buf, len, &tmp, 0)) {
                FClose(hFile);
                return false;
            }
        }
    }

    FClose(hFile);
    return true;
}

bool SavePattern(uint pitch_no, uint pattern_no)
{
    char buf[128], buf2[128], *data, pbits[16][16];
    uint i;
    RGBQUAD colors[256];
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;
    HANDLE hfile;

    WriteToLog(("SavePattern(): Saving tile %d of pitch no. %d...", pattern_no, pitch_no + 1));

    wsprintf(buf, "pt%d-%04d.bmp", pitch_no + 1, pattern_no);

    data = (char*)g_patterns[pitch_no].data + 256 * pattern_no;

    FillBitmapInfo(&bmfh, &bmih, 256, 16, 16);

    for (i = 0; i < 256; i++) {
        colors[i].rgbRed = pt.pal[3 * i];
        colors[i].rgbGreen = pt.pal[3 * i + 1];
        colors[i].rgbBlue = pt.pal[3 * i + 2];
        colors[i].rgbReserved = 0;
    }

    for (i = 0; i < 16; i++)
        memcpy(pbits[i], data + 16 * (15 - i), 16);

    if ((hfile = FCreate(buf, FF_WRITE)) == ERR_HANDLE)
        return FALSE;

    do {
        if (!WriteFile(hfile, &bmfh, sizeof(bmfh), &i, 0))
            break;
        if (!WriteFile(hfile, &bmih, sizeof(bmih), &i, 0))
            break;
        if (!WriteFile(hfile, colors, sizeof(colors), &i, 0))
            break;
        if (!WriteFile(hfile, pbits, 16 * 16, &i, 0))
            break;

        wsprintf(buf2, "Saved %s OK", buf);
        PrintWarning(buf2, WARNING_INTERVAL, ALIGN_CENTER);
        FClose(hfile);
        return TRUE;
    } while (FALSE);

    FClose(hfile);
    return FALSE;
}

// Closes the file in any case.
static bool InsertPatternFromHandle(HANDLE hfile, char *dest)
{
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;
    uint bytesRead;
    bool result = false;

    do {
        if (!ReadFile(hfile, &bmfh, sizeof(bmfh), &bytesRead, 0))
            break;
        if (bytesRead != sizeof(bmfh))
            break;
        if (!ReadFile(hfile, &bmih, sizeof(bmih), &bytesRead, 0))
            break;
        if (bytesRead != sizeof(bmih))
            break;

        if (bmfh.bfType != 'MB')
            break;
        if (bmih.biWidth != 16 || bmih.biHeight != 16)
            break;
        if (bmih.biBitCount != 8 || bmih.biCompression != BI_RGB)
            break;

        if (SetFilePointer(hfile, bmfh.bfOffBits, 0, FILE_BEGIN) != bmfh.bfOffBits)
            break;

        char pbits[16][16];
        if (!ReadFile(hfile, pbits, sizeof(pbits), &bytesRead, 0))
            break;
        if (bytesRead != sizeof(pbits))
            break;

        for (int i = 0; i < 16; i++)
            memcpy(dest + 16 * (15 - i), pbits[i], 16);

        result = true;
    } while (false);

    FClose(hfile);
    return result;
}

void InsertPattern(uint pitch_no, uint pattern_no)
{
    WriteToLog(("InsertPattern(): Replacing tile %d of pitch no. %d", pattern_no, pitch_no + 1));

    char buf[128], buf2[128];

    wsprintf(buf, "pt%d-%04d.bmp", pitch_no + 1, pattern_no);

    HANDLE hfile = FOpen(buf, FF_READ | FF_SHARE_READ | FF_SEQ_SCAN);
    if (hfile == ERR_HANDLE) {
        wsprintf(buf2, "%s not found.", buf);
        PrintWarning(buf2, WARNING_INTERVAL, ALIGN_CENTER);
        return;
    }

    char *dest = (char*)g_patterns[pitch_no].data + 256 * pattern_no;
    if (InsertPatternFromHandle(hfile, dest)) {
        wsprintf(buf2, "Tile replaced with %s OK", buf);
        markPitchForRedraw();
        PrintWarning(buf2, WARNING_INTERVAL, ALIGN_CENTER);
        g_patterns[pitch_no].changed = TRUE;
        g.changedFlags |= PATTERNS_CHANGED;
    } else {
        wsprintf(buf2, "Error replacing tile with %s", buf);
        PrintWarning(buf2, WARNING_INTERVAL, ALIGN_CENTER);
    }
}

bool SaveChangesToPatterns(int forcedSavePitchNo)
{
    WriteToLog(("SaveChangesToPatterns(): Trying to save changes to tiles..."));
    bool saved = false;

    for (int i = 0; i < MAX_PITCH; i++) {
        if (g_patterns[i].changed || i == forcedSavePitchNo) {
            m_pitchFilename[5] = i + 1 + '0';
            strcpy(&m_pitchFilename[7], "dat");
            HANDLE hfile = FCreate(m_pitchFilename, FF_WRITE);
            if (hfile == ERR_HANDLE)
                return FALSE;

            int j;
            for (j = 0; j < g_patterns[i].numPtrs; j++)
                (uint)g_patterns[i].ptrs[j] -= (uint)g_patterns[i].data;

            int ptrAreaSize = g_patterns[i].numPtrs * sizeof(g_patterns->ptrs[0]);
            if (!WriteFile(hfile, g_patterns[i].ptrs, ptrAreaSize, &j, 0) || j != ptrAreaSize) {
                for (j = 0; j < g_patterns[i].numPtrs; j++)
                    (uint)g_patterns[i].ptrs[j] += (uint)g_patterns[i].data;
                FClose(hfile);
                return FALSE;
            }

            FClose(hfile);
            strcpy(&m_pitchFilename[7], "blk");

            if ((hfile = FCreate(m_pitchFilename, FF_WRITE)) == ERR_HANDLE)
                return FALSE;

            if (!WriteFile(hfile, g_patterns[i].data, g_patterns[i].numPatterns *
                256, &j, 0) || (uint)j != g_patterns[i].numPatterns * 256) {
                FClose(hfile);
                return FALSE;
            }

            FClose(hfile);
            for (j = 0; j < g_patterns[i].numPtrs; j++)
                (uint)g_patterns[i].ptrs[j] += (uint)g_patterns[i].data;

            g_patterns[i].changed = FALSE;
            saved = true;
        }
    }
    g.changedFlags &= ~(PATTERNS_CHANGED | PITCH_CHANGED);
    return saved;
}

/* MovePattern

   new_pat - new number for current pattern

   Swaps current and pattern with number new_pat. Return if input text needs to
   be cleared.
*/
bool MovePattern(int new_pat)
{
    byte tmp[256]; /* temporary pattern storage */
    uint cur_pat = pat.curPattern;
    byte *data = (byte*)g_patterns[pat.curPitch].data;

    if (new_pat > g_patterns[pat.curPitch].numPatterns - 1) {
        PrintWarning("Tile number out of range", WARNING_INTERVAL, ALIGN_CENTER);
        return FALSE;
    }

    if (cur_pat == new_pat)
        return TRUE;

    /* swap current and new_pat pattern */
    WriteToLog(("MovePattern(): Swapping tile %d with tile %d", cur_pat, new_pat));
    memcpy(tmp, data + cur_pat * PATTERN_BYTE_SIZE, PATTERN_BYTE_SIZE);
    memcpy(data + cur_pat * PATTERN_BYTE_SIZE, data + new_pat * PATTERN_BYTE_SIZE, PATTERN_BYTE_SIZE);
    memcpy(data + new_pat * PATTERN_BYTE_SIZE, tmp, PATTERN_BYTE_SIZE);

    g.changedFlags |= PATTERNS_CHANGED;
    g_patterns[pat.curPitch].changed = TRUE;
    pat.curPattern = new_pat;

    return TRUE;
}

/* DeleteCurrentPattern

   Deletes current pattern, and shifts following patterns by one. In index sets
   referencing cells to pattern zero.
*/
static void DeleteCurrentPattern()
{
    deletePattern(pat.curPitch, pat.curPattern);
    markPitchForRedraw();
}

/* GotoPattern

   new_pat - number of pattern to go to

   Checks if pattern is valid, and if so, shows it.
*/
bool GotoPattern(int new_pat)
{
    if (new_pat > g_patterns[pat.curPitch].numPatterns - 1) {
        PrintWarning("Tile number out of range", WARNING_INTERVAL, ALIGN_CENTER);
        return FALSE;
    }
    pat.curPattern = new_pat;
    markPitchForRedraw();
    return TRUE;
}

// Insert all successive patterns found in the game directory.
// File pattern: pt<pitch number>-<pattern index, 4 zero padded digits>.bmp
// Not transactional! Leaves patterns half-inserted in case of an error.
static void insertAllPatterns()
{
    int startIndex;
    int numPatterns = getNumberOfConsecutivePatternFilesInDirectory(&startIndex);

    if (!numPatterns) {
        PrintWarning("No tiles found", WARNING_INTERVAL, ALIGN_CENTER);
        return;
    }

    PitchPatterns *patterns = &g_patterns[pat.curPitch];

    if (numPatterns > (int)patterns->numPatterns) {
        if (expandPatterns(patterns, numPatterns)) {
            g.changedFlags |= PITCH_CHANGED;
            resetPitchData(false);
        } else {
            PrintWarning("Memory allocation error\nCouldn't expand tile space", WARNING_INTERVAL, ALIGN_CENTER);
            return;
        }
    }

    char buf[128];

    for (int i = startIndex; i < startIndex + numPatterns; i++) {
        char filename[13];
        wsprintf(filename, "pt%d-%04d.bmp", pat.curPitch + 1, i);

        HANDLE f = FOpen(filename, FF_READ | FF_SEQ_SCAN);
        if (f == ERR_HANDLE) {
            wsprintf(buf, "Failed to open %s", filename);
            PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
            return;
        }

        char *dest = (char *)patterns->data + PATTERN_BYTE_SIZE * i;
        if (!InsertPatternFromHandle(f, dest)) {
            wsprintf(buf, "Error replacing tile %d with %s", i, filename);
            PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
            return;
        }

        g.changedFlags |= PATTERNS_CHANGED;
    }

    wsprintf(buf, "Inserted %d tile%s", numPatterns, numPatterns == 1 ? "" : "s");
    PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
    patterns->changed = true;
    recalculatePatternUsageCount();
    markPitchForRedraw();
}

static void insertNewPatternAtEnd()
{
    PitchPatterns *patterns = &g_patterns[pat.curPitch];
    if (expandPatterns(patterns, patterns->numPatterns + 1)) {
        logInsertNewPatternForUndo(pat.curPitch, patterns);
        memset((char *)patterns->data + (patterns->numPatterns - 1) * PATTERN_BYTE_SIZE, 0, PATTERN_BYTE_SIZE);
        char buf[128];
        wsprintf(buf, "New empty tile %d added", patterns->numPatterns - 1);
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
    } else {
        PrintWarning("Memory allocation error\nCouldn't expand tile space", WARNING_INTERVAL, ALIGN_CENTER);
    }
}

static bool expandPatterns(PitchPatterns *patterns, int numPatterns)
{
    assert(patterns && numPatterns > (int)patterns->numPatterns);

    char *patternData = omalloc(numPatterns * PATTERN_BYTE_SIZE);
    int *used = omalloc(numPatterns * sizeof(patterns->used[0]));

    if (patternData && used) {
        for (int i = 0; i < patterns->numPtrs; i++)
            patterns->ptrs[i] += (uint)patternData - (uint)patterns->data;
        memcpy(patternData, patterns->data, patterns->numPatterns * PATTERN_BYTE_SIZE);
        xfree(patterns->data);
        patterns->data = patternData;
        memcpy(used, patterns->used, patterns->numPatterns * sizeof(patterns->used[0]));
        xfree(patterns->used);
        patterns->used = used;
        patterns->numPatterns = numPatterns;
        g.changedFlags |= PITCH_CHANGED;
        return true;
    } else {
        xfree(patternData);
        xfree(used);
        return false;
    }
}

static int getNumberOfConsecutivePatternFilesInDirectory(int *startIndex)
{
    char fileIndex[10'000] = {0};

    char filenamePattern[] = "pt?-????.bmp";
    filenamePattern[2] = pat.curPitch + '1';

    WIN32_FIND_DATA findData;
    HANDLE hSearch = FindFirstFile(filenamePattern, &findData);

    *startIndex = INT_MAX;
    int numPatterns = 0;

    if (hSearch != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                strlen(findData.cFileName) == sizeof(filenamePattern) - 1)
            {
                int num = 0;
                for (int i = 4; i < 8; i++) {
                    if (!isdigit(findData.cFileName[i]))
                        goto nextFile;
                    num = 10 * num + findData.cFileName[i] - '0';
                }
                assert(!fileIndex[num]);
                fileIndex[num] = true;
                *startIndex = min(num, *startIndex);
            }
nextFile:;
        } while (FindNextFile(hSearch, &findData));
    }

    if (*startIndex == INT_MAX) {
        *startIndex = 0;
        return 0;
    }

    int numFiles = 0;
    for (int i = *startIndex; fileIndex[i]; i++)
        numFiles++;

    return numFiles;
}
