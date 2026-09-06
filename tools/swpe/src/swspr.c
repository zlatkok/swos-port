/* swspr.c

   Handles sprites. Sprites are held in *.dat files, with sprite.dat as index
   file (containing 4 byte offsets). Contains sprite saving/insertion
   routines.
*/

#include "swspr.h"
#include "printstr.h"
#include "draw.h"
#include "debug.h"
#include "alloc.h"
#include "file.h"
#include "util.h"
#include <stdio.h>

static bool InitSprites();
static void InitSpriteMode(uint old_mode);
static bool SpritesKeyProc(uint code, uint data);
static byte *SpritesDraw(byte *pbits, const uint pitch);
static byte *SpritesGetPalette();

static void DatToSprites(Sprite *sprites, uint dat_no, bool force_alloc);
static uint Match(uint col);
static bool GotoSprite(int spr_no);
static void RevertToSaved();

/* palettes */
extern uchar pal[768];
extern uchar gamepal[768];

static schar zoomf = 1; /* zoom factor */

/* help for sprite mode */
static char sprites_help[] = {
    "space/backspace\n"
    "arrows right/left\t- next/previous sprite\n"
    "g\t\t\t\t\t- go directly to sprite\n"
    "page up/down\t\t- \r10 sprites forward/\nbackward\n"
    "0..9\t\t\t\t- \rchange background\ncolor\n"
    "home/end\t\t\t- first/last sprite\n"
    "+/-\t\t\t\t- zoom/unzoom\n"
    "F2\t\t\t\t\t- \rsave current sprite to\nbitmap\n"
    "shift + F2\t\t\t- save all sprites\n"
    "insert\t\t\t\t- \rreplace current sprite\n\rwith corresponding\nbitmap\n"
    "shift + insert\t\t- \rinsert all sprites in\ndirectory\n"
    "i\t\t\t\t\t- \rtoggle sprite\ninformation on/off\n"
    "ctrl + r\t\t\t- revert to saved\n"
    "ctrl + t\t\t\t- \rswitch between\nteam2.dat and team3.dat"
};

/* sprite mode "object" */
const Mode SpriteMode = {
    InitSprites,
    InitSpriteMode,
    Empty,
    SpritesKeyProc,
    nullptr,
    SpritesDraw,
    SpritesGetPalette,
    sprites_help,
    "SPRITE MODE",
    VK_F5,
    0
};

/* asm routines for coding/decoding sprites */
extern void _stdcall ChainSprite(Sprite *spr);
extern void _stdcall UnchainSprite(Sprite *spr);

/* Sprites global information */
Sprites_info s = {3,0,{"SPRITE.DAT",0,0},0,0};

/* files that contain sprite structures that sprite.dat is pointing to */
Dat_file dat_files[NUM_DAT_FILES] = {
    {"CHARSET.DAT", 0, 227,    0, 0, 0, NULL, 0},
    {"SCORE.DAT",   0, 114,  227, 0, 0, NULL, 0},
    {"TEAM1.DAT",   0, 303,  341, 0, 0, NULL, 0},
    {"TEAM3.DAT",   0, 303,  644, 0, 0, NULL, 0},
    {"GOAL1.DAT",   0, 116,  947, 0, 0, NULL, 0},
    {"GOAL1.DAT",   0, 116, 1063, 0, 0, NULL, 0},
    {"BENCH.DAT",   0, 155, 1179, 0, 0, NULL, 0}
};

/* number of *.dat files */
#define files_num sizeof(dat_files)/sizeof(Dat_file)

/* InitSprites

   Read everything needed to display sprites into memory.
*/
bool InitSprites()
{
    uint i, j, tmp;
    Sprite *sprites;
    char buf[256], *p;

    /* global array of sprites which is used for further sprite referencing */
    sprites = (Sprite *)xmalloc(NUM_SPRITES * sizeof(Sprite));

    /* open sprites index file so no one messes with it while we are running */
    s.spritef.handle = FOpen(s.spritef.fname, FF_READ | FF_WRITE | FF_SHARE_READ | FF_SEQ_SCAN);

    if (s.spritef.handle == ERR_HANDLE) {
        wsprintf(buf, "Can't open sprites index file, %s", s.spritef.fname);
        SetErrorMsg(buf);
        return FALSE;
    }

    /* get size of file, and try to allocate the buffer of that size */
    s.spritef.size = GetFileSize(s.spritef.handle, NULL);

    /* check sprite.dat for size */
    if (s.spritef.size != SPRITE_DAT_SIZE) {
        wsprintf(buf,
            "%s has invalid size.\nExpected size: %d, current size "
            ":%d",
            s.spritef.fname, SPRITE_DAT_SIZE, s.spritef.size);
        SetErrorMsg(buf);
        return FALSE;
    }

    /* bail out if no more memory */
    if (!(s.spritef.buffer = (char *)omalloc(s.spritef.size))) {
        wsprintf(buf, "Not enough memory to load file %s", s.spritef.fname);
        SetErrorMsg(buf);
        return FALSE;
    }

    /* open all files containing sprites */
    for (i = 0; i < files_num; i++) {
        /* first try to open the file */
        dat_files[i].handle = FOpen(dat_files[i].fname, FF_READ | FF_WRITE | FF_SHARE_READ | FF_SHARE_WRITE | FF_SEQ_SCAN);

        /* check if file opened successfully */
        if (dat_files[i].handle == ERR_HANDLE) {
            wsprintf(buf, "Can't open %s", dat_files[i].fname);
            SetErrorMsg(buf);
            break;
        }

        /* get size of file, and try to allocate the buffer of that size */
        dat_files[i].size = dat_files[i].old_size =
            GetFileSize(dat_files[i].handle, NULL);

        /* bail out if no more memory */
        if (!(dat_files[i].buffer = (char *)omalloc(dat_files[i].size))) {
            wsprintf(buf, "Not enough memory to load file %s", dat_files[i].fname);
            SetErrorMsg(buf);
            break;
        }

        /* try to read file contents; we'll need this buffer for restoring */
        j = ReadFile(dat_files[i].handle, dat_files[i].buffer, dat_files[i].size, &tmp, NULL);

        /* check for read error */
        if (!j || tmp != dat_files[i].size) {
            wsprintf(buf, "Can't read file %s", dat_files[i].fname);
            SetErrorMsg(buf);
            break;
        }
        FClose(dat_files[i].handle);
    }

    if (i != files_num)
        return FALSE;

    /* transfer sprites from dat files to sprites array */
    for (i = 0; i < files_num; i++)
        DatToSprites(sprites, i, TRUE);

    s.sprites = sprites;

    /* decrypt first pass - unxor */
    for (p = acopyright; p < acopyright + COPYRIGHT_LEN; p++)
        *p ^= 0x5a;

    /* decode all sprites */
    for (i = 0; i < NUM_SPRITES; i++)
        ChainSprite(&sprites[i]);

    s.bk_col = BLUE;

    return TRUE;
}

/* DatToSprites

   sprites    -> array of sprite structures to fill
   dat_no     -  number of dat file to get sprites from
   forc_alloc -  if true force memory allocation; used for first time loading

   Makes sprites array valid by allocating memory for pixels (if force_alloc
   is true), and copying them from dat files. If alloc is false each sprite's
   size is considered and memory is reallocated only if new and old sizes are
   different.
*/
static void DatToSprites(Sprite *sprites, uint dat_no, bool force_alloc)
{
    uint i, spr_index = dat_files[dat_no].start_sprite;
    uchar *p = dat_files[dat_no].buffer, *q;

    /* copy informations to sprites array and allocate memory for pixels */
    for (i = 0; i < dat_files[dat_no].entries; i++) {
        Sprite *s = &sprites[spr_index++], *dat_s = (Sprite*)p;
        int size = dat_s->nlines * dat_s->wquads * 8;

        /* init q in case spr_data does not need reallocation */
        q = s->spr_data;

        /* allocate memory for pixels if needed */
        if (force_alloc) {
            /* carefully here, if alloc is set and sprite aready has assigned
               memory for pixels, memory leak will occur; alloc should be used
               exlusively when called from InitSprites() */
            q = xmalloc(size);
        } else if (s->size != size) {
            xfree(s->spr_data);
            q = xmalloc(size);
        }
        /* copy sprite structure - set changed to FALSE implicitly */
        *s = *dat_s;
        memcpy(s->spr_data = q, p += sizeof(Sprite), s->size = size);
        p += size;
        s->dat_file = dat_no;
    }
}

/* InitSpritesMode

   Called every time when switching to sprite mode.
*/
void InitSpriteMode(uint old_mode)
{
    SetPalette(pal, &g.pbits, TRUE);
}

/* SpritesDraw

   pbits  - pointer to bitmap bits
   pitch  - distance between lines

   Draw routine for sprite mode.
*/
static byte *SpritesDraw(byte *pbits, const uint pitch)
{
    int x = 0, y = 0, i;
    int m = NUM_SPRITES - 1; /* for normalizing sprite number */
    uint s_and_r;
    uchar bk_col, buf[256];
    Sprite *spr;

    s.sprite_no = s.sprite_no > m ? 0 : s.sprite_no < 0 ? m : s.sprite_no;
    s_and_r = s.sprite_no > 1208 && s.sprite_no < 1273;
    if (s_and_r) {
        SetPalette(gamepal, &pbits, FALSE);
        bk_col = Match(s.bk_col);
    } else {
        SetPalette(pal, &pbits, FALSE);
        bk_col = s.bk_col;
    }

    x = (WIDTH - s.sprites[s.sprite_no].width) / 2;
    y = (HEIGHT - s.sprites[s.sprite_no].nlines) / 2;

    for (i = 0; i < HEIGHT; i++)
        memset(pbits + i * pitch, bk_col, WIDTH);
    DrawSprite(x, y, pbits, &s.sprites[s.sprite_no], pitch, -1);

    /* Big R and START_PATTERN sprites should be drawn with game palette and every pixel
       should be OR-ed with 0x70. This is not the best solution, but is
       probably the simplest - I avoid having another specialized sprite
       drawing routine, and don't slow down DrawSprite. Still, it is quite
       interesting algorithm, using only logical operations for plotting, no
       branches.
    */
    if (s_and_r) {
        uchar *p = pbits + y * pitch + x;
        int width = s.sprites[s.sprite_no].wquads * 8; /* two pixels in loop */
        int lines = s.sprites[s.sprite_no].nlines;
        int delta = pitch - width * 2, w;
        byte *spr = s.sprites[s.sprite_no].spr_data;
        for (; lines--; p += delta)
            for (w = width; w-- ;) {
                *p++ |= -!!((*spr & 0xf0) >> 4) & 0x70;
                *p++ |= -!!(*spr++ & 0x0f) & 0x70;
            }
    }

    Zoom(pbits, zoomf, pitch);
    PrintNumber(s.sprite_no, 0, 0, pbits, pitch, TRUE);

    /* prints info about sprite if requested */
    if (s.show_info) {
        spr = &s.sprites[s.sprite_no];
        wsprintf(buf,
            "Sprite information:\n"
            "-----------------\n"
            "Dat file: %s\n"
            "Ordinal: %d\n"
            "Width: %d\n"
            "Height: %d\n"
            "Center x: %d\n"
            "Center y: %d\n"
            "Field 20: 0x%02x",
            dat_files[spr->dat_file].fname, spr->ordinal, spr->width, spr->nlines, spr->center_x, spr->center_y, spr->unk4);
        PrintString(buf, 0, 0, pbits, pitch, TRUE, -1, ALIGN_UPRIGHT);
    }
    return pbits;
}

/* SpritesGetPalette

   Return sprite mode specific palette.
*/
static byte *SpritesGetPalette()
{
    return s.sprite_no >= 1209 && s.sprite_no <= 1272 ? gamepal : pal;
}

/* GetSize

   spr - pointer to sprite
   *w  - gets sprite width
   *h  - gets sprite height

   Gets width and height of a sprite.
*/
void GetSize(Sprite *spr, uint *w, uint *h)
{
    *w = spr->width;
    *h = spr->nlines;
}

static void saveSpriteMetadata(Sprite *spr, int spriteNo)
{
    if (!spr->center_x && !spr->center_y)
        return;

    char filename[64];
    sprintf(filename, "spr%04d.txt", spriteNo);

    HANDLE hfile;
    if ((hfile = FCreate(filename, FF_WRITE)) != ERR_HANDLE) {
        char content[128];
        auto contentLen = sprintf(content, "%d\r\n%d\r\n", spr->center_x, spr->center_y);

        uint tmp;
        WriteFile(hfile, content, contentLen, &tmp, 0);

        FClose(hfile);
    }
}

/* SaveSprite

   spriteNo  - number to give to filename (sprxxxx.bmp)
   bkclr     - background color
   spr       - pointer to sprite to save
   quiet     - controls whether we are allowed to give warnings or not

   Saves sprite to 256 bitmap. First 16 colors are sprite colors, and the 17th
   colors is background color - it is used if sprite is to be replaced, so that
   transparent areas could be preserved.
*/
bool SaveSprite(uint spriteNo, uint bkclr, Sprite *spr, uint quiet)
{
    static char sprfile[] = "spr0000.bmp";
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;
    RGBQUAD colors[17];
    uint width, height, size, i, tmp, padding, line_width;
    uchar *numbuf = sprfile + 3, line[WIDTH], pbits[HEIGHT * WIDTH];
    HANDLE hfile;
    char buf[128];

    /* validate input data */
    if (spr == NULL || spriteNo > NUM_SPRITES - 1)
        return FALSE;

    height = spr->nlines;
    width = spr->width;
    if (width > WIDTH || height > HEIGHT)
        return FALSE;
    line_width = width + 3 & ~3;
    padding = line_width - width;
    size = (width + padding) * height;

    memset(numbuf, '0', 4);

    /* fill background with transparent color and draw sprite */
    memset(pbits, 16, size);
    DrawSprite(0, 0, (uchar*)pbits, &s.sprites[spriteNo], line_width, -1);

    /* bitmaps store pixels upside-down */
    for (i = 0; i < height / 2; i++) {
        memcpy(line, pbits + line_width * i, width);
        memcpy(pbits + line_width * i, pbits + line_width*(height-1-i), width);
        memcpy(pbits + line_width * (height - 1 - i), line, width);
    }

    FillBitmapInfo(&bmfh, &bmih, 17, width, height);

    /* copy palette - first sixteen colors */
    for (i = 0; i < 16; i++) {
        colors[i].rgbRed = pal[3 * i];
        colors[i].rgbGreen = pal[3 * i + 1];
        colors[i].rgbBlue = pal[3 * i + 2];
        colors[i].rgbReserved = 0;
    }

    if (spriteNo > 1208 && spriteNo < 1273) {
        for (i = 0; i < 16; i++) {
            colors[i].rgbRed = gamepal[3 * (i | 0x70)];
            colors[i].rgbGreen = gamepal[3 * (i | 0x70) + 1];
            colors[i].rgbBlue = gamepal[3 * (i | 0x70) + 2];
            colors[i].rgbReserved = 0;
        }
    }

    /* set transparent color */
    colors[16].rgbRed = pal[3 * bkclr];
    colors[16].rgbGreen = pal[3 * bkclr + 1];
    colors[16].rgbBlue = pal[3 * bkclr + 2];
    colors[16].rgbReserved = 0;

    /* convert sprite number to string */
    i = 0;
    tmp = spriteNo;
    do {
        numbuf[i++] = tmp % 10 + 0x30;
        tmp /= 10;
    } while (tmp);

    /* set filename */
    tmp = sprfile[3];
    sprfile[3] = sprfile[6];
    sprfile[6] = tmp;
    tmp = sprfile[4];
    sprfile[4] = sprfile[5];
    sprfile[5] = tmp;

    if ((hfile = FCreate(sprfile, FF_WRITE)) == ERR_HANDLE)
        return FALSE;

    do {
        if (!WriteFile(hfile, &bmfh, sizeof(bmfh), &tmp, 0))
            break;
        if (!WriteFile(hfile, &bmih, sizeof(bmih), &tmp, 0))
            break;
        if (!WriteFile(hfile, colors, sizeof(colors), &tmp, 0))
            break;
        if (!WriteFile(hfile, pbits, size, &tmp, 0))
            break;

        if (!quiet) {
            wsprintf(buf, "Saved %s OK", sprfile);
            PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
        }
        FClose(hfile);

        saveSpriteMetadata(spr, spriteNo);

        return TRUE;
    } while (FALSE);

    FClose(hfile);
    return FALSE;
}


/* SpritesKeyProc

   code - virtual key code
   data - key data

   Handle keys for sprite mode.
*/
bool SpritesKeyProc(uint code, uint data)
{
    switch (code) {
    case '0': s.bk_col = BLACK;       break;
    case '1': s.bk_col = WHITE;       break;
    case '2': s.bk_col = LIGHT_GRAY;  break;
    case '3': s.bk_col = LIGHT_BROWN; break;
    case '4': s.bk_col = BROWN;       break;
    case '5': s.bk_col = YELLOW;      break;
    case '6': s.bk_col = PITCH_GREEN; break;
    case '7': s.bk_col = BRIGHT_BLUE; break;
    case '8': s.bk_col = BLUE;        break;
    case '9': s.bk_col = DARK_BLUE;   break;
    case VK_SPACE:
    case VK_RIGHT:
        s.sprite_no++;
        break;
    case VK_HOME:
        s.sprite_no = 0;
        break;
    case VK_END:
        s.sprite_no = NUM_SPRITES - 1;
        break;
    case VK_BACK:
    case VK_LEFT:
        s.sprite_no--;
        break;
    case VK_NEXT:
        s.sprite_no += 10;
        break;
    case VK_PRIOR:
        s.sprite_no -= 10;
        break;
    case VK_ADD:
        zoomf++;
        zoomf = min(zoomf, MAX_ZOOM);
        break;
    case VK_SUBTRACT:
        zoomf--;
        zoomf = max(1, zoomf);
        break;
    case VK_INSERT:
        if (isShiftDown()) {
            PrintWarning("Please wait... Inserting all sprites", 0, ALIGN_CENTER);
            UpdateScreen();
            PostMessage(g.hWnd, WM_USER_INSERT_ALL, 0, 0);
        } else {
            InsertSprite(s.sprite_no, FALSE);
        }
        break;
    case VK_F2:
        if (isShiftDown()) {
            PrintWarning("Please wait... Saving all sprites", 0, ALIGN_CENTER);
            UpdateScreen();
            PostMessage(g.hWnd, WM_USER_SAVE_ALL, 0, 0);
        } else {
            if (!SaveSprite(s.sprite_no, s.bk_col, &s.sprites[s.sprite_no], FALSE))
                PrintWarning("Save failed", WARNING_INTERVAL, ALIGN_CENTER);
        }
        break;
    case 'I':
        s.show_info ^= 1;
        break;
    case 'R':
        if (!isControlDown())
            return FALSE;
        YNPrompt("Revert to saved", RevertToSaved);
        break;
    case 'T':
        if (!isControlDown())
            return FALSE;
        if (dat_files[3].changed)
            YNPrompt("Discard changes", SwapTeam2And3);
        else
            SwapTeam2And3();
        break;
    case 'G':
        InputNumber("Show sprite number: ", 4, GotoSprite);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}


/* SaveAllSprites

   Saves all sprites to corresponding sprnnnn.bmp files. If there is an error
   saving some sprite, terminates without saving the rest of sprites.
   (may be changed)
*/
void SaveAllSprites()
{
    uint i;
    WriteToLog(("SaveAllSprites(): Entry"));
    /* this must be done in order to show "please wait..." message */
    SendMessage(g.hWnd, WM_PAINT, 0, 0);
    for (i = 0; i < NUM_SPRITES; i++)
        if (!SaveSprite(i, s.bk_col, &s.sprites[i], TRUE))
            break;
    if (i != NUM_SPRITES) {
        PrintWarning("Error: not all sprites saved", 1200, ALIGN_CENTER);
        UpdateScreen();
    } else {
        PrintWarning("", 10, NO_ALIGNMENT);
    }
}


/* BuildDatFile

   dat_no   - dat file index
   data_ofs - starting offset for data (sprite pixels)

   Updates dat file's buffer. Helper function of SaveChangesToSprites.
   Returns new size of the file buffer.
*/
static uint BuildDatFile(uint dat_no, uint data_ofs)
{
    uint i, cnt;
    uchar *p;

    /* no need to rebuild if not changed */
    if (!dat_files[dat_no].changed)
        return dat_files[dat_no].size;

    /* check if need resizing */
    if (dat_files[dat_no].size != dat_files[dat_no].old_size) {
        char *tmp = (char *)xmalloc(dat_files[dat_no].size);
        xfree(dat_files[dat_no].buffer);
        dat_files[dat_no].buffer = tmp;
    }

    cnt = dat_files[dat_no].start_sprite;
    p = dat_files[dat_no].buffer;
    for (i = 0; i < dat_files[dat_no].entries; i++) {
        uint size = s.sprites[cnt].size;
        /* copy sprite struct */
        *(Sprite*)p = s.sprites[cnt];
        /* fix offset to data */
        (uint)((Sprite *)p)->spr_data = p - dat_files[dat_no].buffer + data_ofs + sizeof(Sprite);
        /* these fields are originally always zero - keep them that way */
        ((Sprite*)p)->size = 0;
        ((Sprite*)p)->dat_file = 0;
        ((Sprite*)p)->changed = 0;
        p += sizeof(Sprite);
        /* copy sprite pixels */
        memcpy(p, s.sprites[cnt++].spr_data, size);
        p += size;
    }

    return dat_files[dat_no].size;
}


/* SaveChangesToSprites

   Save any change to sprites back to dat files. Returns success status.
*/
bool SaveChangesToSprites()
{
    uint i, cnt, ofs, sprites[NUM_SPRITES + 1];
    unsigned __int64 j;
    uchar buf[128], *p;

    if (!(g.changedFlags & SPRITES_CHANGED))
        return TRUE;

    WriteToLog(("SaveChangesToSprites(): Saving changes to sprites..."));

    /* check HASH1 */
    for (j = i = 0, p = acopyright; i < COPYRIGHT_LEN; i++, p++) {
        j += i + *p;
        j <<= 1;
        j ^= i + *p;
    }

#ifdef DEBUG
    if (j != HASH1)
        MessageBox(NULL, "HASH1 mismatch, check COPYRIGHT_LEN", "Hash Error", MB_OK | MB_ICONERROR);
#endif

    /* mess up referee sprites if hash doesn't match */
    for (i = 1273; i < 1284; i++) {
        uchar *p = s.sprites[i].spr_data;
        int w, lines = s.sprites[i].nlines;
        while (lines--) {
            w = s.sprites[i].wquads * 8;
            while (w--)
                *p++ += j != HASH1;
        }
    }
    dat_files[files_num - 1].changed |= j != HASH1;

    /* first unchain */
    for (i = 0; i < NUM_SPRITES; i++)
        UnchainSprite(&s.sprites[i]);

    /* calculate offsets for charset.dat */
    for (i = ofs = 0; i < dat_files[0].entries; i++) {
        sprites[i] = ofs;
        ofs += s.sprites[i].size + sizeof(Sprite);
    }

    /* calculate offsets for rest of files */
    for (i = 1, ofs = 0; i < files_num; i++) {
        cnt = dat_files[i].start_sprite;
        for (j = 0; j < dat_files[i].entries; j++) {
            sprites[cnt] = ofs;
            ofs += s.sprites[cnt++].size + sizeof(Sprite);
        }
    }

    /* update buffers of dat files */
    BuildDatFile(0, 0);
    for (i = 1, ofs = 0; i < files_num - 1; i++)
        ofs += BuildDatFile(i, 0);
    BuildDatFile(files_num - 1, ofs);

    /* since everything is copied, it's best to chain sprites here, in case
       there is an error later */
    for (i = 0; i < NUM_SPRITES; i++)
        ChainSprite(&s.sprites[i]);

    /* save all dat buffers to disk */
    for (i = 0; i < files_num; i++) {
        if (!dat_files[i].changed)
            continue;
        uint size;
        if ((int)SetFilePointer(dat_files[i].handle, 0, 0, FILE_BEGIN) < 0 ||
            !WriteFile(dat_files[i].handle, dat_files[i].buffer,
            dat_files[i].size, &size, 0) || size != dat_files[i].size) {
            WriteToLog(("SaveChangesToSprites(): Failed to save %s", dat_files[i].fname));
            /* will there be enough time before quit for user to see this? */
            wsprintf(buf, "Failed to save %s", dat_files[i].fname);
            PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
            return FALSE;
        }
    }

    /* must zero terminate sprite offsets array */
    sprites[NUM_SPRITES] = 0;

    /* save sprite.dat */
    uint size;
    if ((int)SetFilePointer(s.spritef.handle, 0, 0, FILE_BEGIN) < 0 ||
        !WriteFile(s.spritef.handle, sprites, (NUM_SPRITES + 1) * 4, &size, 0) ||
        size != (NUM_SPRITES + 1) * 4) {
        wsprintf(buf, "Failed to save %s", s.spritef.fname);
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
        return FALSE;
    }

    return TRUE;
}


/* InsertSprite

   spriteNo - number of sprite to replace

   Replaces sprite in sprite buffer. Data is read from bitmap with sprxxxx.bmp
   filename (where xxxx is spriteNo), same format used for saving sprites.
   Transparent color is color 17.

   There are small problems with Photoshop 4 - PS seems to optimize bitmap
   palette by deleting duplicate entries, but it doesn't change pixels with
   value 0x10 (17th palette entry).

   This version can handle bitmap sizes different from original sprite.
*/
bool InsertSprite(uint sprite_no, uint quiet)
{
    char buf[128], buf2[128], line[WIDTH];
    HANDLE hfile;
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;
    RGBQUAD colors[16];
    uint i, j, ok = FALSE, dat_file;
    Sprite *spr;

    if (sprite_no >= NUM_SPRITES)
        return FALSE;

    spr = &s.sprites[sprite_no];
    wsprintf(buf, "spr%04d.bmp", sprite_no);

    WriteToLog(("InsertSprite(): Replacing sprite no. %d", sprite_no));

    hfile = FOpen(buf, FF_READ | FF_SHARE_READ | FF_SEQ_SCAN);
    if (hfile == ERR_HANDLE) {
        if (!quiet) {
            wsprintf(buf2, "%s not found.", buf);
            PrintWarning(buf2, WARNING_INTERVAL, ALIGN_CENTER);
        }
        return FALSE;
    }

    do {
        uint line_width, spr_line_width;
        if (!ReadFile(hfile, &bmfh, sizeof(bmfh), &i, 0) || i != sizeof(bmfh))
            break;
        if (!ReadFile(hfile, &bmih, sizeof(bmih), &i, 0) || i != sizeof(bmih))
            break;
        if (bmfh.bfType != 'MB')
            break;
        if (!ReadFile(hfile, colors, sizeof(colors), &i, 0) ||
            i != sizeof(colors))
            break;

        /* check bitmap dimensions */
        if (bmih.biWidth > WIDTH || bmih.biHeight > HEIGHT) {
            FClose(hfile);
            if (!quiet)
                PrintWarning("Sprite too big", WARNING_INTERVAL, ALIGN_CENTER);
            return FALSE;
        }

        /* number of colors must be 256, and there must be no compression */
        if (bmih.biBitCount != 8 || bmih.biCompression != BI_RGB)
            break;

        /* get container index */
        dat_file = s.sprites[sprite_no].dat_file;

        /* check if bitmap can fit, expand pixels area if needed */
        if (bmih.biHeight != spr->nlines ||
            spr->wquads != (bmih.biWidth + 15) / 16) {
            uint new_width = bmih.biWidth + 15 & ~15;
            uint new_size = new_width * bmih.biHeight / 2;
            uchar *p = omalloc(new_size);
            if (!p) {
                FClose(hfile);
                if (!quiet)
                    PrintWarning("Out of memory", WARNING_INTERVAL, ALIGN_CENTER);
                return FALSE;
            }
            xfree(spr->spr_data);
            spr->spr_data = p;

            /* save old dat file size */
            dat_files[dat_file].old_size = dat_files[dat_file].size;

            /* recalculate new dat file's size */
            dat_files[dat_file].size += new_size - spr->size;

            /* set new wquads */
            spr->wquads = new_width / 16;
        }

        /* set everything to zero to eliminate garbage in padding */
        memset(spr->spr_data, 0, bmih.biHeight * spr->wquads * 8);

        spr->width = (short)bmih.biWidth;
        spr->nlines = (short)bmih.biHeight;
        spr->nlines_div4 = spr->nlines / 4;
        spr->size = spr->wquads * spr->nlines * 8;
        spr->changed = TRUE;

        if (!SetFilePointer(hfile, bmfh.bfOffBits, 0, FILE_BEGIN))
            break;

        line_width = bmih.biWidth + 3 & ~3;
        spr_line_width = spr->wquads * 8;

        for (i = 0; i < (uint)bmih.biHeight; i++) {
            if (!ReadFile(hfile, line, line_width, &j, 0))
                break;
            if (j != line_width)
                break;
            for (j = 0; j < (uint)bmih.biWidth; j+= 2) {
                /* combine 2 pixels to one byte and set transparent color */
                char c1 = line[j] == 16 ? 0 : line[j],
                     c2 = line[j + 1] == 16 ? 0 : line[j + 1];
                line[j / 2] = c2 & 0x0f | c1 << 4;
            }
            memcpy(spr->spr_data + (spr->nlines - i - 1) * spr_line_width, line, bmih.biWidth / 2);
        }
        if (i != (uint)bmih.biHeight)
            break;

        g.changedFlags |= SPRITES_CHANGED;
        ok = dat_files[dat_file].changed = TRUE;

    } while (FALSE);

    FClose(hfile);

    if (quiet)
        return ok;

    if (ok) {
        PrintWarning("Sprite replaced OK", WARNING_INTERVAL, ALIGN_CENTER);
    } else {
        wsprintf(buf2, "Error replacing sprite with %s", buf);
        PrintWarning(buf2, WARNING_INTERVAL, ALIGN_CENTER);
    }

    return ok;
}


/* InsertAllSprites

   Inserts all sprites in directory. Collects bitmap names of type
   sprnnnn.bmp, where nnnn must be a number. InsertSprite() will do range
   check.
*/
void InsertAllSprites()
{
    uint i, sprite_no;
    WIN32_FIND_DATA find_data;
    HANDLE hsearch;
    char num_buf[5];

    WriteToLog(("InsertAllSprites(): Entry"));
    /* this must be done in order to show "please wait..." message */
    SendMessage(g.hWnd, WM_PAINT, 0, 0);

    /* should take care of all illformed filenames (e.g. "spr21RR.bmp") */
    hsearch = FindFirstFile("spr????.bmp", &find_data);
    if (hsearch != INVALID_HANDLE_VALUE) {
        do {
            memcpy(num_buf, find_data.cFileName + 3, 4);
            num_buf[4] = '\0';
            sprite_no = str2int(num_buf);
            for (i = 0; i < 5 && isdigit(num_buf[i]); i++);
            if (i == 4 && sprite_no < NUM_SPRITES)
                InsertSprite(sprite_no, TRUE);
        } while (FindNextFile(hsearch, &find_data));
    }
    PrintWarning("", 10, NO_ALIGNMENT);
}

/* RevertToSaved

   Reverts to last saved state. Reverts only sprites.
*/
void RevertToSaved()
{
    RevertSpritesToSaved();
    g.changedFlags &= ~SPRITES_CHANGED;
}

/* RevertSpritesToSaved

   Reverts sprites to last saved state.
*/
void RevertSpritesToSaved()
{
    int i, j;
    WriteToLog(("RevertSpritesToSaved(): Entry"));
    for (i = 0; i < files_num; i++) {
        if (!dat_files[i].changed)
            continue;
        DatToSprites(s.sprites, i, FALSE);
        for (j = 0; j < (int)dat_files[i].entries; j++)
            ChainSprite(&s.sprites[dat_files[i].start_sprite + j]);
        dat_files[i].changed = FALSE;
    }
    g.changedFlags &= ~SPRITES_CHANGED;
}

void SwapTeam2And3()
{
    int fsize, i;
    byte *file_buffer;
    char buf[64];

    dat_files[3].fname[4] ^= 1;
    HANDLE hfile = FOpen(dat_files[3].fname, FF_READ | FF_WRITE | FF_SHARE_READ | FF_SHARE_WRITE | FF_SEQ_SCAN);
    if (hfile == ERR_HANDLE) {
        wsprintf(buf, "Can't open %s", dat_files[3].fname);
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
        dat_files[3].fname[4] ^= 1;
        return;
    }
    fsize = GetFileSize(hfile, NULL);
    // do not misinterpret this as a memory leak - it's replacing memory allocated at program init time
    if (!(file_buffer = omalloc(fsize))) {
        wsprintf(buf, "Not enough memory to load file %s", dat_files[3].fname);
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
        FClose(hfile);
        dat_files[3].fname[4] ^= 1;
        return;
    }
    if (!ReadFile(hfile, file_buffer, fsize, &i, NULL) || i != fsize) {
        wsprintf(buf, "Can't read file %s", dat_files[3].fname);
        PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
        FClose(hfile);
        xfree(file_buffer);
        dat_files[3].fname[4] ^= 1;
        return;
    }
    xfree(dat_files[3].buffer);
    dat_files[3].buffer = file_buffer;
    DatToSprites(s.sprites, 3, 0);
    for (i = 0; i < (int)dat_files[3].entries; i++)
        ChainSprite(&s.sprites[dat_files[3].start_sprite + i]);
    FClose(hfile);
    wsprintf(buf, "Loaded %s OK", dat_files[3].fname);
    PrintWarning(buf, WARNING_INTERVAL, ALIGN_CENTER);
    dat_files[3].changed = FALSE;
    g.changedFlags &= ~SPRITES_CHANGED;
}

/* Match

   col - color index to convert

   Converts color from menu pal to color in game pal to keep the background
   color consistent at all times.
*/
static uint Match(uint col)
{
    switch (col) {
    case 3:   return 3;
    case 2:   return 2;
    case 1:   return 254;
    case 114: return 213;
    case 66:  return 194;
    case 15:  return 15;
    case 10:  return 10;
    case 11:  return 11;
    case 50:  return 73;
    case 46:  return 71;
    default:
        WriteToLog(("Match(): Got unknown color: %d", col));
        return col;
    }
}

/* GotoSprite

   spr_no - sprite number to go to

   Show specified sprite to user. Returns true if input text needs to be
   cleared.
*/
static bool GotoSprite(int spr_no)
{
    if (spr_no > NUM_SPRITES - 1) {
        PrintWarning("Invalid sprite number", WARNING_INTERVAL, ALIGN_CENTER);
        return FALSE;
    }
    s.sprite_no = spr_no;
    return TRUE;
}

/* in drawspr.asm */
extern void _stdcall _DrawSprite(byte *from, byte *where, uint delta, uint spr_delta, uint width, uint height, int col, int odd);

/* DrawSprite

   x     -  x coordinate
   y     -  y coordinate
   where -> pointer to destination screen
   s     -> sprite to draw
   pitch -  distance between lines
   col   -  sprite color

   Draws sprite normally if col is -1, otherwise colors sprite with col. Does
   clipping, and then transfers control to _DrawSprite, which does actual
   drawing.
*/
void DrawSprite(int x, int y, uchar *where, Sprite *s, uint pitch, int col)
{
    byte *from, *wh;
    uint delta, spr_delta;
    int width, height, odd;

    if (!s)
        return;

    width = s->width;
    height = s->nlines;
    from = s->spr_data;
    odd = 0;

    if (x < 0) {
        width += x;
        from -= x / 2;
        odd = x & 1;
        x = 0;
    } else if (x + width >= WIDTH)
        width = WIDTH - x;
    if (y < 0) {
        height += y;
        from -= y * s->wquads * 8;
        y = 0;
    } else if (y + height >= HEIGHT)
        height = HEIGHT - y;

    if (width <= 0 || height <= 0)
        return;

    spr_delta = (s->wquads * 16 - width) / 2 - (odd & !(width & 1));
    delta = pitch - width;
    wh = where + y * pitch + x;
    _DrawSprite(from, wh, delta, spr_delta, width, height, col, odd);
}

/* DrawSpriteInGame

   Draw sprite centered by using it's center x and y fields, as the game does,
   then dispatch to DrawSprite; caller just needs to know sprite index to show
*/
void DrawSpriteInGame(int x, int y, int sprite_no, uchar *where, uint pitch)
{
    if (sprite_no >= 0 && sprite_no < NUM_SPRITES) {
        Sprite *spr = &s.sprites[sprite_no];
        DrawSprite(x - spr->center_x, y - spr->center_y, where, spr, pitch, -1);
    }
}
