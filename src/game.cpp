#include "render.h"
#include "audio.h"
#include "log.h"
#include "controls.h"
#include "menu.h"
#include "util.h"
#include "replayExit.mnu.h"

extern "C" {
    namespace SWOS {
        int CVer_cseg_859CB();
    }
}

uint8_t dseg_17EF68[] = { 3, 6, 12, 129, 192, 96, 0, 0 };
int16_t dseg_17EF4C[] = { -32, 32 };
int16_t dseg_17EF5C[] = { 0, -999, 4 };
int16_t dseg_17EF50[] = { -1, -2, -3 };
int16_t dseg_17EF56[] = { 1, 2, 3 };

int16_t kAngleCoeficients[] = {
    -1, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,    0, 0, 64, 32, 16, 11, 8, 6, 5, 5, 4, 4, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2,    2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 64, 48, 32,
    21, 16, 13, 11, 9, 8, 7, 6, 6, 5, 5,    5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3,
    3, 2, 2, 2, 2, 2, 2,    64, 53, 43, 32, 24, 19, 16, 14, 12, 11, 10, 9 ,
    8, 7, 7, 6, 6, 6, 5,    5, 5, 5, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 64, 56,
    48, 40, 32, 26, 21, 18, 16, 14, 13, 12, 11, 10, 9, 9, 8, 8, 7, 7, 6,
    6, 6, 6, 5, 5, 5, 5,    5, 4, 4, 4, 64, 58, 51, 45, 38, 32, 27, 23, 20,
    18, 16, 15, 13, 12, 11, 11, 10, 9, 9, 8, 8, 8, 7, 7,    7, 6, 6, 6, 6,
    6, 5, 5, 64,    59, 53, 48, 43, 37, 32, 27, 24, 21, 19, 17, 16, 15, 14,
    13, 12, 11, 11, 10, 10, 9, 9, 8, 8, 8, 7, 7,    7, 7, 6, 6, 64, 59, 55,
    50, 46, 41, 37, 32, 28, 25, 22, 20, 19, 17, 16, 15, 14, 13, 12, 12,
    11, 11, 10, 10, 9, 9, 9, 8, 8, 8, 7,    7, 64, 60, 56, 52, 48, 44, 40,
    36, 32, 28, 26, 23, 21, 20, 18, 17, 16, 15, 14, 13, 13, 12, 12, 11,
    11, 10, 10, 9, 9, 9,    9, 8, 64, 60, 57, 53, 50, 46, 43, 39, 36, 32, 29,
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

int16_t kSineCosineTable[] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739,
    9512, 10278,    11039, 11793, 12539, 13279, 14010, 14732, 15446, 16151,
    16846, 17530, 18204,    18867, 19519, 20159, 20787, 21402, 22005, 22594,
    23170, 23731, 24279,    24811, 25329, 25832, 26319, 26790, 27245, 27683,
    28105, 28510, 28898,    29268, 29621, 29956, 30273, 30571, 30852, 31113,
    31356, 31580, 31785,    31971, 32137, 32285, 32412, 32521, 32609, 32678,
    32728, 32757, 32767,    32757, 32728, 32678, 32609, 32521, 32412, 32285,
    32137, 31971, 31785,    31580, 31356, 31113, 30852, 30571, 30273, 29956,
    29621, 29268, 28898,    28510, 28105, 27683, 27245, 26790, 26319, 25832,
    25329, 24812, 24279,    23731, 23170, 22594, 22005, 21403, 20787, 20159,
    19519, 18868, 18204,    17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039,    10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808,
    4011, 3212, 2411, 1608, 804,    0, -804, -1608, -2410, -3212, -4011, -4808,
    -5602, -6393, -7179,    -7962, -8739, -9512, -10278, -11039, -11793, -12539,
    -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868,
    -19519, -20159, -20787, -21403, -22005, -22594, -23170, -23731, -24279,
    -24812, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510,
    -28898, -29268, -29621, -29956, -30273, -30571, -30852, -31113, -31356,
    -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678,
    -32728, -32757, -32767, -32757, -32728, -32678, -32609, -32521, -32412,
    -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245,
    -26790, -26319, -25832, -25329, -24811, -24279, -23731, -23170, -22594,
    -22005, -21402, -20787, -20159, -19519, -18867, -18204, -17530, -16845,
    -16151, -15446, -14732, -14009, -13278, -12539, -11792, -11039, -10278,
    -9512, -8739, -7961,    -7179, -6392, -5602, -4808, -4011, -3211, -2410,
    -1608, -804
};
enum GameStates {
    ST_PLAYERS_TO_INITIAL_POSITIONS = 0,
    ST_GOAL_OUT_LEFT = 1,
    ST_GOAL_OUT_RIGHT = 2,
    ST_KEEPER_HOLDS_BALL = 3,
    ST_CORNER_LEFT = 4,
    ST_CORNER_RIGHT = 5,
    ST_FREE_KICK_LEFT1 = 6,
    ST_FREE_KICK_LEFT2 = 7,
    ST_FREE_KICK_LEFT3 = 8,
    ST_FREE_KICK_CENTER = 9,
    ST_FREE_KICK_RIGHT1 = 10,
    ST_FREE_KICK_RIGHT2 = 11,
    ST_FREE_KICK_RIGHT3 = 12,
    ST_FOUL = 13,
    ST_PENALTY = 14,
    ST_THROW_IN_FORWARD_RIGHT = 15,
    ST_THROW_IN_CENTER_RIGHT = 16,
    ST_THROW_IN_BACK_RIGHT = 17,
    ST_THROW_IN_FORWARD_LEFT = 18,
    ST_THROW_IN_CENTER_LEFT = 19,
    ST_THROW_IN_BACK_LEFT = 20,
    ST_STARTING_GAME = 21,
    ST_CAMERA_GOING_TO_SHOWERS = 22,
    ST_GOING_TO_HALFTIME = 23,
    ST_PLAYERS_GOING_TO_SHOWER = 24,
    ST_RESULT_ON_HALFTIME = 25,
    ST_RESULT_AFTER_THE_GAME = 26,
    ST_FIRST_EXTRA_STARTING = 27,
    ST_FIRST_EXTRA_ENDED = 28,
    ST_FIRST_HALF_ENDED = 29,
    ST_GAME_ENDED = 30,
    ST_PENALTIES = 31,
    ST_GAME_IN_PROGRESS = 100,
    ST_GAME_NOT_RUNNING = 101,
    ST_WAITING_ON_PLAYER = 102
};

void SWOS::InitGame_OnEnter()
{
    // soon after this SWOS will load game palette, trashing old screen contents in the process
    // unlike the original game, which only changes palette register on the VGA card, we need those bytes
    // to persist in order to fade-out properly; so save it here so we can restore them later
    logInfo("Initializing the game...");
    memcpy(linAdr384k + 2 * kVgaScreenSize, linAdr384k, kVgaScreenSize);
}

__declspec(naked) void SWOS::ClearBackground_OnEnter()
{
    // fix SWOS bug, just in case
    _asm {
        xor edx, edx
        retn
    }
}

static void highlightsPausedLoop()
{
    while (paused) {
        SWOS::GetKey();
        MainKeysCheck();
        SWOS::PlayEnqueuedSamples();
    }
}

static bool highlightsAborted()
{
    if (!skipFade)
        return false;

    SWOS::Player1StatusProc();
    if (!pl1Fire) {
        SWOS::Player2StatusProc();
        if (!pl2Fire)
            return false;
    }

    statsFireExit = 1;
    replayState = 0;
    paused = 0;
    SAFE_INVOKE(FadeOutToBlack);
    RestoreCameraPosition();

    return true;
}

static void highlightsMoveToNextScene()
{
    goalBasePtr = reinterpret_cast<dword *>(hilStartOfGoalsBuffer + hilSceneNumber * 19'000);
    currentHilPtr = goalBasePtr - 1;
    nextGoalPtr = reinterpret_cast<dword *>(reinterpret_cast<char *>(goalBasePtr) + 19'000);

    FindStartingDword();
}

static void fadeIfNeeded()
{
    if (!skipFade) {
        FadeIn();
        skipFade = -1;
    }
}

// take over highlights rendering to avoid clearing background last frame before fade, causing players to disappear
void SWOS::PlayHighlightsLoop()
{
    while (true) {
        ReadTimerDelta();

        highlightsPausedLoop();

        GetKey();
        MainKeysCheck();

        if (replayState) {
            if (highlightsAborted())
                break;

            SAFE_INVOKE(ClearBackground);
            RenderReplayFrame();

            if (replayState) {
                Flip();
                fadeIfNeeded();
                continue;
            } else {
                SAFE_INVOKE(FadeOutToBlack);
                RestoreCameraPosition();
                paused = 0;
            }
        }

        if (++hilSceneNumber == numReplaysInHilFile)
            break;

        highlightsMoveToNextScene();
    }

    RestoreGoalPointers();
    paused = 0;
}

dword *validateHilPointerAdd(dword *ptr)
{
    if (ptr == nextGoalPtr)
        ptr = goalBasePtr;

    return ptr;
}

dword *validateHilPointerSub(dword *ptr)
{
    if (ptr < goalBasePtr)
        ptr = nextGoalPtr;

    return ptr;
}

void drawSprite16Pixels(int spriteIndex, int x, int y, bool saveSpritePixelsFlag = true)
{
    int skipSavingSpritePixels = saveSpritePixelsFlag ? 0 : -1;

    D0 = spriteIndex;
    D1 = x;
    D2 = y;

    _asm {
        push ebx
        push esi
        push edi
        push ebp

        mov  ebp, skipSavingSpritePixels
        call DrawSprite16Pixels

        pop  ebp
        pop  edi
        pop  esi
        pop  ebx
    }
}

static void initReplayFrame(dword *p)
{
    if (replayState != 1)
        return;

    replayState = 2;

    savedCameraX = cameraX;
    savedCameraY = cameraY;

    oldCameraX = 0;
    cameraX = 176;
    oldCameraY = 0;
    cameraY = 349;

    SWOS::ResetAnimatedPatternsForBothTeams();

    InitSavedSprites();
    InitBackground();
    ScrollToCurrent();

    auto packedCoordinates = *p;
    clearHiBit(packedCoordinates);

    cameraX = loWord(packedCoordinates);
    cameraY = hiWord(packedCoordinates);

    ScrollToCurrent();
}

void hilDrawResult()
{
    int x = 13;
    int y = 13;

    auto leftScoreDigits = std::div(hilLeftTeamGoals, 10);
    auto leftScoreDigit1 = leftScoreDigits.quot;
    auto leftScoreDigit2 = leftScoreDigits.rem;

    if (leftScoreDigit1) {
        drawSprite16Pixels(leftScoreDigit1 + SPR_BIG_0, x, y);
        x += 12;
    }

    drawSprite16Pixels(leftScoreDigit2 + SPR_BIG_0, x, y);

    x += 15;
    y += 8;
    drawSprite16Pixels(SPR_BIG_DASH, x, y);

    x += 8;
    y -= 8;

    auto rightScoreDigits = std::div(hilRightTeamGoals, 10);
    auto rightScoreDigit1 = rightScoreDigits.quot;
    auto rightScoreDigit2 = rightScoreDigits.rem;

    if (rightScoreDigit1) {
        drawSprite16Pixels(rightScoreDigit1 + SPR_2BIG_0, x, y);
        x += 12;
    }

    drawSprite16Pixels(rightScoreDigit2 + SPR_2BIG_0, x, y);
}

void SWOS::RenderReplayFrame()
{
    auto p = currentGoalPtr;

    initReplayFrame(p);

    auto d = *p++;

    if (hiBitSet(d)) {
        p = validateHilPointerAdd(p);
        d &= 0x7fffffff;

        D1 = loWord(d);
        D2 = hiWord(d);
        MoveView();

        d = *p++;
        p = validateHilPointerAdd(p);

        hilLeftTeamGoals = loWord(d);
        hilRightTeamGoals = hiWord(d);

        d = *p++;
        p = validateHilPointerAdd(p);

        animPatternsState = hiWord(d);

        save68kRegisters();
        DrawAnimatedPatterns();
        restore68kRegisters();

        d = *p++;

        if (d == -1) {
            replayState = 0;
            return;
        }
    }

    while (!hiBitSet(d)) {
        p = validateHilPointerAdd(p);
        auto spriteIndex = d >> 20;
        int x = static_cast<int>(d << 12) >> 22;
        int y = static_cast<int>(d << 22) >> 22;

        drawSprite16Pixels(spriteIndex, x, y);

        d = *p++;
        if (d == -1) {
            replayState = 0;
            return;
        }
    }

    p = validateHilPointerSub(p - 1);
    currentGoalPtr = p;

    if (!noBigReplayLetter) {
        deltaColor = 0x70;
        drawSprite16Pixels(((stoppageTimer >> 1) & 0x1f) + SPR_REPLAY_FRAME_00, 11, 14);
        deltaColor = 0;
    } else {
        hilDrawResult();
    }

    if (replayState == 3 && !paused) {
        frameDelay();
        skipFrameUpdate();
    }
}

static void replayPausedLoop()
{
    while (paused) {
        SWOS::GetKey();
        if (!skipFade)
            break;
        MainKeysCheck();
    }
}

static bool replayAborted()
{
    int checkWhichPlayerFire = 1 + 2;

    if (!userRequestedReplay) {
        if (!teamDataPtr)
            return false;

        if (teamDataPtr->playerNumber == 1)
            checkWhichPlayerFire = 1;
        else if (teamDataPtr->playerNumber == 2)
            checkWhichPlayerFire = 2;
        else if (teamDataPtr->plCoachNum == 1)
            checkWhichPlayerFire = 1;
        else if (teamDataPtr->plCoachNum == 2)
            checkWhichPlayerFire = 2;
    }

    if (checkWhichPlayerFire & 1) {
        SWOS::Player1StatusProc();
        if (pl1Fire)
            return true;
    }

    if (checkWhichPlayerFire & 2) {
        SWOS::Player2StatusProc();
        if (pl2Fire)
            return true;
    }

    return false;
}

void SWOS::PlayReplayLoop()
{
    while (true) {
        ReadTimerDelta();

        replayPausedLoop();

        SWOS::GetKey();
        if (skipFade)
            MainKeysCheck();

        if (!replayState)
            return;

        if (skipFade && replayAborted()) {
            statsFireExit = 1;
            replayState = 0;
            break;
        }

        SAFE_INVOKE(ClearBackground);
        RenderReplayFrame();

        if (!replayState)
            break;

        if (fadeAndSaveReplay) {
            FadeAndSaveReplay();
            fadeAndSaveReplay = 0;
        }

        if (resetHilPtrFlag) {
            FindStartingDword();
            resetHilPtrFlag = 0;
        }

        Flip();

        fadeIfNeeded();
    }

    SAFE_INVOKE(FadeOutToBlack);
    SAFE_INVOKE(ClearBackground);
    RestoreCameraPosition();
}

static void saveCoordinatesForHighlights(int spriteIndex, int x, int y)
{
    D0 = spriteIndex;
    D4 = x;
    D5 = y;
    SaveCoordinatesForHighlights();
}

static void darkenRectangle(int spriteIndex, int x, int y)
{
    D0 = spriteIndex;
    D1 = x;
    D2 = y;
    DarkenRectangle();
}

static void drawSprites(bool saveHihglightCoordinates = true)
{
    SortSprites();

    if (!numSpritesToRender)
        return;

    for (size_t i = 0; i < numSpritesToRender; i++) {
        auto player = sortedSprites[i];

        if (player->pictureIndex < 0)
            continue;

        auto sprite = spriteGraphicsPtr[player->pictureIndex];

        int x = player->x - sprite->centerX - cameraX;
        int y = player->y - sprite->centerY - cameraY - player->z;

        if (x < 336 && y < 200 && x > -sprite->width && y > -sprite->nlines) {
            if (saveHihglightCoordinates && player != &big_S_Sprite && player->teamNumber)
                saveCoordinatesForHighlights(player->pictureIndex, x, y);

            player->beenDrawn = 1;

            if (player == &big_S_Sprite)
                deltaColor = 0x70;

            if (player->pictureIndex == SPR_SQUARE_GRID_FOR_RESULT)
                darkenRectangle(SPR_SQUARE_GRID_FOR_RESULT, x, y);
            else
                drawSprite16Pixels(player->pictureIndex, x, y);
        } else {
            player->beenDrawn = 0;
        }

        deltaColor = 0;
    }
}

static bool fadeOutInitiated;
static bool startingReplay;
static bool gameDone;

void SWOS::FadeOutToBlack_OnEnter()
{
    fadeOutInitiated = true;
}

void SWOS::FindStartingDword_OnEnter()
{
    if (!replayState)
        startingReplay = true;
}

void SWOS::GameOver_OnEnter()
{
    gameDone = true;
}

static void swosSetPalette(const char *palette)
{
    setPalette(palette);
    updateControls();
    frameDelay(0.5);

    // if the game is running, we have to redraw the sprites explicitly
    if (screenWidth == kGameScreenWidth) {
        // do one redraw of the sprites if the game was aborted, ended, or the replay is just starting,
        // but skip forced screen update if playing highlights
        if (fadeOutInitiated && !playHighlightFlag && (gameAborted || startingReplay || gameDone)) {
            // also be careful not to mess with saved highlights coordinates
            drawSprites(false);
            gameAborted = 0;
            fadeOutInitiated = false;
            startingReplay = false;
            gameDone = false;
        }
    }

    updateScreen();
}

// called from SWOS, with palette in esi
__declspec(naked) void SWOS::SetPalette()
{
    __asm {
        push ebx
        push esi
        push ecx    ; used by FadeOut

        push esi
        call swosSetPalette

        add esp, 4
        pop  ecx
        pop  esi
        pop  ebx
        retn
    }
}

void SWOS::DrawSprites()
{
    drawSprites();
}

__declspec(naked) void SWOS::DrawBenchAndSubsMenu_OnEnter()
{
    // we're interfering with ebp during the game loop, so zero it out here just in case
    __asm {
        xor  ebp, ebp
        retn
    }
}

void SWOS::EndProgram()
{
    std::exit(EXIT_FAILURE);
}

void SWOS::StartMainGameLoop()
{
    initGameControls();

    A5 = &leftTeamIngame;
    A6 = &rightTeamIngame;

    EGA_graphics = 0;

    SAFE_INVOKE(GameLoop);

    vsPtr = linAdr384k;

    finishGameControls();
}

// in:
//      D0 - speed
//      D1 - destination    x
//      D2 - destination    y
//      D3 - current x
//      D4 - current y
// out:
//      D0 - angle(0..255) (0 is 270 degrees, start of trigonometric circle is 64)
//      -1 if no movement
//      sign flag : set = no movement, clear = there is movement
//      D1 - delta x
//      D2 - delta y
//
// Delta    xand y  are numbers of pixels that sprite needs to be moved in one cycle to reach
// destination coordinates.
int SWOS::CalculateDeltaXAndY()
{
    int16_t lD1 = D1.asWord() - D3.asWord();
    int16_t lD2 = D2.asWord() - D4.asWord();
    int32_t lD5 = D0.asWord();
    int8_t lD4 = 0;

    bool lD3 = false;
    if (lD1 < 0)
    {
        lD3 = true;
        lD1 = -lD1;
    }

//delta_x_positive:;
    lD4 = 0;
    if (lD2 < 0)
    {
        lD4 = -1;
        lD2 = -lD2;
    }

//delta_y_positive:;
    while(lD1 >= 32)
    {
        lD1 >>= 1;
        lD2 >>= 1;
    }

//check_delta_x:;
    while(lD2 >= 32)
    {
        lD1 >>= 1;
        lD2 >>= 1;
    }

//get_angle_coeficient:;
    lD2 <<= 5;
    lD1 |= lD2;

    int16_t lD0 = kAngleCoeficients[lD1];
    if (lD0 < 0)
    {
        D1 = 0;
        D2 = 0;
        D0 = -1;
        return D0.asInt16();
    }

    lD0 <<= 1;
    if (lD3 == true)
    {
        //left_half_of_circle;
        if (lD4 < 0)
        {
            //quadrant_3
            lD1 = 384;
            lD1 -= lD0;
        }
        else
        {
            lD1 = 384;
            lD1 += lD0;
        }
    }
    else
    {
        if (lD4 < 0)
        {
            //quadrant_4
            lD1 = 128;
            lD1 += lD0;
        }
        else
        {
            lD1 = 128;
            lD1 -= lD0;
        }
    }
//calculate_sine_cosine:;
    lD1 &= 0x1FF;
    lD0 = 512 - lD1;
    lD0 += 256;
    lD0 &= 0x1FF;
    lD0 >>= 1;

    lD2 = lD1;
    lD1 = kSineCosineTable[lD1 / 2];    // D1 = cosine of a direction vector angle
    lD2 += 128;
    lD2 &= 0x1FF;
    lD2 = kSineCosineTable[lD2 / 2];
    lD1 >>= 8;
    lD2 >>= 8;
    
    int32_t fD1 = ((int32_t)lD1) * ((int32_t)lD5);
    int32_t fD2 = ((int32_t)lD2) * ((int32_t)lD5);

    //if (dos) {
        lD5 = fD1; //  D1 = x, D5 = x
        lD5 >>= 2;
        fD1 -= lD5; // D1 = x - x/4
        lD5 >>= 2;
        fD1 -= lD5; // D1 = x - x/4 - x/16
        lD5 >>= 1;
        fD1 -= lD5; // D1 = x - x/4 - x/16 - x/32
        lD5 >>= 1;
        fD1 -= lD5; // D1 = x - x/4 - x/16 - x/32 - x/64

        lD5 = fD2;
        lD5 >>= 2;
        fD2 -= lD5;
        lD5 >>= 2;
        fD2 -= lD5;
        lD5 >>= 1;
        fD2 -= lD5;
        lD5 >>= 1;
        fD2 -= lD5; // D2 = sine    / 256 * speed * 41 / 64
    //}

    D0 = lD0;
    D1 = fD1;
    D2 = fD2;
    return lD0; // D1 = cosine /    256 * speed * 41 / 64
}

/**
 * in:
 *      A6 -> team (general)
 * out:
 *      A0 -> opponents team
 *      carry flag = pass/kick time == 13
 * 
 * Only used by AI.
 *
 * Returns true if passkicktimer >= 13 (JNB - Jump if not below)
 */
int SWOS::CVer_cseg_859CB()
{
    TeamGeneralInfo* TeamInfoPtr = A6;
    A0 = TeamInfoPtr->opponentsTeam;

    return (TeamInfoPtr->passKickTimer >= 13);
}

void SWOS::DoAI()
{
    TeamGeneralInfo* TeamInfoPtr = A6;
    Player* pA2 = A2.as<Player*>();

    if (TeamInfoPtr->resetControls)
        return;

    if (AI_counter)
        --AI_counter;

    if (AI_beginPlayTimer)
        --AI_beginPlayTimer;

    Rand();
    AI_rand = D0;

    TeamInfoPtr->AITimer += 1;
    TeamInfoPtr->currentDirection = -1;
    TeamInfoPtr->joyIsFiring = 0;
    TeamInfoPtr->joyTriggered = 0;
    TeamInfoPtr->quickFire = 0;
    TeamInfoPtr->normalFire = 0;

    if (inSubstitutesMenu)
        return;

    Player* pA5 = TeamInfoPtr->controlledPlayerSprite;
    A5 = TeamInfoPtr->controlledPlayerSprite;
    A4 = TeamInfoPtr->passToPlayerPtr;
    if (pA5)
        D7 = TeamInfoPtr->controlledPlayerSprite->direction;
	else
		D7 = -1;

    //no_controlled_player
    A0 = &ballSprite;
    if (A6 == &rightTeamData)
        D2 = 129; // upper goal line (for   right team)
    else
        D2 = 769; // lower goal line (for   left team)

    D1 = 336;
    D3 = ballSprite.x;
    D4 = ballSprite.y;
    
    D3 -= D1;	// Ball to goal line distance (X)
    D4 -= D2;	// Ball to goal line distance (Y)

    D3 = D3 * D3;	// Y Distance Squared
    D4 = D4 * D4;	// X Distance Squared
    D6 = D3 + D4;	// Total Distance

    D3 = ballSprite.x;
    D4 = ballSprite.y;
    D0 = 256;
    
    if(CalculateDeltaXAndY() < 0)
        D0 = 0;

    //angle_positive
    D5 = D0;
    if (gameStatePl == ST_GAME_IN_PROGRESS)
        goto game_is_in_progress;

    if (A6 != lastPlayedTeam)
        return;

    if (gameState < ST_STARTING_GAME || gameState > ST_GAME_ENDED)
        goto game_not_over;

    if (!team1Computer || !team2Computer)
        goto game_not_over;

    if (gameState == ST_RESULT_ON_HALFTIME) {
        //showing_result_on_halftime
        if (stoppageTimerTotal < 660)
            return;
    } else {
        if (gameState != ST_RESULT_AFTER_THE_GAME) {
            //not_showing_final_result
            if (stoppageTimerTotal < 385)
                return;
        }
        else {
            if (stoppageTimerTotal < 660)   // showing result after the game
                return;
        }
    }

    // interval_expired_fire
    TeamInfoPtr->joyIsFiring = 1;
    return;

game_not_over:;

    D0 = stoppageTimer;
    D0 &= 0x3F;
    D0 += 50;
    D0 += 50;
    if (D0 > stoppageTimerActive) {
        //set_direction
        if (resultTimer)
            return;

        if (gameState == ST_GOAL_OUT_LEFT || gameState == ST_GOAL_OUT_RIGHT || gameState == ST_KEEPER_HOLDS_BALL)
            goto no_player_facing;

        if (gameState < ST_THROW_IN_FORWARD_RIGHT)
            goto no_throw_in;

        if (gameState <= ST_THROW_IN_BACK_LEFT)
            goto no_player_facing;

    no_throw_in:;
        if (gameState != ST_FOUL) {
            if (gameState < ST_FREE_KICK_LEFT1)
                return;

            if (gameState > ST_FREE_KICK_RIGHT3)
                return;
        }
        //foul_or_free_kick
        word direction = D5;
        direction += 16;
        direction &= 0xFF;
        TeamInfoPtr->currentDirection = direction >> 5;
        return;
    }

    if (playingPenalties || gameState == ST_PENALTY) {
        //penalties_after_the_game
        D0 = AI_rand;
        D0 &= 0x07;
        if (!(playerTurnFlags & (1 << (D0 & 0xFF)))) {
            //random_direction_disallowed
            if (!pA5->direction)
                return;

            if (pA5->direction == 4)
                return;
            goto cseg_84748;
        }

        TeamInfoPtr->currentDirection = D0;
        return;
    }

    if (gameState == ST_KEEPER_HOLDS_BALL || gameState == ST_GOAL_OUT_LEFT || gameState == ST_GOAL_OUT_RIGHT) {
        // keepers_ball;
        if (AI_rand & 1)
            goto cseg_84775;

        if (D7 == cameraDirection)
            goto cseg_84748;

        if (ballSprite.x > 336) {
            // cseg_845CC;
            if (D7 == 5 || D7 == 7)
                goto cseg_84748;

            goto cseg_84775;
        }
    
        if (D7 == 1 || D7 == 3)
            goto cseg_84748;

        goto cseg_84775;
    }

    if (gameState == ST_PLAYERS_TO_INITIAL_POSITIONS) {
        // goal_scored;
        if (stoppageTimerActive < 150)
            goto cseg_84775;
        if (D7 == cameraDirection)
            goto cseg_84748;
        goto cseg_84775;
    }

    if (gameState < ST_FREE_KICK_LEFT1)
        goto cseg_844FD;

    if (gameState <= ST_FREE_KICK_RIGHT3)
        goto foul;

cseg_844FD:;
    if (gameState < ST_THROW_IN_FORWARD_RIGHT || gameState > ST_THROW_IN_BACK_LEFT) {
        // not_a_throw_in;
        if (gameState == ST_FOUL)
            goto foul;
        goto cseg_84748;
    }

    D1 = gameState - ST_THROW_IN_FORWARD_RIGHT;
    
    D1 = dseg_17EF68[D1];
    if (A6 != &rightTeamData)
        D1.ror(4);

    goto cseg_84671;

foul:;
    if (AI_rand & 0x0F) {
        // cseg_8470F;
        D0 = (D5 + 16) & 0xFF;
        D0 >>= 5;
        if (D0 == D7)
            goto cseg_84748;

        goto cseg_84815;
    }

    goto cseg_84775;

cseg_84671:;
    if (AI_rand & 0x0F)
        goto cseg_84775;

    if (D1 & (1 << D7.asWord()))
        goto cseg_84748;
    goto cseg_84775;

cseg_84748:;
    D2 = D7;
    if (D2.asInt16() < 0)
        return;

    D2 <<= 5;
    D2 -= D5;
    goto cseg_851CC;

cseg_84775:;
    D0 = D7.data << 5;
    FindClosestPlayerToBallFacing();
    if (A0 == -1)
        goto no_player_facing;
    if (A0.as<Player*>()->teamNumber != pA5->teamNumber)
        goto no_player_facing;

    goto our_player_closest;

    // Unused section truncated
cseg_84815:;
    D0 = (D5 + 16) & 0xFF;
    TeamInfoPtr->currentDirection = (D0.asWord() >> 5);
    return;

no_player_facing:;
    if (stoppageTimer & 0x0E)
        return;

    if (someAIDirectionAllowedFlag >= 0) {
        //cseg_8489B
        if (someAIDirectionAllowedFlag != 1) {
            someAIDirectionAllowedFlag -= 1;

            if (someAIDirectionAllowedFlag != 1)
                return;
        }
        //cseg_848BB
        D1 = -1;
    } else {
        if (someAIDirectionAllowedFlag != -1) {
            someAIDirectionAllowedFlag += 1;
            if (someAIDirectionAllowedFlag != -1)
                return;
        }
        //cseg_84890
        D1 = 1;
    }

    //cseg_848C4
    D0 = (D7 + someAIDirectionAllowedFlag) & 7;

    if ((playerTurnFlags & (1 << (D0 & 0xFF))))
        TeamInfoPtr->currentDirection = D0;
    else
        someAIDirectionAllowedFlag = D1;
    return;

game_is_in_progress:;
    if (playingPenalties || penalty)
        if (TeamInfoPtr->spinTimer >= 0)
            goto we_have_ball_spin;
//no_penalty
    if (AI_counter)
        AISetDirectionAllowed();
//cseg_84955
    if (!pA5)
        return;

    if (TeamInfoPtr->spinTimer >= 0)
        goto we_have_ball_spin;

    if (TeamInfoPtr->plVeryCloseToBall)
        goto theres_a_player_near;
    goto noone_near;
    // truncated unused section

theres_a_player_near:;
    AIHeader();
    if (!D0)
        return;

    //if (!dseg_1309A7)
    //  goto cseg_84A0D;
    /* dseg_1309A7 appears to be never be written to
        mov esi, A6
        mov eax, [esi]
        mov A0, eax
        mov eax, dseg_1309C1
        cmp A0, eax
        jnz short cseg_84A0D
        mov esi, A0
        mov ax, [esi+40]
        or  ax, ax
        jnz return
    */
//cseg_84A0D:;
    if (pA5->direction != 2)
        if (pA5->direction != 6)
            goto cseg_84A53;
    // cseg_84A27
    if (A6 != &leftTeamData) {
        if (ballSprite.y <= 158)
            goto cseg_84AEB;
        goto cseg_84A53;
    }

//cseg_84A44
    if (ballSprite.y >= 740)
        goto cseg_84AEB;

cseg_84A53:;
    if (D6.asInt() > 28800)
        goto cseg_84AEB;
    if (D6.asInt() < 12800)
        goto cseg_84A85;
    if (AI_rand & 0x03)
        goto cseg_84AEB;

cseg_84A85:;
    D1 = 0x0F;
    if (D6.asInt() <= 3200)
        D1 = 0x32;
//cseg_84A9F

	D2 = pA5->direction;
    if (D2.asInt16() < 0)
        goto cseg_84AEB;

	D2.data <<= 5;
	D2.data -= D5.as<int8_t>();

    if (D2.as<int8_t>() <= D1.as<int8_t>()) {
        D1.data = -D1.as<int8_t>();
        if (D2.as<int8_t>() > D1.as<int8_t>())
            goto cseg_850F9;
    }
cseg_84AEB:;
    if (!TeamInfoPtr->field_84) {
        //cseg_84B5B
        if (D6.asInt() < 0x2648 || pA5 == 0)
            goto cseg_84DD3;

        TeamGeneralInfo* pA0 = TeamInfoPtr->opponentsTeam;
        pA2 = pA0->controlledPlayerSprite;
        if (pA2) {
            if (pA2->ballDistance < 800)
                goto cseg_84C00;
            if (pA2->ballDistance < 5000)
                goto cseg_84C93;
        }

        //cseg_84BBE
        pA2 = pA0->passToPlayerPtr;
        if (pA2) {
            if (pA2->ballDistance < 800)
                goto cseg_84C00;
            if (pA2->ballDistance < 5000)
                goto cseg_84C93;
        }
        goto cseg_84DD3;

    cseg_84C00:;
        if (D6.asInt() > 180000)
            goto cseg_84F4B;

        D0 = D7 << 5;
        FindClosestPlayerToBallFacing();
        pA0 = A0;

        if (A0 != -1) {
            if (pA5->teamNumber == pA0->teamNumber)
                goto our_player_closest;
        }
        goto cseg_84D10;

    cseg_84C93:;
        if (AI_rand <= 8) {
            if (D6.asInt() > 48400)
                goto cseg_84F4B;
        }
        //cseg_84CAD

        if (stoppageTimer & 0x0C)
            goto cseg_84DD3;
        
        D0 = D7 << 5;
        FindClosestPlayerToBallFacing();
        if (A0 != -1) {
            if (pA5->teamNumber == pA0->teamNumber)
                goto our_player_closest;
        }

    cseg_84D10:;
        if (TeamInfoPtr->field_84)
            goto cseg_84DD3;

        if ((frameCount & 0x7F) >= 0x20)
            goto cseg_84DD3;
        
        TeamInfoPtr->field_84 = 4;

    //cseg_84D57:;
        if (!TeamInfoPtr->plVeryCloseToBall)
            goto cseg_84E16;

        if (!(stoppageTimer & 0x80)) {
            // cseg_84DA2;
            D0 = pA5->direction - 1;
            TeamInfoPtr->currentDirection = D0 & 7;
            return;
        }

        D0 = pA5->direction + 1;
        TeamInfoPtr->currentDirection = D0 & 7;
        return;
    }

    TeamInfoPtr->field_84 -= 1;
    D0 = D7 << 5;
    FindClosestPlayerToBallFacing();
    if (A0 != -1) {
        if (pA5->teamNumber == A0.as<Player*>()->teamNumber)
            goto our_player_closest;
    }
    //cseg_84D57
    if (!TeamInfoPtr->plVeryCloseToBall)
        goto cseg_84E16;
    
    if ((stoppageTimer & 0x80))
        TeamInfoPtr->currentDirection = (pA5->direction + 1) & 7;
    else
	    TeamInfoPtr->currentDirection = (pA5->direction - 1) & 7;
    return;

cseg_84DD3:;
    if (!TeamInfoPtr->plVeryCloseToBall)
        goto cseg_84E16;

cseg_84DE0:;
    TeamInfoPtr->currentDirection = ((D5 + 16) & 0xFF) >> 5;
    return;

cseg_84E16:;
    TeamInfoPtr->currentDirection = pA5->direction;
    return;

cseg_84E2B:;
    D0 = (pA5->fullDirection + 128 + 16) & 0xFF;
    D0 >>= 5;
    if (D0 == pA5->direction || pA5->ballDistance < 800 || !(stoppageTimer & 0x0E)) {
        // cseg_84EA8;
        TeamInfoPtr->currentDirection = D0;
        return;
    }

    TeamInfoPtr->currentDirection = pA5->direction;
    return;

cseg_84EBC:;
    TeamInfoPtr->currentDirection = D7;
    return;

our_player_closest:;
    TeamInfoPtr->field_84 = 0;
    if (AI_beginPlayTimer)
        return;

    if (CVer_cseg_859CB())
        return;

    AI_beginPlayTimer = 15;
    TeamInfoPtr->currentDirection = D7;
    TeamInfoPtr->quickFire = 1;
    if (AI_maxStoppageTime <= stoppageTimerActive)
        AI_maxStoppageTime = stoppageTimerActive;

    return;

cseg_84F4B:;
	int16_t lD0 = (D5 + 16) & 0xFF;
	lD0 >>= 5;

	lD0 -= D7.asInt16();
	lD0 &= 7;
    if (!lD0 || lD0 == 1 || lD0 == -1) {
        // cseg_84FA0;
        TeamInfoPtr->field_86 = 2;
        if (D6.asInt() <= 115600)
            TeamInfoPtr->field_86 = 1;


        TeamInfoPtr->currentDirection = D7;
        TeamInfoPtr->normalFire = 1;

        D2 = D7 << 5;
        AI_beginPlayTimer = 15;
        D0 = -1;
        if (D2.as<int8_t>() < 0)
            D0 = -D0.asInt();

        if (ballSprite.deltaY >= 0) {
            if (ballSprite.y > 555)
                goto cseg_850E1;
            goto cseg_8504E;
        }
        //cseg_8503F
        if (ballSprite.y < 342)
            goto cseg_850E1;
        
        cseg_8504E:;
        D1 = stoppageTimer;
        D1 &= 0x1C;
        D1 >>= 2;
        if (ballSprite.x >= 193) {
            if (ballSprite.x <= 478) {
                // cseg_85097
                if (!D1)
                    goto cseg_850CF;
                if (D1 < 5)
                    goto cseg_850E1;
                goto cseg_850DA;
            }
        }
        //cseg_85080
        if (ballSprite.x < 118)
            goto cseg_850C5;
        if (ballSprite.x > 553)
            goto cseg_850C5;
        
    //cseg_850AE:;
        if (!D1)
            goto cseg_850DA;
        if (D1 < 4)
            goto cseg_850CF;
        goto cseg_850E1;

    cseg_850C5:;
        if (D1 < 4)
            goto cseg_850E1;
    cseg_850CF:;
        D0 = 0;
        goto cseg_850E1;

    cseg_850DA:;
        D0 = -D0.asInt();

    cseg_850E1:;
        TeamInfoPtr->field_88 = D0.asInt();
        return;

    cseg_850F9:;
        if (AI_beginPlayTimer)
            return;
        if (CVer_cseg_859CB())
            return;
        TeamInfoPtr->field_86 = 0;
        
        //cseg_85178;
        AI_beginPlayTimer = 15;
        TeamInfoPtr->currentDirection = D7;
        TeamInfoPtr->normalFire = 1;

        D0 = -1;
        if (D2.as<int8_t>() < 0)
            D0 = -D0.as<int16_t>();

        TeamInfoPtr->field_88 = D0.asInt();
        return;
    }

cseg_851CC:;
    if (AI_maxStoppageTime <= stoppageTimerActive)
        AI_maxStoppageTime = stoppageTimerActive;

    AI_beginPlayTimer = 15;
    if (gameState == ST_PENALTY || gameState == ST_PENALTIES)
        goto cseg_852AA;

    if (gameState < ST_FREE_KICK_LEFT1)
        goto cseg_8522C;

    if (gameState >= ST_FREE_KICK_RIGHT3)
        goto cseg_85270;

cseg_8522C:;
    if (gameState == ST_CORNER_LEFT || gameState == ST_CORNER_RIGHT)
        goto cseg_85299;
    
    D0 = (AI_rand & 0x18);
    if (D0)
        goto cseg_85270;
    if (D0 == 0x10)
        goto cseg_852AA;
    if (D0 == 8)
        goto cseg_85299;
    goto cseg_85288;

cseg_85270:;
    if (D6.asInt() < 28800)
        goto cseg_852AA;
    if (D6.asInt() < 57800)
        goto cseg_85299;

cseg_85288:;
    TeamInfoPtr->field_86 = 2;
    goto cseg_852B9;
cseg_85299:;
    TeamInfoPtr->field_86 = 1;
    goto cseg_852B9;
cseg_852AA:;
    TeamInfoPtr->field_86 = 0;

cseg_852B9:;
    TeamInfoPtr->currentDirection = D7;
    TeamInfoPtr->normalFire = 1;

    if (gameState == ST_PENALTY || gameState == ST_PENALTIES)
        goto cseg_853C7;

	if (gameState != ST_CORNER_LEFT && gameState != ST_CORNER_RIGHT) {
		AI_beginPlayTimer = 15;
		TeamInfoPtr->currentDirection = D7;
		TeamInfoPtr->normalFire = 1;
		D0 = 0;
		if (gameState == ST_FOUL)
			goto cseg_8536B;

		if (A6 == &leftTeamData) {
			if (pA2->y < 682)
				goto cseg_8536B;
		} else {
			//cseg_8535D
			if (pA2->y > 216) {
			cseg_8536B:;
				D0 = -1;
				if (D2.as<int8_t>() < 0)
					D0 = -D0.asInt();
			}
		}

//cseg_85384
		TeamInfoPtr->field_88 = D0.asInt();
		return;
	}

//cseg_8539C
    D0 = AI_rand & 7;
	if (D0 >= 3) {
		if (D0 < 6) {
			D1 = -1;
			D0 -= D7;
			D0.data -= 1;
			goto cseg_85417;
		}

	cseg_853C7:;
		TeamInfoPtr->field_88 = 0;
		return;
	}

    D1 = 1;
    D0 = D7;
    D0 += 1;

cseg_85417:;
    D0 &= 7;
    if (!(playerTurnFlags & (1 << D0.as<byte>())))
        goto cseg_853C7;

    TeamInfoPtr->field_88 = D1.asInt16();
    return;

noone_near:;

    AIHeader();
    if (!D0)
        return;
    if (A4 == 0)
        goto cseg_854BA;

    Player* pA4 = A4.as<Player*>();
    if ((pA4->ballDistance - pA5->ballDistance) < 50)
        goto cseg_854BA;

    D0 = TeamInfoPtr->passToPlayerPtr;
    TeamInfoPtr->passToPlayerPtr = TeamInfoPtr->controlledPlayerSprite;
    TeamInfoPtr->controlledPlayerSprite = D0;
    return;

cseg_854BA:;
    if (!A4)
        goto cseg_84E2B;

    if (dseg_1309A7)
        goto cseg_84DE0;

    if (leftTeamData.playerNumber || rightTeamData.playerNumber)
        goto cseg_84E2B;

//cseg_85520:;
    if (stoppageTimer & 0x18)
        goto cseg_84EBC;

    D0 = dseg_17EF4C[AI_rand & 2];
    D0 += pA5->fullDirection + 128 + 16;
    D0 &= 0xFF;
    TeamInfoPtr->currentDirection = (D0 >> 5);
    return;

we_have_ball_spin:;
    D1 = TeamInfoPtr->controlledPlDirection;

    if (playingPenalties || penalty || !(AI_rand & 1))
        goto cseg_8568B;

    if (TeamInfoPtr->field_88 < 0) {
        // cseg_856C9
        D0 = D1 - 1;
        D0 &= 7;
        A0 = dseg_17EF50;
        goto cseg_85717;
    }
    if (TeamInfoPtr->field_88 != 0) {
        D0 = D1 + 1;
        D0 &= 7;
        A0 = dseg_17EF56;
        goto cseg_85717;
    }

cseg_8568B:;
    if (TeamInfoPtr->field_86 == 1) {
        TeamInfoPtr->currentDirection = -1;
        return;
    }

//cseg_856BD
    A0 = dseg_17EF5C;

cseg_85717:;
    //D0 <<= 1;
    int16_t* ptr = A0;
    D1 += ptr[TeamInfoPtr->field_86];
    D1 &= 7;
    TeamInfoPtr->currentDirection = D1;
}

// Fix crash when watching 2 CPU players with at least one top-class goalkeeper in the game.
// Goalkeeper skill is scaled to range 0..7 (in D0) but value clamping is skipped in CPU vs CPU mode.
void SWOS::AdjustPlayerSkills_246()
{
    if (D0.asInt16() < 0)
        D0 = 0;
    if (D0.asInt16() > 7)
        D0 = 7;
}

//
// Replay/Exit menu
//

void SWOS::ReplayExitMenuAfterFriendly()
{
    showMenu(replayExitMenu);
}

static void replayExitMenuOnInit()
{
    replaySelected = 0;

    DrawMenu();     // redraw menu so it's ready for the fade-in
    fadeIfNeeded();

    cameraX = 0;
    cameraY = 0;
}

static void replayExitMenuDone(bool replay = false)
{
    replaySelected = replay;
    SAFE_INVOKE(FadeOutToBlack);
    SetExitMenuFlag();
}

static void replayOnSelect()
{
    logInfo("Going for another one!");
    replayExitMenuDone(true);
}

static void exitOnSelect()
{
    replayExitMenuDone();
}
