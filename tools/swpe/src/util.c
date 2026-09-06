#include "util.h"
#include "alloc.h"
#include "debug.h"

/* int2str

   num - number to convert
   buf - buffer to store string

   Converts number to string. Return pointer to passed buffer.
*/
char *int2str(int num, char *buf)
{
    int i = 0, j, neg = FALSE;

    if (num < 0) {
        neg = TRUE;
        num = -num;
    }

    do {
        buf[i++] = num % 10 + 0x30;
        num /= 10;
    } while (num);

    if (neg)
        buf[i++] = '-';
    buf[i] = 0;

    for (j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = tmp;
    }
    return buf;
}


/* str2int

   buf - buffer with number as a string

   Converts string to number and returns it.
*/
int str2int(uchar *buf)
{
    int num = 0;

    while (isdigit(*buf)) {
        num *= 10;
        num += *buf++ - '0';
    }
    return num;
}


/* my_strdup

   src - string to duplicate

   returns: pointer to copy of string src, or empty string if no more memory

   Duplicates string. Memory for copy is allocated from heap.
*/
char *my_strdup(const char *src)
{
    char *p;

    if (p = omalloc(strlen(src) + 1)) {
        strcpy(p, src);
        return p;
    } else
        return "";
}


/* my_strncpy

   dest - ptr to destination string
   src  - ptr to source string
   len  - maximum length to copy

   Copies maximum len characters from src to dest. Dest must be big enough, and
   must not overlap with src.
*/
void my_strncpy(char *dest, const char *src, uint len)
{
    while (len-- && (*dest++ = *src++));
}


/* FillBitmapInfo

   bmfh       - ptr to bitmap file header struct
   bmih       - ptr to bitmap info struct
   num_colors - number of colors in palette
   width      - width of bitmap
   height     - height of bitmap

   Fills bmfh and bmih with all data needed for saving. Works only with 8-bit
   (paletised) bitmaps.
*/
void FillBitmapInfo(BITMAPFILEHEADER *bmfh, BITMAPINFOHEADER *bmih, uint num_colors, uint width, uint height)
{
    uint size = width * height;

    bmfh->bfType = 'MB';
    bmfh->bfSize = sizeof(*bmfh) + sizeof(*bmih) + num_colors * sizeof(RGBQUAD) + size;
    bmfh->bfReserved1 = 0;
    bmfh->bfReserved2 = 0;
    bmfh->bfOffBits = sizeof(*bmfh) + sizeof(*bmih) + num_colors * sizeof(RGBQUAD);
    bmih->biSize = sizeof(*bmih);
    bmih->biWidth = width;
    bmih->biHeight = height;
    bmih->biPlanes = 1;
    bmih->biBitCount = 8;
    bmih->biCompression = BI_RGB;
    bmih->biSizeImage = size;
    bmih->biXPelsPerMeter = 0;
    bmih->biYPelsPerMeter = 0;
    bmih->biClrUsed = num_colors;
    bmih->biClrImportant = num_colors;
}


/* usqrt

   n - number for which to calculate square root

   Returns square root of n. Integer version.
*/
uint usqrt(uint n)
{
    uint s, p;

    for (s = 1, p = 3; s <= n; s += p, p += 2);
    return (p - 3) / 2;
}


/* my_stricmp

   s1 - pointer to first string
   s2 - pointer to second string

   Lexicographicly compares strings case insensitive. Returns an integer less
   than, equal, or greater than zero, indicating that first string is less
   than, equal, or greater than second string.
*/
int my_stricmp(const char *s1, const char *s2)
{
    char ch1, ch2;

    do {
        ch1 = toupper(*s1);
        ch2 = toupper(*s2);
        s1++;
        s2++;
    } while (ch1 && ch1 == ch2);
    return ch1 - ch2;
}

/* Empty

   Empty function when mode doesn't require FinishMode function
*/
void Empty()
{
   return;
}

/* SortNames

   names -> array of char pointers to sort
   n     -  number of strings

   Sort given list of character pointers. Using improved bubble sort algorithm.
*/
void SortNames(char **names, int n)
{
    bool sort_flag;
    int i, last;

    do {
        sort_flag = FALSE;
        for (i = 0, last = n - 1; i < last; i++) {
            if (my_stricmp(names[i], names[i + 1]) > 0) {
                /* interchange strings */
                char *tmp = names[i];
                names[i] = names[i + 1];
                names[i + 1] = tmp;
                /* we did some sorting in this cycle */
                sort_flag = TRUE;
            }
        }
        last--;
    } while (sort_flag);
}
