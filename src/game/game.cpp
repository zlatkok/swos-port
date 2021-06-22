#include "game.h"
#include "windowManager.h"
#include "audio.h"
#include "options.h"
#include "controls.h"
#include "dump.h"
#include "util.h"
#include "replays.h"
#include "pitch.h"
#include "sprites.h"
#include "gameTime.h"
#include "result.h"

constexpr Uint32 kMinimumPreMatchScreenLength = 1'200;

static TeamGame m_topTeamSaved;
static TeamGame m_bottomTeamSaved;

static Uint32 m_loadingStartTick;

static bool m_playingMatch;

static void initGame();
static void initPlayerCardChance();
static void determineStartingTeamAndTeamPlayingUp();
static void initTimeDelta();
static void initPitchBallFactors();
static void initGameVariables();

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

void updateCursor(bool matchRunning)
{
    bool fullScreen = getWindowMode() != kModeWindow;
    bool disableCursor = matchRunning && fullScreen && !gotMousePlayer();
    SDL_ShowCursor(disableCursor ? SDL_DISABLE : SDL_ENABLE);
}

bool isMatchRunning()
{
    return m_playingMatch;
}

// in:
//      A5 -> left team in-game struct
//      A6 -> right team in-game struct
//
void SWOS::InitGameSaveTeams()
{
    swos.topTeamPtr = A5.as<TeamGame *>();
    swos.bottomTeamPtr = A6.as<TeamGame *>();

    m_topTeamSaved = swos.topTeamIngame;
    m_bottomTeamSaved = swos.bottomTeamIngame;

    InitGameRestoreTeams();
}

// in:
//      A5 -> left team in-game struct
//      A6 -> right team in-game struct
//
void SWOS::InitGameRestoreTeams()
{
    swos.topTeamIngame = m_topTeamSaved;
    swos.bottomTeamIngame = m_bottomTeamSaved;

    initGame();
}

static void resetGoalAttempt()
{
    swos.isGoalAttempt = 0;
}

static void initGame()
{
    initGameSprites(swos.topTeamPtr, swos.bottomTeamPtr);
    loadPitch();
    //InitAdvertisements
    //CopyPlayerNameLengths
    initPlayerCardChance();

    Rand();
    swos.gameRandValue = D0;

    if (swos.hilStarted != 1) {
        A4 = &swos.topTeamIngame;

        save68kRegisters();
        ChangeTeamTactics();
        restore68kRegisters();

        A4 = &swos.bottomTeamIngame;

        save68kRegisters();
        ChangeTeamTactics();
        restore68kRegisters();
    }

    determineStartingTeamAndTeamPlayingUp();
    initTimeDelta();
    //arrange pitch type pattern coloring
    initPitchBallFactors();
    initGameVariables();

    InitDisplaySprites();
    resetTime();
    resetResult();
    ClearSaveFlagForGoalSprites();
    resetGoalAttempt();
//    InitAnimatedPatterns();
    StartingMatch();

    if (!swos.g_trainingGame)
        swos.showFansCounter = 100;
}

static void initPlayerCardChance()
{
    assert(swos.ingameGameLength <= 3);

    // when the foul is made 4 bits (1-4) from stoppageTimer are extracted and compared to this value;
    // if greater the player gets a card (yellow or red)
    static const int kPlayerCardChancesPerGameLength[16][4] = {
        4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 8, 9, 10,    // 3 min
        2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 6,     // 5 min
        1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4,     // 7 min
        1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,     // 10 min
    };

    auto chanceTable = kPlayerCardChancesPerGameLength[swos.ingameGameLength];

    Rand();
    int chanceIndex = (D0.asWord() & 0x1e) >> 1;

    swos.playerCardChance = chanceTable[chanceIndex];
}

static void determineStartingTeamAndTeamPlayingUp()
{
    Rand();
    swos.teamPlayingUp = (D0.asWord() & 1) + 1;

    Rand();
    swos.teamStarting = (D0.asWord() & 1) + 1;
}

static void initTimeDelta()
{
    static const int kGameLenSecondsTable[] = { 30, 18, 12, 9 };
    assert(swos.ingameGameLength <= 3);
    swos.timeDelta = kGameLenSecondsTable[swos.ingameGameLength];
}

static void initPitchBallFactors()
{
    static const int kPitchBallSpeedInfluence[] = { -3, 4, 1, 0, 0, -1, -1 };
    static const int kBallSpeedBounceFactorTable[] = { 24, 80, 80, 72, 64, 40, 32 };
    static const int kBallBounceFactorTable[] = { 88, 112, 104, 104, 96, 88, 80 };

    int pitchNumber = swos.pitchNumberAndType & 0xff;
    assert(static_cast<size_t>(pitchNumber) <= 6);

    swos.pitchBallSpeedFactor = kPitchBallSpeedInfluence[pitchNumber];
    swos.ballSpeedBounceFactor = kBallSpeedBounceFactorTable[pitchNumber];
    swos.ballBounceFactor = kBallBounceFactorTable[pitchNumber];
}

static void initGameVariables()
{
    swos.playingPenalties = 0;
    swos.dontShowScorers = 0;
    swos.statsTimeout = 0;
    swos.showStats = 0;
    swos.showingStats = 0;
    swos.controlsSwapped = 0;
    swos.g_inSubstitutesMenu = 0;
    swos.cameraLeavingSubsTimer = 0;
    swos.benchOff = 0;
    swos.g_leavingSubsMenu = 0;
    swos.waitForPlayerToGoInTimer = 0;
    swos.g_substituteInProgress = 0;
    swos.statsTeam1Goals = 0;
    swos.team1GoalsDigit1 = 0;
    swos.team1GoalsDigit2 = 0;
    swos.statsTeam2Goals = 0;
    swos.team2GoalsDigit1 = 0;
    swos.team2GoalsDigit2 = 0;

    swos.team1TotalGoals = 0;
    swos.team2TotalGoals = 0;
    if (swos.secondLeg) {
        swos.team1TotalGoals = swos.team1GoalsFirstLeg;
        swos.team2TotalGoals = swos.team2GoalsFirstLeg;
    }

    swos.team1NumSubs = 0;
    swos.team2NumSubs = 0;
    swos.team1GoalkeeperReplaced = 0;
    swos.team2GoalkeeperReplaced = 0;

    memset(&swos.team1StatsData, 0, sizeof(TeamStatsData));
    memset(&swos.team2StatsData, 0, sizeof(TeamStatsData));
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

void SWOS::EndProgram()
{
    std::exit(EXIT_FAILURE);
}

void SWOS::StartMainGameLoop()
{
    m_playingMatch = true;

    initGameControls();
    initNewReplay();
    updateCursor(true);

    A5 = &swos.topTeamIngame;
    A6 = &swos.bottomTeamIngame;

    swos.EGA_graphics = 0;

    GameLoop();

    updateCursor(false);

    m_playingMatch = false;
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
