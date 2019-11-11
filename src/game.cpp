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

void SWOS::MainKeysCheck_OnEnter()
{
    if (testForPlayerKeys()) {
        lastKey = 0;
        return;
    }

    switch (convertedKey) {
    case 'D':
        toggleDebugOutput();
        break;
    case 'F':
        toggleFastReplay();
        break;
    }

    if (lastKey == kKeyF2)
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

    if (!numSpritesToRender)
        return;

    for (size_t i = 0; i < numSpritesToRender; i++) {
        auto player = sortedSprites[i];

        if (player->spriteIndex < 0)
            continue;

        auto sprite = spritesIndex[player->spriteIndex];

        int x = player->x - sprite->centerX - g_cameraX;
        int y = player->y - sprite->centerY - g_cameraY - player->z;

        SpriteClipper clipper(sprite, x, y);
        if (clipper.clip())
            clipper.clip();

        if (x < 336 && y < 200 && x > -sprite->width && y > -sprite->height) {
        //if (!clipper.clip()) {
            if (saveHihglightCoordinates && player != &big_S_Sprite && player->teamNumber)
                saveCoordinatesForHighlights(player->spriteIndex, x, y);

            player->beenDrawn = 1;

            if (player == &big_S_Sprite)
                deltaColor = 0x70;

            if (player->spriteIndex == kSquareGridForResultSprite)
                darkenRectangle(kSquareGridForResultSprite, x, y);
            else
                //drawSpriteUnclipped(clipper);
                drawSprite(player->spriteIndex, x, y);
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

void SWOS::HandleInstantReplayStateSwitch_OnEnter()
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

void SWOS::EndProgram()
{
    std::exit(EXIT_FAILURE);
}

void SWOS::StartMainGameLoop()
{
    initGameControls();
    initNewReplay();
    updateCursor(true);

    A5 = &leftTeamIngame;
    A6 = &rightTeamIngame;

    EGA_graphics = 0;

    SAFE_INVOKE(GameLoop);

    vsPtr = linAdr384k;

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
    return gameTime[1] * 100 + gameTime[2] * 10 + gameTime[3];
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
    statsTeam1GoalsCopy = statsTeam1Goals;
    statsTeam2GoalsCopy = statsTeam2Goals;

    int totalTeam1Goals = team1Goals;
    int totalTeam2Goals = team2Goals;

    if (totalTeam1Goals == totalTeam2Goals) {
        totalTeam1Goals = statsTeam1Goals + 2 * team1GoalsFirstLeg;
        totalTeam2Goals = statsTeam2Goals + 2 * team2GoalsFirstLeg;

        bool gameTied = !secondLeg || playing2ndGame != 1 || totalTeam1Goals == totalTeam2Goals;
        if (gameTied) {
            if (extraTimeState) {
                extraTimeState = -1;
                StartFirstExtraTime();
            } else if (penaltiesState) {
                penaltiesState = -1;
                StartPenalties();
            } else {
                winningTeamPtr = nullptr;
                EndOfGame();
            }
            return;
        }
    }

    winningTeamPtr = totalTeam1Goals > totalTeam2Goals ? &leftTeamIngame : &rightTeamIngame;
    EndOfGame();
}

static bool prolongLastMinute()
{
    if (gameStatePl != GameState::kInProgress)
        return true;

    constexpr int kUpperPenaltyAreaLowerLine = 216;
    constexpr int kLowerPenaltyAreaUpperLine = 682;
    constexpr int kCenterLine = 449;

    auto ballY = ballSprite.y;
    auto ballInsidePenaltyArea = ballY <= kUpperPenaltyAreaLowerLine || ballY > kLowerPenaltyAreaUpperLine;
    auto attackingTeam = ballY > kCenterLine ? &leftTeamData : &rightTeamData;
    auto attackInProgress = lastTeamPlayed == attackingTeam;

    return ballInsidePenaltyArea || attackInProgress;
}

static void endFirstExtraTime()
{
    EndFirstExtraTime();
}

static void endSecondExtraTime()
{
    int totalTeam1Goals = team1Goals;
    int totalTeam2Goals = team2Goals;

    if (totalTeam1Goals == totalTeam2Goals) {
        totalTeam1Goals = statsTeam1Goals + 2 * team1GoalsFirstLeg;
        totalTeam2Goals = statsTeam2Goals + 2 * team2GoalsFirstLeg;

        bool gameTied = !secondLeg || !playing2ndGame || totalTeam1Goals == totalTeam2Goals;
        if (gameTied) {
            if (penaltiesState) {
                penaltiesState = -1;
                StartPenalties();
            } else {
                winningTeamPtr = nullptr;
                EndOfGame();
            }
            return;
        }
    }

    winningTeamPtr = totalTeam1Goals > totalTeam2Goals ? &leftTeamIngame : &rightTeamIngame;
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

    currentTimeSprite.x = g_cameraX + kMultiDigitTimeXOffset;

    constexpr int kTimeZOffset = 10'000;
    constexpr int kTimeYOffset = 9;

    currentTimeSprite.y = g_cameraY;
    currentTimeSprite.y += kTimeYOffset + kTimeZOffset;

    currentTimeSprite.z = kTimeZOffset;
}

constexpr int kTimeDigitWidth = 6;

static void copyDigitToTimeSprite(int digit, int xOffset, int destSpriteIndex)
{
    assert(digit >= 0 && digit <= 9);
    assert(spritesIndex[kBigTimeDigitSprite0]->width == kTimeDigitWidth);

    int sourceSpriteIndex = kBigTimeDigitSprite0 + digit;

    copySprite(sourceSpriteIndex, destSpriteIndex, xOffset, 0);
}

static void updateTimeSprite()
{
    int spriteIndex = kTimeSprite8Mins;

    if (gameTime[1]) {
        spriteIndex = kTimeSprite118Mins;
        copyDigitToTimeSprite(gameTime[1], 0, spriteIndex);
        copyDigitToTimeSprite(gameTime[2], kTimeDigitWidth, spriteIndex);
        copyDigitToTimeSprite(gameTime[3], 2 * kTimeDigitWidth, spriteIndex);
    } else if (gameTime[2]) {
        spriteIndex = kTimeSprite88Mins;
        copyDigitToTimeSprite(gameTime[2], 0, spriteIndex);
        copyDigitToTimeSprite(gameTime[3], kTimeDigitWidth, spriteIndex);
    } else {
        copyDigitToTimeSprite(gameTime[3], 0, spriteIndex);
    }

    currentTimeSprite.spriteIndex = spriteIndex;
}

static void updateAndPositionTimeSprite()
{
    updateTimeSprite();
    positionTimeSprite();
}

static void bumpGameTime()
{
    if (++gameTime[3] >= 10) {
        gameTime[3] = 0;
        if (++gameTime[2] >= 10) {
            gameTime[2] = 0;
            gameTime[1]++;
        }
    }
}

static void setupLastMinuteSwitchNextFrame()
{
    gameSeconds = -1;
    endGameCounter = 55;
}

void SWOS::ResetTime()
{
    gameSeconds = 0;
    memset(&gameTime, 0, sizeof(gameTime));
    secondsSwitchAccumulator = 0;
    updateAndPositionTimeSprite();
    currentTimeSprite.visible = 0;
}

static void ensureTimeSpriteIsVisible()
{
    if (!currentTimeSprite.visible) {
        currentTimeSprite.visible = true;
        InitDisplaySprites();
    }
}

void SWOS::UpdateTime()
{
    ensureTimeSpriteIsVisible();

    bool minuteSwitchAboutToHappen = gameSeconds < 0;

    if (minuteSwitchAboutToHappen) {
        endGameCounter -= timerDifference;
        if (endGameCounter < 0) {
            if (auto periodEndHandler = getPeriodEndHandler()) {
                gameSeconds = 0;
                stateGoal = 0;
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
    } else if (gameStatePl == GameState::kInProgress && !playingPenalties) {
        secondsSwitchAccumulator -= timeDelta;
        if (secondsSwitchAccumulator < 0) {
            secondsSwitchAccumulator += 54;
            gameSeconds += timerDifference;

            if (gameSeconds >= 60) {
                gameSeconds = 0;
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
    replaySelected = 0;

    DrawMenu();     // redraw menu so it's ready for the fade-in
    fadeIfNeeded();

    g_cameraX = 0;
    g_cameraY = 0;

    SDL_ShowCursor(SDL_ENABLE);
}

static void replayExitMenuDone(bool replay = false)
{
    replaySelected = replay;
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
