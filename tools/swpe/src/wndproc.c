#include <math.h>
#include <stdlib.h>
#include "swspr.h"
#include "printstr.h"
#include "draw.h"
#include "picture.h"
#include "debug.h"
#include "pattern.h"
#include "pitch.h"
#include "hil.h"
#include "menu.h"
#include "alloc.h"
#include "file.h"
#include "util.h"
#include "versus.h"
#include "animation.h"

static void GetKbdSpeedDelay(uint *speed, uint *delay);
static void SetKbdSpeedDelay(uint speed, uint delay);
static bool InitModeKeys();
static uint InputKeyProc(uint code, uint data);
static uint YNKeyProc(uint code, uint data);

const Mode *const g_modes[] = {
    &SpriteMode,
    &PictureMode,
    &PitchMode,
    &PatternMode,
    &HighlightsMode,
    &AnimationMode,
    &VersusMode
};

/* assert that all the modes are listed */
#ifdef DEBUG
int dummy_array[(sizeofarray(g_modes) == NUM_MODES) * 2 - 1];
#endif

extern Pictures_info p;
extern unsigned char pal[768]; /* basic palette (during SWOS menus)          */
extern const char prog_name[]; /* program information                        */
extern const char version[];

/* function to show "save changes y/n" prompt */
static uint QuittingKeyProc(WPARAM wParam, LPARAM lParam);

/* array that holds mappings from keys to modes */
static uchar *modekeys;

static char *m_errBuf;       /* for storing errors from init funcs           */
#define ERR_BUF_SIZE 1024    /* size of error buffer                         */
static char *err_ptr;        /* auxillary pointer for more efficient copying */

static KeyFunc *custom_keyfunc;   /* pointer to custom function to get input */

static bool m_quit;

/* main message dispatch */
LRESULT APIENTRY WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    char buf[256];
    static kbd_delay, kbd_speed;
    static const char kCantInit[] = "Can't init!\n\n";
    static HWND prev_win; /* window that had focus before us */
    uint new_kbd_delay, new_kbd_speed, state, i;
    static int x_click, y_click; /* coordinates of a mouse click */

    /* if program crashes, it still gets some messages (WM_PAINT most notably),
       and then crashes couple more times; this way exception handler is
       called only once */
    if (g.crashed)
        return DefWindowProc(hWnd, msg, wParam, lParam);

    switch (msg) {
    case WM_CREATE:
        /* primary program initialization */
        g.hWnd = hWnd;

        /* save current keyboard speed and delay, and set to max if not already
           at maximum */
        GetKbdSpeedDelay(&kbd_speed, &kbd_delay);
        if (kbd_delay || kbd_speed != 31)
            SetKbdSpeedDelay(31, 0);

        /* allocate memory for error reporting buffer */
        m_errBuf = (char *)xmalloc(1024);

        /* `can't init' message comes before anything else */
        strcpy(m_errBuf, kCantInit);
        err_ptr = m_errBuf + sizeof(kCantInit) - 1;

        /* initialize error messages reporting system, modekeys and menus */
        if (!InitMessages() || !InitModeKeys() || !InitMenus()) {
            MessageBox(NULL, m_errBuf, "Error", MB_OK | MB_ICONERROR);
            return -1;
        }

        /* now call init routines of each mode */
        for (i = 0; i < NUM_MODES; i++)
            if (!g_modes[i]->Init())
                break;

        /* if some mode init routine failed, message will be in m_errBuf */
        if (i != NUM_MODES) {
            MessageBox(NULL, m_errBuf, "Error", MB_OK | MB_ICONERROR);
            return -1;
        }

#ifdef DEBUG
        /* memory allocated so far is considered fixed, and will not be freed
           explicitly; total memory at this point and just before program end
           should be equal; same goes for files */
        SetTrackAlloc(TRUE);
        SetTrackFiles(TRUE);
#endif

        /* starting with sprite mode */
        g_modes[MODE_SPRITES]->InitMode(MODE_SPRITES);

        /* write name of the starting mode */
        PrintWarning(g_modes[MODE_SPRITES]->name, 700, ALIGN_UPRIGHT);

        /* register hot-key, action is to minimize/restore program */
        RegisterHotKey(hWnd, 0xBEEF, MOD_WIN, 'S');
        break;

    case WM_KEYDOWN:
        /* if we're about to quit, no further key processing */
        if (m_quit) {
            QuittingKeyProc(wParam, lParam);
            break;
        }

        /* if there is custom key function, let it handle the input */
        if (custom_keyfunc) {
            if (custom_keyfunc(wParam, lParam))
                UpdateScreen();
            return 0;
        }

        switch (wParam) {
        case VK_F1:
            g.show_help ^= 1;
            break;
        case 'A':
            /* display about */
            wsprintf(buf, "%s %s\n%s", prog_name, version, acopyright);
            PrintWarning(buf, 4000, ALIGN_CENTER);
            break;
#ifdef DEBUG
        case VK_F12:
            /* crash it baby! */
            *(uint*)(0) = 5;
            break;
#endif
        case VK_ESCAPE:
            WriteToLog(("WndProc(): Quitting program..."));
            m_quit = true;
            if (!g.changedFlags)
                PostMessage(g.hWnd, WM_DESTROY, 0, 0);
            else
                PrintWarning("Save changes? (Y/N)", 0, ALIGN_CENTER);
            break;
        default:
            if (modekeys[wParam] < NUM_MODES) {
                uchar mode = modekeys[wParam];
                if (state = 0, g_modes[mode]->modifiers) {
                    state |= GetAsyncKeyState(VK_MENU);
                    state |= GetAsyncKeyState(VK_CONTROL) << 1;
                    state |= GetAsyncKeyState(VK_SHIFT) << 2;
                    state |= GetAsyncKeyState(VK_LWIN) << 3;
                    state |= GetAsyncKeyState(VK_RWIN) << 3;
                }
                if (state == g_modes[mode]->modifiers) {
                    /* it's a mode switch key */
                    SwitchToMode(mode);
                    break;
                }
            }
            /* dispatch to appropriate mode key handler - a handler must
               return TRUE if screen update is needed, FALSE otherwise */
            if (g_modes[g.mode]->KeyProc(wParam, lParam))
                break;
            return DefWindowProc(hWnd, msg, wParam, lParam);
        }
        UpdateScreen();
        break;

    case WM_KEYUP:
        if (g_modes[g.mode]->keyUpProc && g_modes[g.mode]->keyUpProc(wParam, lParam))
            UpdateScreen();
        break;

    case WM_USER_SAVE_ALL:
        SaveAllSprites();
        break;

    case WM_USER_INSERT_ALL:
        InsertAllSprites();
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_F10) {
            SendMessage(hWnd, WM_KEYDOWN, wParam, lParam);
            return 0;
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            hdc = BeginPaint(hWnd, &ps);
            DoDraw(hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

    case WM_DISPLAYCHANGE:
        WriteToLog(("WndProc(): Got WM_DISPLAYCHANGE..."));
        g.r.left = (LOWORD(lParam) - WIDTH) / 2;
        g.r.top = (HIWORD(lParam) - HEIGHT) / 2;
        MoveWindow(hWnd, g.r.left, g.r.top, g.r.right, g.r.bottom, TRUE);
        /* assume fallthrough */

    case WM_ERASEBKGND:
        InvalidateRect(g.hWnd, NULL, FALSE);
        return TRUE;

    /* this is to compensate for moronic windows behaviour - when window that
       has on top attribute is exited, taskbar gets focus, instead of last
       active window */
    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE) {
            WriteToLog(("WndProc(): Previous window: 0x%08x", lParam));
            prev_win = (HWND)lParam;
        }
        g.minimized = HIWORD(wParam);
        break;

    case WM_LBUTTONDBLCLK:
        if (g.mode == MODE_PITCH && PitchLeftButtonDoubleClicked(LOWORD(lParam), HIWORD(lParam))) {
            UpdateScreen();
        } else if (isControlDown()) {
            ShowWindow(hWnd, SW_MINIMIZE);
        }
        break;

    case WM_LBUTTONDOWN:
        if (g.mode == MODE_PITCH && PitchLeftButtonClicked(true, LOWORD(lParam), HIWORD(lParam)))
            break;
        SetCapture(hWnd);
        x_click = LOWORD(lParam);
        y_click = HIWORD(lParam);
        return 0;

    case WM_LBUTTONUP:
        if (g.mode == MODE_PITCH && PitchLeftButtonClicked(false, LOWORD(lParam), HIWORD(lParam)))
            break;
        ReleaseCapture();
        return 0;

    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        if (g.mode == MODE_PITCH) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            PitchRightButtonClicked(msg == WM_RBUTTONDOWN, x, y);
        }
        break;

    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        if (PitchMiddleButtonClicked(msg == WM_MBUTTONDOWN))
            UpdateScreen();
        break;

    case WM_MOUSEMOVE:
        if (g.mode == MODE_PITCH && PitchMouseMoved(LOWORD(lParam), HIWORD(lParam), wParam)) {
            UpdateScreen();
        } else if ((wParam & MK_LBUTTON) == MK_LBUTTON && !isControlDown() && !isShiftDown()) {
            const int kSnapLimit = 10;
            int xMove = LOWORD(lParam) - x_click;
            int yMove = HIWORD(lParam) - y_click;
            /* snap to desktop edges */
            if (g.r.left + xMove < kSnapLimit && g.r.left + xMove > -kSnapLimit)
                xMove = -g.r.left;
            if (g.r.top + yMove < kSnapLimit && g.r.top + yMove > -kSnapLimit)
                yMove = -g.r.top;

            int desktopX = GetSystemMetrics(SM_CXSCREEN);
            int desktopY = GetSystemMetrics(SM_CYSCREEN);
            int limitX = desktopX - g.r.right + g.r.left;
            int limitY = desktopY - g.r.bottom + g.r.top;
            int deltaLimitX = limitX - g.r.left;
            int deltaLimitY = limitY - g.r.top;

            if (deltaLimitX + xMove < kSnapLimit && deltaLimitX + xMove > -kSnapLimit)
                xMove = deltaLimitX;
            if (deltaLimitY + yMove < kSnapLimit && deltaLimitY + yMove > -kSnapLimit)
                yMove = deltaLimitY;

            g.r.left += xMove;
            g.r.right += xMove;
            g.r.top += yMove;
            g.r.bottom += yMove;
            MoveWindow(hWnd, g.r.left, g.r.top, g.r.right - g.r.left, g.r.bottom - g.r.top, TRUE);
        }
        break;

    case WM_MOUSEWHEEL:
        if (g.mode == MODE_PITCH)
            PitchMouseScrolled(GET_WHEEL_DELTA_WPARAM(wParam));
        break;

    case WM_HOTKEY:
        if (wParam == 0xBEEF) {
            WINDOWPLACEMENT wp;
            wp.length = sizeof(wp);
            if (!GetWindowPlacement(hWnd, &wp)) {
                WriteToLog(("Wndproc(): Failed to get window placement"));
                break;
            }
            ShowWindow(hWnd, wp.showCmd == SW_NORMAL ? SW_MINIMIZE : SW_NORMAL);
        }
        break;

    case WM_SETCURSOR:
        if (g.mode == MODE_PITCH) {
            HCURSOR cursor = GetPitchCursor();
            if (cursor) {
                SetCursor(cursor);
                return true;
            }
        }
        break;

    case WM_CLOSE:
        PostMessage(hWnd, WM_KEYDOWN, VK_ESCAPE, 0);
        return 0;

    case WM_DESTROY:
        g_modes[g.mode]->FinishMode();
#if 0
        DeleteObject(g.hbmp);
        DeleteDC(g.hdcMem);
#endif
        /* restore keyboard repeat rate and delay, if not altered */
        GetKbdSpeedDelay(&new_kbd_speed, &new_kbd_delay);
        if (!new_kbd_delay && new_kbd_speed == 31)
            SetKbdSpeedDelay(kbd_speed, kbd_delay);
        /* return focus to proper window */
        WriteToLog(("WndProc(): Returning focus to 0x%08x", prev_win));
        SetActiveWindow(prev_win);
        WriteToLog(("WndProc(): Last active mode was %s", g_modes[g.mode]->name));
        CleanUpPictures();
#ifdef DEBUG
        /* let's see if there's any leaks or open files... */
        ShowMemoryAndFilesUsage();
#endif
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

uint QuittingKeyProc(WPARAM wParam, LPARAM lParam)
{
    switch (wParam) {
    case VK_ESCAPE:
        m_quit = false;
        ClearWarning();
        break;
    case 'Y':
        if (SaveChangesToSprites() && SaveChangesToPatterns(-1))
            PostMessage(g.hWnd, WM_DESTROY, 0, 0);
        break;
    case 'N':
        WriteToLog(("QuittingKeyProc(): Not saving changes"));
        PostMessage(g.hWnd, WM_DESTROY, 0, 0);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

/* SetErrorMsg

   msg - message to add to buffer

   Each message is prepended with 'Reason\n'. It is possible to add more than
   one message to buffer. If any of init functions returns FALSE, m_errBuf is
   shown in message box.
*/
void SetErrorMsg(char *msg)
{
    char *sentinel = m_errBuf + ERR_BUF_SIZE - 2;
    static char areason[] = "Reason:\n";
    if (sentinel - err_ptr >= sizeof(areason)) {
        strcpy(err_ptr, areason);
        err_ptr += sizeof(areason) - 1;
    }
    while (err_ptr < sentinel && (*err_ptr++ = *msg++));
    WriteToLog((err_ptr == sentinel ?
        "SetErrorMsg(): Error buffer is too small" : ""));
    *err_ptr++ = '\n';
    *err_ptr++ = '\0';
}

/* RegisterKeyFunction

   f - pointer to function to collect all input
   returns: pointer to previous function, NULL if there was none
*/
KeyFunc *RegisterKeyFunction(KeyFunc *f)
{
    KeyFunc *p = custom_keyfunc;
    custom_keyfunc = f;
    return p;
}

/* InitModeKeys

   Allocate memory and fill modekeys, array that maps virtual key codes to modes.
*/
static bool InitModeKeys()
{
    int i;
    memset(modekeys = xmalloc(256), NUM_MODES, 256);
    for (i = 0; i < NUM_MODES; i++)
        modekeys[g_modes[i]->vkey] = i;
    return TRUE;
}

/* GetKbdSpeedDelay

   speed - ptr to var that gets keyboard speed
   delay - ptr to var that gets keyboard delay

   Reads keyboard speed and delay.
*/
void GetKbdSpeedDelay(uint *speed, uint *delay)
{
    SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, delay, SPIF_UPDATEINIFILE);
    SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, speed, SPIF_UPDATEINIFILE);
}

/* SetKbdSpeedDelay

   speed - keyboard speed value to set
   delay - keyboard delay value to set

   Sets keyboard speed and delay.
*/
void SetKbdSpeedDelay(uint speed, uint delay)
{
    SystemParametersInfo(SPI_GETKEYBOARDDELAY, delay, 0, SPIF_UPDATEINIFILE);
    SystemParametersInfo(SPI_GETKEYBOARDSPEED, speed, 0, SPIF_UPDATEINIFILE);
}

/* static variables for number input support */
static char *in_prompt;
static byte in_max_digits;
static bool (*in_f)(int);

#define DIGITS_BUF_SIZE 10

static byte dig_buf[DIGITS_BUF_SIZE + 1];
static byte dig_buf_ptr;

/* InputNumber

   prompt     -> string to display
   max_digits -  maximum digits for input
   f          -> callback function when input is successfuly collected

   Inputs number from user, and calls callback function if successfull. Real
   work is done in InputKeyProc(). When input collecting is active, mode
   handlers do not get any keyboard input.
*/
void InputNumber(char *prompt, byte max_digits, bool (*f)(int))
{
    char buf[128];
    in_prompt = prompt;
    in_max_digits = min(max_digits, DIGITS_BUF_SIZE);
    in_f = f;
    RegisterKeyFunction(InputKeyProc);
    dig_buf_ptr = 0;
    wsprintf(buf, "%s%c", prompt, '\f');
    PrintWarning(buf, 0, ALIGN_CENTER);
    /* clear help not to interfere with input string */
    g.show_help = FALSE;
}

/* InputKeyProc

   Processes input from user. Handles only numbers for now.
*/
static uint InputKeyProc(uint code, uint data)
{
    int num, tens, clear = TRUE;
    char buf[128];

    switch(code) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        if (dig_buf_ptr < in_max_digits)
            dig_buf[dig_buf_ptr++] = code;
        break;
    case VK_BACK:
        dig_buf_ptr -= dig_buf_ptr > 0;
        break;
    case VK_RETURN:
        if (!dig_buf_ptr)
            break;
        num = 0; tens = 1;
        while (dig_buf_ptr > 0) {
            num += tens * (dig_buf[--dig_buf_ptr] - '0');
            tens *= 10;
        }
        clear = in_f(num); /* assume fallthrough */
    case VK_ESCAPE:
        RegisterKeyFunction(NULL);
        in_f = NULL;
        if (clear)
            PrintWarning("", 0, NO_ALIGNMENT);
        return TRUE;
    default:
        return FALSE;
    }

    dig_buf[dig_buf_ptr] = '\0';
    wsprintf(buf, "%s%s%c", in_prompt, dig_buf, '\f');
    PrintWarning(buf, 0, ALIGN_CENTER);
    return TRUE;
}

/* function to be called if user presses 'y' */
static void (*yn_f)();

/* YNPrompt

   prompt     -> string to display
   f          -> callback function to call when user presses 'y'

   Draws y/n prompt and calls callback function if user pressed 'y'. Real
   work is done in YNKeyProc(). When input collecting is active, mode
   handlers do not get any keyboard input.
*/
void YNPrompt(char *prompt, void (*f)())
{
    char buf[128];
    yn_f = f;
    RegisterKeyFunction(YNKeyProc);
    wsprintf(buf, "%s? (y/n)", prompt);
    PrintWarning(buf, 0, ALIGN_CENTER);
    g.show_help = FALSE;
}

/* YNKeyProc

   Handles input for y/n prompt. Calls user supplied procedure if user pressed
   'y'.
*/
uint YNKeyProc(uint code, uint data)
{
    switch (code) {
    case 'Y':
    case 'N':
    case VK_ESCAPE:
        PrintWarning("", 0, NO_ALIGNMENT);
        if (code == 'Y')
            yn_f();
        RegisterKeyFunction(NULL);
        yn_f = NULL;
        return TRUE;
    }
    return FALSE;
}

/* SwitchToMode

   mode - mode to switch to

   Switch to this mode, finish previous, write name of new mode in upper right corner.
*/
void SwitchToMode(uint mode)
{
    uint old_mode = g.mode;
    WriteToLog(("SwitchToMode(): Switching to %s...", g_modes[mode]->name));
    g.mode = mode;
    g_modes[old_mode]->FinishMode();
    g_modes[mode]->InitMode(old_mode);
    PrintWarning(g_modes[mode]->name, 700, ALIGN_UPRIGHT);
}
