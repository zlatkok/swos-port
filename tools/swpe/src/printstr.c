#include "swspr.h"
#include "printstr.h"
#include "debug.h"
#include "util.h"
#include "draw.h"

extern Sprites_info s;

static uchar Char2Sprite(char c, uint big);

/* Char2Sprite

   c   - character to convert
   big - TRUE or FALSE for big character set

   returns:    255 - character has no sprite representation
               254 - new line (\n)
               253 - space
               252 - horizontal tab (\t)
               251 - vertical tab (\v)
               250 - anchor (\r)
            <= 249 - sprite index
*/
uchar Char2Sprite(char c, uint big)
{
    uchar spr;
    static uchar cvt_table[] = {
       /* 0    1    2    3    4    5    6    7    8    9  */
/* 0 */  255, 255, 255, 255, 255, 255, 255, 255, 255, 252,
/* 1 */  254, 251,  44, 250, 255, 255, 255, 255, 255, 255,
/* 2 */  255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
/* 3 */  255, 255, 253, 255, 255, 255, 255,  51, 255,   1,
/* 4 */    2,   3,  45,  50,   4,  47,   6,   7,   8,   9,
/* 5 */   10,  11,  12,  13,  14,  15,  16,  17,  49,  52,
/* 6 */  255, 255, 255,  48, 255,  18,  19,  20,  21,  22,
/* 7 */   23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
/* 8 */   33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
/* 9 */   43};

    c = (uchar)toupper(c);
    if (c > sizeof(cvt_table) - 1) {
        spr = 255;
    } else {
        spr = cvt_table[c];
        spr += big ? (spr > sizeof(cvt_table) - 1) ? 0 : 57 : 0;
    }
    return spr;
}

/* GetStringLength

   str   - string to "measure"
   w     - pointer to write width to
   h     - pointer to write height to
   align - alignment flags
   big   - boolean, is this a big font?

   Calculates number of pixels needed to draw string (width and height),
   taking into account flags.

   Special characters:
   new line - After a new line, 1 pixel is added for small font; 2 for big.
   space    - 6 pixels are skipped for big, and 4 for small font.
   hor. tab - Size is 64 pixels for big, 32 pixels for small font.
   ver. tab - Skip 2 lines.
   anchor   - Marks the spot where the next line will begin. It is represented
              with carriage return ('\r'). For example, string
              "one line - \ranchor here\nsecond line" would print as:
              one line - anchor here
                         second line
              Only last anchor is relevant - string
              "one \rline - \ranchor here\nsecond line" gives the same result.
              Anchors are ignored when strings are x centered.
   cursor   - draws SWOS cursor, represented with formfeed ('\f').
*/
void GetStringLength(const char *str, uint *w, uint *h, uint align, uint big)
{
    uint dx, dy, len = 0, height = big ? 8 : 6, anchor = 0;
    uchar spr;

    for (; *str; str++) {
        switch (*str) {
        case ' ':
            len += big ? 6 : 4;
            break;
        case '\n':
            GetStringLength(++str, &dx, &dy, align, big);
            height += dy + (big ? 2 : 1);
            dx += anchor;
            if (!align)
                len = max(len, dx);
            *w = len; *h = height;
            return;
        case '\t':
            /* we will calculate maximum tab value here, it might be less than
               this when printed - well, tabs ARE evil */
            len += big ? 64 : 32;
            break;
        case '\v':
            /* skip two lines */
            height += big ? 20 : 14;
            break;
        case '\r': /* mark anchor */
            anchor = len;
            break;
        default:
            spr = Char2Sprite(*str, big);
            if (spr == 255)
                continue;
            GetSize(&s.sprites[spr], &dx, &dy);
            len += dx + 1;
            height = max(dy, height);
        }
    }
    *w = len; *h = height;
}

/* PrintString

   str   - pointer to string to write
   x     - string x coordinate (may be ignored)
   y     - string y coordinate       -||-
   where - ptr to memory to write string to
   pitch - distance between lines
   big   - write with big font?
   color - color for letters, 0 = original color
   align - alignment flags, can be:
           NO_ALIGNMENT  - string is written to x and y coordinates
           ALIGN_LEFT    - horizontally align text to left edge of screen
           ALIGN_RIGHT   - horizontally align text to right edge of screen
           ALIGN_CENTERX - centers string horizontally
           ALIGN_UP      - vertically align text to upper edge of screen
           ALIGN_BOTTOM  - vertically align text to lower edge of screen
           ALIGN_CENTERY - centers string vertically
           SOLID_BACK    - draw text on solid black background (like titles)

           Combinations are:
           ALIGN_CENTER    = ALIGN_CENTERX | ALIGN_CENTERY
           ALIGN_UPRIGHT   = ALIGN_RIGHT | ALIGN_UP
           ALIGN_UPLEFT    = ALIGN_LEFT | ALIGN_UP
           ALIGN_DOWNRIGHT = ALIGN_BOTTOM | ALIGN_RIGHT
           ALIGN_DOWNLEFT  = ALIGN_BOTTOM | ALIGN_LEFT

   For special characters, see GetStringLength.
*/
void PrintString(const char *str, uint x, uint y, char *where, uint pitch, bool big, int color, uint align)
{
    uchar c;
    uint i, dx, dy, xorig, xalign, yalign, anchor = 0;

    if ((align >> ALIGN_LEFT_BIT & 1) + (align >> ALIGN_RIGHT_BIT & 1) +
        (align >> ALIGN_CENTERX_BIT & 1) > 1) {
        WriteToLog(("PrintString(): Invalid flags for x alignment: 0x%x", align));
        return;
    }

    if ((align >> ALIGN_UP_BIT & 1) + (align >> ALIGN_DOWN_BIT & 1) +
        (align >> ALIGN_CENTERY_BIT & 1) > 1) {
        WriteToLog(("PrintString(): Invalid flags for y alignment: 0x%x", align));
        return;
    }

    GetStringLength(str, &dx, &dy, align & ALIGN_CENTERX, big);
    dx = min(dx, WIDTH);
    dy = min(dy, HEIGHT);

    xalign = align & (ALIGN_RIGHT | ALIGN_LEFT | ALIGN_CENTERX);
    if (xalign == ALIGN_RIGHT)
        x = WIDTH - dx;
    else if (xalign == ALIGN_LEFT)
        x = 0;
    else if (xalign == ALIGN_CENTERX)
        x = (WIDTH - dx) / 2;

    yalign = align & (ALIGN_UP | ALIGN_BOTTOM | ALIGN_CENTERY);
    if (yalign == ALIGN_UP)
        y = 0;
    else if (yalign == ALIGN_BOTTOM)
        y = HEIGHT - dy;
    else if (yalign == ALIGN_CENTERY)
        y = (HEIGHT - dy) / 2;

    /* draw solid text background if requested */
    if (align & SOLID_BACK)
        for (i = 0; i < dy; i++)
            HorLine(where, pitch, x, x + dx - 1, y + i, 3);

    xorig = x;
    for (c = Char2Sprite(*str, big); *str; c = Char2Sprite(*++str, big)) {
        switch (c) {
        case  253: /* space */
            x += big ? 6 : 4;
            break;
        case  254: /* new line */
            y += dy + (big ? 2 : 1);
            if (xalign & ALIGN_CENTERX) {
                GetStringLength(str + 1, &dx, &dy, TRUE, big);
                dx = min(dx, WIDTH);
                x = (WIDTH - dx) / 2;
            } else
                x = xorig + anchor;
            anchor = 0;
            break;
        case 255:  /* can't draw this */
            WriteToLog(("PrintString(): Can't draw this character: 0x%02x",
                        *str));
            continue;
        case 252:  /* horizontal tab */
            x += big ? 64 : 32;
            x &= big ? ~63 : ~31;
            break;
        case 251:  /* vertical tab */
            y += 2 * (big ? 8 : 6);
            break;
        case 250:  /* anchor */
            anchor = x - xorig;
            break;
        default:   /* draw character */
            GetSize(&s.sprites[c], &dx, &dy);
            DrawSprite(x, y, where, &s.sprites[c], pitch, color);
            x += dx + 1;
        }
    }
}

/* PrintNumber

  num   - number to print
  x     - number x coordinate
  y     - number y coordinate
  where - ptr to memory to write number to
  pitch - distance between lines
  big   - write with big font?

  Prints number using sprites.
*/
void PrintNumber(int num, uint x, uint y, char *where, uint pitch, bool big)
{
    char buf[16];
    PrintString(int2str(num, buf), x, y, where, pitch, big, -1, NO_ALIGNMENT);
}

/* PrintSmallNumber

  num   - number to print
  x     - number x coordinate
  y     - number y coordinate
  where - ptr to memory to write number to
  pitch - distance between lines

  Prints small number, like the one that marks players during game.
*/
void PrintSmallNumber(int num, uint x, uint y, char *where, uint pitch)
{
    char buf[16];
    uint i, dx, dy;
    Sprite *spr;
    int2str(num, buf);
    for (i = 0; buf[i]; i++) {
        switch(buf[i]) {
        case '1': case '2': case '3':
        case '4': case '5': case '6':
        case '7': case '8': case '9':
            spr = &s.sprites[buf[i] + 113];
            GetSize(spr, &dx, &dy);
            if (x + dx >= WIDTH)
                return;
            DrawSprite(x, y, where, spr, pitch, -1);
            x += dx + 1;
            break;
        case '0':
            /* unfortunately, no small zero; we'll have to draw it by ourselves */
            if (x + 5 > WIDTH || y + 5 > HEIGHT)
                return;
            HorLine(where, pitch, x, x + 2, y, 2);
            HorLine(where, pitch, x, x + 2, y + 4, 2);
            VerLine(where, pitch, x, y + 1, y + 3, 2);
            VerLine(where, pitch, x + 2, y + 1, y + 3, 2);
            VerLine(where, pitch, x + 3, y + 1, y + 5, 3);
            HorLine(where, pitch, x + 1, x + 2, y + 5, 3);
            VerLine(where, pitch, x + 1, y + 1, y + 3, 3);
            x += 5;
            break;
        }
    }
}
