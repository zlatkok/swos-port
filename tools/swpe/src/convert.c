/* convert.c

   Contains routines for conversion of sprite colors into colors of teams that
   are playing.
*/

#include "convert.h"
#include "swspr.h"
#include "debug.h"
#include "menu.h"
#include "alloc.h"
#include "file.h"
#include <assert.h>

extern Sprites_info s;
extern Dat_file dat_files[NUM_DAT_FILES];
extern Sprite **cvt_sprites;

/* shirt types from team files */
enum shirt_types {
    SHIRT_ORDINARY,
    SHIRT_COLORED_SLEEVES,
    SHIRT_VERTICAL_STRIPES,
    SHIRT_HORIZONTAL_STRIPES
};

/* various color conversion tables */
static byte team1_pl_col_table[] = {
    0, 1, 2, 3, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0
};
static byte team2_pl_col_table[] = {
    0, 1, 2, 3, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0
};

/* format: byte - offset in dest convert table to write,
           byte - value to write;
           table is terminated with negative offset
   tables are for setting skin and hair color to sprite depending on face */
static schar const cvt_table_01[] = {6, 6, 5, 5, 4, 4, 13, 3, 9, 3, 12, 3, -1};
static schar const cvt_table_02[] = {6, 6, 5, 5, 4, 4, 13, 6, 9, 5, 12, 4, -1};
static schar const cvt_table_03[] = {6, 4, 5, 4, 4, 3, 13, 3, 9, 3, 12, 3, -1};
static schar const *convert_tables[] = {
    cvt_table_01, cvt_table_02, cvt_table_03
};

static schar goalie_table[] = {0, 0};


static void ConvertSpriteColors(uint spr_no, byte *col_table)
{
    Sprite *spr = cvt_sprites[spr_no];
    byte *p = spr->spr_data;
    uint cnt = spr->nlines * spr->wquads * 8;
    while (cnt--) {
        byte pix = col_table[*p >> 4];
        pix = pix << 4 | col_table[*p & 0x0f];
        *p++ = pix;
    }
}

static void ConvertColorTable(uint index, byte *col_table)
{
    const schar *p;
    assert(index < sizeofarray(convert_tables));
    for (p = convert_tables[index]; *p >= 0; p += 2)
        col_table[*p] = p[1];
}

static void SetGoalieTable(Team_game *t, schar *table)
{
    int i, index;
    Player_game *p = (Player_game*)(t + 1);
    table[0] = table[1] = -1;
    for (i = index = 0; i < 16; i++, p++) {
        if ((schar)p->position < 0)
            continue;
        if (p->face == table[0])
            continue;
        if (p->position && p->face == table[1])
            continue;
        table[index] = p->face;
        if (i && p->position)
            continue;
        if (++index >= 2)
            return;
    }
}

static bool GetTeamMemory(byte **p, uint size)
{
    if (!(*p = omalloc(size))) {
        SetWarning("Not enough memory to load teams");
        return FALSE;
    }
    return TRUE;
}

static bool CopySprites(uint start_sprite, uint num_sprites, byte **where, uint index)
{
    uint i, size;
    byte *base, *p;
    for (i = size = 0; i < num_sprites; i++)
        size += sizeof(Sprite) + s.sprites[i + start_sprite].size;
    if (!GetTeamMemory(&base, size))
        return FALSE;
    for (p = base, i = 0; i < num_sprites; i++) {
        Sprite *spr = &s.sprites[start_sprite + i];
        cvt_sprites[index++] = (Sprite*)p;
        memcpy(p, spr, sizeof(Sprite));
        ((Sprite*)p)->spr_data = p + sizeof(Sprite);
        memcpy(p += sizeof(Sprite), spr->spr_data, spr->size);
        p += spr->size;
    }
    *where = base;
    return TRUE;
}

static bool LoadTeamFile(char *fname, byte **buf)
{
    uint size, num_read;
    HANDLE hfile;
    WriteToLog(("LoadTeamFile(): Loading team file `%s' from disk...", fname));
    hfile = FOpen(fname, FF_READ | FF_SEQ_SCAN | FF_SHARE_READ);
    if (hfile == ERR_HANDLE) {
        char buf[128];
        wsprintf(buf, "Could not load %s", fname);
        SetWarning(buf);
        return FALSE;
    }
    size = GetFileSize(hfile, NULL);
    if (!GetTeamMemory(buf, size)) {
        SetWarning("Not enough memory to load file");
        FClose(hfile);
        return FALSE;
    }
    if (!ReadFile(hfile, *buf, size, &num_read, NULL) || num_read != size) {
        SetWarning("Error reading team file");
        xfree(*buf);
        FClose(hfile);
        return FALSE;
    }
    FClose(hfile);
    return TRUE;
}

static int players[] = {0, 303};

static bool SetShirtType(uint type1, uint type2)
{
    int i, j;
    byte *team_bufs[2], *goalie_bufs[2];
    static char type2dat[] = {'1', '2', '1', '3'};
    static char dat_file[] = "team4.dat";
    uchar types[2] = {type1, type2};
    extern void _stdcall ChainSprite(Sprite *spr);

    for (i = 0; i < 2; i++) {
        char dat_no = types[i] > 3 ? types[i] + '0' : type2dat[types[i]];
        WriteToLog(("SetShirtType(): Using team%c.dat", dat_no));
        if (dat_no == '1') {
            if (!CopySprites(341, 303, &team_bufs[i], players[i])) {
                if (i > 0)
                    xfree(team_bufs[i - 1]);
                return FALSE;
            }
        } else if ((dat_no == '2' || dat_no == '3') &&
            dat_files[3].fname[4] == dat_no) {
            if (!CopySprites(644, 303, &team_bufs[i], players[i])) {
                if (i > 0)
                    xfree(team_bufs[i - 1]);
                return FALSE;
            }
        } else {
            uint index;
            byte *p;
            dat_file[4] = dat_no;
            if (!LoadTeamFile(dat_file, &team_bufs[i]))
                return FALSE;
            p = team_bufs[i];
            for (index = players[i], j = 0; j < 303; j++) {
                Sprite *spr = (Sprite*)p;
                spr->spr_data = p + sizeof(Sprite);
                spr->size = spr->wquads * 8 * spr->nlines;
                p += sizeof(Sprite) + spr->size;
                ChainSprite(spr);
                cvt_sprites[index++] = spr;
            }
        }
    }
    /* do the goalkeepers */
    if (CopySprites(947, 116, &goalie_bufs[0], 303 + 303)) {
        if (CopySprites(1063, 116, &goalie_bufs[1], 303 + 303 + 116))
            return TRUE;
        xfree(goalie_bufs[0]);
    }
    xfree(team_bufs[0]);
    xfree(team_bufs[1]);
    return FALSE;
}


bool SetSpriteTeamColors(Team_game *team1, Team_game *team2)
{
    Team_game * const teams[] = {team1, team2, NULL}, *team;
    int i, j;
    static const int goalies[] = {606, 722};
    static byte *const col_tables[] = {team1_pl_col_table, team2_pl_col_table};
    int col1, col2;

    if (!SetShirtType(team1->pr_shirt_type, team2->pr_shirt_type))
        return FALSE;
    for (i = 0; team = teams[i]; i++) {
        SetGoalieTable(team, goalie_table);
        if (team->pr_shirt_type != SHIRT_VERTICAL_STRIPES) {
            col1 = team->pr_shirt_col;
            col2 = team->pr_stripes_col;
        } else {
            col1 = team->pr_stripes_col;
            col2 = team->pr_shirt_col;
        }
        col_tables[i][11] = col1;
        col_tables[i][10] = col2;
        col_tables[i][14] = (byte)team->pr_shorts_col;
        col_tables[i][15] = (byte)team->pr_socks_col;
        ConvertColorTable(0, col_tables[i]);
        for (j = players[i]; j < players[i] + 101; j++)
            ConvertSpriteColors(j, col_tables[i]);
        ConvertColorTable(1, col_tables[i]);
        for (; j < players[i] + 202; j++)
            ConvertSpriteColors(j, col_tables[i]);
        ConvertColorTable(2, col_tables[i]);
        for (; j < players[i] + 303; j++)
            ConvertSpriteColors(j, col_tables[i]);
        ConvertColorTable(goalie_table[0], col_tables[i]);
        for (j = goalies[i]; j < goalies[i] + 58; j++)
            ConvertSpriteColors(j, col_tables[i]);
        if (goalie_table[1] >= 0) {
            ConvertColorTable(goalie_table[1], col_tables[i]);
            for (; j < goalies[i] + 116; j++)
                ConvertSpriteColors(j, col_tables[i]);
        }
    }
    return TRUE;
}

void CleanupTeamSprites()
{
    xfree(cvt_sprites[0]);
    xfree(cvt_sprites[303]);
    xfree(cvt_sprites[303 + 303]);
    xfree(cvt_sprites[303 + 303 + 116]);
}