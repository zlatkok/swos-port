#include <assert.h>
#include "debug.h"
#include "alloc.h"
#include "file.h"
#include "util.h"
#include "printstr.h"
#include "draw.h"
#include "hil.h"
#include "pitch.h"
#include "swspr.h"
#include "pattern.h"
#include "convert.h"
#include "menu.h"

extern Sprites_info s;
extern PitchInfo pt;

/* array of pointers to color converted sprites that'll be used for drawing */
Sprite **cvt_sprites;

extern uchar pal[768];           /* standard menu palette */

static bool InitHighlights();
static void InitHighlightsMode(uint old_mode);
static bool HighlightKeyProc(uint code, uint data);
static void FinishHighlightsMode();
static byte *HighlightsGetPalette();
static void ReadHilRplFile(Menu_entry *m, const uint tag);
static uint PlayingHilKeyProc(uint code, uint data);

/* help text for highlights mode */
static char hils_help[] = {
    "\n"
    "SELECT HIGHLIGHT OR REPLAY FILE TO SEE AND\n"
    "PRESS ENTER. REPLAY SPEED IS CONTROLLED WITH\n"
    "+/-. PRESS ESCAPE TO STOP WATCHING. PRESS F TO\n"
    "TOGGLE FRAME RATE DISPLAY. PRESS P FOR PAUSE.\n"
    "WHILE PAUSED PRESS N TO ADVANCE TO NEXT FRAME."
};


static int *hil_base;
static int *hil_ptr;
static int *hil_end;
static byte scene_number;
static byte pitch_no;

static Menu *hil_menu;
static Menu *rpl_menu;


/* highlights mode "object" */
Mode HighlightsMode = {
    InitHighlights,
    InitHighlightsMode,
    FinishHighlightsMode,
    HighlightKeyProc,
    nullptr,
    MenuDraw,
    HighlightsGetPalette,
    hils_help,
    "REPLAY MODE",
    VK_F9,
    0
};


/* FindFiles

   pattern            -> string to match files with
   names              -  aray of pointers to string
   max_files          -  maximum files to collect
   too_much_files_str -> string to show if maximum files is exceeded

   Reads maximum of max_files filenames based on pattern from current
   directory into aray pointed to by names. Shows too_much_files_str if too
   many files found.
*/
static int FindFiles(const char *pattern, char **names, const uint max_files, const char *too_much_files_str)
{
    int num_files = 0;
    WIN32_FIND_DATA find_data;
    HANDLE hsearch = FindFirstFile(pattern, &find_data);
    if (hsearch != INVALID_HANDLE_VALUE) {
        do {
            uint len;
            char *fname;
            if (num_files > 44) {
                g.mode = MODE_HIL; /* teeny weeny hack */
                SetWarning(too_much_files_str);
                num_files--;
                break;
            }
            /* cAlternateFileName will be empty if there is no long filename */
            fname = find_data.cAlternateFileName;
            fname = *fname ? fname : find_data.cFileName;
            /* cut ".hil" */
            len = strlen(fname) - 3;
            names[num_files] = xmalloc(len);
            my_strncpy(names[num_files], fname, --len);
            names[num_files++][len] = '\0';
        } while (FindNextFile(hsearch, &find_data));
    }
    FindClose(hsearch);

    return num_files;
}


/* MainMenuSelect

   m   -> pointer to selected entry
   tag -  menu id

   Main menu dispatch function. See what's selected and show appropriate menu.
   Show warning if there are no files of selected kind.
*/
static void MainMenuSelect(Menu_entry *m, uint tag)
{
    static const char msg[] = "NO %s FILES FOUND";
    char buf[64];
    Menu *menu;
    switch (m->ordinal) {
    case 0:
        wsprintf(buf, msg, "HIGHLIGHT");
        menu = hil_menu;
        break;
    case 1:
        wsprintf(buf, msg, "REPLAY");
        menu = rpl_menu;
        break;
    default:
        WriteToLog(("MainMenuSelect(): code for entry %d not inserted", m->ordinal));
        return;
    }
    if (menu) {
        HideCurrentMenu();
        SetActiveMenu(menu);
    } else {
        SetWarning(buf);
        return;
    }
}


/* InitHighlights

   Builds a highlights menu out of highlight file names in current directory.
*/
bool InitHighlights()
{
    char *hil_names[45], *rpl_names[45], buf[128];
    static const char msg[] = "More than 45 %s files found.\nSWOS may behave "
                              "erratically.";
    int num_hils, num_rpls;
    const int flags = MF_INVISIBLE | MF_SORT_ITEMS | MF_HAS_RETURN_BUTTON;
    Menu *m;

    wsprintf(buf, msg, "highlight");
    num_hils = FindFiles("*.hil", hil_names, 45, buf);
    wsprintf(buf, msg, "replay");
    num_rpls = FindFiles("*.rpl", rpl_names, 45, buf);
    if (!num_hils && !num_rpls)
        return TRUE;

    hil_menu =
        (Menu *)CreateMenu("VIEW HIGHLIGHTS", num_hils, hil_names, MODE_HIL, ReadHilRplFile, ORANGE_TO_BROWN, flags, '.HIL');
    rpl_menu = (Menu *)CreateMenu("VIEW REPLAYS", num_rpls, rpl_names, MODE_HIL, ReadHilRplFile, ORANGE_TO_BROWN, flags, '.RPL');
    hil_names[0] = "VIEW HIGHLIGHTS"; hil_names[1] = "VIEW REPLAYS";
    m = (Menu *)CreateMenu("SELECT VIEW", 2, hil_names, MODE_HIL, MainMenuSelect, BLUE_TO_PURPLE, MF_BIG_FONT, 'MAIN');
    /* attach menus to parent menu */
    if (hil_menu)
        hil_menu->parent = m;
    if (rpl_menu)
        rpl_menu->parent = m;

    /* get memory for sprites with converted colors */
    cvt_sprites = (Sprite**)xmalloc((303 * 2 + 2 * 116) * sizeof(Sprite*));

    return TRUE;
}


static uint old_mode;

/* InitHighlightsMode

   Called every time when switching to highlights mode.
*/
static void InitHighlightsMode(uint a_old_mode)
{
    SetPalette(pal, &g.pbits, TRUE);
    if (!SetTimer(g.hWnd, HIL_TIMER_ID, CURSOR_INTERVAL, (TIMERPROC)CursorFlashCallBack))
        WriteToLog(("InitHighlightsMode(): Failed to set cursor timer."));
    if (a_old_mode != MODE_HIL && !hil_menu && !rpl_menu) {
        SetWarning("No highlight and replay files found.");
        old_mode = a_old_mode | 0x8000;
        return;
    }
}


/* FinishHighlightsMode

   Called every time when switching from highlights mode.
*/
void FinishHighlightsMode()
{
    KillTimer(g.hWnd, HIL_TIMER_ID);
    if (hil_base) {
        xfree(hil_base);
        hil_base = NULL;
    }
    old_mode = MODE_HIL;
}


/* HighlightKeyProc

   code - virtual key code
   data - key data

   Handle keys in higlight mode. Returns TRUE if screen update is needed,
   FALSE otherwise. If there are no menus, bit 15 in old_mode will be set.
   If user pressed return to get rid of warning, return to mode that was
   previously active.
*/
static bool HighlightKeyProc(uint code, uint data)
{
    if (old_mode & 0x8000 && code == VK_RETURN) {
        SwitchToMode(old_mode &= ~0x8000);
        return TRUE;
    }
    return MenuKeyProc(code, data);
}


#define HIL_PAUSED        1
#define HIL_SHOW_FPS      2
#define HIL_ADVANCE_FRAME 4
#define HIL_IS_REPLAY     8

static byte hil_flags;


/* HilUpdate

   Called at SWOS game loop rate. Skips screen update if replay is paused.
*/
static void HilUpdate()
{
    if (hil_flags & HIL_PAUSED)
        return;
    UpdateScreen();
}


/* HilDraw

   pbits  - pointer to bitmap bits
   pitch  - distance between lines

   Does drawing for highlights replaying.
*/
//#define GOOFY_MOVE_WINDOW_MODE
static byte *HilDraw(byte *pbits, const uint pitch)
{
    int d, team1_goals, team2_goals, delta_time, *last_frame;
    static int ref_time;
    static byte frames, fps;
#ifndef GOOFY_MOVE_WINDOW_MODE
    char buf[64];
#endif

    if (hil_ptr >= hil_end) {
        PostMessage(g.hWnd, WM_KEYDOWN, VK_ESCAPE, 0);
        return pbits;
    }
    last_frame = hil_ptr;
    frames++;
    delta_time = GetTickCount() - ref_time;
    if (delta_time >= 400) {
        fps = 1000 * frames / delta_time;
        ref_time = GetTickCount();
        frames = 0;
        if (hil_flags & HIL_PAUSED)
            fps = 0;
    }

    while (*hil_ptr >= 0)
        hil_ptr++;
    d = *hil_ptr++ & 0x7fffffff;
    team1_goals = *hil_ptr & 0xffff;
    team2_goals = *hil_ptr >> 16;
#ifdef GOOFY_MOVE_WINDOW_MODE
    {
        int camera_x = d & 0xffff, camera_y = (d >> 16) - 16;
        int desktop_width = GetSystemMetrics(SM_CXSCREEN) - WIDTH;
        int desktop_height = GetSystemMetrics(SM_CYSCREEN) - HEIGHT;
        int window_x = g.r.left, window_y = g.r.top;
        //if (desktop_width > PITCH_W)
            window_x = camera_x;
        /*else
            window_x = camera_x * desktop_width / (PITCH_W - WIDTH);
        window_y = camera_y * desktop_height / (PITCH_H - HEIGHT);*/
        window_y = camera_y;
        MoveWindow(g.hWnd, window_x, window_y, WIDTH, HEIGHT, FALSE);
    }
#endif
    DrawPitch(pbits, d & 0xffff, (d >> 16) - 16, pitch, pitch_no, FALSE,FALSE);
    for (hil_ptr += 2; (d = *hil_ptr) >= 0; hil_ptr++) {
        int x, y, index;
        Sprite *spr;
        assert(hil_ptr < hil_end);
        index = d >> 20;
        if (index > 340 && index < 1179)
            spr = cvt_sprites[index - 341];
        else
            spr = &s.sprites[index];
        x = d >> 10 & 0x3ff;
        y = d & 0x3ff;
        /* x and y are signed values, bit 9 is sign bit */
        x <<= 22; x >>= 22;
        y <<= 22; y >>= 22;
        DrawSprite(x, y, pbits, spr, pitch, -1);
    }

#ifndef GOOFY_MOVE_WINDOW_MODE
    wsprintf(buf, "%d-%d", team1_goals, team2_goals);
    PrintString(buf, 0, 0, pbits, pitch, TRUE, -1, 0);
#endif
    if (hil_flags & HIL_SHOW_FPS) {
        PrintString("fps", 270, 0, pbits, pitch, TRUE, -1, 0);
        PrintNumber(fps, 302, 0, pbits, pitch, TRUE);
    }
    if (*hil_ptr == -1) {
        hil_ptr = hil_base + ++scene_number * 19000 / 4;
        while (hil_ptr < hil_end && *hil_ptr >= 0)
            hil_ptr++;
        /* act as if escape was pressed - to avoid drawing one
           frame with wrong palette */
        if (hil_flags & HIL_IS_REPLAY || hil_ptr >= hil_end) {
            PostMessage(g.hWnd, WM_KEYDOWN, VK_ESCAPE, 0);
            return pbits;
        }
    }
    if (hil_flags & HIL_PAUSED && !(hil_flags & HIL_ADVANCE_FRAME))
        hil_ptr = last_frame;
    hil_flags &= ~HIL_ADVANCE_FRAME;
    return pbits;
}


/* HighlightsGetPalette

   Returns pointer to highlights mode specific palette.
*/
byte *HighlightsGetPalette()
{
    return hil_base ? PatternsGetPalette() : pal;
}


/* ReadHilRplFile

   m   -> our menu entry
   tag -  menu id

   Called when highlight or replay file is selected from menu. Read that file
   into memory and do necessary preparations for playback.
*/
void ReadHilRplFile(Menu_entry *m, const uint tag)
{
    HANDLE hfile;
    char buf[256];
    char fname[13];
    Hil_header h;
    dword hsize, fsize, nreps, size_read, data_part;
    const char *what;

    strcpy(fname, m->text);
    switch (tag) {
    case '.HIL':
        strcat(fname, ".HIL");
        what = "highlight";
        break;
    case '.RPL':
        strcat(fname, ".RPL");
        what = "replay";
        break;
    default:
        WriteToLog(("ReadHilRplFile(): Invalid menu tag"));
        what = "";
        break;
    }

    hfile = FOpen(fname, FF_READ | FF_SHARE_READ | FF_SEQ_SCAN);
    if (hfile == ERR_HANDLE) {
        wsprintf(buf, "Can't open %s", fname);
        SetWarning(buf);
        return;
    }

    fsize = GetFileSize(hfile, NULL);
    data_part = fsize - sizeof(Hil_header);
    nreps = data_part / 19000;
    ReadFile(hfile, &h, sizeof(Hil_header), &hsize, 0);
    if (hsize != sizeof(Hil_header) || h.sig1 != 8 || h.sig2 != 193626 ||
        data_part % 4 || tag == '.HIL' && (data_part % 19000 ||
        nreps != h.num_replays)) {
        wsprintf(buf, "%s is not a %s file", fname, what);
        SetWarning(buf);
        FClose(hfile);
        return;
    }

    hil_ptr = hil_base = xmalloc(data_part);
    nreps *= 19000;
    size_read = tag == '.RPL' ? data_part : nreps;
    hil_end = hil_base + size_read / 4;
    scene_number = 0;
    if (!ReadFile(hfile, hil_base, size_read, &hsize, 0) ||
        hsize != size_read) {
        SetWarning("Error reading file");
        FClose(hfile);
        xfree(hil_base);
        return;
    }

    FClose(hfile);
    RegisterSWOSProc(HilUpdate);
    pitch_no = h.pitch_no;
    SetPitchPalette(h.pitchType);

    /* adjust colors to teams that are playing */
    if (!SetSpriteTeamColors((Team_game*)h.team1, (Team_game*)h.team2)) {
        WriteToLog(("ReadHilRplFile(): Failed to convert team colors... "
            "Aborting."));
        PlayingHilKeyProc(VK_ESCAPE, 0);
        return;
    }

    g.show_help = FALSE;
    ResetFrameRate();
    KillTimer(g.hWnd, HIL_TIMER_ID);
    hil_flags = -(tag == '.RPL') & HIL_IS_REPLAY;
    ((Mode*)&HighlightsMode)->Draw = HilDraw;
    RegisterKeyFunction(PlayingHilKeyProc);
    WriteToLog(("ReadHilRplFile(): Playing %s...", fname));
}


/* PlayingHilKeyProc

   code - virtual key code
   data - key data

   Collects input while hil file is playing. Installed as a keyboard filter
   function to take over escape key and to minimize input overhead while
   performance intensive task is playing.
*/
uint PlayingHilKeyProc(uint code, uint data)
{
    switch (code) {
    case VK_ESCAPE:
        ((Mode*)&HighlightsMode)->Draw = MenuDraw;
        RegisterSWOSProc(NULL);
        xfree(hil_base);
        hil_base = NULL;
        SetPalette(pal, &g.pbits, FALSE);
        RegisterKeyFunction(NULL);
        CleanupTeamSprites();
        SetTimer(g.hWnd, HIL_TIMER_ID, CURSOR_INTERVAL, (TIMERPROC)CursorFlashCallBack);
        WriteToLog(("PlayingHilKeyProc(): Playback ended."));
        UpdateScreen();
        break;
    case 'F':
        hil_flags ^= HIL_SHOW_FPS;
        break;
    case VK_ADD:
        SetFrameRate(GetFrameRate() + 1);
        break;
    case VK_SUBTRACT:
        SetFrameRate(GetFrameRate() - 1);
        break;
    case 'P':
        hil_flags ^= HIL_PAUSED;
        break;
    case 'N':
        if (hil_flags & HIL_PAUSED) {
            hil_flags |= HIL_ADVANCE_FRAME;
            return TRUE;
        }
        break;
    }
    return FALSE;
}
