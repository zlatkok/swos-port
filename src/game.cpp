#include "render.h"
#include "audio.h"
#include "log.h"
#include "controls.h"
#include "menu.h"
#include "util.h"
#include "replayExit.mnu.h"

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
            ReplaySegment();

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

void SWOS::ReplaySegment()
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
        ReplaySegment();

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

        auto sprite = (*spriteGraphicsPtr)[player->pictureIndex];

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
