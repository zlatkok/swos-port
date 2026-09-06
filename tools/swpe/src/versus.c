/* versus.c

   Versus mode, where player attempts to beat a band of football bullies, who
   terrorize his home village.
*/

#include <assert.h>
#include "util.h"
#include "pattern.h"
#include "pitch.h"
#include "swspr.h"
#include "versus.h"
#include "draw.h"
#include "printstr.h"
#include "util.h"

/* help for versus mode */
static char versus_help[] = {
    "HELP NOT WRITTEN YET"
};

static bool InitVersusMode();
static void VersusModeSwitch(uint old_mode);
static bool VersusKeyProc(uint code, uint data);
static byte *VersusDraw(byte *pbits, uint pitch);
static void GameLoop();

/* versus mode "object" */
const Mode VersusMode = {
    InitVersusMode,
    VersusModeSwitch,
    FinishVersusMode,
    VersusKeyProc,
    nullptr,
    VersusDraw,
    PatternsGetPalette,
    versus_help,
    "TEST MODE",
    VK_F11,
    0
};

static int spriteStandingFacingUp[] =           { 341, -999 };
static int spriteStandingFacingUpRight[] =      { 363, -999 };
static int spriteStandingFacingRight[] =        { 347, -999 };
static int spriteStandingFacingBottomRight[] =  { 357, -999 };
static int spriteStandingFacingBottom[] =       { 344, -999 };
static int spriteStandingFacingBottomLeft[] =   { 354, -999 };
static int spriteStandingFacingLeft[] =         { 350, -999 };
static int spriteStandingFacingTopLeft[] =      { 360, -999 };

static AnimationTable playerStandingAnimTable = {
    5,
    spriteStandingFacingUp,
    spriteStandingFacingUpRight,
    spriteStandingFacingRight,
    spriteStandingFacingBottomRight,
    spriteStandingFacingBottom,
    spriteStandingFacingBottomLeft,
    spriteStandingFacingLeft,
    spriteStandingFacingTopLeft
};

static int playerRunningTop[] =         { 343, 341, 342, 341, -999 };
static int playerRunningTopRight[] =    { 364, 363, 362, 363, -999 };
static int playerRunningRight[] =       { 349, 347, 348, 347, -999 };
static int playerRunningBottomRight[] = { 358, 357, 356, 357, -999 };
static int playerRunningBottom[] =      { 346, 344, 345, 344, -999 };
static int playerRunningBottomLeft[] =  { 355, 354, 353, 354, -999 };
static int playerRunningLeft[] =        { 352, 350, 351, 350, -999 };
static int playerRunningTopLeft[] =     { 361, 360, 359, 360, -999 };

static AnimationTable playerRunningAnimTable = {
    5,
    playerRunningTop,
    playerRunningTopRight,
    playerRunningRight,
    playerRunningBottomRight,
    playerRunningBottom,
    playerRunningBottomLeft,
    playerRunningLeft,
    playerRunningTopLeft
};

static PlayerSkill mainPlayerSkill = { 8, 10, 9, 7, 10, 11, 12 };
static Player mainPlayer;

static bool InitVersusMode()
{
    mainPlayer.x = ((PITCH_W - WIDTH) / 2 + WIDTH / 2) << 16;
    mainPlayer.destX = mainPlayer.x >> 16;
    mainPlayer.y = ((PITCH_H - HEIGHT) / 2 + HEIGHT / 2 - 15) << 16;
    mainPlayer.destY = mainPlayer.y >> 16;
    mainPlayer.animationTable = &playerStandingAnimTable;
    mainPlayer.playerSkill = &mainPlayerSkill;
    mainPlayer.pictureIndex = 344;
    mainPlayer.direction = 4;
    return TRUE;
}

static void VersusModeSwitch(uint old_mode)
{
    GameLoop();
    RegisterSWOSProc(GameLoop);
}

void FinishVersusMode()
{
    RegisterSWOSProc(NULL);
}

static bool VersusKeyProc(uint code, uint data)
{
    return FALSE;
}

static byte *RenderFrame(byte *pbits, uint pitch);

static byte *VersusDraw(byte *pbits, uint pitch)
{
    return RenderFrame(pbits, pitch);
}

static int currentCameraX;
static int currentCameraY;

static void MoveCamera()
{
    currentCameraX = (PITCH_W - WIDTH) / 2;
    currentCameraY = (PITCH_H - HEIGHT) / 2;
    /*currentCameraX = (short)(mainPlayer.x >> 16) - WIDTH / 2;
    currentCameraY = (short)(mainPlayer.y >> 16) - HEIGHT / 2;*/
}

static void SetPlayerDestination(Player *player, int direction)
{
    static const int defaultDestOffsets[8][2] = {
        0, -1000, 1000, -1000, 1000, 0, 1000, 1000, 0, 1000, -1000, 1000, -1000, 0, -1000, -1000
    };
    int flags = 0;
    int x = (short)(player->x >> 16), y = (short)(player->y >> 16);
    /* make sure we don't run out of allowed playground area */
    if (x < 79)
        flags |= 0xe0;
    if (x > 592)
        flags |= 0x0e;
    if (y < 111)
        flags |= 0x83;
    if (y > 755)
        flags |= 0x38;

    if (direction >= 0 && direction <= 7) {
        player->direction = direction;
        if (!(1 << direction & flags)) {
            player->destX = (x + defaultDestOffsets[direction][0]) & 0xffff;
            player->destY = (y + defaultDestOffsets[direction][1]) & 0xffff;
            return;
        }
    }
    /* trying to run out or not moving, stop the player */
    player->destX = x;
    player->destY = y;
}

static void UpdatePlayerSpeed(Player *player)
{
    const int MAX_SPEED = 1280;
    static const int playerSpeedsGameInProgress[] = { 928, 974, 1020, 1066, 1112, 1158, 1204, 1250 };
    player->speed = playerSpeedsGameInProgress[min(max(player->playerSkill->speed / 2, 0), sizeofarray(playerSpeedsGameInProgress) - 1)];
    /* so the slower players would get greater delay between frames */
    player->frameDelay = max(0, MAX_SPEED - player->speed) / 128 + 6; //always 0? fix
}

static void CalculateDeltaXAndY(Player *player)
{
    /* approximately 32767 * sin (90 * x / 64) in degrees, -128..127 range when high byte is discarded */
    static const int sineCosineTable[256] = {
        0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793, 12539,
        13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18867, 19519, 20159, 20787, 21402, 22005, 22594,
        23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621,
        29956, 30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678,
        32728, 32757, 32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356,
        31113, 30852, 30571, 30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832,
        25329, 24812, 24279, 23731, 23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846,
        16151, 15446, 14732, 14010, 13279, 12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808,
        4011, 3212, 2411, 1608, 804, 0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962,
        -8739, -9512, -10278, -11039, -11793, -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
        -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594, -23170, -23731, -24279, -24812, -25329,
        -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956, -30273, -30571,
        -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728,
        -32757, -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580,
        -31356, -31113, -30852, -30571, -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245,
        -26790, -26319, -25832, -25329, -24811, -24279, -23731, -23170, -22594, -22005, -21402, -20787, -20159,
        -19519, -18867, -18204, -17530, -16845, -16151, -15446, -14732, -14009, -13278, -12539, -11792, -11039,
        -10278, -9512, -8739, -7961, -7179, -6392, -5602, -4808, -4011, -3211, -2410, -1608, -804
    };
    /*
        divides 90 degrees into positions 0..64 (1st quadrant)
        x - horizontal axis delta
        y - vertical axis delta
        for x >= y, table[y][x] = 32 * y / x
        for x < y,  table[y][x] = 64 - table[x][y]
    */
    static const int angleCoeficients[32][32] = {
        -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 32, 16, 11, 8, 6, 5, 5, 4, 4, 3, 3, 3,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 64, 48, 32,
        21, 16, 13, 11, 9, 8, 7, 6, 6, 5, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3,
        3, 2, 2, 2, 2, 2, 2, 64, 53, 43, 32, 24, 19, 16, 14, 12, 11, 10, 9,
        8, 7, 7, 6, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 64, 56,
        48, 40, 32, 26, 21, 18, 16, 14, 13, 12, 11, 10, 9, 9, 8, 8, 7, 7, 6,
        6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 4, 64, 58, 51, 45, 38, 32, 27, 23, 20,
        18, 16, 15, 13, 12, 11, 11, 10, 9, 9, 8, 8, 8, 7, 7, 7, 6, 6, 6, 6,
        6, 5, 5, 64, 59, 53, 48, 43, 37, 32, 27, 24, 21, 19, 17, 16, 15, 14,
        13, 12, 11, 11, 10, 10, 9, 9, 8, 8, 8, 7, 7, 7, 7, 6, 6, 64, 59, 55,
        50, 46, 41, 37, 32, 28, 25, 22, 20, 19, 17, 16, 15, 14, 13, 12, 12,
        11, 11, 10, 10, 9, 9, 9, 8, 8, 8, 7, 7, 64, 60, 56, 52, 48, 44, 40,
        36, 32, 28, 26, 23, 21, 20, 18, 17, 16, 15, 14, 13, 13, 12, 12, 11,
        11, 10, 10, 9, 9, 9, 9, 8, 64, 60, 57, 53, 50, 46, 43, 39, 36, 32, 29,
        26, 24, 22, 21, 19, 18, 17, 16, 15, 14, 14, 13, 13, 12, 12, 11, 11,
        10, 10, 10, 9, 64, 61, 58, 54, 51, 48, 45, 42, 38, 35, 32, 29, 27, 25,
        23, 21, 20, 19, 18, 17, 16, 15, 15, 14, 13, 13, 12, 12, 11, 11, 11,
        10, 64, 61, 58, 55, 52, 49, 47, 44, 41, 38, 35, 32, 29, 27, 25, 23,
        22, 21, 20, 19, 18, 17, 16, 15, 15, 14, 14, 13, 13, 12, 12, 11, 64,
        61, 59, 56, 53, 51, 48, 45, 43, 40, 37, 35, 32, 30, 27, 26, 24, 23,
        21, 20, 19, 18, 17, 17, 16, 15, 15, 14, 14, 13, 13, 12, 64, 62, 59,
        57, 54, 52, 49, 47, 44, 42, 39, 37, 34, 32, 30, 28, 26, 24, 23, 22,
        21, 20, 19, 18, 17, 17, 16, 15, 15, 14, 14, 13, 64, 62, 59, 57, 55,
        53, 50, 48, 46, 43, 41, 39, 37, 34, 32, 30, 28, 26, 25, 24, 22, 21,
        20, 19, 19, 18, 17, 17, 16, 15, 15, 14, 64, 62, 60, 58, 55, 53, 51,
        49, 47, 45, 43, 41, 38, 36, 34, 32, 30, 28, 27, 25, 24, 23, 22, 21,
        20, 19, 18, 18, 17, 17, 16, 15, 64, 62, 60, 58, 56, 54, 52, 50, 48,
        46, 44, 42, 40, 38, 36, 34, 32, 30, 28, 27, 26, 24, 23, 22, 21, 20,
        20, 19, 18, 18, 17, 17, 64, 62, 60, 58, 56, 55, 53, 51, 49, 47, 45,
        43, 41, 40, 38, 36, 34, 32, 30, 29, 27, 26, 25, 24, 23, 22, 21, 20,
        19, 19, 18, 18, 64, 62, 60, 59, 57, 55, 53, 52, 50, 48, 46, 44, 43,
        41, 39, 37, 36, 34, 32, 30, 29, 27, 26, 25, 24, 23, 22, 21, 21, 20,
        19, 19, 64, 62, 61, 59, 57, 56, 54, 52, 51, 49, 47, 45, 44, 42, 40,
        39, 37, 35, 34, 32, 30, 29, 28, 26, 25, 24, 23, 23, 22, 21, 20, 20,
        64, 62, 61, 59, 58, 56, 54, 53, 51, 50, 48, 46, 45, 43, 42, 40, 38,
        37, 35, 34, 32, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 21, 64, 62,
        61, 59, 58, 56, 55, 53, 52, 50, 49, 47, 46, 44, 43, 41, 40, 38, 37,
        35, 34, 32, 31, 29, 28, 27, 26, 25, 24, 23, 22, 22, 64, 63, 61, 60,
        58, 57, 55, 54, 52, 51, 49, 48, 47, 45, 44, 42, 41, 39, 38, 36, 35,
        33, 32, 31, 29, 28, 27, 26, 25, 24, 23, 23, 64, 63, 61, 60, 58, 57,
        56, 54, 53, 51, 50, 49, 47, 46, 45, 43, 42, 40, 39, 38, 36, 35, 33,
        32, 31, 29, 28, 27, 26, 25, 25, 24, 64, 63, 61, 60, 59, 57, 56, 55,
        53, 52, 51, 49, 48, 47, 45, 44, 43, 41, 40, 39, 37, 36, 35, 33, 32,
        31, 30, 28, 27, 26, 26, 25, 64, 63, 61, 60, 59, 58, 56, 55, 54, 52,
        51, 50, 49, 47, 46, 45, 44, 42, 41, 40, 38, 37, 36, 35, 33, 32, 31,
        30, 29, 28, 27, 26, 64, 63, 62, 60, 59, 58, 57, 55, 54, 53, 52, 50,
        49, 48, 47, 46, 44, 43, 42, 41, 39, 38, 37, 36, 34, 33, 32, 31, 30,
        29, 28, 27, 64, 63, 62, 60, 59, 58, 57, 56, 55, 53, 52, 51, 50, 49,
        47, 46, 45, 44, 43, 41, 40, 39, 38, 37, 36, 34, 33, 32, 31, 30, 29,
        28, 64, 63, 62, 61, 59, 58, 57, 56, 55, 54, 53, 51, 50, 49, 48, 47,
        46, 45, 43, 42, 41, 40, 39, 38, 37, 35, 34, 33, 32, 31, 30, 29, 64,
        63, 62, 61, 60, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 47, 46, 45,
        44, 43, 42, 41, 40, 39, 38, 36, 35, 34, 33, 32, 31, 30, 64, 63, 62,
        61, 60, 59, 58, 57, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44,
        43, 42, 41, 39, 38, 37, 36, 35, 34, 33, 32, 31, 64, 63, 62, 61, 60,
        59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 47, 46, 45, 44, 43, 42,
        41, 40, 39, 38, 37, 36, 35, 34, 33, 32
    };

    int angle;
    int x = (short)(player->x >> 16);
    int y = (short)(player->y >> 16);
    int deltaX = (short)(player->destX & 0xffff) - x;
    int deltaY = (short)(player->destY & 0xffff) - y;
    int goingLeft = FALSE;
    int goingUp = FALSE;
    if (deltaX < 0) {
        deltaX = -deltaX;
        goingLeft = TRUE;
    }
    if (deltaY < 0) {
        deltaY = -deltaY;
        goingUp = TRUE;
    }
    /* bring it in range of our table */
    while (deltaX >= 32) {
        deltaX /= 2;
        deltaY /= 2;
    }
    while (deltaY >= 32) {
        deltaX /= 2;
        deltaY /= 2;
    }
    player->wasMoving = player->isMoving;
    /* get an angle in 1st quadrant discretized to 0..64 (0 = 0 degrees, 32 = 45 degrees, 64 = 90 degrees) */
    angle = angleCoeficients[deltaY][deltaX];
    if (angle < 0) {
        player->angle = -1;
        player->deltaX = 0;
        player->deltaY = 0;
        player->isMoving = FALSE;
    } else {
        /* now map the angle to its rightfull position in full circle
           0..256 range, goes counter-clockwise, 0 = facing bottom */
        if (goingLeft)
            angle = goingUp ? 192 - angle : 192 + angle;
        else
            angle = goingUp ? angle + 64 : 64 - angle;
        /* 41 / 64 is some magic constant, to slow things down perhaps (it's equal to 205 / 320) */
        player->deltaX = sineCosineTable[angle & 255] / 256 * player->speed * 41 / 64;
        player->deltaY = sineCosineTable[angle + 64 & 255] / 256 * player->speed * 41 / 64;
        player->angle = (256 - (angle & 255) + 128) & 255;
        player->isMoving = TRUE;
    }
}

static void SetAnimationTable(Player *player)
{
    int direction = player->direction >= 0 && player->direction <= 7 ? player->direction : 4;

    AnimationTable *animTable = player->animationTable;
    if (player->wasMoving && !player->isMoving)
        animTable = &playerStandingAnimTable;
    else if (!player->wasMoving && player->isMoving)
        animTable = &playerRunningAnimTable;

    player->frameIndicesTable = animTable->frameTables[direction];
    assert(player->frameIndicesTable);
    /* unlike SWOS we are calling this every frame, so it's crucial not to
       reset the table variables if animation table remains unchanged */
    if (player->animationTable != animTable) {
        player->animationTable = animTable;
        player->frameDelay = animTable->frameDelay;
        player->delayedFramesTimer = -1;
        player->frameIndex = -1;
        player->cycleFramesTimer = 1;
    }
    /* ignoring starting_direction for now */
}

static void SetNextFrame(Player *player)
{
    /* we're ignoring been_drawn flag for now */
    if (!--player->cycleFramesTimer) {
        player->frameIndex++;
        player->cycleFramesTimer = player->frameDelay;
        while (TRUE) {
            int frame = player->frameIndicesTable[player->frameIndex];
            if (frame >= 0)
                break;
            else if (frame == -999)
                player->frameIndex = 0;
            else if (frame == -101) {
                player->frameIndex--;
                return;
            } else if (frame <= -100)
                player->frameIndex += frame + 100;
            else {
                player->frameDelay = -frame;
                player->cycleFramesTimer = -frame;
                player->frameIndex++;
            }
        }
        player->delayedFramesTimer++;
        player->pictureIndex = player->frameOffset + player->frameIndicesTable[player->frameIndex];
    }
}

static void MovePlayer(Player *player)
{
    /* ignoring been_drawn flag for now */
    /* also ignoring animation table setting at this place */
    int x = (short)((player->x += player->deltaX) >> 16);
    int y = (short)((player->y += player->deltaY) >> 16);
    int destX = (short)(player->destX & 0xffff);
    int destY = (short)(player->destY & 0xffff);
    if (player->deltaX > 0 ? x >= destX : x <= destX) {
        player->x = (player->destX & 0xffff) << 16;
        player->deltaX = 0;
    }
    if (player->deltaY > 0 ? y > destY : y <= destY) {
        player->y = (player->destY & 0xffff) << 16;
        player->deltaY = 0;
    }
}

static void DrawBackground(byte *pbits, uint pitch)
{
    SetPitchPalette(FROZEN);
    DrawPitch((char *)pbits, currentCameraX, currentCameraY, pitch, FROZEN, FALSE, FALSE);
}

static Player *players[1];
static int numPlayers = 1;

static void DrawSprites(byte *pbits, uint pitch)
{
    int i;
    players[0] = &mainPlayer; /* <------- */
    for (i = 0; i < numPlayers; i++) {
        if (players[i]->pictureIndex >= 0) {
            Player *pl = players[i];
            int x = (short)(pl->x >> 16);
            int y = (short)(pl->y >> 16);
            int z = (short)(pl->z >> 16);
            /* this func will take care of sprite centering */
            DrawSpriteInGame(x - currentCameraX, y - currentCameraY - z, pl->pictureIndex, pbits, pitch);
            /* ignore highlights and setting clipped flag for now */
        }
    }
}

byte *RenderFrame(byte *pbits, uint pitch)
{
    char buf[64];
    DrawBackground(pbits, pitch);
    DrawSprites(pbits, pitch);
    PrintString(int2str(mainPlayer.direction, buf), 0, 0, pbits, pitch, FALSE, -1, ALIGN_UPLEFT);
    return pbits;
}

static void UpdateFrame(int currentDirection)
{
    MoveCamera();
    SetPlayerDestination(&mainPlayer, currentDirection);
    CalculateDeltaXAndY(&mainPlayer);
    SetAnimationTable(&mainPlayer);
    UpdatePlayerSpeed(&mainPlayer);
    SetNextFrame(&mainPlayer);
    MovePlayer(&mainPlayer);
}

static void GameLoop()
{
    unsigned char keys[256];
    int direction = -1, directionFlags;
    int up, down, left, right, fire;
    static int lastFrameFire;
    static __int64 lastFireTime;

    GetKeyboardState((PBYTE)keys);

    up = (keys[VK_UP] & 0x80) >> 7;
    down = (keys[VK_DOWN] & 0x80) >> 7;
    left = (keys[VK_LEFT] & 0x80) >> 7;
    right = (keys[VK_RIGHT] & 0x80) >> 7;
    fire = (keys[VK_CONTROL] & 0x80) >> 7;

    if (fire && !lastFrameFire) {
        QueryPerformanceCounter((LARGE_INTEGER*)lastFireTime);
        lastFrameFire = TRUE;
    }

    directionFlags = 1 * up + 2 * right + 4 * down + 8 * left;
    switch (directionFlags) {
    case 1:
    case 5:
    case 15:
        direction = 0;
        break;
    case 3:
    case 7:
        direction = 1;
        break;
    case 2:
        direction = 2;
        break;
    case 6:
    case 14:
        direction = 3;
        break;
    case 4:
        direction = 4;
        break;
    case 12:
        direction = 5;
        break;
    case 8:
    case 10:
        direction = 6;
        break;
    case 9:
    case 13:
    case 11:
        direction = 7;
        break;
    default:
        direction = -1;
        break;
    }

    UpdateFrame(direction);
    UpdateScreen();
}
