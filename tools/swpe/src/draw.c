#include <assert.h>
#include "swspr.h"
#include "draw.h"
#include "picture.h"
#include "pattern.h"
#include "pitch.h"
#include "debug.h"
#include "alloc.h"
#include "util.h"
#include "printstr.h"

extern uchar pal[768];        /* palettes */
extern uchar gamepal[768];
extern char *g_pitchTypes[];  /* pitch type string descriptions */

extern Sprites_info s;        /* informations about g_modes */
extern Pictures_info p;
extern PitchInfo pt;
extern Patterns_info pat;

/* variables for printing warnings */
static char *fsw_buffer;
static uint fsw_align;
static uint fsw_flag;

static HDC     hdcMem;  /* our memory hdc for drawing */
static HBITMAP hbmp;    /* handle to bitmap in memory */

/* clears warning when time expires */
static void CALLBACK WarningOff(HWND hWnd, uint msg, uint idEvent, uint time);

/* DoDraw

   hdc   - Device context of our window

   Actual drawing is done here and presented through GDI.
*/
void DoDraw(HDC hdc)
{
    int txt_col; /* color of text, when in picture mode */
    uint pitch;
    byte *pbits;

    pitch = WIDTH;
    pbits = g.pbits;

    /* do the actual drawing */
    pbits = g_modes[g.mode]->Draw(pbits, pitch);

    txt_col = g.mode == MODE_PICTURES ? p.pic_current->text_color : -1;

    if (g.show_help) {
        char buf[4096];
        uint flags = ALIGN_CENTERY;
        strcpy(buf, "F1\t\t\t\t\t- toggle help\nA\t\t\t\t\t- about\n");
        strcat(buf, g_modes[g.mode]->help);
        flags |= -(g.mode == MODE_HIL) & SOLID_BACK;
        PrintString(buf, 16, 16, pbits, pitch, FALSE, txt_col, flags);
    }

    if (fsw_flag)
        PrintString(fsw_buffer, 0, 0, pbits, pitch, TRUE, txt_col, fsw_align);

    StretchBlt(hdc, 0, 0, 3 * WIDTH, 3 * HEIGHT, hdcMem, 0, 0, WIDTH, HEIGHT, SRCCOPY);
    g.pbits = pbits;
}

/* SetPalette

   pal   - pointer to palette to set (VGA, 0..63)
   pbits - address of pointer to bitmap bits (for windowed mode)
   force - if true, force palette change

   Set the GDI framebuffer palette. Disallow unnecessary palette changes,
   unless force is set.
*/
void SetPalette(uchar *pal, byte **ppbits, bool force)
{
    static uchar *old_pal;
    if (!force && old_pal == pal)
        return;
    *ppbits = CreateDIB(pal);
    old_pal = pal;
}

/* UpdateScreen

   Schedule a GDI repaint.
*/
void UpdateScreen()
{
    InvalidateRect(g.hWnd, NULL, FALSE);
}


/* Zoom

   pbits  - pointer to actual graphics to zoom
   factor - how many times to magnify
   pitch  - distance to the start of the next line

   Graphics is assumed to be WIDTH x HEIGHT dimensions.
   Works only with whole numbers (2x, 3x, 4x, ...)
*/
void Zoom(char *pbits, uint factor, uint pitch)
{
    char scrcopy[HEIGHT][WIDTH];
    uint i, x, startx, starty, y, endx, endy, modx, mody;
    uint width, height, destx, desty;

    if (factor <= 1 || factor > MAX_ZOOM)
        return;

#ifdef DEBUG
    memset(scrcopy, 0xcc, HEIGHT * WIDTH);
#endif

    startx = WIDTH * (factor - 1) / (2 * factor);
    starty = HEIGHT * (factor - 1) / (2 * factor);
    width = WIDTH / factor;
    height = HEIGHT / factor;
    endx = startx + width;
    endy = starty + height;
    modx = WIDTH % factor;
    mody = HEIGHT % factor;

    for (desty = 0, y = starty; y < endy; y++, desty += factor)
        for (destx = 0, x = startx; x < endx; x++, destx += factor)
            for (i = 0; i < factor; i++)
                memset(&scrcopy[desty+i][destx], *(pbits+y*pitch+x), factor);

    if (modx != 0)
        for (desty = 0, y = starty; y < endy; y++, desty += factor)
            for (i = 0, x = endx; i < factor; i++)
                memset(&scrcopy[desty+i][WIDTH-modx], *(pbits+y*pitch+x),modx);

    for (desty = HEIGHT - mody; desty < HEIGHT; desty++)
        for (destx = 0, x = startx, y = endy; x<endx; x++, destx += factor)
            memset(&scrcopy[desty][destx], *(pbits+y*pitch+x), factor);


    for (i = 0; i < mody; i++)
        memset(&scrcopy[HEIGHT - mody + i][WIDTH - modx], *(pbits + endy * pitch + endx), modx);
    for (i = 0; i < HEIGHT; i++)
        memcpy(pbits + i * pitch, scrcopy[i], WIDTH);
}

/* InitMessages

   Allocates buffer for warning messages. Returns TRUE or FALSE depending on
   result of memory allocation routine.
*/
bool InitMessages()
{
    fsw_buffer = omalloc(FSW_SIZE);
    return !fsw_buffer ? SetErrorMsg("Out of memory"), FALSE : TRUE;
}

/* flag that is set when timer is active */
static uchar timer = 0;

/* PrintWarning

   str   - string to show on screen
   time  - how long to keep it on screen, 0 forever
   align - how to align text

   Text to be shown is limited by the size if fsw_buffer. It is FSW_SIZE.
   For alignment flags explanation see PrintString.
*/
void PrintWarning(char *str, uint time, uint align)
{
    if (timer)
        KillTimer(g.hWnd, MSG_TIMER_ID);
    my_strncpy(fsw_buffer, str, FSW_SIZE);
    fsw_buffer[FSW_SIZE - 1] = 0;
    fsw_align = align;
    fsw_flag = TRUE;
    if (time) {
        SetTimer(g.hWnd, MSG_TIMER_ID, time, (TIMERPROC)WarningOff);
        timer = TRUE;
    }
}

void ClearWarning()
{
    WarningOff(0, 0, 0, 0);
}

/* WarningOff

   hWnd    - handle of window associated with timer
   msg     - WM_TIMER message
   idEvent - timer's identifier
   time    - number of milliseconds since Windows has started

   Parameters are irrelevant and are ignored. This procedure stops timer, clears
   fsw_flag and timer flag, and updates screen to erase previous message.
*/
void CALLBACK WarningOff(HWND hWnd, uint msg, uint idEvent, uint time)
{
    fsw_flag = FALSE;
    KillTimer(g.hWnd, MSG_TIMER_ID);
    timer = FALSE;
    UpdateScreen();
}

/* CreateDIB

   pal -> palette for DIB

   Create DIB with specified palette, and select it to offscreen context. If
   DIB already exists, destroy it.
*/
byte *CreateDIB(uchar *pal)
{
    static HBITMAP hbmpOld;
    BITMAPINFO256 bminfo;
    HDC hdc;
    uint i;
    byte *pbits;

    hdc = GetDC(g.hWnd);
    bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bminfo.bmiHeader.biWidth = WIDTH;
    bminfo.bmiHeader.biHeight = -HEIGHT; /* negative, top-down DIB */
    bminfo.bmiHeader.biPlanes = 1;
    bminfo.bmiHeader.biBitCount = 8;
    bminfo.bmiHeader.biCompression = BI_RGB;
    bminfo.bmiHeader.biSizeImage = 0;
    bminfo.bmiHeader.biXPelsPerMeter = 0;
    bminfo.bmiHeader.biYPelsPerMeter = 0;
    bminfo.bmiHeader.biClrUsed = 256;
    bminfo.bmiHeader.biClrImportant = 256;

    for (i = 0; i < 256; i++) {
        bminfo.bmiColors[i].rgbRed = pal[3 * i];
        bminfo.bmiColors[i].rgbGreen = pal[3 * i + 1];
        bminfo.bmiColors[i].rgbBlue = pal[3 * i + 2];
        bminfo.bmiColors[i].rgbReserved = 0;
    }

    /* the order of operations must be exactly like this - I learned that the
       hard way */
    if (hbmp) {
        SelectObject(hdcMem, hbmpOld);
        DeleteObject(hbmp);
    }

    hbmp = CreateDIBSection(hdc, (LPBITMAPINFO)&bminfo, DIB_RGB_COLORS, &pbits, NULL, 0);
    assert(pbits);
    if (!hbmp || !pbits)
        return nullptr;

    if (hdcMem)
        DeleteDC(hdcMem);
    hdcMem = CreateCompatibleDC(hdc);
    hbmpOld = (HBITMAP)SelectObject(hdcMem, hbmp);
    ReleaseDC(g.hWnd, hdc);
    return pbits;
}

/* HorLine

   where - pointer to memory to draw horizontal line
   pitch - distance between lines
   x1    - starting row
   x2    - ending row
   y     - column
   color - color of the line

   Draw horizontal line in specified color.
*/
void HorLine(char *where, uint pitch, uint x1, uint x2, uint y, uint color)
{
    memset(where + y * pitch + x1, color, x2 - x1 + 1);
}

/* VerLine

   where - pointer to memory to draw horizontal line
   pitch - distance between lines
   x     - row
   y1    - starting column
   y2    - ending column
   color - color of the line

   Draw vertical line in specified color.
*/
void VerLine(char *where, uint pitch, uint x, uint y1, uint y2, uint color)
{
    for (where += y1 * pitch + x; y1 <= y2; y1++, where += pitch)
        *where = color;
}
