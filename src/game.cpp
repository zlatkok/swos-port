#include "game.h"
#include "render.h"
#include "audio.h"
#include "log.h"
#include "render.h"
#include "options.h"
#include "controls.h"
#include "menu.h"
#include "dump.h"
#include "replays.h"
#include "sprites.h"
#include "replayExit.mnu.h"

constexpr int F_ScrollLeft = 0;
constexpr int F_ScrollRight = 1;

constexpr int F_NoHorizontalScroll = 2;
constexpr int F_FillPatternsRightColumn = 3;
constexpr int F_FillPatternsLeftColumn = 4;

constexpr int F_ScrollUp = 5;
constexpr int F_ScrollDown = 6;

constexpr int F_NoVerticalScroll = 7;
constexpr int F_FillPatternsBottomRow = 8;
constexpr int F_FillPatternsTopRow = 9;

int g_replayViewLoop = 0;

short patternTableHorz[48] = { 0, };
short patternTableVert[32] = { 0, };

short patternTableHorz_PCscreen[32] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	16, -1, 17, -1, 18, -1, 19, -1, 20, -1, 21, -1, 22, -1, 23, -1
};
short patternTableVert_PCscreen[32] = {
	 0,  1,  2,  3,	 4, -1,  5, -1,  6, -1,  7,	-1,  8, -1,  9, -1,
	10, -1, 11, -1, 12, -1, 13, -1, 14, -1, 15, -1, 16, -1, 17, -1
};

short patternTableHorz_Amigascreen[32] = {
	0,  1,   2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, -1, 19, -1, 20, -1, 21, -1, 22, -1, 23, -1, 24, -1
};
short patternTableVert_Amigascreen[32] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, -1, 13, -1,
	14, -1, 15, -1, 16, -1, 17, -1, 18, -1, 19, -1, 20, -1, 21, -1
};

short patternTableHorz_Widescreen[48] = {
	0,  1, -1,   2,  3, -1, 4,  5, -1,   6,  7, -1, 8,  9, -1,  10, 11, 12, 13, 14, -1, 15, 16, -1,
	17, 18, -1, 19, 20, -1, 21, 22, 23, 24, 25, -1, 26, 27, -1, 28, 29, -1, 30, 31, -1, 32, 33, -1
};
short patternTableVert_Widescreen[32] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, -1, 13, -1,
	14, -1, 15, -1, 16, -1, 17, -1, 18, -1, 19, -1, 20, -1, 21, -1
};

int PatternHorzNum_plus4 = 24;
int PatternVertNum_plus5 = 18;

word patternIndex; // A3
word pixelOffset = 0;
word most_xTile;   // leftmost and rightmost x tile
word most_yTile;   //   upmost and downmost  y tile
word xTile, yTile, xScreenTile, yScreenTile;
dword zero01 = 0;

int c_deltaX, c_deltaY;
int  _deltaX,  _deltaY;
int _A0, _A1, _A2, _A3;
word g_fadeMode = 0;
word g_scoresMode = 4;
word g_infoMode = 0;

// Important variables
//      scrollTile*       - Current tile number shown top left
//      scrollTileOffset* - Pixel offsets inside current tile (D5, D6)
//                          (Used together with tile number above)

void FillPatternsRightColumn();
void FillPatternsVertical();
void DrawPatternHorizontal();

void FillPatternsBottomRow();
void FillPatternsHorizontal();
void DrawPatternVertical();

void NoHorizontalScroll();
void FillPatternsLeftColumn();

void NoVerticalScroll();
void FillPatternsTopRow();

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

        if (x < kVgaWidth && y < kVgaHeight && x > -sprite->width && y > -sprite->height) {
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

    if (g_fadeMode == 0) {
        updateScreen_MenuScreen();
    }
    else {
        updateScreen_GameScreen();
    }
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

void SWOS::MainKeysCheck()
{
    dword _D0, _D5;
    TeamGeneralInfo *_temp, *_A1, *_A2;
	
	// New code
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

	// Original code
    if (lastKey == kKeyF2)
        makeScreenshot();	

    if (lastKey == 0) return;
    if (paused != 0) goto test_if_game_paused;
    if (showingStats != 0) goto showing_stats;
    if (playHighlightFlag != 0) goto test_if_game_paused;
    if (normalReplayRunning != 0) goto test_if_game_paused;
    if (convertedKey != 'R') goto not_replay;
    if (skipFade == 0) return;
    if (g_inSubstitutesMenu != 0) return;
    if (gameState == GameState::kResultAfterTheGame) return;
    if (gameState == GameState::kResultOnHalfTime) return;

    instantReplayFlag = 1;
    if (replayState != 0) return;
    userRequestedReplay = 1;
    return;

not_replay:
    if (convertedKey != 'H') goto not_h;
    if (normalReplayRunning != 0) return;
    if (replayState != 0) return;
    if (gameState != GameState::kResultAfterTheGame) return;
    SaveHighlightsPointers();
    return;

not_h:
    if (lastKey != KEY_F9) goto not_f9;
    g_spinBigS = g_spinBigS ^ 1;
    return;

not_f9:
    if (lastKey != KEY_F5) goto not_f5;
    g_infoMode++;
    if (g_infoMode > 2) {
        g_infoMode = 0;
    }
    if (g_infoMode == 0) {
        dseg_168941 = 4;
        g_scoresMode = 4;
    }
    else if (g_infoMode == 1) {
        dseg_168941 = 3;
        g_scoresMode = 4;
    }
    else if (g_infoMode == 2) {
        dseg_168941 = 4;
        g_scoresMode = 3;
    }
    return;

not_f5:
    if (lastKey != KEY_F6) goto not_f6;
    g_scoresMode++;
    if (g_scoresMode > 4) g_scoresMode = 1;
    return;

not_f6:
    if (lastKey != KEY_F7) goto not_f7;
    dseg_168941++;
    if (dseg_168941 > 4) dseg_168941 = 1;
    return;

not_f7:
    if (lastKey != KEY_F4) goto not_f4;
    return;


not_f4:
    if (lastKey != KEY_F10) goto not_f10;
    nullsub_F10();
    if (g_crowdChantsOn != 0) goto turn_off_crowd_chants;
    SWOS::TurnCrowdChantsOn();
    goto crowd_chants_out;

turn_off_crowd_chants:
    SWOS::TurnCrowdChantsOff();

crowd_chants_out:
    return;

not_f10:
    if (lastKey != KEY_F8) goto not_f8;

    if (g_memAllOk == 2) goto cseg_162E1;
    g_muteCommentary = g_muteCommentary ^ 1;

cseg_162E1:
    convertedKey = 0;
    return;

not_f8:
    if (lastKey != KEY_PAGE_UP) goto not_page_up;
    if (replayState != 0) return;
    if (rightTeamData.playerNumber != 0) goto call_bench1;
    if (rightTeamData.plCoachNum == 0) return;

call_bench1:
    bench2Called = 1;
    convertedKey = 0;
    return;

not_page_up:
    if (lastKey != KEY_PAGE_DOWN) goto not_page_down;
    if (replayState != 0) return;

    if (leftTeamData.playerNumber != 0) goto call_bench2;
    if (leftTeamData.plCoachNum == 0) return;

call_bench2:
    bench1Called = 1;
    convertedKey = 0;
    return;

not_page_down:
    if (convertedKey != 'S') goto not_s;
    if (gameStatePl == GameState::kInProgress) return;
    if (g_inSubstitutesMenu != 0) return;
    if (replayState != 0) return;

    if (gameState < GameState::kStartingGame) goto game_halted_allow_stats;
    if (gameState <= GameState::kGameEnded) return;

game_halted_allow_stats:
    AllowStatistics();
    convertedKey = 0;
    return;

showing_stats:
    if (convertedKey != 'S') return;
    StatisticsOff();
    convertedKey = 0;
    return;

not_s:
    if (convertedKey != 'W') goto not_w;
    return;

not_w:
    if (convertedKey != 1) goto not_escape;
    if (replayState != 0) return;
    if (playGame == 0) goto check_highlights;

    playGame = 0;
    gameAborted = 1;
    if (gameStatePl == GameState::kInProgress) goto ongoing_game_interrupted;
    if (gameState == GameState::kPlayersGoingToShower) return;
    if (gameState == GameState::kResultAfterTheGame) return;

ongoing_game_interrupted:
    stateGoal = 0;
    _A1 = &leftTeamData;
    _A2 = &rightTeamData;

    if (_A1->teamNumber == 1) goto cseg_16491;

    _temp = _A1;
    _A1 = _A2;
    _A2 = _temp;

cseg_16491:
    if (_A1->playerNumber != 0) goto player1;
    if (_A1->isPlCoach != 0) goto player_coach1;
    if (_A2->playerNumber != 0) goto punish_player2;
    if (_A2->isPlCoach != 0) goto punish_player2;
    goto two_player_game_cancelled;

player_coach1:
    if (_A2->playerNumber != 0) goto two_player_game_cancelled;
    if (_A2->isPlCoach != 0) goto two_player_game_cancelled;
    goto punish_player1;

player1:
    if (_A2->playerNumber != 0) goto two_player_game_cancelled;
    if (_A2->isPlCoach != 0) goto two_player_game_cancelled;
    goto punish_player1;

two_player_game_cancelled:
    extraTimeState = 0;
    penaltiesState = 0;
    gameCanceled = 1;
    convertedKey = 0;
    return;

punish_player2:
    if (gameTime == 0) goto two_player_game_cancelled;
    _D0 = team2Goals;
    statsTeam1Goals += 5;
    team1Goals += 5;

add_team1_goals_loop:
    if (_D0 < team1Goals) goto team2_loses;
    statsTeam1Goals++;
    team1Goals++;
    goto add_team1_goals_loop;

team2_loses:
    _D0 -= team1Goals;
    _D0 = -1 * _D0;
    _D5 = _D0;
    _D0 = 1;
    AssignFakeGoalsToScorers();

fake_goals_assigned_go_back:
    statsTeam1GoalsCopy = statsTeam1Goals;
    statsTeam2GoalsCopy = statsTeam2Goals;
    extraTimeState = 0;
    convertedKey = 0;
    return;


punish_player1:
    if (gameTime == 0) goto two_player_game_cancelled;
    _D0 = team1Goals;
    statsTeam2Goals += 5;
    team2Goals += 5;

right_team_aborted:
    if (_D0 < team2Goals) goto team1_loses;
    statsTeam2Goals++;
    team2Goals++;
    goto right_team_aborted;

team1_loses:
    _D0 -= team2Goals;
    _D0 = -1 * _D0;
    _D5 = _D0;
    _D0 = 2;
    AssignFakeGoalsToScorers();
    goto fake_goals_assigned_go_back;

check_highlights:
    if (playHighlightFlag == 0) return;
    playHighlightFlag = 0;

    return;

not_escape:
    if (convertedKey != ' ') goto test_if_game_paused;
    if (playHighlightFlag != 0) goto test_if_game_paused;
    if (normalReplayRunning != 0) goto test_if_game_paused;
    if (spaceReplayTimer != 0) return;

    fadeAndSaveReplay = 1;
    spaceReplayTimer = 100;
    return;

test_if_game_paused:
    if (convertedKey != 'P') return;
    if (g_inSubstitutesMenu != 0) return;

    paused = paused ^ 1;
    convertedKey = 0;
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
	
	// for New scoreboard
    if (dseg_168941 == 4) {
        if (g_scoresMode == 4) {
			if (getScreenMode() >= 1) {
				currentTimeSprite.y += 16;
				currentTimeSprite.x += 6;
			}
        }
        else {
            currentTimeSprite.y -= 1000;
            currentTimeSprite.x -= 1000;
        }   
    }
    else {
        currentTimeSprite.y -= 1000;
        currentTimeSprite.x -= 1000;
    }  	
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
    endGameCounter = GS_55_50;
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
            secondsSwitchAccumulator += GS_54_49;
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

void SWOS::FadeIn()
{
    __asm {
        mov ebx, offset g_workingPalette
        mov g_fadeMode, 0
    }
    SAFE_INVOKE(FadeOut);
}

void SWOS::FadeIn2()
{
    __asm {
        mov ebx, offset g_workingPalette
        mov g_fadeMode, 1
    }
    SAFE_INVOKE(FadeOut);
}

static void ShowClockAndScoresFunc() {
    if (g_scoresMode == dseg_168941) return;
    if (g_scoresMode == 1 || g_scoresMode == 2) {
        if (resultTimer != 0) return;
    }
    if (replayState != 0) return;
    if (g_replayViewLoop != 0) return;

    char buf[256];
    char *name1, *name2;
    char _name1[64], _name2[64];
    int _clockTime;
    int ox, oy, x, y;

    name1 = (char *)leftTeamIngame.teamName;
    name2 = (char *)rightTeamIngame.teamName;

    strncpy(_name1, name1, 3); _name1[3] = 0;
    strncpy(_name2, name2, 3); _name2[3] = 0;
    _clockTime = gameTime[1] * 100 + gameTime[2] * 10 + gameTime[3];
    
    if (g_scoresMode == 4) {
        ox = 10;         oy = 5;
    }
    else if (g_scoresMode == 1) {
        ox = 10;         oy = kVgaHeight-5-8;
    }
    else if (g_scoresMode == 2) {
        ox = kVgaWidth-10-70;  oy = kVgaHeight-5-8;
    }
    else if (g_scoresMode == 3) {
        ox = kVgaWidth-10-70;  oy = 5;
    }

    // Show clock
    snprintf(buf, sizeof(buf), "%d'", _clockTime);
    x = 0;
    y = oy + 2;
    printStringEx(buf, x, y, kWhiteText2, ox+19+2, false, ALIGN_RIGHT);

    // Show Score
    snprintf(buf, sizeof(buf), "%d-%d", team1Goals, team2Goals);
    x = 0;
    y = oy + 2;
    printStringEx(buf, x, y, kWhiteText2, (ox+49)*2, false, ALIGN_CENTERX);
}

__declspec(naked) void SWOS::ShowClockAndScores()
{
    __asm {
        call ShowClockAndScoresFunc
        retn
    }
}

// =============================
//      C++ SCROLL ROUTINES
// =============================

void ScrollLeft()
{
    auto _offset = scrollTileOffsetX - 1;   
    if (_offset < 0) {
        _offset = _offset & 0x0f;
        
        scrollTileX--;
        xPitchDatOffset--;  // one pattern column to left   
        copyFirstYTile = scrollTileOffsetY;
    }
    scrollTileOffsetX = _offset;
}

void ScrollRight()
{
    auto _offset = scrollTileOffsetX + 1;
    if (_offset >= 16) {
        _offset = _offset & 0x0f;
        
        scrollTileX++;
        xPitchDatOffset++;  // one pattern column to right  
        copyFirstYTile = scrollTileOffsetY;
    }
    scrollTileOffsetX = _offset;
}

void ScrollUp()
{
    if (yPitchDatOffset >= 0) {
        auto _offset = scrollTileOffsetY - 1;
        if (_offset < 0) {
            _offset = _offset & 0x0f;
            
            scrollTileY--;
            yPitchDatOffset--;  // one pattern row to up
            copyFirstXTile = scrollTileOffsetX;
        }
        scrollTileOffsetY = _offset;
    }
}

void ScrollDown()
{
    auto _offset = scrollTileOffsetY + 1;
    if (_offset >= 16) {
        _offset = _offset & 0x0f;

        scrollTileY++;
        yPitchDatOffset++;  // one pattern row to down
        copyFirstXTile = scrollTileOffsetX;
    }
    scrollTileOffsetY = _offset;
}


void NoHorizontalScroll()
{
    FillPatternsRightColumn();
    FillPatternsLeftColumn();
}

// in:
//      most_xTile - x tile offset from current position
void FillPatternsRightColumn()
{
    most_xTile = PatternHorzNum_plus4 - 1;   // right most x tile
    FillPatternsVertical();
}

void FillPatternsLeftColumn()
{
    most_xTile = 0;    // left most x tile  
    FillPatternsVertical();
}

// in:
//      pixelOffset - current pattern pixel offset (0..15)
void FillPatternsVertical()
{
    // pixelOffset = scrollTileOffsetX or CopyFirstXTile
    pixelOffset = pixelOffset & 0x0f;   
    
    // 2 * scroll offset as index (even index)
    patternIndex = pixelOffset * 2;
    
    DrawPatternHorizontal();
    DrawPatternHorizontal();
}

//  in:
//      most_xTile   - x tile offset from current position
//      patternIndex - tile y offset (table)
void DrawPatternHorizontal()
{
    short _tile = patternTableVert[patternIndex];

    patternIndex++;     // increase for next call

    if (_tile >= 0) {
        // yScreenTile = y screen coordinate / 16
        yScreenTile = scrollTileY;                  
        yScreenTile += _tile;                       
        
        // xScreenTile = x screen coordinate / 16
        xScreenTile = scrollTileX;                  
        xScreenTile += most_xTile;      

        // xTile = x index in g_pitchDatBuffer
        xTile = xPitchDatOffset;                    
        xTile += most_xTile;                       
        
        // yTile = y index in g_pitchDatBuffer
        yTile = yPitchDatOffset;                       
        yTile += _tile;                             
        
        D1 = xTile;
        D2 = yTile;
        D3 = xScreenTile;
        D4 = yScreenTile;
        SWOS::DrawBackPattern();
    }   
}


void NoVerticalScroll()
{
    FillPatternsBottomRow();    
    FillPatternsTopRow();
}

void FillPatternsBottomRow()
{
    most_yTile = PatternVertNum_plus5 - 1;   // down most y tile
    FillPatternsHorizontal();
}

void FillPatternsTopRow()
{
    most_yTile = 0;  // up most y tile
    FillPatternsHorizontal();
}

// in:
//      pixelOffset - current pattern pixel offset (0..15)
void FillPatternsHorizontal()
{
    // pixelOffset = scrollTileOffsetY or CopyFirstYTile
    
    // 2 * scroll offset as index (even index)
    patternIndex = pixelOffset * DrawPatternVerticalCount;
    
    for (auto i = 1; i <= DrawPatternVerticalCount; i++)
        DrawPatternVertical();
}

//  in:
//      most_yTile   - y tile offset from current position
//      patternIndex - tile x offset (table)
void DrawPatternVertical()
{
    short _tile = patternTableHorz[patternIndex];

    // increase for next call
    patternIndex++;

    if (_tile >= 0) {

        // xScreenTile = x screen coordinate / 16
        xScreenTile = scrollTileX;
        xScreenTile += _tile;
        
        // yScreenTile = y screen coordinate / 16
        yScreenTile = scrollTileY;
        yScreenTile += most_yTile;
        
        // xTile = x index in g_pitchDatBuffer
        xTile = xPitchDatOffset;
        xTile += _tile;

        // yTile = y index in g_pitchDatBuffer
        yTile = yPitchDatOffset;
        yTile += most_yTile;            
        
        D1 = xTile;
        D2 = yTile;
        D3 = xScreenTile;
        D4 = yScreenTile;
        SWOS::DrawBackPattern();        
    }
}


void call_A0()
{
    if (_A0 == F_ScrollLeft)                ScrollLeft();
    if (_A0 == F_ScrollRight)               ScrollRight();
}

void call_A1()
{
    if (_A1 == F_NoHorizontalScroll)        NoHorizontalScroll();
    if (_A1 == F_FillPatternsRightColumn)   FillPatternsRightColumn();
    if (_A1 == F_FillPatternsLeftColumn)    FillPatternsLeftColumn();
}

void call_A2()
{
    if (_A2 == F_ScrollUp)                  ScrollUp();
    if (_A2 == F_ScrollDown)                ScrollDown();
}

void call_A3()
{
    if (_A3 == F_NoVerticalScroll)          NoVerticalScroll();
    if (_A3 == F_FillPatternsBottomRow)     FillPatternsBottomRow();
    if (_A3 == F_FillPatternsTopRow)        FillPatternsTopRow();
}


// Loop scrolling pixel by pixel until we arrive at the desired screen location
void SWOS::ScrollPitch()
{
    // Scroll left or right
    if (_deltaX == 0) {
        _A0 = F_ScrollLeft;
        _A1 = F_NoHorizontalScroll;
    }
    else {
        _A0 = F_ScrollRight;
        _A1 = F_FillPatternsRightColumn;
        if (_deltaX < 0) {
            _deltaX *= -1;
            _A0 = F_ScrollLeft;
            _A1 = F_FillPatternsLeftColumn;
        }
    }

    // Scroll down or up
    if (_deltaY == 0) {
        _A2 = F_ScrollDown;
        _A3 = F_NoVerticalScroll;
    }
    else {
        _A2 = F_ScrollDown;
        _A3 = F_FillPatternsBottomRow;
        if (_deltaY < 0) {
            _deltaY *= -1;
            _A2 = F_ScrollUp;
            _A3 = F_FillPatternsTopRow;
        }
    }

    // check if need drawing - loop
    while (true) {
        // return if _deltaX == _deltaY (scrolling done)
        if (_deltaX == 0 && _deltaY == 0) break;

        // _deltaX != 0, first call (ScrollLeft or ScrollRight)
        if (_deltaX != 0) {
            call_A0();
            _deltaX--;

            // second call, actual drawing
            // (NoHorizontalScroll or FillPatternRightColumn or FillPatternLeftColumn)
            pixelOffset = scrollTileOffsetX;
            call_A1();
        }

        // delta y != 0, first call (ScrollUp or ScrollDown)
        if (_deltaY != 0) {
            call_A2();
            _deltaY--;

            // second call, actual drawing
            // (NoVerticalScroll or FillPatternBottomRow or FillPatternsTopRow)
            pixelOffset = scrollTileOffsetY;
            call_A3();
        }

        pixelOffset = copyFirstXTile;       
        if (pixelOffset >= 0) {
            call_A1();
            //copyFirstXTile = -1;
        }
        
        pixelOffset = copyFirstYTile;
        if (pixelOffset >= 0) {
            call_A3();
            //copyFirstYTile = -1;
        }
    }
}

// Important variables and numbers
//  scrollTile* variables mark current tile number shown top left
//  scrollTileOffset* are pixel offsets inside current tile
//  (used together with tile number above)
//  they are set by table - control scroll algorithm
//
//  384 * 16 + 16: 1 empty row on top, and 1 empty column on the left
//                (actually they get filled while scrolling)
//
// Following lines should be added in @export section in symbols.txt
// vscreenDeltaY should be dword (not word).
//
//  vscreenDeltaX, word
//  vscreenDeltaY, dword
//  scrollTileX, word    - current tile number shown top left
//  scrollTileY, word
//  scrollTileOffsetX, word - pixel offsets inside current tile (used together with tile number above)
//  scrollTileOffsetY, word
void SWOS::UpdateVisibleScreenPtr()
{
    vscreenDeltaX = scrollTileX * 16 + scrollTileOffsetX;
    vscreenDeltaY = kGameScreenWidth * (scrollTileY * 16 + scrollTileOffsetY);
    vsPtr = linAdr384k + vscreenDeltaX + vscreenDeltaY + kGameScreenWidth * 16 + 16;
}

//  Sets view to cameraX and cameraY.
//  Updates oldCameraX and oldCameraY.
void SWOS::ScrollToCurrent()
{
    cameraCoordinatesInvalid = 1;

    c_deltaX = g_cameraX - g_oldCameraX;
    c_deltaY = g_cameraY - g_oldCameraY;

    _deltaX = c_deltaX;
    _deltaY = c_deltaY;
    
    SWOS::ScrollPitch();
    SWOS::UpdateVisibleScreenPtr();

    g_oldCameraX = g_cameraX;
    g_oldCameraY = g_cameraY;
}

/*
void SWOS::UpdateTime_385()
{   
    if (dseg_168941 == 4) {
        if (g_scoresMode == 4) {
            currentTimeSprite.y -= -8;
            currentTimeSprite.x -= 0;
            
            currentTimeSprite.y += 8;
            currentTimeSprite.x += 6;
        }
        else {
            currentTimeSprite.y -= 1000;
            currentTimeSprite.x -= 1000;
        }   
    }
    else {
        currentTimeSprite.y -= 1000;
        currentTimeSprite.x -= 1000;
    }   
}
*/

void SWOS::SetCurrentPlayerName_163()
{
	if (getScreenMode() >= 1) {
		currentPlayerNameSprite.y += 7;
		if (g_scoresMode == 4) {
			currentPlayerNameSprite.y += 7;
		}
	}
	else {
		if (isNewScoreboard() && (dseg_168941 == 4) && (g_scoresMode == 3)) {
		}
		else {
			if ((dseg_168941 == 4) && (g_scoresMode == 4)) {
			}
			else {
				currentPlayerNameSprite.y += 7;
			}
		}
	}
}