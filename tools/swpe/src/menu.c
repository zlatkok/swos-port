/* menu.c

   Menu drawing system. Mode needs to register draw and keyproc functions from
   menu to implement it. Mode also needs to install and deinstall cursor
   callback when needed.
*/

#include "menu.h"
#include "printstr.h"
#include "picture.h"
#include "debug.h"
#include "draw.h"
#include "alloc.h"
#include "util.h"
#include <stdlib.h>

typedef struct Warning {
    char *text;
    uint mode;
    struct Warning *next;
} Warning;

static Warning *warn_head; /* head of linked list of pending warnings        */
static Warning *warn_tail; /* tail of               -||-                     */
static Menu *menu_head;    /* head of linked list of menus                   */
static Menu *cur_menu;     /* menu that is current and has focus             */

#define MAX_CURSOR_FLASH_COLORS sizeof(flash_cols)

/* flash colors table */
static uchar flash_cols[] = {2, 2, 1, 1, 7, 7, 1, 1};

/* current cursor shade */
static uchar cur_index;

extern Sws_256_pic *stad_ptr;    /* pictures needed as backgrounds and for */
extern Sws_256_pic *swtitle_ptr; /* special effects */
extern Sws_256_pic *fill_ptr;

#define WARNING_SIZE 256

static void DrawFrame(char *where, uint pitch, uint x1, uint y1, uint x2, uint y2, uchar thickness, uchar color);

/* ReturnToPrevMenu

   Make current menu invisible and give focus to parent menu.
*/
static void ReturnToPrevMenu()
{
    if (!cur_menu->parent) {
        WriteToLog(("ReturnToPrevMenu(): Menu has no parent to return to!"));
        return;
    }
    cur_menu->visible = FALSE;
    cur_menu = cur_menu->parent;
    cur_menu->visible = TRUE;
}


/* InitMenus

   Called once at program start. Do any required menu system initialization.
*/
bool InitMenus()
{
    return TRUE;
}


/* CreateMenu

   title -> menu title
   n     -> number of entries
   mode  -  this menu will be shown only in this mode
   names -> array of n pointers to names of entries
   foo   -> callback function that will be called when some entry is selected
   color -> background color for entries
   flags -  flags that control type of menu
   tag   -  user supplied (possibly) unique value for distinction between menus

   Create specified menu and sets it as front menu. Only pointers to entry
   strings are copied, caller needs to provide permanent storage.
   Automagically arranges entries in columns.
*/
Menu *CreateMenu(const char *title, int n, char **names, const uint mode, void (*foo)(Menu_entry *m, uint tag), const uchar color,
    const uint flags, const uint tag)
{
    Menu *m;
    int i, j, width, height, columns, column_size, xorig, yorig, mod;
    int x, y, entry, one_more, max_y, big, last_mod_entry, mod_limit;

    if (!n) {
        WriteToLog(("CreateMenu(): Will not create zero entries menu."));
        return NULL;
    }

    /* add one extra entry if menu has return button */
    one_more = !!(flags & MF_HAS_RETURN_BUTTON);

    /* see which font to use for text */
    big = !!(flags & MF_BIG_FONT);

    m = xmalloc(sizeof(Menu) + (n - !one_more) * sizeof(Menu_entry));
    m->mode = mode;
    m->color = color;
    m->active_entry = 0;
    m->num_entries = n;
    m->title = title;
    m->visible = !(flags & MF_INVISIBLE);
    m->menu_func = foo;
    m->big_font = big;
    m->has_back_btn = one_more;
    m->parent = NULL;
    m->tag = tag;
    m->width = 0;
    /* sort entries list if requested */
    if (flags & MF_SORT_ITEMS)
        SortNames(names, n);
    for (i = 0; i < n; i++) {
        uint w, h;
        GetStringLength(names[i], &w, &h, NO_ALIGNMENT, big);
        m->width = max(m->width, w);
        m->height = h;
        m->entries[i].text = names[i];
        m->entries[i].text_len = w;
        m->entries[i].ordinal = i;
        m->entries[i].color = color;
    }

    m->height += 2 + (2 & -big);
    m->width += 8 + (2 & -big);
    columns = (n * (m->height + 1) - 1 + HEIGHT - 32 - 1) / (HEIGHT - 32);
    width = columns * (m->width + 4) - 4;
    column_size = n / columns;
    mod = n % columns;
    height = (column_size + !!mod) * (m->height + 1) - 1;
    x = xorig = (WIDTH - width) / 2;
    y = yorig = (HEIGHT - height) / 2;
    mod_limit = (columns - mod) / 2;

    for (last_mod_entry = entry = i = max_y = 0; i < columns; i++) {
        int mod_entry = i >= mod_limit && mod > 0;
        mod -= mod_entry;
        for (j = 0; j < column_size + mod_entry; j++, entry++) {
            Menu_entry *e = m->entries + entry;
            if (!i || j >= column_size && !last_mod_entry)
                e->left = -1;
            else
                e->left = e->ordinal - column_size - last_mod_entry;
            if (i >= columns - 1 || j >= column_size && !mod)
                e->right = -1;
            else
                e->right = e->ordinal + column_size + mod_entry;
            if (!j)
                e->up = -1;
            else
                e->up = e->ordinal - 1;
            if (j >= column_size - 1 + mod_entry)
                e->down = -1;
            else
                e->down = e->ordinal + 1;
            e->x = x;
            e->y = y + j * (m->height + 1 + (2 & -big));
            max_y = max(max_y, m->entries[entry].y);
            if (one_more && j >= column_size - 1 + mod_entry)
                e->down = n;
        }
        x += m->width + 4 + (2 & -big);
        y = yorig;
        last_mod_entry = mod_entry;
    }
    if (one_more) {
        Menu_entry *e = m->entries + n;
        static const char ret_str[] = "BACK";
        uint w, h;
        uint min_index = 0, min_delta = WIDTH + HEIGHT + 1;
        GetStringLength(ret_str, &w, &h, NO_ALIGNMENT, big);
        e->text = ret_str;
        e->text_len = w;
        e->x = WIDTH / 2 - m->width / 2;
        e->y = max_y + m->height + 1 + (2 & -big) + 4;
        e->color = BLUE_TO_PURPLE;
        e->left = e->right = e->down = -1;
        /* find closest entry */
        for (i = 0; i < m->num_entries; i++) {
            uint delta;
            delta = abs(e->x - m->entries[i].x) + abs(e->y - m->entries[i].y);
            if (delta <= min_delta) {
                min_delta = delta;
                min_index = i;
            }
        }
        e->up = min_index;
        e->ordinal = m->num_entries++;
        /* give focus to back button */
        m->active_entry = n;
    }
    /* link to list, be first in front */
    m->next = menu_head;
    return cur_menu = menu_head = m;
}


/* DrawMenu

   m     -> menu to draw
   pbits -  pointer to bitmap bits
   pitch -  distance between lines

   Draws menu m where pbits point. Also draws glowing frame arounf current
   item.
*/
void DrawMenu(Menu *m, byte *pbits, const uint pitch)
{
    int i;
    Menu_entry *e;
    int big = m->big_font;

    /* draw title */
    DrawFrame(pbits, pitch, 0, 0, WIDTH - 1, 15, 2, LIGHT_GRAY);
    DrawMenuBack(pbits, pitch, 2, 2, WIDTH - 3, 13, PINK_TO_BROWN);
    PrintString(m->title, 0, 4, pbits, pitch, TRUE,-1, ALIGN_CENTERX);

    /* draw items */
    for (i = 0; i < m->num_entries; i++) {
        e = m->entries + i;
        DrawMenuBack(pbits, pitch, e->x, e->y, e->x + m->width - 1, e->y + m->height - 1, e->color);
        PrintString(
            e->text, e->x + (m->width - e->text_len) / 2 + big, e->y + 1 + big, pbits, pitch, m->big_font, WHITE, NO_ALIGNMENT);
    }
    /* draw glowing frame */
    e = m->entries + m->active_entry;
    DrawFrame(pbits, pitch, e->x, e->y, e->x + m->width - 1, e->y + m->height - 1, 1, flash_cols[cur_index]);
}


/* DrawMenuBack

   where  -> where to draw
   pitch  -  distance between lines
   x1, y1 -  x and y coordinates of upper left point
   x2, y2 -  x and y coordinates of lower right point
   color  -  color to use for filling

   Draws menu background using original SWOS menu "texture" (fill.256).
*/
void DrawMenuBack(char *where, uint pitch, uint x1, uint y1, uint x2, uint y2, uchar color)
{
    static uchar colortable[] = {96, 96, 96, 32, 64, 64, 64, 96, 32, 96, 128,
                                 160, 64, 192, 224, 224};
    uchar *fill_ofs = fill_ptr->picture + y1 * WIDTH + x1;
    uint fill_delta = WIDTH - x2 + x1 - 1, x;
    uint pitch_delta = pitch - x2 + x1 - 1;
    color = colortable[color];
    for (where += pitch * y1 + x1; y1 <= y2; y1++, where += pitch_delta) {
        for (x = x1; x <= x2; x++)
            *where++ = *fill_ofs++ + color;
        fill_ofs += fill_delta;
    }
}


/* DeleteWarning

   w    -> warning to delete
   prev -> warning before w in list

   Deletes warning w from warnings linked list.
*/
static void DeleteWarning(Warning *w, Warning *prev)
{
    if (!w)
        return;
    if (warn_head == warn_tail)
        prev = NULL;
    if (w != warn_head && prev)
        prev->next = w->next;
    else
        warn_head = warn_head->next;
    if (w == warn_tail)
        warn_tail = prev;
    xfree(w);
}


/* MenuKeyProc

   wParam - virtual key code
   lParam - key data

   Handle keys in menu. Returns TRUE if screen update is needed, FALSE
   otherwise. If warning is active, respond to enter only (removes warning).
*/
bool MenuKeyProc(WPARAM wParam, LPARAM lParam)
{
    Menu_entry *e;
    Warning *w, *prev = nullptr;

    if (wParam == VK_RETURN) {
        for (w = warn_head; w && w->mode != g.mode; prev = w, w = w->next);
        if (w) {
            DeleteWarning(w, prev);
            return TRUE;
        }
    }

    if (!cur_menu)
        return FALSE;

    e = cur_menu->entries + cur_menu->active_entry;
    switch (wParam) {
    case VK_BACK:
        if (cur_menu->has_back_btn)
            ReturnToPrevMenu();
        else
            return FALSE;
        break;
    case VK_HOME:
        cur_menu->active_entry = 0;
        break;
    case VK_END:
        cur_menu->active_entry = cur_menu->num_entries - 1;
        break;
    case VK_RETURN:
        if (cur_menu->active_entry == cur_menu->num_entries - 1 &&
            cur_menu->has_back_btn) {
            ReturnToPrevMenu();
            break;
        }
        cur_menu->menu_func(cur_menu->entries + cur_menu->active_entry, cur_menu->tag);
        break;
    case VK_UP:
        if (e->up < 0)
            return FALSE;
        cur_menu->active_entry = e->up;
        break;
    case VK_DOWN:
        if (e->down < 0)
            return FALSE;
        cur_menu->active_entry = e->down;
        break;
    case VK_LEFT:
        if (e->left < 0)
            return FALSE;
        cur_menu->active_entry = e->left;
        break;
    case VK_RIGHT:
        if (e->right < 0)
            return FALSE;
        cur_menu->active_entry = e->right;
        break;
    default:
        return FALSE;
    }
    return TRUE;
}


/* SetWarning

   str -> string to show as a warning

   Set given string as a warning. Will be shown to user in the order of
   appearance.
*/
void SetWarning(const char *str)
{
    Warning *w = (Warning*)xmalloc(sizeof(Warning));
    w->text = my_strdup(str);
    w->next = NULL;
    w->mode = g.mode;
    //if (warn_tail)
    //    warn_tail->next = w;
    warn_tail = w;
    if (!warn_head)
        warn_head = w;
}


/* MenuDraw

   pbits  - pointer to bitmap bits
   pitch  - distance between lines

   Draw routine for menu system.
*/
byte *MenuDraw(byte *pbits, const uint pitch)
{
    int i;
    Menu *m;
    Warning *wr, *p = nullptr;
    uint w, h;
    int x, y;

    /* directly copy swtitle pixels */
    for (i = 0; i < HEIGHT; i++)
        memcpy(pbits + i * pitch, swtitle_ptr->picture + i * WIDTH, WIDTH);
    /* see if we have a warning */
    for (wr = warn_head; wr && wr->mode != g.mode; p = wr, wr = wr->next);
    if (!wr) {
        /* no warning, just draw menus */
        for (m = menu_head; m; m = m->next)
            if (m->visible && m->mode == g.mode)
                DrawMenu(m, pbits, pitch);
        return pbits;
    }
    GetStringLength(wr->text, &w, &h, ALIGN_CENTER, FALSE);
    if (w >= WIDTH || h >= HEIGHT) {
        WriteToLog(("MenuDraw(): Got too big warning! Dimensions: %d x %d", w, h));
        DeleteWarning(wr, p);
    }
    x = (WIDTH - w - 10) / 2;
    y = (HEIGHT - h - 35) / 2;
    DrawFrame(pbits, pitch, x, y, x + w + 10, y + h + 35, 1, LIGHT_GRAY);
    DrawMenuBack(pbits, pitch, x + 1, y + 1, x + w + 10 - 1, y + h + 35 - 1, YELLOW_GREEN_BLACK);
    PrintString("WARNING", 0, y + 5, pbits, pitch, FALSE, -1, ALIGN_CENTERX);
    PrintString(wr->text, 0, y + 15, pbits, pitch, FALSE, -1, ALIGN_CENTERX);
    /* draw "OK" button */
    DrawFrame(pbits, pitch, (WIDTH - 45) / 2 - 2, y + h + 20, (WIDTH - 45) / 2 + 45, y + h + 30, 1, flash_cols[cur_index]);
    DrawMenuBack(pbits, pitch, (WIDTH - 45) / 2 - 1, y + h + 21, (WIDTH - 45) / 2 + 45 - 1, y + h + 29, BRIGHT_BLUE_TO_BLUE);
    PrintString("OK", 0, y + h + 23, pbits, pitch, FALSE, -1, ALIGN_CENTERX);
    return pbits;
}


/* DrawFrame

   where     -> where to draw
   pitch     -  distance between lines
   x1, y1    -  coordinates of upper left point
   x2. y2    -  coordinates of lower right point
   thickness -  thickness of frame in pixels; expands inwards
   color     -  color to fill frame with

   Draws frame of specified thickness and color.
*/
void DrawFrame(char *where, uint pitch, uint x1, uint y1, uint x2, uint y2, uchar thickness, uchar color)
{
    while (thickness--) {
        HorLine(where, pitch, x1, x2, y1, color);
        HorLine(where, pitch, x1, x2, y2, color);
        VerLine(where, pitch, x1, y1 + 1, y2 - 1, color);
        VerLine(where, pitch, x2, y1 + 1, y2 - 1, color);
        x1++; y1++; x2--; y2--;
    }
}


/* CursorFlashCallBack

   hWnd    - handle of window associated with timer
   msg     - WM_TIMER message
   id      - timer's identifier
   time    - number of miliseconds since Windows has started

   Parameters are ignored. Cycles one cursor color, and forces screen update.
*/
void CALLBACK CursorFlashCallBack(HWND hWnd, uint msg, uint id, uint time)
{
    cur_index++;
    cur_index %= MAX_CURSOR_FLASH_COLORS;
    UpdateScreen();
}


/* HideCurrentMenu

   Hides currently active menu.
*/
void HideCurrentMenu()
{
    cur_menu->visible = FALSE;
}


/* SetActiveMenu

   m -> menu to gain focus

   Gives keyboard focus to this menu.
*/
void SetActiveMenu(Menu *m)
{
    cur_menu = m;
    m->visible = TRUE;
}


/* HasPendingWarnings

   mode - mode for which to check warnings

   Returns boolean value that shows if there are any warnings for specified
   mode.
*/
bool HasPendingWarnings(uint mode)
{
    Warning *w;
    for (w = warn_head; w && w->mode != mode; w = w->next);
    return !!w;
}
