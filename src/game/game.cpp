#include "game.h"
#include "gameLoop.h"
#include "windowManager.h"
#include "render.h"
#include "audio.h"
#include "music.h"
#include "chants.h"
#include "comments.h"
#include "sfx.h"
#include "options.h"
#include "controls.h"
#include "amigaMode.h"
#include "gameControls.h"
#include "keyBuffer.h"
#include "dump.h"
#include "util.h"
#include "replays.h"
#include "pitch.h"
#include "pitchConstants.h"
#include "direction.h"
#include "bench.h"
#include "updateBench.h"
#include "team.h"
#include "player.h"
#include "sprites.h"
#include "gameSprites.h"
#include "animation.h"
#include "gameTime.h"
#include "ball.h"
#include "referee.h"
#include "playerNameDisplay.h"
#include "spinningLogo.h"
#include "result.h"
#include "stats.h"
#include "random.h"
#include "menus.h"
#include "drawMenu.h"
#include "versusMenu.h"
#include "stadiumMenu.h"

constexpr int kWheelZoomFrames = 6;

static TeamGame m_topTeamSaved;
static TeamGame m_bottomTeamSaved;

static int16_t m_savedTeam1Goals;
static int16_t m_savedTeam2Goals;
static int16_t m_team1PenaltyShooterIndex;
static int16_t m_team2PenaltyShooterIndex;
static int16_t m_team1PenaltyAttempts;
static int16_t m_team2PenaltyAttempts;

static bool m_gamePaused;

static bool m_blockZoom;
static int m_zoomFrames;

static void saveTeams();
static void restoreTeams();
static void cancelGame();
static void initializeIngameTeamsAndStartGame(TeamFile *team1, TeamFile *team2, int minSubs, int maxSubs, int paramD7);
static void processPostGameData(TeamFile *team1, TeamFile *team2, int paramD7);
static void initPlayerCardChance();
static void determineStartingTeamAndTeamPlayingUp();
static void initPitchBallFactors();
static void initGameVariables();
static void startingMatch();

// Initializes everything except the sprite graphics, which are needed for the stadium menu.
void initMatch(TeamGame *topTeam, TeamGame *bottomTeam, bool saveOrRestoreTeams)
{
    saveOrRestoreTeams ? saveTeams() : restoreTeams();

    initMatchSprites(topTeam, bottomTeam);
    setPitchTypeAndNumber();
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
        startingMatch();

        if (!swos.g_trainingGame)
            swos.showFansCounter = 100;

        loadCommentary();
    }

    loadSoundEffects();
    loadIntroChant();

    m_blockZoom = false;
    m_zoomFrames = 0;
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
    A1 = &swos.topTeamInGame;
    SetTeamSecondaryColors();

    D0 = team2->prShirtType;
    D1 = team2->prStripesColor;
    D2 = team2->prBasicColor;
    D3 = team2->prShortsColor;
    D4 = team2->prSocksColor;
    A1 = &swos.bottomTeamInGame;
    SetTeamSecondaryColors();

    A2 = team1;
    A3 = team2;
    SetInGameTeamsPrimaryColors();

    if (swos.g_gameType != kGameTypeCareer && swos.g_allPlayerTeamsEqual) {
        GetAveragePlayerPriceInSelectedTeams();
        swos.averagePlayerPrice = D0;
    }

    A2 = team1;
    A4 = &swos.topTeamInGame;
    A6 = &swos.team1AppPercent;
    InitInGameTeamStructure();

    A2 = team2;
    A4 = &swos.bottomTeamInGame;
    A6 = &swos.team2AppPercent;
    InitInGameTeamStructure();

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
        showVersusMenu(&swos.topTeamInGame, &swos.bottomTeamInGame, swos.gameName, swos.gameRound, []() {
            loadStadiumSprites(&swos.topTeamInGame, &swos.bottomTeamInGame);
        });
    }

    if (!swos.isGameFriendly && swos.g_gameType != kGameTypeDiyCompetition)
        swos.gameLengthInGame = 0;
    else
        swos.gameLengthInGame = swos.g_gameLength;
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

    gameLoop(&swos.topTeamInGame, &swos.bottomTeamInGame);

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

    lastScancode = pressed ? scancode : SDL_SCANCODE_UNKNOWN;
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
            enableSpinningLogo(!spinningLogoEnabled());
            break;
        case SDL_SCANCODE_F10:
            {
                bool chantsEnabled = areCrowdChantsEnabled();
                setCrowdChantsEnabled(!chantsEnabled);
            }
            break;
        case SDL_SCANCODE_PAGEUP:
            if (swos.bottomTeamData.playerNumber || swos.bottomTeamData.playerCoachNumber)
                requestBench2();
            break;
        case SDL_SCANCODE_PAGEDOWN:
            if (swos.topTeamData.playerNumber || swos.topTeamData.playerCoachNumber)
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

    int wheelDirection = mouseWheelAmount();
    if (wheelDirection)
        m_zoomFrames = wheelDirection > 0 ? kWheelZoomFrames : -kWheelZoomFrames;

    auto keys = SDL_GetKeyboardState(nullptr);
    bool controlHeld = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
    bool shiftHeld = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

    bool zoomInRequested = keys[SDL_SCANCODE_KP_PLUS] || m_zoomFrames > 0;
    bool zoomOutRequested = keys[SDL_SCANCODE_KP_MINUS] || m_zoomFrames < 0;

    bool zoomKeysHeld = zoomInRequested || zoomOutRequested;
    bool resetToDefault = keys[SDL_SCANCODE_KP_MULTIPLY] != 0;

    if (resetToDefault) {
        return resetZoom();
    } else if ((controlHeld || shiftHeld) && zoomKeysHeld) {
        if (m_blockZoom)
            return false;
        m_blockZoom = true;
    } else {
        m_blockZoom = false;
    }

    if (m_zoomFrames)
        m_zoomFrames > 0 ? m_zoomFrames-- : m_zoomFrames++;

    auto step = controlHeld ? kZoomStep : 0;

    if (zoomInRequested)
        return zoomIn(step);
    else if (zoomOutRequested)
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

void initTeamsData()
{
    swos.currentScorer.reset();
    swos.lastPlayerBeforeGoalkeeper.reset();
    swos.goalScored = 0;
    swos.runSlower = 0;
    swos.whichCard = 0;
    swos.bookedPlayer.reset();
    swos.playerHadBall = 0;
    swos.lastKeeperPlayed.reset();
    swos.lastTeamPlayed.reset();
    swos.lastPlayerPlayed.reset();
    swos.penalty = 0;
    swos.goalCameraMode = 0;
    swos.goalOut = 0;
    swos.fireBlocked = 0;
    swos.lastTeamPlayedBeforeBreak.reset();
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;
    swos.stoppageEventTimer = 0;
    swos.inGameCounter = 0;
    swos.gameStatePl = GameState::kInProgress;
    swos.gameState = GameState::kInProgress;
    swos.breakState = 0;
    swos.breakCameraMode = CameraBreakMode::kInactive;

    auto team = &swos.topTeamData;
    auto opponentTeam = &swos.bottomTeamData;

    for (int i = 0; i < 2; i++) {
        auto playerNo = i == 0 ? swos.topTeamPlayerNo : swos.bottomTeamPlayerNo;
        auto coachNo = i == 0 ? swos.topTeamCoachNo : swos.bottomTeamCoachNo;
        auto isPlCoach = i == 0 ? swos.pl1Coach : swos.pl2Coach;
        auto players = i == 0 ? swos.team1SpritesTable : swos.team2SpritesTable;
        auto tactics = i == 0 ? swos.pl1Tactics : swos.pl2Tactics;
        auto inGameTeamPtr = i == 0 ? swos.topTeamPtr : swos.bottomTeamPtr;
        auto teamStatsData = i == 0 ? &swos.team1StatsData : &swos.team2StatsData;
        if (i > 0 || swos.teamPlayingUp != 1)
            std::swap(team, opponentTeam);

        team->opponentTeam = opponentTeam;
        team->inGameTeamPtr = inGameTeamPtr;
        team->teamStatsPtr.set(teamStatsData);
        team->playerNumber = playerNo;
        team->playerCoachNumber = coachNo;
        team->isPlCoach = isPlCoach;
        team->players = players;
        team->teamNumber = i + 1;
        team->tactics = tactics;
        team->goalkeeperPlaying = 0;
        team->resetControls = 0;
        team->updatePlayerIndex = kNumPlayersInLineup - 1;
        team->controlledPlayer.reset();
        team->passToPlayerPtr.reset();
        team->passingKickingPlayer.reset();
        team->playerHasBall = 0;
        team->goalkeeperDivingLeft = 0;
        team->goalkeeperDivingRight = 0;
        team->ballOutOfPlayOrKeeper = 0;
        team->goaliePlayingOrOut = 0;
        team->passingBall = 0;
        team->passingToPlayer = 0;
        team->playerSwitchTimer = 0;
        team->ballInPlay = 0;
        team->ballOutOfPlay = 0;
        team->passKickTimer = 0;
        team->ballCanBeControlled = 0;
        team->ofs114 = 0;
        team->ballControllingDirection = Direction::kNoDirection;
        team->ofs116 = 0;
        team->wonTheBallTimer = 0;
        team->spinTimer = -1;
        team->ofs60 = 0;
        team->goalkeeperSavedCommentTimer = 0;
        team->lastHeadingTacklingPlayer.reset();
        team->ofs78 = 0;
        team->headerOrTackle = 0;
        team->shooting = 0;
        team->passInProgress = 0;
    }
}

void initPlayersBeforeEnteringPitch()
{
    // x, y pairs for 11 players in both teams
    static const std::array<int16_t, 2 * 2 * kNumPlayersInLineup> kTeamsStartingCoordinates = {
        300, 69, 280, 46, 260, 34, 240, 24, 220, 16, 200, 9, 180, 3, 160, -2, 140, -8, 120, -9, 100, -11,
        300, -65, 280, -42, 260, -30, 240, -20, 220, -12, 200, -5, 180, 1, 160, 6, 140, 12, 120, 13, 100, 15,
    };

    auto players = swos.teamPlayingUp == 1 ? swos.team1SpritesTable : swos.team2SpritesTable;
    auto coordinates = kTeamsStartingCoordinates.data();

    removeReferee();

    for (int teamIndex = 0; teamIndex < 2; teamIndex++) {
        for (int playerIndex = 0; playerIndex < kNumPlayersInLineup; playerIndex++) {
            auto player = *players++;
            assert(player);
            int xJitter = SWOS::rand() & 7;
            player->x.setWhole(*coordinates++ + kRightThrowInLine + 1 + xJitter);
            player->y.setWhole(*coordinates++ + kPitchCenterY);
            player->z.setWhole(0);
            player->destX = player->x.whole();
            player->destY = player->y.whole();
            player->speed = 0;
            player->setToNormalState();
            player->playerDownTimer = 0;
            player->frameIndex = -1;
            player->cycleFramesTimer = 1;
            player->clearImage();
            player->direction = Direction::kTop;
            player->onScreen = 1;
            if (swos.gameState == GameState::kStartingGame) {
                player->sentAway = 0;
                player->cards = 0;
                player->injuryLevel = 0;
            }
            setPlayerAnimationTable(*player, getPlayerNormalStandingAnimTable());
        }
        players = swos.teamPlayingUp == 2 ? swos.team1SpritesTable : swos.team2SpritesTable;
    }
}

void playersLeavingPitch()
{
    constexpr int kDelayBeforeLeavingPitch = 275;

    swos.hideBall = 0;
    swos.stoppageEventTimer = kDelayBeforeLeavingPitch;
    swos.gameState = GameState::kPlayersGoingToShower;
    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.gameStatePl = GameState::kStopped;
    swos.cameraDirection = Direction::kNoDirection;
    swos.lastTeamPlayedBeforeBreak = &swos.topTeamData;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;
    stopAllPlayers();
    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;
    swos.stateGoal = 0;
}

void startPenalties()
{
    swos.penaltiesState = -1;
    m_savedTeam1Goals = swos.statsTeam1Goals;
    m_savedTeam2Goals = swos.statsTeam2Goals;
    swos.statsTeam1Goals = 0;
    swos.team1GoalsDigit1 = 0;
    swos.team1GoalsDigit2 = 0;
    swos.statsTeam2Goals = 0;
    swos.team2GoalsDigit1 = 0;
    swos.team2GoalsDigit2 = 0;
    swos.team1PenaltyGoals = 0;
    swos.team2PenaltyGoals = 0;
    swos.teamPlayingUp = (SWOS::rand() & 1) + 1;
    swos.teamStarting = (SWOS::rand() & 1) + 1;
    m_team1PenaltyShooterIndex = 11;
    m_team2PenaltyShooterIndex = 11;
    m_team1PenaltyAttempts = 0;
    m_team2PenaltyAttempts = 0;
    swos.playingPenalties = 1;
    swos.dontShowScorers = 1;
    // make all players available as penalty takers
    for (auto players : { swos.team1SpritesTable, swos.team2SpritesTable }) {
        for (int playerIndex = 0; playerIndex < kNumPlayersInLineup; playerIndex++) {
            auto player = *players++;
            assert(player);
            player->cards = 0;
            player->sentAway = 0;  // this why it's possible for red carded player to shoot penalties
        }
    }
    nextPenalty();
}

// Next penalty is about to be shot in a penalty shootout after the game. Check if penalties are over.
// If not, initialize globals for penalty and set penaltyShooterSprite.
//
void nextPenalty()
{
    bool penaltiesFinished = false;
    if (m_team1PenaltyAttempts + m_team2PenaltyAttempts < 10) {
        // still in regular penalties (first five)
        // if they score all remaining attempts and still can't match other team's score then we're done here
        penaltiesFinished = swos.team1PenaltyGoals + 5 - m_team1PenaltyAttempts < swos.team2PenaltyGoals ||
            swos.team2PenaltyGoals + 5 - m_team2PenaltyAttempts < swos.team1PenaltyGoals;
    } else {
        // sudden death
        penaltiesFinished = m_team1PenaltyAttempts == m_team2PenaltyAttempts &&
            swos.team1PenaltyGoals != swos.team2PenaltyGoals;
    }

    if (penaltiesFinished) {
        swos.playingPenalties = 0;
        swos.statsTeam1Goals = m_savedTeam1Goals;
        swos.statsTeam2Goals = m_savedTeam2Goals;
        playersLeavingPitch();
    } else {
        // go on with the next penalty
        getBallSprite().z.setWhole(0);
        swos.teamPlayingUp = 3 - swos.teamPlayingUp;
        swos.teamStarting = 3 - swos.teamStarting;
        swos.hideBall = 0;
        initTeamsData();
        swos.gameState = GameState::kPenaltyShootout;
        swos.breakCameraMode = CameraBreakMode::kInactive;
        swos.cameraDirection = Direction::kTop;
        swos.playerTurnFlags = makeDirectionMask(
            Direction::kTopLeft, Direction::kTop, Direction::kTopRight);
        swos.foulXCoordinate = kPitchCenterX;
        swos.foulYCoordinate = kTopPenaltySpotY;
        swos.gameStatePl = GameState::kStopped;
        swos.lastTeamPlayedBeforeBreak = &swos.bottomTeamData;
        swos.stoppageTimerTotal = 0;
        swos.stoppageTimerActive = 0;
        stopAllPlayers();
        swos.cameraXVelocity = 0;
        swos.cameraYVelocity = 0;
        swos.penaltiesTimer = 0;
        auto penaltyShooterIndex = swos.lastTeamPlayedBeforeBreak->teamNumber == 1 ?
            &m_team1PenaltyShooterIndex : &m_team2PenaltyShooterIndex;
        if (!--*penaltyShooterIndex)
            *penaltyShooterIndex = 10;
        assert(*penaltyShooterIndex >= 0 && *penaltyShooterIndex < 11);
        swos.penaltyShooterSprite = swos.lastTeamPlayedBeforeBreak->players[*penaltyShooterIndex];
        if (swos.lastTeamPlayedBeforeBreak->teamNumber == 1)
            m_team1PenaltyAttempts++;
        else
            m_team2PenaltyAttempts++;
    }
}

void startFirstExtraTime()
{
    swos.halfNumber = 1;
    swos.teamPlayingUp = (SWOS::rand() & 1) + 1;
    swos.teamStarting = (SWOS::rand() & 1) + 1;
    swos.hideBall = 0;
    initTeamsData();
    swos.stoppageEventTimer = 110;
    swos.gameState = GameState::kFirstExtraStarting;
    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.gameStatePl = GameState::kStopped;
    swos.cameraDirection = Direction::kNoDirection;
    swos.lastTeamPlayedBeforeBreak = swos.teamStarting == swos.teamPlayingUp ?
        &swos.topTeamData : &swos.bottomTeamData;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;
    stopAllPlayers();
    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;
}

void endFirstExtraTime()
{
    swos.halfNumber = 2;
    swos.teamPlayingUp = 3 - swos.teamPlayingUp;
    swos.teamStarting = 3 - swos.teamStarting;
    swos.hideBall = 0;
    initTeamsData();
    swos.stoppageEventTimer = 110;
    swos.gameState = GameState::kFirstExtraEnded;
    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.gameStatePl = GameState::kStopped;
    swos.cameraDirection = Direction::kNoDirection;
    swos.lastTeamPlayedBeforeBreak = swos.teamStarting == swos.teamPlayingUp ?
        &swos.topTeamData : &swos.bottomTeamData;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;
    stopAllPlayers();
    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;
}

void checkIfGoalkeeperClaimedTheBall()
{
    if (swos.gameState == GameState::kKeeperHoldsTheBall) {
        assert(swos.lastTeamPlayedBeforeBreak);
        auto team = swos.lastTeamPlayedBeforeBreak;
        // the original game fails to set A2 to ball sprite
        goalkeeperClaimedTheBall(*team, *team->players[0], swos.ballSprite);
    } else {
        swos.breakCameraMode = CameraBreakMode::kInactive;
        swos.gameStatePl = GameState::kStopped;
        swos.stoppageTimerTotal = 0;
        swos.stoppageTimerActive = 0;
        stopAllPlayers();
        swos.cameraXVelocity = 0;
        swos.cameraYVelocity = 0;
    }
}

static void saveTeams()
{
    m_topTeamSaved = swos.topTeamInGame;
    m_bottomTeamSaved = swos.bottomTeamInGame;
}

static void restoreTeams()
{
    swos.topTeamInGame = m_topTeamSaved;
    swos.bottomTeamInGame = m_bottomTeamSaved;
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
                for (const auto teamData : { std::make_pair(team1, &swos.topTeamInGame), std::make_pair(team2, &swos.bottomTeamInGame) }) {
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

            A1 = &swos.topTeamInGame;
            A2 = team1;
            UpdatePlayerInjuries();

            A1 = &swos.bottomTeamInGame;
            A2 = team2;
            UpdatePlayerInjuries();
        }
    }
}

static void initPlayerCardChance()
{
    assert(swos.gameLengthInGame <= 3);

    // when the foul is made 4 bits (1-4) from currentGameTick are extracted and compared to this value;
    // if greater the player gets a card (yellow or red)
    static const int kPlayerCardChancesPerGameLength[16][4] = {
        4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 8, 9, 10,    // 3 min
        2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 6,     // 5 min
        1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4,     // 7 min
        1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,     // 10 min
    };

    auto chanceTable = kPlayerCardChancesPerGameLength[swos.gameLengthInGame];

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
    static const int8_t kPitchBallSpeedReductionAdjustments[] = {-3, 4, 1, 0, 0, -1, -1};
    static const int8_t kPitchBallSpeedReductionAdjustmentsAmiga[] = {-2, 2, 3, 0, 0, -1, -1};
    static const int8_t kBallSpeedBounceFactors[] = { 24, 80, 80, 72, 64, 40, 32 };
    static const int8_t kballZAxisDampenFactors[] = { 88, 112, 104, 104, 96, 88, 80 };

    int pitchType = getPitchType();
    assert(static_cast<size_t>(pitchType) <= 6);

    const auto& pitchBallSpeedInfluence =
        amigaModeActive() ? kPitchBallSpeedReductionAdjustmentsAmiga : kPitchBallSpeedReductionAdjustments;
    initPitchDependentBallPhysics(pitchBallSpeedInfluence[pitchType],
        kBallSpeedBounceFactors[pitchType], kballZAxisDampenFactors[pitchType]);
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

    memset(&swos.team1StatsData, 0, sizeof(TeamStatsData));
    memset(&swos.team2StatsData, 0, sizeof(TeamStatsData));

    constexpr int kShotChanceTableOffset = offsetof(TeamGeneralInfo, shotChanceTable);
    memset((char *)&swos.topTeamData + kShotChanceTableOffset, 0, sizeof(swos.topTeamData) - kShotChanceTableOffset);
    memset((char *)&swos.bottomTeamData + kShotChanceTableOffset, 0, sizeof(swos.bottomTeamData) - kShotChanceTableOffset);

    swos.goalCounter = 0;
    swos.stateGoal = 0;

    swos.frameCount = 0;
    swos.cameraXVelocity = swos.cameraYVelocity = 0;

    swos.pl1Fire = 0;
    swos.pl2Fire = 0;

    swos.longFireFlag = swos.longFireTime = 0;

    swos.currentGameTick = 0;
    swos.currentTick = 0;

    swos.AI_turnDirection = 1;
}

static void startingMatch()
{
    constexpr int kStartingBallY = 449;
    constexpr int kInitialDelayBeforeKickOff = 100;

    swos.halfNumber = 1;
    swos.hideBall = 0;
    setBallPosition(kBallOffCourtX, kStartingBallY);

    initTeamsData();    // careful, this function resets some stuff, so keep it up here for now

    swos.stoppageEventTimer = kInitialDelayBeforeKickOff;
    swos.gameState = GameState::kStartingGame;
    swos.gameStatePl = GameState::kStopped;
    swos.lastTeamPlayedBeforeBreak = &swos.topTeamData;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;

    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.cameraDirection = Direction::kNoDirection;
    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;

    stopAllPlayers();
    initPlayersBeforeEnteringPitch();
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
void SWOS::InitializeInGameTeamsAndStartGame()
{
    swos.plg_D0_param = D0;
    swos.plg_D3_param = D3;

    invokeWithSaved68kRegisters([]() {
        auto topTeamFile = A1.as<TeamFile *>();
        auto bottomTeamFile = A2.as<TeamFile *>();
        int minSubs = D1.asWord();
        int maxSubs = D2.asWord();
        initializeIngameTeamsAndStartGame(topTeamFile, bottomTeamFile, minSubs, maxSubs, D7.asWord());
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
