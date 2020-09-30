#include "game.h"
#include "render.h"
#include "audio.h"
#include "render.h"
#include "options.h"
#include "controls.h"
#include "menu.h"
#include "dump.h"
#include "replays.h"
#include "sprites.h"
#include "replayExit.mnu.h"

constexpr Uint32 kMinimumPreMatchScreenLength = 1'200;

static Uint32 m_loadingStartTick;

static void updateCursor(bool matchRunning)
{
    bool fullScreen = getWindowMode() != kModeWindow;
    bool disableCursor = matchRunning && fullScreen && !gotMousePlayer();
    SDL_ShowCursor(disableCursor ? SDL_DISABLE : SDL_ENABLE);
}

void checkKeyboardShortcuts(SDL_Scancode scanCode, bool pressed)
{
    static bool altDown, shiftDown;

    switch (scanCode) {
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
        altDown = pressed;
        break;
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
        shiftDown = pressed;
        break;
    case SDL_SCANCODE_F1:
        // preserve alt-F1, ultra fast exit from SWOS (actually meant for invoking the debugger ;))
        if (pressed && altDown) {
            logInfo("Shutting down via keyboard shortcut...");
            std::exit(EXIT_SUCCESS);
        }
        break;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        if (pressed && altDown)
            shiftDown ? toggleFullScreenMode() : toggleBorderlessMaximizedMode();

        updateCursor(isMatchRunning());
        break;
    }
}

void SWOS::InitGame_OnEnter()
{
    // soon after this SWOS will load the game palette, trashing old screen contents in the process
    // unlike the original game, which only changes palette register on the VGA card, we need those bytes
    // to persist in order to fade-out properly; so save it here so we can restore them later
    logInfo("Initializing the game...");
    memcpy(swos.linAdr384k + 2 * kVgaScreenSize, swos.linAdr384k, kVgaScreenSize);
}

__declspec(naked) void SWOS::ClearBackground_OnEnter()
{
    // fix SWOS bug, just in case
#ifdef SWOS_VM
    SwosVM::edx = 0;
#else
    _asm {
        xor edx, edx
        retn
    }
#endif
}

void SWOS::MainKeysCheck_OnEnter()
{
    if (testForPlayerKeys()) {
        swos.lastKey = 0;
        return;
    }

    switch (swos.convertedKey) {
    case 'D':
        toggleDebugOutput();
        break;
    case 'F':
        toggleFastReplay();
        break;
    }

    if (swos.lastKey == kKeyF2)
        makeScreenshot();
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

    if (!swos.numSpritesToRender)
        return;

    for (size_t i = 0; i < swos.numSpritesToRender; i++) {
        auto player = swos.sortedSprites[i].asPtr();

        assert(player->spriteIndex == -1 || player->spriteIndex > kMaxMenuSprite && player->spriteIndex < kNumSprites);

        if (player->spriteIndex < 0)
            continue;

        auto sprite = swos.spritesIndex[player->spriteIndex];

        int x = player->x - sprite->centerX - swos.g_cameraX;
        int y = player->y - sprite->centerY - swos.g_cameraY - player->z;

        SpriteClipper clipper(sprite, x, y);
        if (clipper.clip())
            clipper.clip();

        if (x < 336 && y < 200 && x > -sprite->width && y > -sprite->height) {
        //if (!clipper.clip()) {
            if (saveHihglightCoordinates && player != &swos.big_S_Sprite && player->teamNumber)
                saveCoordinatesForHighlights(player->spriteIndex, x, y);

            player->beenDrawn = 1;

            if (player == &swos.big_S_Sprite)
                swos.deltaColor = 0x70;

            if (player->spriteIndex == kSquareGridForResultSprite)
                darkenRectangle(kSquareGridForResultSprite, x, y);
            else
                //drawSpriteUnclipped(clipper);
                drawSprite(player->spriteIndex, x, y);
        } else {
            player->beenDrawn = 0;
        }

        swos.deltaColor = 0;
    }
}

static bool fadeOutInitiated;
static bool startingReplay;
static bool gameDone;

void SWOS::FadeOutToBlack_OnEnter()
{
    fadeOutInitiated = true;
}

void SWOS::HandleInstantReplayStateSwitch_OnEnter()
{
    if (!swos.replayState)
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
    if (swos.screenWidth == kGameScreenWidth) {
        // do one redraw of the sprites if the game was aborted, ended, or the replay is just starting,
        // but skip forced screen update if playing highlights
        if (fadeOutInitiated && !swos.playHighlightFlag && (swos.gameAborted || startingReplay || gameDone)) {
            // also be careful not to mess with saved highlights coordinates
            drawSprites(false);
            swos.gameAborted = 0;
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
#ifdef SWOS_VM
    using namespace SwosVM;
    push(ebx);
    push(esi);
    push(ecx);
    auto palette = SwosVM::esi.asPtr();
    swosSetPalette(palette);
    pop(ecx);
    pop(esi);
    pop(ebx);
#else
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
#endif
}

void SWOS::DrawSprites()
{
    drawSprites();
}

void SWOS::EndProgram()
{
    std::exit(EXIT_FAILURE);
}

void SWOS::StartMainGameLoop()
{
    initGameControls();
    initNewReplay();
    updateCursor(true);

    A5 = &swos.leftTeamIngame;
    A6 = &swos.rightTeamIngame;

    swos.EGA_graphics = 0;

    SAFE_INVOKE(GameLoop);

    swos.vsPtr = swos.linAdr384k;

    finishGameControls();
    updateCursor(false);
}

void SWOS::GameEnded()
{
    finishCurrentReplay();
}

void SWOS::ShowStadiumInit_OnEnter()
{
    m_loadingStartTick = SDL_GetTicks();
}

void SWOS::InsertPauseBeforeTheGameIfNeeded()
{
    if (!doNotPauseLoadingScreen()) {
        auto diff = SDL_GetTicks() - m_loadingStartTick;
        if (diff < kMinimumPreMatchScreenLength) {
            auto pause = kMinimumPreMatchScreenLength - diff;
            logInfo("Pausing loading screen for %d milliseconds (target: %d)", pause, kMinimumPreMatchScreenLength);
            SDL_Delay(pause);
        }
    }
}

// Fix crash when watching 2 CPU players with at least one top-class goalkeeper in the game.
// Goalkeeper skill is scaled to range 0..7 (in D0) but value clamping is skipped in CPU vs CPU mode.
void SWOS::FixTwoCPUsGameCrash()
{
    if (D0.asInt16() < 0)
        D0 = 0;
    if (D0.asInt16() > 7)
        D0 = 7;
}

static dword gameTimeInMinutes()
{
    // endianess alert
    return swos.gameTime[1] * 100 + swos.gameTime[2] * 10 + swos.gameTime[3];
}

static bool isGameAtMinute(dword minute)
{
    return minute == gameTimeInMinutes();
}

static void endFirstHalf()
{
    EndFirstHalf();
    BumpAllPlayersLastPlayedHalfPostGame();
}

static void endSecondHalf()
{
    BumpAllPlayersLastPlayedHalfPostGame();
    swos.statsTeam1GoalsCopy = swos.statsTeam1Goals;
    swos.statsTeam2GoalsCopy = swos.statsTeam2Goals;

    int totalTeam1Goals = swos.team1Goals;
    int totalTeam2Goals = swos.team2Goals;

    if (totalTeam1Goals == totalTeam2Goals) {
        totalTeam1Goals = swos.statsTeam1Goals + 2 * swos.team1GoalsFirstLeg;
        totalTeam2Goals = swos.statsTeam2Goals + 2 * swos.team2GoalsFirstLeg;

        bool gameTied = !swos.secondLeg || swos.playing2ndGame != 1 || totalTeam1Goals == totalTeam2Goals;
        if (gameTied) {
            if (swos.extraTimeState) {
                swos.extraTimeState = -1;
                StartFirstExtraTime();
            } else if (swos.penaltiesState) {
                swos.penaltiesState = -1;
                StartPenalties();
            } else {
                swos.winningTeamPtr = nullptr;
                EndOfGame();
            }
            return;
        }
    }

    swos.winningTeamPtr = totalTeam1Goals > totalTeam2Goals ? &swos.leftTeamIngame : &swos.rightTeamIngame;
    EndOfGame();
}

static bool prolongLastMinute()
{
    if (swos.gameStatePl != GameState::kInProgress)
        return true;

    constexpr int kUpperPenaltyAreaLowerLine = 216;
    constexpr int kLowerPenaltyAreaUpperLine = 682;
    constexpr int kCenterLine = 449;

    auto ballY = swos.ballSprite.y;
    auto ballInsidePenaltyArea = ballY <= kUpperPenaltyAreaLowerLine || ballY > kLowerPenaltyAreaUpperLine;
    auto attackingTeam = ballY > kCenterLine ? &swos.topTeamData : &swos.bottomTeamData;
    auto attackInProgress = swos.lastTeamPlayed == attackingTeam;

    return ballInsidePenaltyArea || attackInProgress;
}

static void endFirstExtraTime()
{
    EndFirstExtraTime();
}

static void endSecondExtraTime()
{
    int totalTeam1Goals = swos.team1Goals;
    int totalTeam2Goals = swos.team2Goals;

    if (totalTeam1Goals == totalTeam2Goals) {
        totalTeam1Goals = swos.statsTeam1Goals + 2 * swos.team1GoalsFirstLeg;
        totalTeam2Goals = swos.statsTeam2Goals + 2 * swos.team2GoalsFirstLeg;

        bool gameTied = !swos.secondLeg || !swos.playing2ndGame || totalTeam1Goals == totalTeam2Goals;
        if (gameTied) {
            if (swos.penaltiesState) {
                swos.penaltiesState = -1;
                StartPenalties();
            } else {
                swos.winningTeamPtr = nullptr;
                EndOfGame();
            }
            return;
        }
    }

    swos.winningTeamPtr = totalTeam1Goals > totalTeam2Goals ? &swos.leftTeamIngame : &swos.rightTeamIngame;
    EndOfGame();
}

using LastPeriodMinuteHandler = void (*)();

LastPeriodMinuteHandler getPeriodEndHandler()
{
    switch (gameTimeInMinutes()) {
    case 45:  return endFirstHalf;
    case 90:  return endSecondHalf;
    case 105: return endFirstExtraTime;
    case 120: return endSecondExtraTime;
    default: return nullptr;
    }
}

bool isNextMinuteLastInPeriod()
{
    return getPeriodEndHandler() != nullptr;
}

static void positionTimeSprite()
{
    constexpr int kMultiDigitTimeXOffset = 20 - 6;

    swos.currentTimeSprite.x = swos.g_cameraX + kMultiDigitTimeXOffset;

    constexpr int kTimeZOffset = 10'000;
    constexpr int kTimeYOffset = 9;

    swos.currentTimeSprite.y = swos.g_cameraY;
    swos.currentTimeSprite.y += kTimeYOffset + kTimeZOffset;

    swos.currentTimeSprite.z = kTimeZOffset;
}

constexpr int kTimeDigitWidth = 6;

static void copyDigitToTimeSprite(int digit, int xOffset, int destSpriteIndex)
{
    assert(digit >= 0 && digit <= 9);
    assert(swos.spritesIndex[kBigTimeDigitSprite0]->width == kTimeDigitWidth);

    int sourceSpriteIndex = kBigTimeDigitSprite0 + digit;

    copySprite(sourceSpriteIndex, destSpriteIndex, xOffset, 0);
}

static void updateTimeSprite()
{
    int spriteIndex = kTimeSprite8Mins;

    if (swos.gameTime[1]) {
        spriteIndex = kTimeSprite118Mins;
        copyDigitToTimeSprite(swos.gameTime[1], 0, spriteIndex);
        copyDigitToTimeSprite(swos.gameTime[2], kTimeDigitWidth, spriteIndex);
        copyDigitToTimeSprite(swos.gameTime[3], 2 * kTimeDigitWidth, spriteIndex);
    } else if (swos.gameTime[2]) {
        spriteIndex = kTimeSprite88Mins;
        copyDigitToTimeSprite(swos.gameTime[2], 0, spriteIndex);
        copyDigitToTimeSprite(swos.gameTime[3], kTimeDigitWidth, spriteIndex);
    } else {
        copyDigitToTimeSprite(swos.gameTime[3], 0, spriteIndex);
    }

    swos.currentTimeSprite.spriteIndex = spriteIndex;
}

static void updateAndPositionTimeSprite()
{
    updateTimeSprite();
    positionTimeSprite();
}

static void bumpGameTime()
{
    if (++swos.gameTime[3] >= 10) {
        swos.gameTime[3] = 0;
        if (++swos.gameTime[2] >= 10) {
            swos.gameTime[2] = 0;
            swos.gameTime[1]++;
        }
    }
}

static void setupLastMinuteSwitchNextFrame()
{
    swos.gameSeconds = -1;
    swos.endGameCounter = 55;
}

void SWOS::ResetTime()
{
    swos.gameSeconds = 0;
    memset(&swos.gameTime, 0, sizeof(swos.gameTime));
    swos.secondsSwitchAccumulator = 0;
    updateAndPositionTimeSprite();
    swos.currentTimeSprite.visible = 0;
}

static void ensureTimeSpriteIsVisible()
{
    if (!swos.currentTimeSprite.visible) {
        swos.currentTimeSprite.visible = true;
        InitDisplaySprites();
    }
}

void SWOS::UpdateTime()
{
    ensureTimeSpriteIsVisible();

    bool minuteSwitchAboutToHappen = swos.gameSeconds < 0;

    if (minuteSwitchAboutToHappen) {
        swos.endGameCounter -= swos.timerDifference;
        if (swos.endGameCounter < 0) {
            if (auto periodEndHandler = getPeriodEndHandler()) {
                swos.gameSeconds = 0;
                swos.stateGoal = 0;
                PlayEndGameWhistleSample();
                periodEndHandler();
            }
            updateAndPositionTimeSprite();
        } else {
            if (prolongLastMinute()) {
                setupLastMinuteSwitchNextFrame();
                positionTimeSprite();
            }
        }
    } else if (swos.gameStatePl == GameState::kInProgress && !swos.playingPenalties) {
        swos.secondsSwitchAccumulator -= swos.timeDelta;
        if (swos.secondsSwitchAccumulator < 0) {
            swos.secondsSwitchAccumulator += 54;
            swos.gameSeconds += swos.timerDifference;

            if (swos.gameSeconds >= 60) {
                swos.gameSeconds = 0;
                bumpGameTime();

                if (isGameAtMinute(1) || isGameAtMinute(46))
                    BumpAllPlayersLastPlayedHalfAtGameStart();

                if (isNextMinuteLastInPeriod()) {
                    setupLastMinuteSwitchNextFrame();
                    positionTimeSprite();
                } else {
                    updateAndPositionTimeSprite();
                }
            }
        }
    }

    positionTimeSprite();
}

//
// Replay/Exit menu after the end of a friendly game
//

void SWOS::ReplayExitMenuAfterFriendly()
{
    showMenu(replayExitMenu);
}

static void replayExitMenuOnInit()
{
    swos.replaySelected = 0;

    DrawMenu();     // redraw menu so it's ready for the fade-in
    fadeIfNeeded();

    swos.g_cameraX = 0;
    swos.g_cameraY = 0;

    SDL_ShowCursor(SDL_ENABLE);
}

static void replayExitMenuDone(bool replay = false)
{
    swos.replaySelected = replay;
    SAFE_INVOKE(FadeOutToBlack);
    SetExitMenuFlag();

    if (replay)
        initNewReplay();

    updateCursor(replay);
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
