#include "game.h"
#include "gameLoop.h"
#include "windowManager.h"
#include "audio.h"
#include "music.h"
#include "chants.h"
#include "comments.h"
#include "sfx.h"
#include "options.h"
#include "controls.h"
#include "gameControls.h"
#include "keyBuffer.h"
#include "dump.h"
#include "util.h"
#include "replays.h"
#include "pitch.h"
#include "bench.h"
#include "benchControls.h"
#include "sprites.h"
#include "gameSprites.h"
#include "gameTime.h"
#include "playerNameDisplay.h"
#include "spinningLogo.h"
#include "result.h"
#include "stats.h"
#include "random.h"
#include "menus.h"
#include "drawMenu.h"
#include "versusMenu.h"
#include "stadiumMenu.h"

static TeamGame m_topTeamSaved;
static TeamGame m_bottomTeamSaved;

static bool m_gamePaused;

static bool m_blockZoom;

static void saveTeams();
static void restoreTeams();
static void cancelGame();
static void initializeIngameTeamsAndStartGame(TeamFile *team1, TeamFile *team2, int minSubs, int maxSubs, int paramD7);
static void processPostGameData(TeamFile *team1, TeamFile *team2, int paramD7);
static void initPlayerCardChance();
static void determineStartingTeamAndTeamPlayingUp();
static void initPitchBallFactors();
static void initGameVariables();

// Initializes everything except the sprite graphics, which are needed for the stadium menu.
void initMatch(TeamGame *topTeam, TeamGame *bottomTeam, bool saveOrRestoreTeams)
{
    saveOrRestoreTeams ? saveTeams() : restoreTeams();

    initMatchSprites(topTeam, bottomTeam);
//    SetPitchTypeAndNumber();
    loadPitch();
    //InitAdvertisements

    if (!replayingNow()) {
        swos.topTeamPtr = topTeam;
        swos.bottomTeamPtr = bottomTeam;

        initPlayerCardChance();

        swos.gameRandValue = SWOS::rand();

        A4 = topTeam;
        invokeWithSaved68kRegisters(ApplyTeamTactics);
        A4 = bottomTeam;
        invokeWithSaved68kRegisters(ApplyTeamTactics);

        determineStartingTeamAndTeamPlayingUp();

//arrange pitch type pattern coloring
        initPitchBallFactors();
        initGameVariables();

        initDisplaySprites();
        resetGameTime();
        resetResult(topTeam->teamName, bottomTeam->teamName);
        initStats();
//    InitAnimatedPatterns();
        StartingMatch();

        if (!swos.g_trainingGame)
            swos.showFansCounter = 100;

        loadCommentary();
    }

    loadSoundEffects();
    loadIntroChant();

    m_blockZoom = false;
}

void initializeIngameTeams(int minSubs, int maxSubs, TeamFile *team1, TeamFile *team2)
{
    swos.gameMinSubstitutes = minSubs;
    swos.gameMaxSubstitutes = maxSubs;
    swos.gameTeam1 = team1;
    swos.gameTeam2 = team2;

    A0 = team1;
    D0 = maxSubs;
    cseg_8F66C();

    A0 = team2;
    D0 = maxSubs;
    cseg_8F66C();

    swos.teamsLoaded = 0;
    swos.poolplyrLoaded = 0;

    swos.team1Computer = 0;
    swos.team2Computer = 0;

    if (team1->teamControls == kComputerTeam)
        swos.team1Computer = -1;
    if (team2->teamControls == kComputerTeam)
        swos.team2Computer = -1;

    D0 = team1->prShirtType;
    D1 = team1->prStripesColor;
    D2 = team1->prBasicColor;
    D3 = team1->prShortsColor;
    D4 = team1->prSocksColor;
    A1 = &swos.topTeamIngame;
    SetTeamSecondaryColors();

    D0 = team2->prShirtType;
    D1 = team2->prStripesColor;
    D2 = team2->prBasicColor;
    D3 = team2->prShortsColor;
    D4 = team2->prSocksColor;
    A1 = &swos.bottomTeamIngame;
    SetTeamSecondaryColors();

    A2 = team1;
    A3 = team2;
    cseg_2FB5E();

    if (swos.g_gameType != kGameTypeCareer && swos.g_allPlayerTeamsEqual) {
        GetAveragePlayerPriceInSelectedTeams();
        swos.averagePlayerPrice = D0;
    }

    A2 = team1;
    A4 = &swos.topTeamIngame;
    A6 = &swos.team1AppPercent;
    InitIngameTeamStructure();

    A2 = team2;
    A4 = &swos.bottomTeamIngame;
    A6 = &swos.team2AppPercent;
    InitIngameTeamStructure();

    if (swos.isGameFriendly) {
        swos.team1NumAllowedInjuries = 4;
        swos.team2NumAllowedInjuries = 4;
    } else {
        int maxInjuries = swos.g_gameType == kGameTypeCareer ? 4 : 2;

        D0 = -1;
        D1 = 0;
        A0 = team1;
        GetNumberOfAvailablePlayers();

        swos.team1NumAllowedInjuries = std::max(0, maxInjuries - D6.asWord());

        D0 = -1;
        D1 = 0;
        A0 = team2;
        GetNumberOfAvailablePlayers();

        swos.team2NumAllowedInjuries = std::max(0, maxInjuries - D6.asWord());
    }

    if (showPreMatchMenus()) {
        showVersusMenu(&swos.topTeamIngame, &swos.bottomTeamIngame, swos.gameName, swos.gameRound, []() {
            loadStadiumSprites(&swos.topTeamIngame, &swos.bottomTeamIngame);
        });
    }

    if (!swos.isGameFriendly && swos.g_gameType != kGameTypeDiyCompetition)
        swos.ingameGameLength = 0;
    else
        swos.ingameGameLength = swos.g_gameLength;
}

void matchEnded()
{
    finishCurrentReplay();
}

void startMainGameLoop()
{
    startFadingOutMusic();

    initGameControls();
    initNewReplay();
    updateCursor(true);

    gameLoop(&swos.topTeamIngame, &swos.bottomTeamIngame);

    updateCursor(false);
}

void checkGlobalKeyboardShortcuts(SDL_Scancode scancode, bool pressed)
{
    static SDL_Scancode lastScancode;

    switch (scancode) {
    case SDL_SCANCODE_F1:
        // preserve alt-F1, ultra fast exit from SWOS (actually meant for invoking the debugger ;))
        if (pressed && (SDL_GetModState() & KMOD_ALT)) {
            logInfo("Shutting down via keyboard shortcut...");
            std::exit(EXIT_SUCCESS);
        }
        break;
    case SDL_SCANCODE_F2:
        if (pressed && scancode != lastScancode)
            makeScreenshot();
        break;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        {
            auto mod = SDL_GetModState();
            if (pressed && (mod & KMOD_ALT))
                (mod & KMOD_SHIFT) ? toggleFullScreenMode() : toggleBorderlessMaximizedMode();

            updateCursor(isMatchRunning());
        }
        break;
    }

    lastScancode = scancode;
}

bool checkGameKeys()
{
    bool zoomChanged = checkZoomKeys();

    auto key = getKey();

    if (key == SDL_SCANCODE_UNKNOWN || testForPlayerKeys(key))
        return zoomChanged;

    if (key == SDL_SCANCODE_D) {
        toggleDebugOutput();
        return zoomChanged;
    }

    if (isGamePaused()) {
        if (key == SDL_SCANCODE_P && !inBench())
            m_gamePaused = false;
    } else if (showingUserRequestedStats()) {
        if (key == SDL_SCANCODE_S)
            hideStats();
    } else if (!replayingNow()) {
        switch (key) {
        case SDL_SCANCODE_P:
            if (!inBench())
                togglePause();
            break;
        case SDL_SCANCODE_H:
            if (swos.gameState == GameState::kResultAfterTheGame)
                requestFadeAndReplayHighlights();
            break;
        case SDL_SCANCODE_R:
            if (!inBench() && swos.gameState != GameState::kResultAfterTheGame &&
                swos.gameState != GameState::kResultOnHalftime)
                requestFadeAndInstantReplay();
            break;
        case SDL_SCANCODE_S:
            if (swos.gameStatePl != GameState::kInProgress && !inBench() &&
                (swos.gameState < GameState::kStartingGame || swos.gameState > GameState::kGameEnded))
                toggleStats();
            break;
        case SDL_SCANCODE_SPACE:
            {
                constexpr Uint32 kSaveReplayRequestCooldown = 1'500;
                static Uint32 lastSaveRequestTime;
                auto now = SDL_GetTicks();
                if (now - lastSaveRequestTime > kSaveReplayRequestCooldown) {
                    requestFadeAndSaveReplay();
                    lastSaveRequestTime = now;
                }
            }
            break;
        case SDL_SCANCODE_F8:
            toggleMuteCommentary();
            break;
        case SDL_SCANCODE_F9:
            toggleSpinningLogo();
            break;
        case SDL_SCANCODE_F10:
            {
                bool chantsEnabled = areCrowdChantsEnabled();
                setCrowdChantsEnabled(!chantsEnabled);
            }
            break;
        case SDL_SCANCODE_PAGEUP:
            if (swos.bottomTeamData.playerNumber || swos.bottomTeamData.plCoachNum)
                requestBench2();
            break;
        case SDL_SCANCODE_PAGEDOWN:
            if (swos.topTeamData.playerNumber || swos.topTeamData.plCoachNum)
                requestBench1();
            break;
        case SDL_SCANCODE_ESCAPE:
            cancelGame();
            break;
        }
    }

    return zoomChanged;
}

bool checkZoomKeys()
{
    constexpr auto kZoomStep = .25f;

    auto keys = SDL_GetKeyboardState(nullptr);
    bool controlHeld = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
    bool shiftHeld = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    bool zoomKeysHeld = keys[SDL_SCANCODE_KP_PLUS] || keys[SDL_SCANCODE_KP_MINUS];

    if ((controlHeld || shiftHeld) && zoomKeysHeld) {
        if (m_blockZoom)
            return false;
        m_blockZoom = true;
    } else {
        m_blockZoom = false;
    }

    auto step = controlHeld ? kZoomStep : 0;

    if (keys[SDL_SCANCODE_KP_PLUS])
        return zoomIn(step);
    else if (keys[SDL_SCANCODE_KP_MINUS])
        return zoomOut(step);
    else
        return false;
}

void updateCursor(bool matchRunning)
{
    bool fullScreen = getWindowMode() != kModeWindow;
    bool disableCursor = matchRunning && fullScreen && !gotMousePlayer();
    SDL_ShowCursor(disableCursor ? SDL_DISABLE : SDL_ENABLE);
}

bool isGamePaused()
{
    return m_gamePaused;
}

void pauseTheGame()
{
    m_gamePaused = true;
}

void togglePause()
{
    m_gamePaused = !m_gamePaused;
}

static void saveTeams()
{
    m_topTeamSaved = swos.topTeamIngame;
    m_bottomTeamSaved = swos.bottomTeamIngame;
}

static void restoreTeams()
{
    swos.topTeamIngame = m_topTeamSaved;
    swos.bottomTeamIngame = m_bottomTeamSaved;
}

static void rigTheScoreForPlayerToLose(int playerNo)
{
    bool team1Player = playerNo == 1;
    auto& loserTotalGoals = team1Player ? swos.team1TotalGoals : swos.team2TotalGoals;
    auto& loserStatGoals = team1Player ? swos.statsTeam1Goals : swos.statsTeam1Goals;
    auto& loserStatGoalsCopy = team1Player ? swos.statsTeam1GoalsCopy : swos.statsTeam1GoalsCopy;
    auto& winnerTotalGoals = team1Player ? swos.team2TotalGoals : swos.team1TotalGoals;
    auto& winnerStatGoals = team1Player ? swos.statsTeam2Goals : swos.statsTeam1Goals;
    auto& winnerStatsGoalsCopy = team1Player ? swos.statsTeam2GoalsCopy : swos.statsTeam1GoalsCopy;

    winnerTotalGoals += 5;
    winnerStatGoals += 5;
    while (loserTotalGoals >= winnerTotalGoals) {
        winnerTotalGoals++;
        winnerStatGoals++;
    }

    D0 = playerNo;
    D5 = winnerTotalGoals - loserTotalGoals;
    AssignFakeGoalsToScorers();

    winnerStatsGoalsCopy = winnerStatGoals;
    loserStatGoalsCopy = loserStatGoals;
}

static void cancelGame()
{
    if (swos.playGame) {
        swos.playGame = 0;
        if (swos.gameState == GameState::kInProgress ||
            swos.gameState != GameState::kPlayersGoingToShower && swos.gameState != GameState::kResultAfterTheGame) {
            swos.stateGoal = 0;
            auto team1 = &swos.topTeamData;
            auto team2 = &swos.bottomTeamData;
            if (swos.topTeamData.teamNumber != 1)
                std::swap(team1, team2);

            bool team1Cpu = !team1->playerNumber && !team1->isPlCoach;
            bool team2Cpu = !team2->playerNumber && !team2->isPlCoach;
            bool playerVsCpu = team1Cpu && !team2Cpu || !team1Cpu && team2Cpu;

            if (playerVsCpu && !gameAtZeroMinute()) {
                int teamNo = team1Cpu ? 2 : 1;
                rigTheScoreForPlayerToLose(teamNo);
            } else {
                swos.gameCanceled = 1;
            }
            swos.extraTimeState = 0;
            swos.penaltiesState = 0;
        }
    }
}

static void initializeIngameTeamsAndStartGame(TeamFile *team1, TeamFile *team2, int minSubs, int maxSubs, int paramD7)
{
    initializeIngameTeams(minSubs, maxSubs, team1, team2);
    saveCurrentMenuAndStartGameLoop();
    processPostGameData(team1, team2, paramD7);

    startMenuSong();
    enqueueMenuFadeIn();
}

static void processPostGameData(TeamFile *team1, TeamFile *team2, int paramD7)
{
    if (!swos.gameCanceled) {
        if (!swos.g_trainingGame) {
            A1 = team1;
            A2 = team2;
            D0 = swos.plg_D3_param;
            cseg_30BD1();

            if (paramD7 < 0) {
                return;
            } else {
                for (const auto teamData : { std::make_pair(team1, &swos.topTeamIngame), std::make_pair(team2, &swos.bottomTeamIngame) }) {
                    auto teamFile = teamData.first;
                    auto teamGame = teamData.second;

                    A1 = teamGame;
                    A2 = teamFile;
                    D7 = paramD7;
                    cseg_2F3AB();

                    A1 = teamGame;
                    A2 = teamFile;
                    UpdatePlayerInjuries();

                    A1 = teamGame;
                    A2 = teamFile;
                    cseg_2F194();

                    A1 = teamGame;
                    A2 = teamFile;
                    cseg_2F0E2();
                }
            }
        } else {
            swos.gameTeam1->andWith0xFE |= 1;
            swos.gameTeam2->andWith0xFE |= 1;

            A1 = &swos.topTeamIngame;
            A2 = team1;
            UpdatePlayerInjuries();

            A1 = &swos.bottomTeamIngame;
            A2 = team2;
            UpdatePlayerInjuries();
        }
    }
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

    int chanceIndex = (SWOS::rand() & 0x1e) >> 1;

    swos.playerCardChance = chanceTable[chanceIndex];
}

static void determineStartingTeamAndTeamPlayingUp()
{
    swos.teamPlayingUp = (SWOS::rand() & 1) + 1;
    swos.teamStarting = (SWOS::rand() & 1) + 1;
}

static void initPitchBallFactors()
{
    static const int kPitchBallSpeedInfluence[] = { -3, 4, 1, 0, 0, -1, -1 };
    static const int kBallSpeedBounceFactorTable[] = { 24, 80, 80, 72, 64, 40, 32 };
    static const int kBallBounceFactorTable[] = { 88, 112, 104, 104, 96, 88, 80 };

    int pitchType = swos.pitchNumberAndType & 0xff;
    assert(static_cast<size_t>(pitchType) <= 6);

    swos.pitchBallSpeedFactor = kPitchBallSpeedInfluence[pitchType];
    swos.ballSpeedBounceFactor = kBallSpeedBounceFactorTable[pitchType];
    swos.ballBounceFactor = kBallBounceFactorTable[pitchType];
}

static void initGameVariables()
{
    swos.playingPenalties = 0;
    swos.dontShowScorers = 0;
    swos.statsTimer = 0;
    swos.g_waitForPlayerToGoInTimer = 0;
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

// in:
//      D0 = 1
//      D1 = min substitutes
//      D2 = max substitutes
//      D3 = 0
//      D7 = -1
//      A1 -> team 1 (structures from file)
//      A2 -> team 2
//
void SWOS::InitializeIngameTeamsAndStartGame()
{
    swos.plg_D0_param = D0;
    swos.plg_D3_param = D3;

    invokeWithSaved68kRegisters([]() {
        initializeIngameTeamsAndStartGame(A1, A2, D1, D2, D7);
    });

    SwosVM::ax = swos.gameCanceled;
    SwosVM::flags.zero = !SwosVM::ax;
}

void SWOS::EndProgram()
{
    std::exit(EXIT_FAILURE);
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
