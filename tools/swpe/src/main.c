
/*
   Swos Sprite Viewer (former)
   Swos Picture Editor

   (c) Karakas Zlatko 2002, 2003, 2004, 2005.
*/

#include "debug.h"
#include "util.h"
#include "file.h"
#include <stdlib.h>

const char class_name[] = "SWOS";                /* name of the window class */
const char prog_name[]  = "SWOS Picture Editor"; /* name of the program      */
const char version[]    = "v0.99.2026";          /* version of the program   */

/* copyright string - "(C) Zlatko Karakas 2002-2022." */
uchar acopyright[] = {
    0x12, 0x25, 0x07, 0xfa, 0x38, 0x86, 0xaf, 0xde, 0x9d, 0x41, 0xfa, 0x7d,
    0xaf, 0x00, 0xaf, 0x9d, 0xaf, 0x35, 0xfa, 0x40, 0xaa, 0xaa, 0x40, 0xeb,
    0x40, 0xaa, 0x40, 0x75, 0x9c
};

/* global informations used throughout the program */
Global_info g = {1, 0, 0, 0, 0, MODE_SPRITES, 0, 0, {0}};

static uchar hp_timer;      /* is high performance timer available?          */
static void (*SWOS_proc)(); /* pointer to function to call at SWOS main loop */
                            /* rate                                          */
static __int64 hpt_freq;    /* frequency of high precision timer, if         */
                            /* available                                     */
static __int64 e;           /* error from last frame                         */
static __int64 ref_time;    /* time of drawing last frame                    */
static int frame_rate;      /* current frame rate                            */
static uint start_time;     /* start time when RegisterSWOSProc() is called  */
                            /* (in ticks)                                    */

/* HPTimeCompare

   Returns true if time interval has elapsed and frame needs to be redrawn.
   HP stands for High Performance.
*/
static bool HPTimeCompare()
{
    __int64 d;
    QueryPerformanceCounter((LARGE_INTEGER*)&d);
    d -= ref_time;
    if (frame_rate * d + e >= hpt_freq) {
        e = frame_rate * d - hpt_freq;
        QueryPerformanceCounter((LARGE_INTEGER*)&ref_time);
        return TRUE;
    }
    return FALSE;
}

/* LPTimeCompare

   Same as above, LP stands for Low Performance.
*/
static bool LPTimeCompare()
{
    uint d = GetTickCount();
    d -= (uint)ref_time;
    __int64 ticksPerFrame = (__int64)frame_rate * d;
    e += ticksPerFrame * d;
    if (ticksPerFrame * d + e >= 1000) {
        e = ticksPerFrame * d - 1000;
        ref_time = GetTickCount();
        return TRUE;
    }
    return FALSE;
}

long _stdcall GlobalExceptionHandler(struct _EXCEPTION_POINTERS *ei);
LRESULT APIENTRY WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* default frame rate that SWOS works on */
#define FRAME_RATE 70

/* minimum and maximum frame rate */
#define MIN_FRAME_RATE 50
#define MAX_FRAME_RATE 100

/* RegisterSWOSProc

   p - pointer to procedure to call at SWOS main loop rate
   returns: old SWOS procedure

   Procedure will be called at default rate of approximately 70 Hz.
*/
void (*RegisterSWOSProc(void (*p)()))()
{
    void (*old_proc)() = SWOS_proc;
    SWOS_proc = p;
    e = 0;
    frame_rate = FRAME_RATE;
    /* reset timer */
    if (hp_timer)
        QueryPerformanceCounter((LARGE_INTEGER*)&ref_time);
    else
        start_time = GetTickCount();
    return old_proc;
}

/* GetFrameRate

   Returns current frame rate for SWOSProc.
*/
int GetFrameRate()
{
    return frame_rate;
}

/* SetFrameRate

   new_frame_rate - new frame rate to set

   Sets new frame rate for SWOSProc, and normalizes it.
*/
void SetFrameRate(int new_frame_rate)
{
    frame_rate = new_frame_rate;
    frame_rate = min(frame_rate, MAX_FRAME_RATE);
    frame_rate = max(frame_rate, MIN_FRAME_RATE);
}

/* ResetFrameRate

   Reset SWOSProc frame rate to default value.
*/
void ResetFrameRate()
{
    frame_rate = FRAME_RATE;
}

/* swspr main(), debug and release versions */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    MSG msg;

#ifdef DEBUG
    /*xpos = ypos = 0;*/
    OpenLogFile();
#endif

    /* check if an instance of SWPE is already running */
    if (CreateMutex(NULL, TRUE, "SWPE_mutex") &&
        GetLastError() == ERROR_ALREADY_EXISTS)
        ExitProcess(0);

    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)GlobalExceptionHandler);

    /* see if high performance timer is present on system */
    if (QueryPerformanceFrequency((LARGE_INTEGER*)&hpt_freq)) {
        WriteToLog(("main(): Using high performance timer, frequency = %u", hpt_freq));
        hp_timer = TRUE;
    } else {
        MessageBox(NULL,
            "High precision timer is unavailable. Highlights"
            " mode will not function optimally.",
            "Warning", MB_ICONWARNING | MB_APPLMODAL);
    }

    wc.cbClsExtra = 0;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.cbWndExtra = 0;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hInstance = GetModuleHandle(0);
    wc.hIcon = wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(1));
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = class_name;
    wc.lpszMenuName = NULL;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;

    const int kWidth = RES_MULTIPLIER * WIDTH;
    const int kHeight = RES_MULTIPLIER * HEIGHT;

    g.r.left = 0;
    g.r.top = 0;
    g.r.right = kWidth;
    g.r.bottom = kHeight;

    if (!AdjustWindowRect(&g.r, WS_POPUP, 0)) {
        g.r.left = 0;
        g.r.top = 0;
        g.r.right = kWidth;
        g.r.bottom = kHeight;
    }

    auto width = GetSystemMetrics(SM_CXSCREEN);
    auto height = GetSystemMetrics(SM_CYSCREEN);
    g.r.left = (width - g.r.right) / 2;
    g.r.top = (height - g.r.bottom) / 2;
    g.r.bottom += abs(g.r.top);
    g.r.right += abs(g.r.left);

    if (!RegisterClassEx(&wc) ||
        !CreateWindow(class_name, prog_name, WS_POPUP, g.r.left, g.r.top, g.r.right, g.r.bottom, 0, 0, 0, 0)) {
        WriteToLog(("main(): Failed to register class or create window."));
        ExitProcess(-1);
    }

    SetWindowPos(g.hWnd, HWND_TOP, g.r.left, g.r.top, g.r.right - g.r.left, g.r.bottom - g.r.top, 0);
    ShowWindow(g.hWnd, SW_NORMAL);
    UpdateWindow(g.hWnd);

    /* game like message loop */
    for (;;) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                break;
            DispatchMessage(&msg);
        } else {
            if (SWOS_proc && !g.minimized) {
                if (hp_timer ? HPTimeCompare() : LPTimeCompare())
                    SWOS_proc();
                else
                    Sleep(1);
            } else {
                WaitMessage();
            }
        }
    }

#ifdef DEBUG
    CloseLogFile();
#endif
    ExitProcess(msg.wParam);
}

/* GlobalExceptionHandler

   si - ptr to _EXCEPTION_POINTERS structure

   Appends registers state to errlog.txt. Also writes base address, address of
   main and exception address. Shows messagebox with that data to user.
*/
long _stdcall GlobalExceptionHandler(struct _EXCEPTION_POINTERS *ei)
{
    char buf[1024], buf2[2048];
    CONTEXT *ctx = (CONTEXT*)ei->ExceptionRecord;
    HANDLE herrfile;
    uint tmp;
    SYSTEMTIME st;
    static char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                           "Thursday", "Friday", "Saturday"};

    g.crashed = TRUE;
    GetLocalTime(&st);
    wsprintf(buf, "%s, %02d.%02d.%04d., %02d:%02d:%02d\n", days[st.wDayOfWeek], st.wDay, st.wMonth, st.wYear, st.wHour,
        st.wMinute, st.wSecond);
    wsprintf(buf2,
        "Program terminating.\nVersion: %s\nWindows: NT\n"
        "Exception code: %X\nException address: %08lX\nBase address: "
        "%08lX\nAddress of main(): %08lX\nRegisters:\nEAX: %08lX EBX: "
        "%08lX ECX: %08lX\nEDX: %08lX ESI: %08lX EDI: %08lX\nEBP: %08lX "
        "ESP: %08lX EIP: %08lX\n\n",
        version, ei->ExceptionRecord->ExceptionCode, ei->ExceptionRecord->ExceptionAddress, GetModuleHandle(0), WinMain, ctx->Eax,
        ctx->Ebx, ctx->Ecx, ctx->Edx, ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp, ctx->Eip);

#ifdef DEBUG
    WriteToLog((buf));
    WriteToLog((buf2));
    CloseLogFile();
#endif
    herrfile = FOpen("errlog.txt", FF_OPEN_ALWAYS | FF_WRITE);
    if (herrfile != INVALID_HANDLE_VALUE) {
        SetFilePointer(herrfile, 0, 0, FILE_END);
        WriteFile(herrfile, buf, strlen(buf), &tmp, 0);
        WriteFile(herrfile, buf2, strlen(buf2), &tmp, 0);
    }
    FClose(herrfile);
    ShowWindow(g.hWnd, SW_HIDE);
    MessageBox(g.hWnd, buf2, "Fatal error", MB_ICONERROR | MB_TASKMODAL);
    ExitProcess(-1);
    /* to keep the compiler happy */
    return EXCEPTION_EXECUTE_HANDLER;
}
