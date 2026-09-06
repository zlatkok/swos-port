#include "pitch.h"
#include "pitchUndo.h"
#include "debug.h"
#include "pattern.h"
#include "printstr.h"
#include "draw.h"
#include "alloc.h"
#include "file.h"
#include "util.h"
#include <stdlib.h>
#include <assert.h>

static bool InitPitch();
static void InitPitchMode(uint old_mode);
static void FinishPitchMode();
static bool PitchKeyProc(uint code, uint data);
static bool pitchKeyUpProc(uint code, uint data);
static byte *PitchDraw(byte *pbits, const uint pitch);
static void PitchSmoothScroll();
static void tryRevertingPitch();
static void inputPatternNumberToChange();
static bool setPatternNumberAtCursor(int patternNo);
static bool changeCurrentTile(int delta);
static void inputPatternNumberToChangeAllInstances();
static bool changeAllPatternInstancesAtCursor(int patternNo);
static void tryDeletingCurrentPattern();
static void optimizePatterns();
static void setBrushPattern();
static bool updatePatternNumber(uint *destPattern, int patternNo, bool aggregated);
static void overwriteCurrentPatternWith(uint *srcPattern);
static uint getPatternAtCursorIndex();
static uint *getPatternAtCursorIndexPointer();
static int getPatternNumber(uint *patternPtr);
static bool validatePatternNumber(int patternNo);
static void setPitchX(int x);
static void setPitchY(int y);

extern char *g_pitchTypes[];
/* initialized by InitPatterns */
extern PitchPatterns g_patterns[];

/* help for pitch mode */
static char pitch_help[] = {
    "arrows\t\t\t\t- scroll pitch\n"
    "page up/down\t\t- \rscroll up/down by\nabout half of screen\n"
    "space/backspace\t\t- go to next/prev pitch\n"
    "home/end\t\t\t- go to corners of pitch\n"
    "+/-\t\t\t\t- change pitch type\n"
    "control + N\t\t\t- \rshow/hide tile\nnumbers\n"
    "control + U\t\t\t- \rshow/hide tile\nusage count\n"
    "control + E\t\t\t- toggle edit mode\n"
    "control + Z\t\t\t- undo\n"
    "control + Y\t\t\t- redo\n"
    "control + S\t\t\t- save changes\n"
    "control + shift + S\t- force save\n"
    "control + R\t\t\t- revert to saved state\n"
    "control + O\t\t\t- optimize pitch tiles\n"
    "F2\t\t\t\t\t- save pitch to bitmap\n"
    "insert\t\t\t\t- load pitch from bitmap\n"
    "in edit mode:\n"
    "ctrl + arrows up/down\t- change tile number\n"
    "g\t\t\t\t\t- input tile number\n"
    "c\t\t\t\t\t- change all instances"
};

const Mode PitchMode = {
    InitPitch,
    InitPitchMode,
    FinishPitchMode,
    PitchKeyProc,
    pitchKeyUpProc,
    PitchDraw,
    PatternsGetPalette,
    pitch_help,
    "PITCH MODE",
    VK_F7,
    0
};

/* true when smooth scrolling is active, on by default */
static byte m_smoothScroll = TRUE;

/* for speeding up scroll when arrow is held longer */
static byte m_scrollOffset = 1;

static bool m_showCurrentPatternNumber;
static bool m_showUsageCount;
static uint *m_inputPattern;
static uint *m_brushPattern;

static int m_lastEditModePitchX = -1;
static int m_lastEditModePitchY = -1;

static int m_highlightedPatterns[MAX_PITCH] = { -1, -1, -1, -1, -1, -1 };

PitchInfo pt = {0};
static HCURSOR m_editCursor;

/* flashing cursor constants */
#define CURSOR_INTERVAL 100
#define CURSOR_BASE_COLOR 52
#define MAX_CURSOR_FLASH_COLORS 8

/* scrolling limits */
/* smooth-scroll state */
static byte m_maxScrollLines;
#define PAGE_SCROLL_LINES 100

static bool SavePitch(uint pitch_no, uint pitchType, bool export);
static void DrawCursor(char *where, uint pitch, uint x, uint y, uint color);
static void CursorFlashOn();
static void CursorFlashOff();
static void CALLBACK CursorFlashCallBack(HWND hWnd, uint msg, uint idEvent, uint time);
static bool insertPitch(uint pitch_no);
static void scrollPitchUp();
static void scrollPitchDown();
static void scrollPitchLeft();
static void scrollPitchRight();

/* colors for pitch types */
const uchar pat_cols[MAX_PITCH_TYPES][COLOR_TABLE_SIZE] = {
    {72, 72, 48, 64, 72, 48, 56, 72, 48, 94, 94, 80, 77, 77, 66, 31, 31, 26,
     45, 45, 39, 68, 59, 43, 80, 78, 69},
    {56, 40, 0, 48, 40, 0, 40, 40, 0, 100, 93, 83, 88, 61, 0, 34, 22, 0, 47,
     29, 0, 42, 30, 4, 92, 78, 61},
    {24, 72, 0, 56, 72, 0, 24, 72, 0, 92, 97, 80, 66, 87, 24, 30, 45, 0, 36,
     55, 0, 51, 62, 0, 90, 91, 75},
    {24, 56, 0, 56, 56, 0, 24, 56, 0, 91, 97, 76, 43, 75, 18, 23, 35, 0, 30,
     45, 0, 47, 45, 0, 82, 82, 64},
    {56, 72, 0, 48, 72, 0, 72, 72, 0, 90, 96, 76, 64, 85, 23, 30, 45, 0, 37,
     55, 0, 62, 64, 1, 96, 92, 68},
    {72, 72, 0, 64, 72, 0, 56, 72, 0, 89, 92, 75, 67, 89, 24, 30, 45, 0, 36,
     55, 0, 67, 60, 0, 94, 82, 69},
    {72, 56, 0, 64, 56, 0, 56, 56, 0, 100, 93, 74, 90, 69, 0, 39, 30, 0, 52,
     39, 0, 40, 50, 0, 88, 77, 56},
};

/* InitPitch

   Make sure the edit mode cursor is loaded OK.
*/
bool InitPitch()
{
    m_editCursor = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(2), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE);
    return m_editCursor != NULL;
}

/* InitPitchMode

   Called every time when switching to pitch mode.
*/
void InitPitchMode(uint old_mode)
{
    SetPitchPalette(pt.pitchType);
    if (m_smoothScroll) {
        m_maxScrollLines = 5;
        RegisterSWOSProc(PitchSmoothScroll);
    } else {
        m_maxScrollLines = 15;
    }
    if (pt.editMode)
        SetCursor(m_editCursor);
}

/* FinishPitchMode

   Called every time when switching from pitch mode.
*/
void FinishPitchMode()
{
    RegisterSWOSProc(NULL);
    SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW)));
}

/* PitchKeyProc

   wParam - virtual key code
   lParam - key data

   Handle keys for pitch mode.
*/
bool PitchKeyProc(uint code, uint data)
{
    /* if smooth scroll is on, we'll handle arrows through it, other keys
       normally; lParam == -1 is signal that call is from our code */
    switch (code) {
    case VK_RIGHT:
    case VK_LEFT:
    case VK_UP:
    case VK_DOWN:
        if (m_smoothScroll && data != -1 && !pt.editMode)
            return FALSE;
    }
    switch (code) {
    case VK_SPACE:
        pt.curPitch++;
        pt.curPitch = min(pt.curPitch, MAX_PITCH - 1);
        break;
    case VK_HOME:
        pt.x = pt.y = 0;
        if (pt.editMode)
            pt.curx = pt.cury = 0;
        break;
    case VK_END:
        pt.x = PITCH_W - WIDTH;
        pt.y = PITCH_H - HEIGHT;
        if (pt.editMode) {
            pt.curx = WIDTH - PATTERN_LENGTH;
            pt.cury = HEIGHT - PATTERN_LENGTH / 2;
        }
        break;
    case VK_BACK:
        pt.curPitch--;
        pt.curPitch = max(pt.curPitch, 0);
        break;
    case VK_LEFT:
        if (!pt.editMode) {
            scrollPitchLeft();
        } else {
            assert(pt.curx % PATTERN_LENGTH == 0 && pt.cury % PATTERN_LENGTH == 0);
            uint *srcPattern = getPatternAtCursorIndexPointer();
            pt.curx -= PATTERN_LENGTH;
            if (pt.curx < 0) {
                setPitchX(pt.x - PATTERN_LENGTH);
                pt.curx = 0;
            }
            if (isShiftDown())
                overwriteCurrentPatternWith(srcPattern);
        }
        break;
    case VK_RIGHT:
        if (!pt.editMode) {
            scrollPitchRight();
        } else {
            assert(pt.curx % PATTERN_LENGTH == 0 && pt.cury % PATTERN_LENGTH == 0);
            uint *srcPattern = getPatternAtCursorIndexPointer();
            pt.curx += PATTERN_LENGTH;
            pt.curx = min(pt.curx, PITCH_W - PATTERN_LENGTH - 1);
            if (pt.curx >= WIDTH) {
                setPitchX(pt.x + PATTERN_LENGTH);
                pt.curx -= PATTERN_LENGTH;
            }
            if (isShiftDown())
                overwriteCurrentPatternWith(srcPattern);
        }
        break;
    case VK_UP:
        if (!pt.editMode) {
            scrollPitchUp();
        } else {
            assert(pt.curx % PATTERN_LENGTH == 0 && pt.cury % PATTERN_LENGTH == 0);
            if (isControlDown()) {
                return changeCurrentTile(+1);
            } else {
                uint *srcPattern = getPatternAtCursorIndexPointer();
                pt.cury -= PATTERN_LENGTH;
                if (pt.cury < 0) {
                    setPitchY(pt.y - PATTERN_LENGTH);
                    pt.cury = 0;
                }
                if (isShiftDown())
                    overwriteCurrentPatternWith(srcPattern);
            }
        }
        break;
    case VK_DOWN:
        if (!pt.editMode) {
            scrollPitchDown();
        } else {
            assert(pt.curx % PATTERN_LENGTH == 0 && pt.cury % PATTERN_LENGTH == 0);
            if (isControlDown()) {
                return changeCurrentTile(-1);
            } else {
                uint *srcPattern = getPatternAtCursorIndexPointer();
                pt.cury += PATTERN_LENGTH;
                pt.cury = min(pt.cury, PITCH_H - 16 - 1);
                if (pt.cury >= HEIGHT) {
                    setPitchY(pt.y + PATTERN_LENGTH);
                    pt.cury -= PATTERN_LENGTH;
                }
                if (isShiftDown())
                    overwriteCurrentPatternWith(srcPattern);
            }
        }
        break;
    case VK_NEXT:
        setPitchY(pt.y + PAGE_SCROLL_LINES);
        if (pt.editMode) {
            pt.y = pt.y & -PATTERN_LENGTH;
            pt.cury = HEIGHT - 8;
        }
        break;
    case VK_PRIOR:
        setPitchY(pt.y - PAGE_SCROLL_LINES);
        if (pt.editMode) {
            pt.y = pt.y & -PATTERN_LENGTH;
            pt.cury = 0;
        }
        break;
    case VK_ADD:
        pt.pitchType++;
        pt.pitchType = min(pt.pitchType, MAX_PITCH_TYPES - 1);
        SetPitchPalette(pt.pitchType);
        break;
    case VK_SUBTRACT:
        pt.pitchType--;
        pt.pitchType = max(pt.pitchType, 0);
        SetPitchPalette(pt.pitchType);
        break;
    case VK_RETURN:
        if (isControlDown()) {
            setBrushPattern();
            break;
        }
        return false;
    case VK_F2:
        if (!SavePitch(pt.curPitch, pt.pitchType, isShiftDown()))
            PrintWarning("Save failed", WARNING_INTERVAL, ALIGN_CENTER);
        break;
    case VK_INSERT:
        if (insertPitch(pt.curPitch)) {
            resetPitchData(true);
            markPitchForRedraw();
        }
        break;
    case VK_DELETE:
        if (pt.editMode)
            tryDeletingCurrentPattern();
        else
            return false;
        break;
    case VK_TAB:
        m_showCurrentPatternNumber = true;
        break;
    case 'E':
        if (isControlDown()) {
            if (pt.editMode ^= 1) {
                if (pt.x != m_lastEditModePitchX || pt.y != m_lastEditModePitchY) {
                    POINT point;
                    GetCursorPos(&point);
                    ScreenToClient(g.hWnd, &point);
                    point.x = min(point.x, (WIDTH - PATTERN_LENGTH) * RES_MULTIPLIER);
                    point.y = min(point.y, HEIGHT * RES_MULTIPLIER);
                    pt.curx = (short)(point.x / RES_MULTIPLIER) & -PATTERN_LENGTH;
                    pt.cury = (short)(point.y / RES_MULTIPLIER) & -PATTERN_LENGTH;
                }
                CursorFlashOn();
                SetCursor(m_editCursor);
            } else {
                m_lastEditModePitchX = pt.x;
                m_lastEditModePitchY = pt.y;
                CursorFlashOff();
                SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW)));
            }
            markPitchForRedraw();
            MessageBeep(MB_OK);
        }
        break;
    case 'N':
        if (isControlDown()) {
            if (pt.numbers ^= 1)
                m_showUsageCount = false;
            markPitchForRedraw();
            break;
        }
        return false;
    case 'S':
        if (isControlDown()) {
            bool doSave = isShiftDown() || (g.changedFlags & (PITCH_CHANGED | PATTERNS_CHANGED));
            if (doSave) {
                int forcedSavePitchNo = isShiftDown() ? pt.curPitch : -1;
                if (SaveChangesToPatterns(forcedSavePitchNo))
                    PrintWarning("Pitch saved", WARNING_INTERVAL, ALIGN_CENTER);
                else
                    PrintWarning("Error saving pitch", WARNING_INTERVAL, ALIGN_CENTER);
                break;
            }
        }
        return false;
    case 'R':
        if (isControlDown()) {
            if (g.changedFlags & (PITCH_CHANGED | PATTERNS_CHANGED))
                YNPrompt("Discard changes", tryRevertingPitch);
            else
                tryRevertingPitch();
            break;
        }
        return false;
    case 'G':
        if (pt.editMode) {
            inputPatternNumberToChange();
            break;
        }
        return false;
    case 'C':
        if (pt.editMode) {
            inputPatternNumberToChangeAllInstances();
            break;
        }
        return false;
    case 'Z':
    case 'Y':
        if (isControlDown())
            return undoRedoPitchOperation(pt.curPitch, code == 'Z');
        else
            return false;
    case 'U':
        if (isControlDown()) {
            if (m_showUsageCount = !m_showUsageCount)
                pt.numbers = 0;
            markPitchForRedraw();
            break;
        }
        return false;
    case 'O':
        if (isControlDown()) {
            optimizePatterns();
            break;
        }
        return false;
    case 'T':
        // undocumented command
        if (isControlDown()) {
            bool success = saveIndexFile(pt.curPitch);
            PrintWarning(success ? "INDEX FILE SAVED" : "SAVE FAILED", WARNING_INTERVAL, ALIGN_CENTER);
            break;
        }
        return false;
    default:
        return false;
    }
    return TRUE;
}

bool pitchKeyUpProc(uint code, uint data)
{
    switch (code) {
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
        m_scrollOffset = 1;
        return true;
    case VK_TAB:
        if (!(isKeyDown(VK_MBUTTON))) {
            m_showCurrentPatternNumber = false;
            return true;
        }
    }
    return false;
}

/* PitchDraw

   pbits  - pointer to bitmap bits
   pitch  - distance between lines

   Draw routine for pitch mode.
*/
static byte *PitchDraw(byte *pbits, const uint pitch)
{
    char buf[256];
    DrawPitch(pbits, pt.x, pt.y, pitch, pt.curPitch, pt.editMode, pt.numbers);
    wsprintf(buf, "PITCH %d - %s%s", pt.curPitch + 1, g_pitchTypes[pt.pitchType], pt.editMode ? " (edit mode)" : "");
    PrintString(buf, 0, 0, pbits, pitch, TRUE, -1, NO_ALIGNMENT);
    return pbits;
}

/* DrawPitch

   where      - ptr to memory to draw pitch to
   x          - x coordinate
   y          - y    -||-
   pitch      - distance between lines
   pitchType  - pitch type (index)
   grid       - draw grid and cursor if set
   numbers    - draw pattern numbers if set

   Draws pitch at coordinates x and y to where. Uses pitchType to see if
   drawing is really needed, by comparing it to the saved value. Draws pattern
   numbers if numbers is set. The algorithm isn't very efficient, but it's
   simple. Since the tile screen is wider and taller than virtual screen by
   at least one and a half of pattern, there is no need to clip the cursor -
   it will always be inside the tile screen.
*/
void DrawPitch(char *where, uint x, uint y, uint pitch, uint pitchType, bool grid, bool numbers)
{
    const int kGridColor = 254;
    const int kBrushPatternColor = 116;

    enum {
        kMaxPatterns = 2310,
        kTileScreenWidth = kPatternsPerWidth * PATTERN_LENGTH,
        kTileScreenHeight = kPatternsPerHeight * PATTERN_LENGTH,
    };

    static uint s_oldPitchType = 1;
    // area that will hold the part of pitch that fits the screen (and is slightly larger)
    static char tileScreen[kTileScreenWidth * kTileScreenHeight];

    int topLeftPatternIndex = y / PATTERN_LENGTH * PITCH_PATTERN_W + x / PATTERN_LENGTH;
    uint ofsx = x % PATTERN_LENGTH;
    uint ofsy = y % PATTERN_LENGTH;

    uint *cursorPattern = m_showCurrentPatternNumber ? getPatternAtCursorIndexPointer() : nullptr;

    if (s_oldPitchType != pitchType || pt.curtile != topLeftPatternIndex) {
        for (int row = 0; row < kPatternsPerHeight; row++) {
            for (int col = 0; col < kPatternsPerWidth; col++) {
                uint pat = START_PATTERN + topLeftPatternIndex + PITCH_PATTERN_W * row + col;
                if (pat < kMaxPatterns) {
                    uint *from = &g_patterns[pitchType].ptrs[pat];
                    CopyPattern((char *)*from, tileScreen + PATTERN_BYTE_SIZE * kPatternsPerWidth * row +
                        PATTERN_LENGTH * col, kTileScreenWidth);
                    int patternNo = getPatternNoFromPtr(&g_patterns[pitchType], from);
                    if (patternNo == m_highlightedPatterns[pt.curPitch]) {
                        for (int i = 0; i < PATTERN_LENGTH; i++)
                            for (int j = 0; j < PATTERN_LENGTH; j++)
                                tileScreen[kTileScreenWidth * (PATTERN_LENGTH * row + i) + PATTERN_LENGTH * col + j] |= 0x80;
                        const int kHighlightColor = 11;
                        int x1 = col * PATTERN_LENGTH;
                        int x2 = (col + 1) * PATTERN_LENGTH - 1;
                        int y1 = row * PATTERN_LENGTH;
                        int y2 = row * PATTERN_LENGTH + PATTERN_LENGTH - 1;
                        HorLine(tileScreen, kTileScreenWidth, x1, x2, y1, kHighlightColor);
                        HorLine(tileScreen, kTileScreenWidth, x1, x2, y2, kHighlightColor);
                        VerLine(tileScreen, kTileScreenWidth, x1, y1 + 1, y2 - 1, kHighlightColor);
                        VerLine(tileScreen, kTileScreenWidth, x2, y1 + 1, y2 - 1, kHighlightColor);
                    }
                    if (grid)
                        VerLine(tileScreen, kTileScreenWidth, col * PATTERN_LENGTH, row * PATTERN_LENGTH,
                            kTileScreenHeight - 1, kGridColor);
                    if (numbers || m_showUsageCount || m_showCurrentPatternNumber && from == cursorPattern) {
                        int number = m_showUsageCount ? g_patterns[pitchType].used[patternNo] : patternNo;
                        PrintSmallNumber(number, 2, 2,
                            tileScreen + PATTERN_BYTE_SIZE * kPatternsPerWidth * row + PATTERN_LENGTH * col, kTileScreenWidth);
                    }
                }
            }
            if (grid)
                HorLine(tileScreen, kTileScreenWidth, 0, kTileScreenWidth, row * PATTERN_LENGTH, kGridColor);
        }

        // draw red outline on the pattern selected as brush (if any)
        if (grid && m_brushPattern) {
            int index = ((uint)m_brushPattern - (uint)g_patterns[pitchType].ptrs) / sizeof(uint *) - START_PATTERN - topLeftPatternIndex;
            int row = index / PITCH_PATTERN_W;
            int col = index % PITCH_PATTERN_W;
            if (row >= 0 && row < kPatternsPerHeight && col >= 0 && col < kPatternsPerWidth) {
                int x1 = col * PATTERN_LENGTH;
                int x2 = (col + 1) * PATTERN_LENGTH;
                if (col >= kPatternsPerWidth - 2)
                    x2--;
                int y1 = row * PATTERN_LENGTH;
                int y2 = row * PATTERN_LENGTH + PATTERN_LENGTH;
                if (row >= kPatternsPerHeight - 2)
                    y2--;
                HorLine(tileScreen, kTileScreenWidth, x1, x2, y1, kBrushPatternColor);
                HorLine(tileScreen, kTileScreenWidth, x1, x2, y2, kBrushPatternColor);
                VerLine(tileScreen, kTileScreenWidth, x1, y1 + 1, y2 - 1, kBrushPatternColor);
                VerLine(tileScreen, kTileScreenWidth, x2, y1 + 1, y2 - 1, kBrushPatternColor);
            }
        }

        pt.curtile = topLeftPatternIndex;
        s_oldPitchType = pitchType;
    }

    if (grid && pt.curx + 17 < 16 * kPatternsPerWidth && pt.cury + 17 < kPatternsPerHeight * 16)
        DrawCursor(tileScreen, 16 * kPatternsPerWidth, pt.curx, pt.cury, CURSOR_BASE_COLOR + pt.curColor);

    for (int i = 0; i < HEIGHT; i++)
        memcpy(where + i * pitch, tileScreen + (i + ofsy) * 16 * kPatternsPerWidth + ofsx, WIDTH);
}

void PitchMouseScrolled(int delta)
{
    if (delta) {
        if (!pt.editMode)
            m_scrollOffset = m_maxScrollLines;
        if (isControlDown()) {
            changeCurrentTile(delta > 0 ? +1 : -1);
        } else {
            if (delta >= 0)
                scrollPitchUp();
            else
                scrollPitchDown();
        }
        UpdateScreen();
    }
}

/* GetPitchCursor

   Return special pitch edit mode cursor (loaded at startup), or null if we're not supposed to show it.
*/
HCURSOR GetPitchCursor()
{
    return !IsIconic(g.hWnd) && pt.editMode ? m_editCursor : NULL;
}

int getHighlightedPattern(int pitchNo)
{
    assert(pitchNo >= 0 && pitchNo < MAX_PITCH);
    return m_highlightedPatterns[pitchNo];
}

void setHighlightedPattern(int pitchNo, int patternNo)
{
    assert(pitchNo >= 0 && pitchNo < MAX_PITCH);
    assert(patternNo >= -1 && patternNo < (int)g_patterns[pitchNo].numPatterns);
    m_highlightedPatterns[pitchNo] = patternNo;
}

// If patterns (data or indices) were messed with, be sure to call this function.
void resetPitchData(bool clearUndo)
{
    m_inputPattern = nullptr;
    m_brushPattern = nullptr;
    if (clearUndo)
        clearUndoBuffers();
}

bool undoRedoPitchOperation(int pitchNo, bool undo)
{
    char buf[128];
    bool result = undo ? undoPitchEdit(pitchNo, &g_patterns[pitchNo], buf) :
        redoPitchEdit(pitchNo, &g_patterns[pitchNo], buf);
    if (result) {
        recalculatePatternUsageCount();
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
        markPitchForRedraw();
    }
    return result;
}

/* SavePitch

   pitch_no   - number of pitch to save
   pitchType  - type of pitch to save (determines palette)
   export     - if true, we are saving this bitmap with extra row that
                represents animated patterns

   Saves pitch with pitch_no and type pitchType to bitmap. Name of bitmap is
   in format: pitch<number of pitch>-<pitch type (normal, frozen, etc.)>.bmp.
*/
bool SavePitch(uint pitch_no, uint pitchType, bool export)
{
    char buf[128], buf2[128], *data;
    uint i, j;
    HANDLE hfile;
    RGBQUAD colors[256];
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;

    WriteToLog(("SavePitch(): Saving pitch no. %d, pitch type %s", pitch_no + 1, g_pitchTypes[pitchType]));

    if (!(data = omalloc(PITCH_W * PITCH_H)))
        return FALSE;

    wsprintf(buf, "pitch%d-%s.bmp", pitch_no + 1, g_pitchTypes[pitchType]);

    FillBitmapInfo(&bmfh, &bmih, 256, PITCH_W, PITCH_H);

    for (i = 0; i < 256; i++) {
        colors[i].rgbRed = pt.pal[3 * i];
        colors[i].rgbGreen = pt.pal[3 * i + 1];
        colors[i].rgbBlue = pt.pal[3 * i + 2];
        colors[i].rgbReserved = 0;
    }

    for (i = 0; i < PITCH_PATTERN_H; i++) {
        for (j = 0; j < PITCH_PATTERN_W; j++) {
            uint from = g_patterns[pt.curPitch].ptrs[START_PATTERN + PITCH_PATTERN_W * i + j];
            CopyPattern((char*)from, data + PATTERN_BYTE_SIZE * PITCH_PATTERN_W * i + PATTERN_LENGTH * j, PITCH_W);
        }
    }

    for (i = 0; i < PITCH_H / 2; i++) {
        char line[PITCH_W];
        memcpy(line, data + PITCH_W * i, PITCH_W);
        memcpy(data + PITCH_W * i, data + PITCH_W * (PITCH_H - 1 - i),PITCH_W);
        memcpy(data + PITCH_W * (PITCH_H - 1 - i), line, PITCH_W);
    }

    if ((hfile = FCreate(buf, FF_WRITE)) == ERR_HANDLE) {
        xfree(data);
        return FALSE;
    }

    do {
        if (!WriteFile(hfile, &bmfh, sizeof(bmfh), &i, 0))
            break;
        if (!WriteFile(hfile, &bmih, sizeof(bmih), &i, 0))
            break;
        if (!WriteFile(hfile, colors, sizeof(colors), &i, 0))
            break;
        if (!WriteFile(hfile, data, PITCH_W * PITCH_H, &i, 0))
            break;

        wsprintf(buf2, "Saved %s OK", buf);
        PrintWarning(buf2, WARNING_INTERVAL, ALIGN_CENTER);

        FClose(hfile);
        xfree(data);

        return TRUE;
    } while (FALSE);

    FClose(hfile);
    xfree(data);
    return FALSE;
}

/* SetPitchPalette

   pitchType - type of palette

   Sets pitch palette to specified type using pat_cols.
*/
void SetPitchPalette(uint pitchType)
{
    uint i;
    /* indices of colors that need to be changed with pitch type */
    static uchar where[] = {0, 7, 9, 78, 79, 80, 81, 106, 107};

    WriteToLog(("SetPitchPalette(): Switching to pitch type %s palette", g_pitchTypes[pt.pitchType]));

    /* copy corresponding colors from pat_cols */
    for (i = 0; i < sizeof(where); i++) {
        pt.pal[3 * where[i]] = (pat_cols[pitchType][3 * i] & ~1) << 1;
        pt.pal[3 * where[i] + 1] = (pat_cols[pitchType][3 * i + 1] & ~1) << 1;
        pt.pal[3 * where[i] + 2] = (pat_cols[pitchType][3 * i + 2] & ~1) << 1;
    }

    SetPalette(pt.pal, &g.pbits, TRUE);
}

void markPitchForRedraw()
{
    // force cached pitch regeneration from tiles when drawing it the next time
    pt.curtile++;
}

void recalculatePatternUsageCount()
{
    PitchPatterns *patterns = &g_patterns[pt.curPitch];
    memset(patterns->used, 0, patterns->numPatterns * sizeof(patterns->used[0]));
    for (int i = 0; i < patterns->numPtrs; i++) {
        int patternNo = getPatternNoAtIndex(patterns, i);
        patterns->used[patternNo]++;
    }
}

static int m_dragX, m_dragY;
static int m_brushPatternX, m_brushPatternY;

bool PitchLeftButtonClicked(bool pressed, int mouseX, int mouseY)
{
    if (isAltDown() && pressed) {
        SwitchToMode(MODE_PATTERNS);
        showPattern(pt.curPitch, getPatternAtCursorIndex());
    } else if (isShiftDown() && isControlDown() && pressed) {
        int x = (pt.x + mouseX / RES_MULTIPLIER) / PATTERN_LENGTH;
        int y = (pt.y + mouseY / RES_MULTIPLIER) / PATTERN_LENGTH;
        uint patternIndex = START_PATTERN + PITCH_PATTERN_W * y + x;
        int patternNo = getPatternNoAtIndex(&g_patterns[pt.curPitch], patternIndex);
        if (getHighlightedPattern(pt.curPitch) == patternNo)
            setHighlightedPattern(pt.curPitch, -1);
        else
            setHighlightedPattern(pt.curPitch, patternNo);
        markPitchForRedraw();
        UpdateScreen();
        return true;
    } else if (pt.editMode) {
        if (pressed) {
            m_dragX = mouseX;
            m_dragY = mouseY;
            if (isControlDown())
                setBrushPattern();
        } else {
            m_dragX = m_dragY = -1;
        }
        return true;
    }
    return false;
}

bool PitchLeftButtonDoubleClicked(int mouseX, int mouseY)
{
    if (pt.editMode) {
        inputPatternNumberToChange();
        return true;
    } else {
        return false;
    }
}

void PitchRightButtonClicked(bool pressed, int mouseX, int mouseY)
{
    if (!pt.editMode)
        return;

    if (pressed) {
        m_brushPatternX = pt.curx;
        m_brushPatternY = pt.cury;
        if (!m_brushPattern) {
            m_brushPattern = getPatternAtCursorIndexPointer();
        } else {
            PitchMouseMoved(mouseX, mouseY, MK_RBUTTON);
        }
    } else {
        m_brushPatternX = m_brushPatternY = -1;
        m_brushPattern = nullptr;
    }
}

bool PitchMiddleButtonClicked(bool pressed)
{
    if (!isKeyDown(VK_TAB) && m_showCurrentPatternNumber != pressed) {
        m_showCurrentPatternNumber = pressed;
        markPitchForRedraw();
        return true;
    } else {
        return false;
    }
}

bool PitchMouseMoved(int mouseX, int mouseY, uint buttons)
{
    if (!pt.editMode)
        return false;

    pt.curx = pt.x % PATTERN_LENGTH + mouseX / RES_MULTIPLIER & -PATTERN_LENGTH;
    pt.cury = pt.y % PATTERN_LENGTH + mouseY / RES_MULTIPLIER & -PATTERN_LENGTH;

    if ((buttons & MK_LBUTTON) == MK_LBUTTON) {
        int deltaX = m_dragX - mouseX;
        int deltaY = m_dragY - mouseY;
        if (abs(deltaX) > RES_MULTIPLIER) {
            setPitchX(pt.x + deltaX / RES_MULTIPLIER);
            m_dragX = mouseX - deltaX % RES_MULTIPLIER;
        }
        if (abs(deltaY) > RES_MULTIPLIER) {
            setPitchY(pt.y + deltaY / RES_MULTIPLIER);
            m_dragY = mouseY - deltaY % RES_MULTIPLIER;
        }
    } else if ((buttons & MK_RBUTTON) == MK_RBUTTON && m_brushPattern) {
        bool disallowSkewedLine = isShiftDown() && pt.curx != m_brushPatternX && pt.cury != m_brushPatternY;
        if (disallowSkewedLine)
            return false;

        uint *patternDest = getPatternAtCursorIndexPointer();
        if (patternDest == m_brushPattern)
            return false;

        int brushPatterNo = getPatternNumber(m_brushPattern);
        return updatePatternNumber(patternDest, brushPatterNo, false);
    }
    return true;
}

static bool timer = FALSE;

/* CursorFlashOn

   Starts cursor flashing. Turns on cursor timer.
*/
void CursorFlashOn()
{
    if (timer)
        KillTimer(g.hWnd, PITCH_TIMER_ID);
    SetTimer(g.hWnd, PITCH_TIMER_ID, CURSOR_INTERVAL, (TIMERPROC)CursorFlashCallBack);
    timer = TRUE;
}

/* CursorFlahsOff

   Ends cursor flashing. Turns off cursor timer.
*/
void CursorFlashOff()
{
    KillTimer(g.hWnd, PITCH_TIMER_ID);
    timer = FALSE;
    UpdateScreen();
}

/* CursorFlashCallBack

   hWnd    - handle of window associated with timer
   msg     - WM_TIMER message
   idEvent - timer's identifier
   time    - number of milliseconds since Windows has started

   Parameters are ignored. Cycles one cursor color, and redraws pitch.
*/
void CALLBACK CursorFlashCallBack(HWND hWnd, uint msg, uint idEvent, uint time)
{
    pt.curColor++;
    pt.curColor %= MAX_CURSOR_FLASH_COLORS;
    markPitchForRedraw();
    UpdateScreen();
}

/* DrawCursor

   where - ptr to memory to draw cursor
   pitch - distance between lines
   x     - x coordinate
   y     - y    -||-
   color - cursor color

   Draws cursor - two lines thick rectangle 17 x 17, in specified color.
*/
void DrawCursor(char *where, uint pitch, uint x, uint y, uint color)
{
    HorLine(where, pitch, x, x + 16, y, color);
    HorLine(where, pitch, x, x + 16, y + 1, color);
    HorLine(where, pitch, x, x + 16, y + 16, color);
    HorLine(where, pitch, x, x + 16, y + 16 - 1, color);
    VerLine(where, pitch, x, y, y + 16, color);
    VerLine(where, pitch, x + 1, y, y + 16, color);
    VerLine(where, pitch, x + 16, y, y + 16, color);
    VerLine(where, pitch, x + 16 - 1, y, y + 16, color);
}

/* insertPitch

   pitch_no - pitch number to replace

   Replaces pitch with specified number with pitch read from bitmap.
*/
static bool insertPitch(uint pitch_no)
{
    HANDLE hfile;
    static char pitch_fname[] = "pitchx.bmp";
    char buf[128], *pitch;
    char data[PITCH_PATTERN_W * PITCH_PATTERN_H][PATTERN_BYTE_SIZE], (*d)[PATTERN_BYTE_SIZE] = data;
    char *tmp, line[PITCH_PATTERN_W * 16];
    uint where[PITCH_PATTERN_H][PITCH_PATTERN_W] = {0}, sums[PITCH_PATTERN_H * PITCH_PATTERN_W];
    int i, j, k, l, ok = FALSE, curPattern = -1;
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;

    if (pitch_no > 5)
        return false;

    pitch_fname[5] = pitch_no + '1';

    WriteToLog(("insertPitch(): Inserting %s...", pitch_fname));

    if ((hfile = FOpen(pitch_fname, FF_READ | FF_SHARE_READ)) == ERR_HANDLE) {
        wsprintf(buf, "%s not found.", pitch_fname);
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
        return false;
    }

    do {
        /* variables explanation:
           data        - unique patterns stack
           curPattern  - patterns stack pointer
           sums        - sums of patterns in stack (stack of sums)
           where       - indices of patterns for pitch
           pitch       - pitch pixels from bitmap
           d           - current pattern pointer

           Create index and data files from bitmap. Data file will contain only
           unique patterns, and every duplicate pattern will have same index in
           index file.
        */

        /* allocate memory for pitch and read pixels - memory must be
           allocated from heap, because there's not enough stack for alloca  */
        pitch = omalloc(PITCH_W * PITCH_H);
        if (!pitch)
            break;

        if (!ReadFile(hfile, &bmfh, sizeof(bmfh), &i, 0) || i != sizeof(bmfh))
            break;
        if (!ReadFile(hfile, &bmih, sizeof(bmih), &i, 0) || i != sizeof(bmih))
            break;
        if (bmfh.bfType != 'MB')
            break;

        /* dimensions must be PITCH_W x PITCH_H and must be no compression */
        if (bmih.biWidth != PITCH_W || bmih.biHeight != PITCH_H)
            break;
        if (bmih.biBitCount != 8 || bmih.biCompression != BI_RGB)
            break;

        if (!SetFilePointer(hfile, bmfh.bfOffBits, 0, FILE_BEGIN))
            break;

        if (!ReadFile(hfile, pitch, PITCH_W * PITCH_H, &i, 0))
            break;
        if (i != PITCH_W * PITCH_H)
            break;

        /* turn around bitmap pixels */
        for (i = 0; i < PITCH_H / 2; i++) {
            memcpy(line, (char*)pitch + i * PITCH_W, PITCH_W);
            memcpy((char*)pitch + i * PITCH_W, (char*)pitch + (PITCH_H - i - 1)
                   * PITCH_W, PITCH_W);
            memcpy((char*)pitch + PITCH_W * (PITCH_H - i - 1), line, PITCH_W);
        }

        /* if this is not training pitch, insert blank pattern with index zero,
           so that subsequent patterns will start with index 1 (for animated patterns) */
        if (pitch_no != 5) {
            memset(d, 0, PATTERN_BYTE_SIZE);
            d++; curPattern++;
            sums[0] = 0;
        }

        /* do for every 16 x 16 pattern... */
        for (i = 0; i < PITCH_PATTERN_H; i++) {
            for (j = 0; j < PITCH_PATTERN_W; j++) {
                uint match = FALSE, cur_sum = 0;

                /* calculate sum for current pattern */
                for (k = 0; k < 16; k++) {
                    for (l = 0; l < 16; l++) {
                        cur_sum += *(pitch + PITCH_W * i * 16 + j * 16 + k * PITCH_W + l);
                    }
                }

                /* see if we already had that pattern, and set match if so */
                for (k = curPattern; k >= 0; k--) {
                    if (sums[k] == cur_sum) {
                        for (l = 0; l < 16; l++) {
                            char *where = &data[k][l * 16];
                            char *from = pitch + PITCH_W * i * 16 + j * 16 + PITCH_W * l;
                            if (memcmp(where, from, 16))
                                break;
                        }
                        if (l == 16) {
                            match = TRUE;
                            break;
                        }
                    }
                }

                /* if there is a match, just write pattern index, otherwise
                   also copy pattern to stack and bump patterns stack pointer */
                if (match) {
                    where[i][j] = k;
                } else {
                    for (k = 0; k < 16; k++) {
                        char *where = (char*)d + k * 16;
                        memcpy(where, pitch + i * PITCH_W * 16 + j * 16 + PITCH_W * k, 16);
                    }
                    where[i][j] = ++curPattern;
                    sums[curPattern] = cur_sum;
                    d++;
                }
                /* training pitch doesn't have animated patterns */
                if (pitch_no != 5 && curPattern > 0 && curPattern < 25 && curPattern & 1) {
                    /* pattern number is in range 1..24, and is odd, make it a
                       copy of previous; this pattern won't be in index */
                    for (k = 0; k < 16; k++) {
                        char *where = (char*)d + k * 16;
                        memcpy(where, pitch + i * PITCH_W * 16 + j * 16 + PITCH_W * k, 16);
                    }
                    /* don't match this pattern, match previous for indices */
                    sums[++curPattern] = cur_sum + 1;
                    d++;
                }
            }
        }

        /* increase curPattern - it is zero based index */
        tmp = (char*)omalloc(PATTERN_BYTE_SIZE * ++curPattern);
        if (!tmp)
            break;

        PitchPatterns *patterns = &g_patterns[pitch_no];
        xfree(patterns->data);
        memcpy(tmp, data, curPattern * PATTERN_BYTE_SIZE);

        int usedSize = curPattern * sizeof(*patterns->used);
        if (curPattern > (int)patterns->numPatterns) {
            xfree(patterns->used);
            patterns->used = xmalloc(usedSize);
        }
        memset(patterns->used, 0, usedSize);

        for (i = 0; i < patterns->numPtrs; i++) {
            if (i < START_PATTERN || i >= PITCH_PATTERN_H * PITCH_PATTERN_W) {
                uint patternNo = patterns->ptrs[i] / PATTERN_BYTE_SIZE;
                if (patternNo >= (uint)curPattern) {
                    patternNo = curPattern - 1;
                    patterns->ptrs[i] = patternNo * PATTERN_BYTE_SIZE;
                } else {
                    patterns->ptrs[i] -= (uint)patterns->data;
                }
                patterns->used[patternNo]++;
                patterns->ptrs[i] += (uint)tmp;
            }
        }

        patterns->data = tmp;

        for (i = 0; i < PITCH_PATTERN_H; i++)
            for (j = 0; j < PITCH_PATTERN_W; j++)
                (char*)patterns->ptrs[START_PATTERN + PITCH_PATTERN_W * i + j] = where[i][j] * PATTERN_BYTE_SIZE + tmp;

        patterns->numPatterns = curPattern;
        patterns->changed = TRUE;
        g.changedFlags |= PITCH_CHANGED;

        ok = TRUE;
    } while (FALSE);

    if (!ok) {
        PrintWarning("Error inserting pitch", WARNING_INTERVAL, ALIGN_CENTER);
    } else {
        uint interval = WARNING_INTERVAL;
        if (curPattern <= SWOS_PATTERNS_LIMIT) {
            wsprintf(buf, "Pitch replaced with %s OK.", pitch_fname);
        } else {
            wsprintf(buf, "To many unique tiles: %d\nSWOS can handle only %d", curPattern, SWOS_PATTERNS_LIMIT);
            interval = 4000;
        }
        PrintWarning(buf, interval, ALIGN_CENTER);
        resetPitchData(true);
    }

    xfree(pitch);
    FClose(hfile);

    return ok;
}

static void scrollPitchUp()
{
    setPitchY(pt.y - ++m_scrollOffset);
    m_scrollOffset = min(m_maxScrollLines, m_scrollOffset);
}

static void scrollPitchDown()
{
    setPitchY(pt.y + ++m_scrollOffset);
    m_scrollOffset = min(m_maxScrollLines, m_scrollOffset);
}

static void scrollPitchLeft()
{
    setPitchX(pt.x - ++m_scrollOffset);
    m_scrollOffset = min(m_maxScrollLines, m_scrollOffset);
}
static void scrollPitchRight()
{
    setPitchX(pt.x + ++m_scrollOffset);
    m_scrollOffset = min(m_maxScrollLines, m_scrollOffset);
}

static void PitchSmoothScroll()
{
    char keys[256];
    int redraw;

    /* edit mode requires carefulness */
    if (pt.editMode)
        return;

    if (!GetKeyboardState(keys)) {
        RegisterSWOSProc(NULL);
        m_smoothScroll = FALSE;
        return;
    }
    redraw = 0;
    /* we're only interested in arrows; other keys can get too fast... */
    if (keys[VK_RIGHT] & 0x80)
        redraw |= PitchKeyProc(VK_RIGHT, -1);
    if (keys[VK_LEFT] & 0x80)
        redraw |= PitchKeyProc(VK_LEFT, -1);
    if (keys[VK_UP] & 0x80)
        redraw |= PitchKeyProc(VK_UP, -1);
    if (keys[VK_DOWN] & 0x80)
        redraw |= PitchKeyProc(VK_DOWN, -1);
    if (redraw)
        UpdateScreen();
}

static void tryRevertingPitch()
{
    char errorBuf[265];
    if (LoadPitch(pt.curPitch, errorBuf)) {
        PrintWarning("Pitch reloaded", WARNING_INTERVAL, ALIGN_CENTER);
        g.changedFlags &= ~(PITCH_CHANGED | PATTERNS_CHANGED);
        resetPitchData(true);
        markPitchForRedraw();
    } else {
        char buf[256];
        wsprintf(buf, "Error loading pitch\n%s", errorBuf);
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
    }
}

static void inputPatternNumberToChange()
{
    m_inputPattern = getPatternAtCursorIndexPointer();
    InputNumber("Set to tile: ", 3, setPatternNumberAtCursor);
}

static bool setPatternNumberAtCursor(int patternNo)
{
    if (!validatePatternNumber(patternNo))
        return false;

    return updatePatternNumber(m_inputPattern, patternNo, false);
}

static bool changeCurrentTile(int delta)
{
    uint *patternDest = getPatternAtCursorIndexPointer();
    int patternNo = getPatternNumber(patternDest) + delta;
    return updatePatternNumber(patternDest, patternNo, false);
}

static void inputPatternNumberToChangeAllInstances()
{
    m_inputPattern = getPatternAtCursorIndexPointer();
    InputNumber("Set all instances to: ", 3, changeAllPatternInstancesAtCursor);
}

static bool changeAllPatternInstancesAtCursor(int newPattern)
{
    if (!validatePatternNumber(newPattern))
        return false;

    int oldPattern = getPatternNumber(m_inputPattern);
    int numChanged = 0;

    PitchPatterns *patterns = &g_patterns[pt.curPitch];
    bool aggregated = false;

    for (int i = 0; i < patterns->numPtrs; i++) {
        int currentPattern = getPatternNoAtIndex(patterns, i);
        if (currentPattern == oldPattern) {
            updatePatternNumber(&patterns->ptrs[i], newPattern, aggregated);
            aggregated = true;
            numChanged++;
        }
    }

    char buf[128];
    wsprintf(buf, "CHANGED %d INSTANCE%s OF %d TO %d", numChanged, numChanged == 1 ? "" : "S", oldPattern, newPattern);
    PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);

    return true;
}

static void tryDeletingCurrentPattern()
{
    uint *patternPtr = getPatternAtCursorIndexPointer();
    int patternNo = getPatternNumber(patternPtr);

    PitchPatterns *patterns = &g_patterns[pt.curPitch];
    if (patternNo >= (int)patterns->numPatterns || patterns->numPatterns < 2)
        return;

    deletePattern(pt.curPitch, patternNo);

    char buf[128];
    wsprintf(buf, "Tile %d deleted", patternNo);
    PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
}

static void optimizePatterns()
{
    PitchPatterns *patterns = &g_patterns[pt.curPitch];

    int *translationMap = omalloc(patterns->numPatterns * sizeof(int));
    char *patternData = omalloc(patterns->numPatterns * PATTERN_BYTE_SIZE);
    int *used = omalloc(patterns->numPatterns * sizeof(used[0]));
    uint *ptrs = omalloc(patterns->numPtrs * sizeof(patterns->ptrs[0]));

    if (translationMap && patternData && used && ptrs) {
        memset(translationMap, -1, patterns->numPatterns * sizeof(translationMap[0]));
        int numPatterns = 0;

        for (int i = 0; i < patterns->numPtrs; i++) {
            if (i < START_PATTERN || i >= patterns->numPtrs - START_PATTERN) {
                ptrs[i] = (uint)patternData;
            } else {
                int srcPatternNo = getPatternNoAtIndex(patterns, i);
                int dstPatternNo = translationMap[srcPatternNo];
                if (dstPatternNo < 0) {
                    memcpy(patternData + numPatterns * PATTERN_BYTE_SIZE,
                        (char *)patterns->data + srcPatternNo * PATTERN_BYTE_SIZE, PATTERN_BYTE_SIZE);
                    translationMap[srcPatternNo] = numPatterns;
                    dstPatternNo = numPatterns++;
                    assert(numPatterns <= (int)patterns->numPatterns);
                }
                ptrs[i] = dstPatternNo * PATTERN_BYTE_SIZE + (uint)patternData;
            }
        }

        int patternsRemoved = patterns->numPatterns - numPatterns;
        assert(patternsRemoved >= 0);

        bool identical = patternsRemoved == 0;
        if (identical)
            identical = !memcmp(patterns->data, patternData, numPatterns * PATTERN_BYTE_SIZE);
        if (identical) {
            for (int i = 0; i < patterns->numPtrs; i++) {
                if (patterns->ptrs[i] - (uint)patterns->data != ptrs[i] - (uint)patternData) {
                    identical = false;
                    break;
                }
            }
        }

        if (!identical) {
            logTileOptimizationForUndo(patterns, g.changedFlags);
            patterns->data = patternData;
            patterns->used = used;
            patterns->ptrs = ptrs;
            patterns->numPatterns = numPatterns;
        } else {
            xfree(patternData);
            xfree(used);
            xfree(ptrs);
        }

        char buf[128] = "OPTIMIZATION COMPLETE.\n";
        if (patternsRemoved > 0)
            wsprintf(buf + strlen(buf), "REMOVED %d TILES", patternsRemoved);
        else if (identical)
            strcat(buf, "NO CHANGES.");
        else
            strcat(buf, "TILES RENUMBERED.");
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);

        recalculatePatternUsageCount(); // can do it inline, but let it assert-check the new data

        patterns->changed = true;
        g.changedFlags |= PITCH_CHANGED | PATTERNS_CHANGED;

        resetPitchData(false);
        markPitchForRedraw();
    } else {
        xfree(patternData);
        xfree(used);
        xfree(ptrs);
        PrintWarning("Memory allocation failure", WARNING_INTERVAL, ALIGN_CENTER);
    }
    xfree(translationMap);
}

static void setBrushPattern()
{
    uint *currentPattern = getPatternAtCursorIndexPointer();
    m_brushPattern = currentPattern == m_brushPattern ? nullptr : currentPattern;
}

static bool updatePatternNumber(uint *destPattern, int patternNo, bool aggregated)
{
    assert(destPattern);

    if (patternNo >= 0 && patternNo < (int)g_patterns[pt.curPitch].numPatterns) {
        PitchPatterns *patterns = &g_patterns[pt.curPitch];
        int oldPatternNo = getPatternNumber(destPattern);
        if (oldPatternNo != patternNo) {
            logPatternChangedForUndo(destPattern - patterns->ptrs, oldPatternNo, patternNo, aggregated, g.changedFlags);
            patterns->used[oldPatternNo]--;
            patterns->used[patternNo]++;
            *destPattern = patternNo * PATTERN_BYTE_SIZE + (uint)patterns->data;
            patterns->changed = TRUE;
            g.changedFlags |= PITCH_CHANGED;
            markPitchForRedraw();
            return true;
        }
    }
    return false;
}

static void overwriteCurrentPatternWith(uint *srcPattern)
{
    srcPattern = m_brushPattern ? m_brushPattern : srcPattern;
    uint *dstPattern = getPatternAtCursorIndexPointer();
    if (srcPattern != dstPattern) {
        int srcPatternNo = getPatternNumber(srcPattern);
        updatePatternNumber(dstPattern, srcPatternNo, false);
    }
}

static uint getPatternAtCursorIndex()
{
    return getPatternNumber(getPatternAtCursorIndexPointer());
}

/* getPatternAtCursorIndexPointer

   Returns pointer to pattern data in index of currently highlighted pattern in edit mode.
*/
static uint *getPatternAtCursorIndexPointer()
{
    int cX, cY;
    if (pt.editMode) {
        cX = pt.curx;
        cY = pt.cury;
    } else {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(g.hWnd, &pt);
        cX = pt.x / RES_MULTIPLIER;
        cY = pt.y / RES_MULTIPLIER;
    }
    cX = min(max(cX, 0), WIDTH - 1);
    cY = min(max(cY, 0), HEIGHT - 1);
    uint x = (pt.x + cX) / PATTERN_LENGTH;
    uint y = (pt.y + cY) / PATTERN_LENGTH;
    uint patternIndex = START_PATTERN + PITCH_PATTERN_W * y + x;
    return &g_patterns[pt.curPitch].ptrs[patternIndex];
}

static int getPatternNumber(uint *patternPtr)
{
    return getPatternNoFromPtr(&g_patterns[pt.curPitch], patternPtr);
}

static bool validatePatternNumber(int patternNo)
{
    if (!pt.editMode)
        return false;

    if (patternNo > g_patterns[pt.curPitch].numPatterns - 1) {
        PrintWarning("Pattern number out of range", WARNING_INTERVAL, ALIGN_CENTER);
        return false;
    }

    return true;
}

static void setPitchX(int x)
{
    pt.x = x;
    pt.x = max(pt.x, 0);
    pt.x = min(pt.x, PITCH_W - WIDTH);
}

static void setPitchY(int y)
{
    pt.y = y;
    pt.y = max(pt.y, 0);
    pt.y = min(pt.y, PITCH_H - HEIGHT);
}
