#ifndef __PRINTSTR_H_
#define __PRINTSTR_H_

/* PrintString alignment flags */
enum alignment {
    ALIGN_LEFT_BIT,
    ALIGN_RIGHT_BIT,
    ALIGN_CENTERX_BIT,
    ALIGN_UP_BIT,
    ALIGN_DOWN_BIT,
    ALIGN_CENTERY_BIT,
    SOLID_BACK_BIT
};

#define NO_ALIGNMENT      0
#define ALIGN_LEFT        1 << ALIGN_LEFT_BIT
#define ALIGN_RIGHT       1 << ALIGN_RIGHT_BIT
#define ALIGN_CENTERX     1 << ALIGN_CENTERX_BIT
#define ALIGN_UP          1 << ALIGN_UP_BIT
#define ALIGN_BOTTOM      1 << ALIGN_DOWN_BIT
#define ALIGN_CENTERY     1 << ALIGN_CENTERY_BIT
#define SOLID_BACK        1 << SOLID_BACK_BIT

#define ALIGN_CENTER      ALIGN_CENTERX | ALIGN_CENTERY
#define ALIGN_UPRIGHT     ALIGN_RIGHT | ALIGN_UP
#define ALIGN_UPLEFT      ALIGN_LEFT | ALIGN_UP
#define ALIGN_DOWNRIGHT   ALIGN_BOTTOM | ALIGN_RIGHT
#define ALIGN_DOWNLEFT    ALIGN_BOTTOM | ALIGN_LEFT

void GetStringLength(const char *str, uint *w, uint *h, uint align, uint big);
void PrintString(const char *str, uint x, uint y, char *where, uint pitch, bool big, int color, uint align);
void PrintNumber(int num, uint x, uint y, char *where, uint pitch, bool big);
void PrintSmallNumber(int num, uint x, uint y, char *where, uint pitch);

#endif