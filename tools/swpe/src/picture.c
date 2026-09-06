/*
   picture.c

   Handles SWOS pictures, files with extension 256. Their size is exactly 64768
   bytes. First 64000 bytes are pixels - 8 bit, dimensions 320 x 200. Last 768
   bytes are palette, VGA style - R, G, B values respectively, in range 0 - 63.
   All picture files in directory are read, and doubly linked circular list is
   created. Then user can view pictures by walking that list.
*/

#include <assert.h>
#include "printstr.h"
#include "draw.h"
#include "picture.h"
#include "debug.h"
#include "alloc.h"
#include "file.h"
#include "util.h"

static bool InitPictures();
static void InitPicturesMode(uint old_mode);
static bool PicturesKeyProc(uint code, uint data);
static byte *PicturesDraw(byte *pbits, const uint pitch);
static byte *PicturesGetPalette();

#define DELTA_THRESHOLD 381

extern uchar pal[768];

/* help for pictures mode */
static char pictures_help[] = {
    "space/backspace\n"
    "arrows right/left\t- next/previous picture\n"
    "+/-\t\t\t\t- brighten/darken picture\n"
    "F2\t\t\t\t\t- save picture as bitmap\n"
    "F3\t\t\t\t\t- reload picture"
};

/* picture mode "object" */
const Mode PictureMode = {
    InitPictures,
    InitPicturesMode,
    Empty,
    PicturesKeyProc,
    nullptr,
    PicturesDraw,
    PicturesGetPalette,
    pictures_help,
    "PICTURE MODE",
    VK_F6,
    0
};

/* Global information about pictures state */
Pictures_info p = {0,0,0,0,0};

/* pointers to pictures needed for menu drawing */
Sws_256_pic *stad_ptr;
Sws_256_pic *swtitle_ptr;
Sws_256_pic *fill_ptr;

struct {            /* files needed */
    char *fname;
    char found;
    Sws_256_pic **ptr;
} static fn[] = {
    "stad.256", 0, &stad_ptr,
    "swtitle.256", 0, &swtitle_ptr,
    "fill.256", 0, &fill_ptr
};

/* InitPictures

   Read pictures list from current directory into doubly linked circular list.
   Allocate memory for gamma corrected palette. Must find STAD.256 or reports
   failure.
*/
bool InitPictures()
{
    WIN32_FIND_DATA find_data;
    HANDLE hsearch;
    int i, got_all, sort_flag, num_pics = 0;
    char buf[256];

    hsearch = FindFirstFile("*.256", &find_data);
    if (hsearch != INVALID_HANDLE_VALUE) {
        Sws_256_pic *q = NULL, *last_pic = NULL, *sws_pic = NULL;
        /* connect prev fields and initialize structures */
        do {
            sws_pic = xmalloc(sizeof(Sws_256_pic));
            sws_pic->filename = my_strdup(find_data.cFileName);
            sws_pic->is_loaded = 0;
            sws_pic->error = NO_ERROR;
            sws_pic->prev = q;
            sws_pic->text_color = -1;
            sws_pic->picture = NULL;
            q = sws_pic;
            num_pics++;
            /* check for required pictures */
            for (i = 0; i < sizeofarray(fn); i++)
                if (!fn[i].found) {
                    if (fn[i].found = !my_stricmp(find_data.cFileName,
                        fn[i].fname)) {
                        LoadPicture(sws_pic);
                        if (sws_pic->error) {
                            wsprintf(buf, "Error loading %s", fn[i].fname);
                            SetErrorMsg(buf);
                            FindClose(hsearch);
                            return FALSE;
                        }
                        *fn[i].ptr = sws_pic;
                    }
                }
        } while (FindNextFile(hsearch, &find_data));

        /* connect next fields */
        for (last_pic = q, q = NULL; sws_pic; sws_pic = sws_pic->prev) {
            sws_pic->next = q;
            q = sws_pic;
        }

        /* connect first and last node - make list circular */
        last_pic->next = q;
        q->prev = last_pic;

        /* set current picture pointer */
        p.pic_current = q;
    }

    FindClose(hsearch);

    /* allocate memory for gamma corrected palette */
    p.palette = xmalloc(768);

    got_all = TRUE;
    for (i = 0; i < sizeofarray(fn); i++) {
        if (!fn[i].found) {
            char buf[256];
            wsprintf(buf, "%s missing!", fn[i].fname);
            SetErrorMsg(buf);
            got_all = FALSE;
        }
    }

    /* sort pictures list by name, if not empty */
    if (p.pic_current) {
        do {
            Sws_256_pic *m = p.pic_current;
            sort_flag = FALSE;
            while (m->next != p.pic_current) {
                Sws_256_pic *n = m->next;
                if (my_stricmp(n->filename, m->filename) < 0) {
                    m->prev->next = n;
                    n->next->prev = m;
                    m->next = n->next;
                    n->next = m;
                    n->prev = m->prev;
                    m->prev = n;
                    sort_flag = TRUE;
                    if (m == p.pic_current)
                        p.pic_current = n;
                }
                m = m->next;
            }
        } while (sort_flag);
    }

    return got_all;
}

/* InitPicturesMode

   Called every time when switching to pictures mode
*/
void InitPicturesMode(uint old_mode)
{
    if (!p.pic_current) {
        WriteToLog(("InitPicturesMode(): No pictures in dir."));
        PrintWarning("No pictures to display", 1200, ALIGN_CENTER);
        return;
    }
    if (!p.pic_current->is_loaded)
        LoadPicture(p.pic_current);

    memcpy(p.palette, p.pic_current->palette, 768);
    p.old_gamma = MAX_GAMMA + 1;
    ApplyGammaToPalette();
    SetPalette(p.palette, &g.pbits, TRUE);
    p.is_pal_valid = TRUE;
}

/* PicturesKeyProc

   code - virtual key code
   data - key data

   Handle keys for pictures mode.
*/
bool PicturesKeyProc(uint code, uint data)
{
    char buf[64];

    switch (code) {
    case VK_SPACE:
    case VK_RIGHT:
    case VK_BACK:
    case VK_LEFT:
        /* set palette to invalid, since this is new picture */
        p.is_pal_valid = FALSE;

        /* move to next or previous picture in list */
        p.pic_current = code == VK_SPACE || code == VK_RIGHT ?
            p.pic_current->next : p.pic_current->prev;

        /* load it, if necessary */
        if (!p.pic_current->is_loaded)
            LoadPicture(p.pic_current);

        /* copy this picture's palette to global pictures palette */
        memcpy(p.palette, p.pic_current->palette, 768);

        /* gamma needs to be reaplied; force update by setting old_gamma to
           impossible value so this request doesn't get rejected */
        p.old_gamma = MAX_GAMMA + 1;
        ApplyGammaToPalette();

        /* also, if we are in fullscreen, we must explicitly request palette
           change */
        SetPalette(p.palette, &g.pbits, TRUE);
        break;

    case VK_ADD:
    case VK_SUBTRACT:
        /* add or subtract 1 */
        p.gamma += (code == VK_ADD) * 2 - 1;

        /* normalize value */
        p.gamma = code == VK_ADD ? min(MAX_GAMMA, p.gamma) : max(MIN_GAMMA, p.gamma);

        /* if already at max or min value, no need to update palette */
        p.is_pal_valid = p.gamma == p.old_gamma;

        /* update palette with new gamma (only when necessary) */
        ApplyGammaToPalette();

        /* if needed, request fullscreen palette update */
        SetPalette(p.palette, &g.pbits, TRUE);

        WriteToLog(("PicturesKeyProc(): Setting gamma to %d", p.gamma));

        /* write current gamma value */
        wsprintf(buf, "GAMMA: %d", p.gamma);
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_UPRIGHT);
        break;

    case VK_F2:
        /* save picture to bitmap */
        if (!p.pic_current->error && !SavePicture(p.pic_current))
            PrintWarning("Save failed", WARNING_INTERVAL, ALIGN_CENTER);
        break;

    case VK_F3:
        /* reload picture */
        p.pic_current->is_loaded = FALSE;
        LoadPicture(p.pic_current);
        SetPalette(p.palette, &g.pbits, TRUE);
        break;

    default:
        return FALSE;
    }
    return TRUE;
}

/* PicturesDraw

   pbits  - pointer to bitmap bits
   pitch  - distance between lines

   Draw routine for pictures mode.
*/
byte *PicturesDraw(byte *pbits, const uint pitch)
{
    DrawPicture(&pbits, p.pic_current, pitch);
    PrintString(p.pic_current->filename, 0, 0, pbits, pitch, TRUE,
        p.pic_current->text_color, ALIGN_UPLEFT);
    return pbits;
}

/* PicturesGetPalette

   Return valid palette for pictures mode when palette needs updating in
   fullscreen mode.
*/
byte *PicturesGetPalette()
{
    return p.palette;
}

/* LoadPicture

   sws_pic - pointer to picture struct to fill with data

   Load picture from disk and find brightest color for text. If there are no
   bright colors in palette, set least used color to white. Set sws_pic->error
   in case of an error, and set its palette to standard menu palette. Set
   sws_pic->is_loaded if everything went ok. If picture is already loaded,
   return.
*/
void LoadPicture(Sws_256_pic *sws_pic)
{
    uint bytes_read, read_res, i, max_delta, max_color;
    HANDLE hfile;

    if (sws_pic->is_loaded)
        return;

    sws_pic->palette = pal; /* set menu palette in case of an error */

    hfile = FOpen(sws_pic->filename, FF_READ | FF_SHARE_READ | FF_SEQ_SCAN);
    if (hfile == ERR_HANDLE) {
        sws_pic->error = PIC_ERROR_FILE;
        return;
    }

    if (GetFileSize(hfile, 0) != 64768) {
        FClose(hfile);
        sws_pic->error = PIC_ERROR_INV_SIZE;
        return;
    }

    if (!sws_pic->picture)
        sws_pic->picture = omalloc(64768);
    if (!sws_pic->picture) {
        FClose(hfile);
        sws_pic->error = PIC_ERROR_NO_MEMORY;
        return;
    }

    read_res = ReadFile(hfile, sws_pic->picture, 64768, &bytes_read, 0);
    if (!read_res || bytes_read != 64768) {
        xfree(sws_pic->picture);
        sws_pic->picture = NULL;
        FClose(hfile);
        sws_pic->error = PIC_ERROR_READING;
        return;
    }

    FClose(hfile);
    sws_pic->error = PIC_NO_ERROR;
    sws_pic->is_loaded = TRUE;
    sws_pic->palette = sws_pic->picture + 64000;

    /* palette colors are in range 0..63; bring them into 0..255 range */
    for (i = 0; i < 768; i++)
        sws_pic->palette[i] *= 4;

    if (sws_pic->text_color < 0) {
        /* find brightest color */
        for (i = max_color = max_delta = 0; i < 256; i++) {
            uchar r = sws_pic->palette[3 * i];
            uchar g = sws_pic->palette[3 * i + 1];
            uchar b = sws_pic->palette[3 * i + 2];
            uint delta = usqrt(r * r + g * g + b * b);
            max_delta = delta >= max_delta ? max_color = i, delta : max_delta;
        }

        /* if not bright enough */
        if (max_delta < DELTA_THRESHOLD) {
            uchar *p = sws_pic->picture;
            int colors[256] = {0}, min_used;

            WriteToLog(("LoadPicture(): Can't find color bright enough in %s", sws_pic->filename));

            /* find color usage */
            while (p < sws_pic->picture + 64000) colors[*p++]++;

            /* find least used color */
            for (i = min_used = 0; i < 256; i++)
                min_used = colors[i] < colors[min_used] ? i : min_used;

            /* set least used color to white */
            sws_pic->palette[3 * min_used] = 255;
            sws_pic->palette[3 * min_used + 1] = 255;
            sws_pic->palette[3 * min_used + 2] = 255;

            WriteToLog(("LoadPicture(): Replaced color %d with white in %s", min_used, sws_pic->filename));

            max_color = min_used;
        }

        sws_pic->text_color = max_color;
    }
}

/* DrawPicture

   pbits   - addres of pointer to surface to draw on
   sws_pic - picture to draw
   pitch   - distance between lines

   Draws a picture to pbits. Palette changes are handled here, and pbits can
   be changed during that.
*/
void DrawPicture(byte **pbits, Sws_256_pic *sws_pic, uint pitch)
{
    uint i;
    if (sws_pic->error) {
        *pbits = CreateDIB(pal);
        memset(*pbits, 0, WIDTH * HEIGHT);
        if (sws_pic->error == PIC_ERROR_FILE)
            PrintString("Can't open file", 0, 0, *pbits, pitch, TRUE, -1, ALIGN_CENTER);
        else if (sws_pic->error == PIC_ERROR_READING)
            PrintString("Error reading file", 0, 0, *pbits, pitch, TRUE, -1, ALIGN_CENTER);
        else if (sws_pic->error == PIC_ERROR_NO_MEMORY)
            PrintString("Not enough memory for picture", 0, 0, *pbits, pitch, TRUE, -1, ALIGN_CENTER);
        else if (sws_pic->error == PIC_ERROR_INV_SIZE)
            PrintString("Invalid picture size", 0, 0, *pbits, pitch, TRUE, -1, ALIGN_CENTER);
        else
            PrintString("Unknown error", 0, 0, *pbits, pitch, TRUE, -1, ALIGN_CENTER);
        return;
    }

    WriteToLog(("DrawPicture(): Drawing picture: %s", sws_pic->filename));

    assert(*pbits);

    if (!p.is_pal_valid) {
        *pbits = CreateDIB(p.palette);
        p.is_pal_valid = TRUE;
        /* just to be sure */
        assert(*pbits);
        if (!*pbits)
            return;
    }

    for(i = 0; i < HEIGHT; i++)
        memcpy(*pbits + pitch * i, sws_pic->picture + WIDTH * i, WIDTH);

    /* load next in advance */
    LoadPicture(p.pic_current->next);
}

/* SavePicture

   sws_pic - picture to save

   Saves .256 picture to bitmap with corresponding filename.
*/
bool SavePicture(Sws_256_pic *sws_pic)
{
    uint i, save_res;
    char *pbits;
    RGBQUAD colors[256];
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;
    char fname[MAX_PATH];
    char buf[MAX_PATH];
    HANDLE hfile;

    if (sws_pic->error || !sws_pic->is_loaded) {
        WriteToLog(("SavePicture(): Trying to save corrupt or not loaded "
            "picture."));
        return FALSE;
    }

    if (!(pbits = omalloc(WIDTH * HEIGHT)))
        return FALSE;

    for (i = 0; i < HEIGHT / 2; i++) {
        memcpy(pbits + i * WIDTH, sws_pic->picture+(HEIGHT-i-1)* WIDTH, WIDTH);
        memcpy(pbits + (HEIGHT-i-1) * WIDTH, sws_pic->picture+i*WIDTH, WIDTH);
    }

    for (i = 0; i < 256; i++) {
        colors[i].rgbRed = sws_pic->palette[3 * i];
        colors[i].rgbGreen = sws_pic->palette[3 * i + 1];
        colors[i].rgbBlue = sws_pic->palette[3 * i + 2];
        colors[i].rgbReserved = 0;
    }

    FillBitmapInfo(&bmfh, &bmih, 256, WIDTH, HEIGHT);

    my_strncpy(fname, sws_pic->filename, MAX_PATH);
    strcpy(fname + strlen(fname) - 3, "bmp");

    if ((hfile = FCreate(fname, FF_WRITE)) == ERR_HANDLE)
        return xfree(pbits), FALSE;

    save_res = TRUE;
    if (!WriteFile(hfile, &bmfh, sizeof(bmfh), &i, 0) ||
        !WriteFile(hfile, &bmih, sizeof(bmih), &i, 0) ||
        !WriteFile(hfile, colors, sizeof(colors), &i, 0) ||
        !WriteFile(hfile, pbits, 64000, &i, 0))
        save_res = FALSE;

    FClose(hfile);
    xfree(pbits);

    if (!save_res)
        return FALSE;

    wsprintf(buf, "Saved %s OK", fname);
    PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);

    return TRUE;
}

/* ApplyGammaToPalette

   Applies gamma correction to pictures palette (brightens it or darkens it).
*/
void ApplyGammaToPalette()
{
    uchar *p1, *p2;
    int tmp;

    /* check if palette really needs updating */
    if (p.old_gamma == p.gamma)
        return;

    /* make greater steps */
    p.gamma *= 2;

    p1 = p.palette;
    p2 = p.pic_current->palette;

    if (p.gamma >= 0)
        for (; p1 < p.palette + 768; p1++, p2++) {
            tmp = *p2 + p.gamma;
            tmp |= -(tmp > 255);
            *p1 = tmp;
        }
    else
        for (; p1 < p.palette + 768; p1++, p2++) {
            tmp = *p2 + p.gamma;
            tmp &= -(tmp >= 0);
            *p1 = tmp;
        }

    p.gamma /= 2;

    /* register palette update */
    p.old_gamma = p.gamma;

    /* don't affect text color; but carefully! it might be -1 in case of an
       error */
    if (p.pic_current->text_color >= 0)
        memcpy(p.palette + 3 * p.pic_current->text_color,
            p.pic_current->palette + 3 * p.pic_current->text_color, 3);
}

/* CleanUpPictures

   Release memory used by pictures so we don't get annoying memory leak warning.
*/
void CleanUpPictures()
{
    Sws_256_pic *q = p.pic_current;
    int i;
    if (q)
        do {
            /* these were allocated outside of memory checking jurisdiction */
            bool found = FALSE;
            for (i = 0; i < sizeofarray(fn); i++)
                if (!my_stricmp(fn[i].fname, q->filename)) {
                    found = TRUE;
                    break;
                }
            if (!found) {
                xfree(q->picture);
                q->picture = NULL;
                q->is_loaded = 0;
            }
            q = q->next;
        } while (q != p.pic_current);
}
