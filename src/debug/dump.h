#pragma once

enum AlignFlagsBits {
    ALIGN_LEFT_BIT,
    ALIGN_RIGHT_BIT,
    ALIGN_CENTERX_BIT,
    ALIGN_UP_BIT,
    ALIGN_DOWN_BIT,
    ALIGN_CENTERY_BIT,
    HEX_BIT,
    OCTAL_BIT,
    BIN_BIT,
    UNSIGNED_BIT,
    SIGN_PREFIX_BIT,
    BIG_FONT_BIT
};

enum StringAlignment {
    NO_ALIGNMENT = 0,
    ALIGN_LEFT = 1 << ALIGN_LEFT_BIT,
    ALIGN_RIGHT = 1 << ALIGN_RIGHT_BIT,
    ALIGN_CENTERX = 1 << ALIGN_CENTERX_BIT,
    ALIGN_UP = 1 << ALIGN_UP_BIT,
    ALIGN_DOWN = 1 << ALIGN_DOWN_BIT,
    ALIGN_CENTERY = 1 << ALIGN_CENTERY_BIT,
    FL_HEX = 1 << HEX_BIT,
    FL_OCTAL = 1 << OCTAL_BIT,
    FL_BIN = 1 << BIN_BIT,
    FL_UNSIGNED = 1 << UNSIGNED_BIT,
    FL_SIGN_PREFIX = 1 << SIGN_PREFIX_BIT,
    FL_BIG_FONT = 1 << BIG_FONT_BIT,

    ALIGN_CENTER = ALIGN_CENTERX | ALIGN_CENTERY,
    ALIGN_UPRIGHT = ALIGN_RIGHT | ALIGN_UP,
    ALIGN_UPLEFT = ALIGN_LEFT | ALIGN_UP,
    ALIGN_DOWNRIGHT = ALIGN_DOWN | ALIGN_RIGHT,
    ALIGN_DOWNLEFT = ALIGN_DOWN | ALIGN_LEFT,
};

//#if defined(DEBUG) && !defined(SWOS_TEST)
void getStringLength(char *str, int *w, int *h, bool align, bool big);
void printString(char *str, int x, int y, int color, bool big = false, int align = NO_ALIGNMENT);
void printStringEx(char *str, int x, int y, int color, int width, bool big = false, int align = NO_ALIGNMENT);
void dumpVariables();
void toggleDebugOutput();

//#else
//#define getStringLength(p1, p2, p3, p4, p5) ((void)0)
//#define printString(p1, p2, p3, p4, ...)  ((void)0)
//#define dumpVariables()  ((void)0)
//#define toggleDebugOutput() ((void)0)
//#endif
