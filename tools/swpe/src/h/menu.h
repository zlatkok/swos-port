#ifndef __MENU_H_
#define __MENU_H_

typedef struct _Menu_entry {
    int   x;
    int   y;
    const char *text;
    uint  ordinal;
    uint  text_len;
    schar up;
    schar down;
    schar left;
    schar right;
    uchar color;
} Menu_entry;

typedef struct _Menu {
    uint  width;                       /* width of single entry              */
    uint  height;                      /* height of single entry             */
    const char *title;                 /* title of menu                      */
    uchar visible:1;                   /* is this menu visible?              */
    uchar big_font:1;                  /* draw items text with big text?     */
    uchar has_back_btn:1;              /* has return to prev menu button?    */
    uchar mode;                        /* mode that has created this menu    */
    uint  tag;                         /* user supplied value                */
    void  (*menu_func)(Menu_entry *m, uint tag); /* on selected function     */
    uchar num_entries;                 /* number of entries in menu          */
    schar active_entry;                /* active entry index (or -1)         */
    uchar color;                       /* entries background color           */
    struct _Menu *parent;              /* menu that called this one          */
    struct _Menu *next;                /* next menu in list                  */
    Menu_entry entries[1];             /* number of entries follows          */
} Menu;

/* colors for menu entry background */
#define PINK_TO_BROWN        1
#define BLUE_TO_BLACK        3
#define ORANGE_TO_BROWN      4
#define BRIGHT_RED_TO_RED   10
#define BLUE_TO_PURPLE      11
#define BRIGHT_BLUE_TO_BLUE 13
#define YELLOW_GREEN_BLACK  14

enum menu_flags {
    MF_NO_FLAGS = 0,
    MF_INVISIBLE = 1,
    MF_HAS_RETURN_BUTTON = 2,
    MF_SORT_ITEMS = 4,
    MF_BIG_FONT = 8
};

bool InitMenus();
Menu *CreateMenu(const char *title, int n, char **names, const uint mode, void (*foo)(Menu_entry *m, uint tag), const uchar color,
    const uint flags, const uint tag);
void DrawMenu(Menu *m, byte *pbits, const uint pitch);
void DrawMenuBack(char *where, uint pitch, uint x1, uint y1, uint x2, uint y2, uchar color);
bool MenuKeyProc(WPARAM wParam, LPARAM lParam);
byte *MenuDraw(byte *pbits, const uint pitch);
void SetWarning(const char *str);
void HideCurrentMenu();
void SetActiveMenu(Menu *m);
bool HasPendingWarnings(uint mode);

/* this must be installed by the caller for cursor to work */
void CALLBACK CursorFlashCallBack(HWND hWnd, uint msg, uint id, uint time);

#define CURSOR_INTERVAL 70

#endif
