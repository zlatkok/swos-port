#include "gameLoop.h"
#include "game.h"
#include "render.h"
#include "windowManager.h"
#include "timer.h"
#include "gameControls.h"
#include "spinningLogo.h"
#include "playerNameDisplay.h"
#include "gameSprites.h"
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

constexpr float kGameEndCameraX = 176;
constexpr float kGameEndCameraY = 80;

static bool m_fadeAndSaveReplay;
static bool m_fadeAndInstantReplay;
static bool m_fadeAndReplayHighlights;

static bool m_doFadeIn;

static bool m_playingMatch;

#ifdef SWOS_TEST
static std::function<void()> m_gameLoopStartHook = []() {};
static std::function<void()> m_gameLoopEndHook = []() {};
#endif

static void initGameLoop();
static void drawFrame(bool recordingEnabled);
static void gameFadeOut();
static void gameFadeIn();
static void updateTimersHandlePauseAndStats();
static void handleKeys();
static void coreGameUpdate();
static void handleHighlightsAndReplays();
static void gameOver();
static bool gameEnded(TeamGame *topTeam, TeamGame *bottomTeam);
static void pausedLoop();
static void showStatsLoop();
static void loadCrowdChantSampleIfNeeded();
static void initGoalSprites();

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
            updateTimersHandlePauseAndStats();

            handleKeys();

            coreGameUpdate();
            drawFrame(true);

            bool skipUpdate = false;

            if (m_doFadeIn) {
                gameFadeIn();
                m_doFadeIn = false;
                skipUpdate = true;
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

    unloadMenuBackground();

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

    swos.stoppageTimer = 0;
    swos.lastStoppageTimerValue = 0;

    initFrameTicks();
}

#ifdef SWOS_TEST
static void drawFrame(bool) {}
#else
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
#endif

static void gameFadeOut()
{
    fadeOut([]() { drawFrame(false); });
}

static void gameFadeIn()
{
    fadeIn([]() { drawFrame(false); });
}

void updateTimersHandlePauseAndStats()
{
    ReadTimerDelta();

    pausedLoop();

    if (statsEnqueued())
        showStatsLoop();

    swos.frameCount++;

    if (swos.spaceReplayTimer)
        swos.spaceReplayTimer--;
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
    UpdateCameraBreakMode();    // convert
    auto postUpdateTeamControls = updateTeamControls();
    UpdatePlayersAndBall();    // main game engine update
    postUpdateTeamControls();
    UpdateBall();
    MovePlayers();
    updateReferee();
    updateCornerFlags();
    updateSpinningLogo();
    //ManageAdvertisements();
    DoGoalkeeperSprites();
    UpdateControlledPlayerNumbers();
    MarkPlayer();
    updateCurrentPlayerName();
    BookPlayer();
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

// Executed when the game is over.
static void gameOver()
{
    gameFadeOut();

    setCameraX(kGameEndCameraX);
    setCameraY(kGameEndCameraY);
    drawPitchAtCurrentCamera();

    D1 = kPitchCenterX;
    D2 = kPitchCenterY;
    SetBallPosition();

    swos.resultTimer = 30'000;
    swos.stoppageEventTimer = 1'650;
    swos.gameState = GameState::kResultAfterTheGame;
    swos.breakCameraMode = -1;
    swos.gameStatePl = GameState::kStopped;
    swos.cameraDirection = -1;
    swos.lastTeamPlayedBeforeBreak = &swos.topTeamData;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;

    StopAllPlayers();

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

    unloadMenuBackground();

    swos.team1NumAllowedInjuries = 4;
    swos.team2NumAllowedInjuries = 4;

    swos.playGame = 1;

    initMatch(swos.topTeamPtr, swos.bottomTeamPtr, false);

    return false;
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
    swos.goal1TopSprite.pictureIndex = kTopGoalSprite;
    swos.goal2BottomSprite.pictureIndex = kBottomGoalSprite;
}
