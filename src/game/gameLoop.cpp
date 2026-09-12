#include "gameLoop.h"
#include "game.h"
#include "ball.h"
#include "team.h"
#include "render.h"
#include "animation.h"
#include "windowManager.h"
#include "timer.h"
#include "gameControls.h"
#include "spinningLogo.h"
#include "playerNameDisplay.h"
#include "sprites.h"
#include "gameSprites.h"
#include "updateSprite.h"
#include "updatePlayers.h"
#include "gameTime.h"
#include "bench.h"
#include "drawBench.h"
#include "result.h"
#include "stats.h"
#include "pitch.h"
#include "pitchConstants.h"
#include "referee.h"
#include "menus.h"
#include "menuBackground.h"
#include "camera.h"
#include "controls.h"
#include "keyBuffer.h"
#include "audio.h"
#include "music.h"
#include "comments.h"
#include "sfx.h"
#include "chants.h"
#include "options.h"
#include "replays.h"
#include "stadiumMenu.h"
#include "replayExitMenu.h"
#include "util.h"
#include "FixedPoint.h"

constexpr FixedPoint kGameEndCameraX = 176;
constexpr FixedPoint kGameEndCameraY = 80;

static bool m_fadeAndSaveReplay;
static bool m_fadeAndInstantReplay;
static bool m_fadeAndReplayHighlights;

static bool m_doFadeIn;

static bool m_playingMatch;

static int m_penaltiesInterval = 110;
static int m_initalKickInterval = 825;
static int m_goalCameraInterval = 55;
static int m_allowPlayerControlCameraInterval = 550;

#ifdef SWOS_TEST
static std::function<void()> m_gameLoopStartHook = []() {};
static std::function<void()> m_gameLoopEndHook = []() {};
#endif

static void initGameLoop();
static void drawFrame(bool recordingEnabled);
static void gameFadeOut();
static void gameFadeIn();
static void updateTimers();
static void handlePauseAndStats();
static void handleKeys();
static void coreGameUpdate();
static void handleHighlightsAndReplays();
static void gameOver();
static bool gameEnded(TeamGame *topTeam, TeamGame *bottomTeam);
static void updateGameTimersAndCameraBreakMode();
static void pausedLoop();
static void showStatsLoop();
static void loadCrowdChantSampleIfNeeded();
static void initGoalSprites();

static void markPlayer();
static void setCameraMovingToShowerState();
static void firstHalfJustEnded();
static void goToHalftime();
static void prepareForInitialKick();

void gameLoop(TeamGame *topTeam, TeamGame *bottomTeam)
{
    swos.playGame = 1;

    showStadiumScreenAndFadeOutMusic(topTeam, bottomTeam, swos.gameMaxSubstitutes);

    do {
        initGameLoop();
        markFrameStartTime();

        // the really real main game loop ;)
        while (true) {
#ifdef SWOS_TEST
            m_gameLoopStartHook();
#endif
            loadCrowdChantSampleIfNeeded();
            updateTimers();
            handlePauseAndStats();

            handleKeys();

            coreGameUpdate();
            drawFrame(true);

            bool skipUpdate = false;

            if (m_doFadeIn) {
                gameFadeIn();
                m_doFadeIn = false;
                skipUpdate = true;
                swos.currentGameTick = 0;   // for tests (to remain original game compatible)
                swos.lastGameTick = 0;
            }

            handleHighlightsAndReplays();

            if (!swos.playGame) {
                gameFadeOut();
                break;
            }

            if (!skipUpdate)
                updateScreen(true);

#ifdef SWOS_TEST
            m_gameLoopEndHook();
#endif
        }
    } while (!gameEnded(topTeam, bottomTeam));

    m_playingMatch = false;
}

void showStadiumScreenAndFadeOutMusic(TeamGame *topTeam, TeamGame *bottomTeam, int maxSubstitutes)
{
    if (showPreMatchMenus()) {
        showStadiumMenu(topTeam, bottomTeam, maxSubstitutes, [&]() {
            initMatch(topTeam, bottomTeam, true);
        });
    } else {
        initMatch(topTeam, bottomTeam, true);
    }

    waitForMusicToFadeOut();
}

void requestFadeAndSaveReplay()
{
    m_fadeAndSaveReplay = true;
}

void requestFadeAndInstantReplay()
{
    m_fadeAndInstantReplay = true;
}

void requestFadeAndReplayHighlights()
{
    m_fadeAndReplayHighlights = true;
}

bool isMatchRunning()
{
    return m_playingMatch;
}

void setPenaltiesInterval(int interval)
{
    m_penaltiesInterval = interval;
}

void setInitalKickInterval(int interval)
{
    m_initalKickInterval = interval;
}

void setGoalCameraInterval(int interval)
{
    m_goalCameraInterval = interval;
}

void setAllowPlayerControlCameraInterval(int interval)
{
    m_allowPlayerControlCameraInterval = interval;
}

#ifdef SWOS_TEST
void setGameLoopStartHook(std::function<void()> hook)
{
    m_gameLoopStartHook = hook ? hook : []() {};
}

void setGameLoopEndHook(std::function<void()> hook)
{
    m_gameLoopEndHook = hook ? hook : []() {};
}
#endif

static void initGameLoop()
{
    m_playingMatch = true;
    m_doFadeIn = true;

    initGameAudio();
    playCrowdNoise();

    if (swos.playGame)
        playFansChant4lSample();

    flushKeyBuffer();

    initBenchBeforeMatch();

    swos.trainingGameCopy = swos.g_trainingGame;
    swos.gameCanceled = 0;
    swos.saveHighlightScene = 0;
    swos.instantReplayFlag = 0;

    m_fadeAndSaveReplay = false;
    m_fadeAndInstantReplay = false;
    m_fadeAndReplayHighlights = false;

    setCameraToInitialPosition();

    ReadTimerDelta();

    waitForKeyboardAndMouseIdle();

    swos.currentGameTick = 0;
    swos.lastFrameTicks = 0;

    initFrameTicks();

    resetGameControls();
}

static void drawFrame(bool recordingEnabled)
{
    setReplayRecordingEnabled(recordingEnabled);
    float xOffset, yOffset;
    std::tie(xOffset, yOffset) = drawPitchAtCurrentCamera();
    startNewHighlightsFrame();
    drawSprites(xOffset, yOffset);
    drawBench(xOffset, yOffset);
    drawPlayerName();
    drawGameTime();
    drawStatsIfNeeded();
    drawResult();
    drawSpinningLogo();
}

static void gameFadeOut()
{
    fadeOut([]() { drawFrame(false); });
}

static void gameFadeIn()
{
    fadeIn([]() { drawFrame(false); });
}

static void updateTimers()
{
    ReadTimerDelta();

    swos.frameCount++;

    if (swos.spaceReplayTimer)
        swos.spaceReplayTimer--;
}

static void handlePauseAndStats()
{
    pausedLoop();

    if (statsEnqueued())
        showStatsLoop();
}

static void handleKeys()
{
    processControlEvents();
    checkGameKeys();
}

static void coreGameUpdate()
{
    moveCamera();
    playEnqueuedSamples();
    updateGameTime();
    initGoalSprites();
    updateGameTimersAndCameraBreakMode();
    if (!updateFireBlocked()) {
        auto team = selectTeamForUpdate();
        updateTeamControls(team);
        updatePlayers(team);    // main game engine update
        postUpdateTeamControls(team);
    }
    updateBall();
    movePlayers();
    updateReferee();
    updateCornerFlags();
    updateSpinningLogo();
    //ManageAdvertisements();
    DoGoalkeeperSprites();
    updateControlledPlayerNumbers();
    markPlayer();
    updateCurrentPlayerName();
    updateBookedPlayerNumberSprite();
    updateResult();
    SWOS::DrawAnimatedPatterns(); // remove/re-implement
    updateBench();
    updateStatistics();
}

static void handleHighlightsAndReplays()
{
    // TODO: remove when UpdateCameraBreakMode() is converted
    if (swos.saveHighlightScene) {
        saveHighlightScene();
        swos.saveHighlightScene = 0;
    }

    if (m_fadeAndSaveReplay) {
        gameFadeOut();
        saveHighlightScene();
        m_doFadeIn = true;
        m_fadeAndSaveReplay = false;
    }

    // remove instantReplayFlag when UpdateCameraBreakMode() is converted
    if (m_fadeAndInstantReplay || swos.instantReplayFlag) {
        gameFadeOut();

        bool userRequested = true;
        if (swos.instantReplayFlag) {
            userRequested = false;
            swos.instantReplayFlag = 0;
            swos.goalCameraMode = 0;
        }

        playInstantReplay(userRequested);

        m_doFadeIn = true;
        m_fadeAndInstantReplay = false;
        swos.goalCameraMode = 0;    // is this necessary?
    }

    if (m_fadeAndReplayHighlights) {
        gameFadeOut();
        playHighlights(true);
        m_doFadeIn = true;
        m_fadeAndReplayHighlights = false;
    }
}

// Executed when the match finishes.
static void gameOver()
{
    gameFadeOut();

#ifdef SWOS_TEST
    setCameraX(FixedPoint(kGameEndCameraX.whole(), getCameraX().fraction()));
    setCameraY(FixedPoint(kGameEndCameraY.whole(), getCameraY().fraction()));
#else
    setCameraX(kGameEndCameraX);
    setCameraY(kGameEndCameraY);
#endif
    drawPitchAtCurrentCamera();
    setBallPosition(kBallOffCourtX, kPitchCenterY);

    swos.resultTimer = 30'000;
    swos.stoppageEventTimer = 1'650;
    swos.gameState = GameState::kResultAfterTheGame;
    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.gameStatePl = GameState::kStopped;
    swos.cameraDirection = Direction::kNoDirection;
    swos.lastTeamPlayedBeforeBreak = &swos.topTeamData;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;

    stopAllPlayers();

    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;

    playEndGameCrowdSampleAndComment();

    m_doFadeIn = true;
}

void SWOS::GameOver()
{
    gameOver();
}

static bool gameEnded(TeamGame *topTeam, TeamGame *bottomTeam)
{
    stopAudio();
    matchEnded();
    refreshReplayGameData();
    setStandardMenuBackgroundImage();

    if (!swos.isGameFriendly || swos.g_trainingGame)
        return true;

    bool replaySelected = showReplayExitMenuAfterFriendly();
    if (!replaySelected)
        return true;

    swos.team1NumAllowedInjuries = 4;
    swos.team2NumAllowedInjuries = 4;

    swos.playGame = 1;

    initMatch(swos.topTeamPtr, swos.bottomTeamPtr, false);

    return false;
}

static bool firePressedDuringBreak()
{
    if (swos.topTeamData.firePressed || swos.bottomTeamData.firePressed)
        return true;

    const auto playerUsesControl = [](int control) {
        return swos.topTeamData.playerCoachNumber == control ||
            swos.bottomTeamData.playerCoachNumber == control;
    };

    if (playerUsesControl(1)) {
        SWOS::Player1StatusProc();
        if (swos.pl1Fire)
            return true;
    }
    if (playerUsesControl(2)) {
        SWOS::Player2StatusProc();
        if (swos.pl2Fire)
            return true;
    }

    if (swos.gameState == GameState::kResultAfterTheGame) {
        SWOS::Player1StatusProc();
        if (swos.pl1Fire)
            return true;
        SWOS::Player2StatusProc();
        return swos.pl2Fire != 0;
    }
    return false;
}

static bool finishBreakTransmission()
{
    switch (swos.gameState) {
    case GameState::kResultOnHalftime:
        swos.resultTimer = -swos.resultTimer;
        swos.statsTimer = -swos.statsTimer;
        setCameraMovingToShowerState();
        prepareForInitialKick();
        swos.stoppageEventTimer = 0;
        return true;

    case GameState::kResultAfterTheGame:
        swos.resultTimer = -swos.resultTimer;
        swos.statsTimer = -swos.statsTimer;
        swos.playGame = 0;
        swos.stoppageEventTimer = 0;
        return true;

    case GameState::kStartingGame:
        swos.showFansCounter = 0;
        prepareForInitialKick();
        swos.stoppageEventTimer = 0;
        return true;

    case GameState::kCameraGoingToShowers:
        prepareForInitialKick();
        swos.stoppageEventTimer = 0;
        return true;
    }
    return false;
}

static void processStoppedGameState()
{
    switch (swos.gameState) {
    case GameState::kResultOnHalftime:
        swos.resultTimer = -swos.resultTimer;
        swos.statsTimer = -swos.statsTimer;
        setCameraMovingToShowerState();
        return;

    case GameState::kResultAfterTheGame:
        swos.resultTimer = -swos.resultTimer;
        swos.statsTimer = -swos.statsTimer;
        swos.playGame = 0;
        return;

    case GameState::kStartingGame:
    case GameState::kCameraGoingToShowers:
    case GameState::kFirstExtraStarting:
    case GameState::kFirstExtraEnded:
        prepareForInitialKick();
        return;

    case GameState::kFirstHalfEnded:
        firstHalfJustEnded();
        return;

    case GameState::kGoingToHalftime:
        swos.resultTimer = -swos.resultTimer;
        swos.statsTimer = -swos.statsTimer;
        goToHalftime();
        return;

    case GameState::kGameEnded:
        playersLeavingPitch();
        return;

    case GameState::kPlayersGoingToShower:
        SWOS::GameOver();
        return;
    }

    swos.gameStatePl = swos.gameState;
    swos.breakCameraMode = CameraBreakMode::kWaitingForBallToStop;
}

static void setPlayersDestinationState(TeamGeneralInfo& team, DestinationState state)
{
    if (!team.resetControls) {
        for (int i = 0; i < 11; ++i)
            team.players[i]->destReachedState = state;
    }
}

static bool allVisiblePlayersArrived()
{
    const bool requireOffscreenPlayers =
        swos.gameState == GameState::kPlayersGoingToInitialPositions ||
        swos.gameState == GameState::kPenalty ||
        swos.gameState == GameState::kPenaltyShootout;

    const TeamGeneralInfo *teams[] = { &swos.topTeamData, &swos.bottomTeamData };
    for (const auto team : teams) {
        for (int i = 0; i < 11; ++i) {
            const auto player = team->players[i];
            if ((requireOffscreenPlayers || player->onScreen) &&
                player->destReachedState != DestinationState::kReached)
                return false;
        }
    }
    return true;
}

static void updateCameraBreakMode()
{
    assert(swos.lastTeamPlayedBeforeBreak);
    auto& team = *swos.lastTeamPlayedBeforeBreak;

    switch (swos.breakCameraMode) {
    case CameraBreakMode::kWaitingForBallToStop:
        if (swos.ballSprite.deltaX || swos.ballSprite.deltaY)
            return;
        swos.breakCameraMode = CameraBreakMode::kPreparingBreak;
        if (swos.gameState != GameState::kKeeperHoldsTheBall)
            swos.stoppageEventTimer = swos.goalCameraMode ? 75 : m_goalCameraInterval;
        break;

    case CameraBreakMode::kPreparingBreak:
        if (!swos.playingPenalties && swos.goalCameraMode) {
            if (swos.g_autoSaveHighlights)
                swos.saveHighlightScene = 1;
            enqueueCrowdChantsReload();
            if (swos.g_autoReplays) {
                swos.userRequestedReplay = 0;
                swos.instantReplayFlag = 1;
                swos.loadCrowdChantSampleFlag = 1;
            }
        }
        swos.currentScorer.reset();
        if (!swos.playingPenalties || swos.gameState == GameState::kPenaltyShootout) {
            if (swos.gameState != GameState::kKeeperHoldsTheBall)
                setBallPosition(swos.foulXCoordinate, swos.foulYCoordinate);
            swos.breakCameraMode = CameraBreakMode::kPositioningPlayers;
        }
        break;

    case CameraBreakMode::kPositioningPlayers:
        if (!swos.whichCard && swos.cameraCoordinatesValid) {
            swos.goalScored = 0;
            if (swos.gameState == GameState::kKeeperHoldsTheBall) {
                swos.gameStatePl = GameState::kWaitingOnPlayer;
                swos.inGameCounter = 0;
                swos.breakState = static_cast<word>(swos.gameState);
            }
            setPlayersDestinationState(swos.topTeamData, DestinationState::kStarting);
            setPlayersDestinationState(swos.bottomTeamData, DestinationState::kStarting);
            if (swos.gameState == GameState::kKeeperHoldsTheBall) {
                swos.goalie1Sprite.destReachedState = DestinationState::kReached;
                swos.goalie2Sprite.destReachedState = DestinationState::kReached;
            }
            swos.breakCameraMode = CameraBreakMode::kChangingDirections;
        }
        break;

    case CameraBreakMode::kChangingDirections:
        swos.breakCameraMode = CameraBreakMode::kWaitingForPlayers;
        break;

    case CameraBreakMode::kWaitingForPlayers:
        if (swos.refState || swos.injuriesForever || !allVisiblePlayersArrived())
            break;
        swos.runSlower = 0;
        if (swos.goalCameraMode) {
            // ResetAnimatedPatternsForBothTeams();
        }
        swos.breakCameraMode = CameraBreakMode::kClearingCard;
        break;

    case CameraBreakMode::kClearingCard:
        swos.whichCard = 0;
        swos.bookedPlayer.reset();
        swos.breakCameraMode = CameraBreakMode::kPreparingRestart;
        break;

    case CameraBreakMode::kPreparingRestart:
        swos.ballOutOfGameTimer = 0;
        swos.breakCameraMode = CameraBreakMode::kWaitingForPlayerControl;
        break;

    case CameraBreakMode::kWaitingForPlayerControl: {
        team.ballInPlay = 1;
        if (++swos.ballOutOfGameTimer >= m_allowPlayerControlCameraInterval) {
            checkIfGoalkeeperClaimedTheBall();
            break;
        }
        if (team.controlledPlayer) {
            auto& player = *team.controlledPlayer;
            setPlayerAnimationTable(player, player.state == PlayerState::kThrowIn ?
                getAboutToThrowInAnimTable() : getPlayerNormalStandingAnimTable());
            team.ballOutOfPlay = 1;
            swos.breakCameraMode = CameraBreakMode::kCompletingRestart;
            if (swos.gameState != GameState::kKeeperHoldsTheBall)
                swos.resultTimer = 31'000;
        }
        break;
    }

    case CameraBreakMode::kCompletingRestart:
        if (swos.gameState != GameState::kKeeperHoldsTheBall)
            playRefereeWhistleSample();
        swos.goalCameraMode = 0;
        swos.gameStatePl = GameState::kWaitingOnPlayer;
        swos.inGameCounter = 0;
        swos.breakState = static_cast<word>(swos.gameState);
        break;

    default:
        assert(false && "Invalid camera break mode");
    }
}

static void updateGameTimersAndCameraBreakMode()
{
    if (swos.playingPenalties && swos.gameState != GameState::kPenaltyShootout &&
        ++swos.penaltiesTimer == m_penaltiesInterval)
        nextPenalty();

    if (swos.gameStatePl == GameState::kInProgress) {
        swos.inGameCounter += swos.lastFrameTicks;
        return;
    }

    swos.stoppageTimerTotal += swos.lastFrameTicks;

    if (swos.gameStatePl == GameState::kWaitingOnPlayer) {
        swos.stoppageTimerActive += swos.lastFrameTicks;
        if (!swos.lastTeamPlayedBeforeBreak->playerNumber &&
            swos.stoppageTimerActive >= m_initalKickInterval) {
            prepareForInitialKick();
        }
        return;
    }

    if (swos.gameState >= GameState::kStartingGame && swos.gameState <= GameState::kGameEnded &&
        firePressedDuringBreak() && finishBreakTransmission()) {
        return;
    }

    if (swos.stoppageEventTimer) {
        swos.stoppageEventTimer -= swos.lastFrameTicks;
        if (static_cast<int16_t>(swos.stoppageEventTimer) > 0)
            return;
    }
    swos.stoppageEventTimer = 0;

    if (swos.gameStatePl == GameState::kStopped) {
        processStoppedGameState();
        return;
    }

    if (!swos.g_inSubstitutesMenu && !swos.g_waitForPlayerToGoInTimer)
        updateCameraBreakMode();
}

static void pausedLoop()
{
    int oldWidth = -1, oldHeight = -1;

    while (isGamePaused()) {
        processControlEvents();

        int width, height;
        std::tie(width, height) = getWindowSize();

        if (checkGameKeys() || width != oldWidth || height != oldHeight) {
            oldWidth = width;
            oldHeight = height;
            markFrameStartTime();
            drawFrame(false);
            updateScreen(true);
            continue;
        }

        auto events = getPlayerEvents(kPlayer1) | getPlayerEvents(kPlayer2);
        if (events & (kGameEventKick | kGameEventPause)) {
            togglePause();
            break;
        }

        SDL_Delay(100);

        // make sure to reset the timer or delta time will be heavily skewed after the pause
        markFrameStartTime();
    }
}

static void showStatsLoop()
{
    drawFrame(false);
    updateScreen();

    do {
        SDL_Delay(100);
        processControlEvents();
        checkGameKeys();
        if (isAnyPlayerFiring()) {
            swos.fireBlocked = 1;
            hideStats();
        }
    } while (showingUserRequestedStats());
}

static void loadCrowdChantSampleIfNeeded()
{
    if (swos.loadCrowdChantSampleFlag) {
        loadCrowdChantSample();
        swos.loadCrowdChantSampleFlag = 0;
    }
}

static void initGoalSprites()
{
    swos.goal1TopSprite.setImage(kTopGoalSprite);
    swos.goal2BottomSprite.setImage(kBottomGoalSprite);
}

static void markPlayer()
{
    const auto& team = swos.currentGameTick & 0x10 ? swos.bottomTeamData : swos.topTeamData;
    const auto teamInfo = team.inGameTeamPtr;
    swos.playerMarkSprite.clearImage();
    if (teamInfo->markedPlayer >= 0 && teamInfo->markedPlayer < 11) {
        const auto& playerSprite = team.players[teamInfo->markedPlayer];
        if (playerSprite->state == PlayerState::kNormal) {
            swos.playerMarkSprite.setImage(kPlayerMarkSprite);
            swos.playerMarkSprite.x.setWhole(playerSprite->x.whole());
            swos.playerMarkSprite.y.setWhole(playerSprite->y.whole());
            // place it 20 pixels above the player
            swos.playerMarkSprite.z.setWhole(playerSprite->z.whole() + 20);
        }
    }
}

static void setCameraMovingToShowerState()
{
    swos.halfNumber = 2;
    swos.teamPlayingUp = 3 - swos.teamPlayingUp;
    swos.teamStarting = 3 - swos.teamStarting;
    setBallPosition(kBallOffCourtX, kPitchCenterY);
    swos.hideBall = 0;
    initTeamsData();
    swos.stoppageEventTimer = 110;
    swos.gameState = GameState::kCameraGoingToShowers;
    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.gameStatePl = GameState::kStopped;
    swos.cameraDirection = Direction::kNoDirection;
    swos.lastTeamPlayedBeforeBreak = &swos.topTeamData;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;
    stopAllPlayers();
    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;
    initPlayersBeforeEnteringPitch();
}

static void firstHalfJustEnded()
{
    swos.hideBall = 0;
    swos.stoppageEventTimer = 275;
    swos.gameState = GameState::kGoingToHalftime;
    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.gameStatePl = GameState::kStopped;
    swos.cameraDirection = Direction::kNoDirection;
    swos.lastTeamPlayed = &swos.topTeamData;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;
    stopAllPlayers();
    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;
    swos.stateGoal = 0;
}

static void goToHalftime()
{
    setBallPosition(kBallOffCourtX, kPitchCenterY);
    swos.resultTimer = kEndOfHalfResultTimer;
    swos.stoppageEventTimer = 770;
    swos.gameState = GameState::kResultOnHalftime;
    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.gameStatePl = GameState::kStopped;
    swos.cameraDirection = Direction::kNoDirection;
    swos.lastTeamPlayedBeforeBreak = &swos.topTeamData;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;
    stopAllPlayers();
    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;
}

static void prepareForInitialKick()
{
    setBallPosition(kPitchCenterX, kPitchCenterY);

    auto lastTeamPlayedBeforeBreak = &swos.topTeamData;
    auto cameraDirection = Direction::kBottom;
    auto playerTurnFlags = makeDirectionMask(Direction::kRight, Direction::kBottomRight,
        Direction::kBottom, Direction::kBottomLeft, Direction::kLeft);

    if (swos.teamStarting != swos.teamPlayingUp) {
        lastTeamPlayedBeforeBreak = &swos.bottomTeamData;
        cameraDirection = Direction::kTop;
        playerTurnFlags = makeDirectionMask(Direction::kTop, Direction::kTopRight,
            Direction::kRight, Direction::kLeft, Direction::kTopLeft);
    }

    swos.gameState = GameState::kPlayersGoingToInitialPositions;
    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.gameStatePl = GameState::kStopped;
    swos.foulXCoordinate = kPitchCenterX;
    swos.foulYCoordinate = kPitchCenterY;
    swos.cameraDirection = cameraDirection;
    swos.playerTurnFlags = playerTurnFlags;
    swos.lastTeamPlayedBeforeBreak = lastTeamPlayedBeforeBreak;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;
    stopAllPlayers();
    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;
}
