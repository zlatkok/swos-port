#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

/* name clashes... */
#define NOMENUS

#pragma warning (push,1)
#include <windows.h>
#pragma warning (pop)
#pragma hdrstop

#ifndef DEBUG
#pragma check_stack(off)
#endif

/* define alloca */
#define alloca(a) _alloca(a)

#define sizeofarray(x) (sizeof(x)/sizeof(x[0]))

/* frequently used types */
typedef unsigned int uint;
typedef unsigned char bool;
typedef unsigned short word;
typedef unsigned char uchar;
typedef signed char schar;
typedef unsigned long ulong;
typedef unsigned short ushort;
typedef uint dword;
typedef unsigned char byte;

typedef struct _BITMAPINFO256 {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[256];
} BITMAPINFO256;

typedef struct _LOGPALETTE256 {
    WORD palVersion;
    WORD palNumEntries;
    PALETTEENTRY palPalEntry[256];
} LOGPALETTE256;

/* program modes - only one can be active */
typedef enum ProgramMode {
    MODE_SPRITES,
    MODE_PICTURES,
    MODE_PITCH,
    MODE_PATTERNS,
    MODE_HIL,
    MODE_ANIMATION,
    MODE_VERSUS,
    NUM_MODES
} ProgramMode;

typedef enum ProgramModeFlags {
    SPRITES_CHANGED = 1,
    PICTURES_CHANGED = 2,
    PITCH_CHANGED = 4,
    PATTERNS_CHANGED = 8,
    HIL_CHANGED = 16,
    VERSUS_CHANGED = 32,
} ProgramModeFlags;

/* when changing, update instance in main.c */
typedef struct _Global_info {
    uint               active:1;     /* do we have focus?                    */
    uint               crashed:1;    /* program crashed?                     */
    uint               minimized:1;  /* is program minimized?                */
    uint               show_help:1;  /* show help for current mode           */
    byte               changedFlags; // any data has changed (each mode sets its flag)
    ProgramMode        mode;         /* current program mode                 */
    HWND               hWnd;         /* hwnd of main (and only) window       */
    uchar              *pbits;       /* pointer to bitmap bits               */
    RECT               r;            /* window coordinates rect              */
} Global_info;

/* some return values:
   Init() - FALSE stops the program from executing
   KeyProc() - TRUE means update screen, FALSE don't
*/
typedef struct _Mode {
    bool (*Init)();                  /* called once on program start         */
    void (*InitMode)(uint old_mode); /* called on each switch to this mode   */
    void (*FinishMode)();            /* called when leaving this mode        */
    bool (*KeyProc)(uint, uint);     /* mode keyboard handler                */
    bool (*keyUpProc)(uint, uint);   // handler for key depresses
    byte *(*Draw)(byte *, const uint); /* draw this mode                     */
    byte *(*GetPalette)();           /* return currently valid palette       */
    char *help;                      /* help text                            */
    char *name;                      /* name string of mode                  */
    uint vkey;                       /* key code for switching to this mode  */
    uint modifiers;                  /* modifiers for key: ctrl, alt, etc.   */
} Mode;

/* mode structure must be accessible from whole program */
extern const Mode *const g_modes[];

/* width and height of screen */
#define WIDTH  320
#define HEIGHT 200

#define RES_MULTIPLIER 3          // multiply the original resolution to get the window dimensions

#define MAX_ZOOM 10               /* maximum zoom factor                     */

#define FSW_SIZE 2048             /* max warning characters                  */
#define WARNING_INTERVAL 1000     /* standard warning length in milliseconds */

#define WM_USER_SAVE_ALL   (WM_USER + 1000 + 0)
#define WM_USER_INSERT_ALL (WM_USER + 1000 + 1)

/* for showing failure reasons to user from init functions */
void SetErrorMsg(char *msg);

/* copyright string */
extern uchar acopyright[];

/* hashes for copyright string */
#define HASH1 60940548442ull
#define HASH2 458971u

/* length of copyright string */
#define COPYRIGHT_LEN 29

typedef uint (KeyFunc)(uint, uint);

KeyFunc *RegisterKeyFunction(KeyFunc *f);    /* for registering custom       */
                                             /* keyboard handling functions  */
void (*RegisterSWOSProc(void (*p)()))();     /* for registering function to  */
                                             /* be called at SWOS game rate  */
void YNPrompt(char *prompt, void (*f)());    /* function for handling y/n    */
                                             /* prompts                      */
void SwitchToMode(uint mode);                /* switch program to this mode  */
int GetFrameRate();                          /* get/set/reset frame rate for */
void SetFrameRate(int new_frame_rate);       /* SWOSProc                     */
void ResetFrameRate();

/* function for getting input from user */
void InputNumber(char *prompt, byte max_digits, bool (*f)(int));

extern Global_info g;

/* menu palette colors */
#define LIGHT_GRAY    1
#define WHITE         2
#define BLACK         3
#define RED          10
#define BRIGHT_BLUE  11
#define PITCH_GREEN  14
#define YELLOW       15
#define DARK_BLUE    46
#define BLUE         50
#define BROWN        66
#define LIGHT_BROWN 114

/* timer IDs */
enum timers {
    MSG_TIMER_ID = 1,    /* timer ID for messages                            */
    PITCH_TIMER_ID,      /* timer ID for pitch cursor (in edit mode)         */
    HIL_TIMER_ID         /* timer for highlights menu current entry          */
};
