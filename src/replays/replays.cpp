// Implementation of highlights and replays is here. Replays are essentially one big highlights scene.

#include "replays.h"
#include "ReplayDataStorage.h"
#include "replayOptions.h"
#include "hilFile.h"
#include "render.h"
#include "timer.h"
#include "audio.h"
#include "comments.h"
#include "music.h"
#include "sfx.h"
#include "pitch.h"
#include "controls.h"
#include "gameControls.h"
#include "keyBuffer.h"
#include "options.h"
#include "game.h"
#include "gameLoop.h"
#include "gameTime.h"
#include "benchControls.h"
#include "camera.h"
#include "sprites.h"
#include "renderSprites.h"
#include "text.h"
#include "stats.h"
#include "menus.h"
#include "menuBackground.h"
#include "drawMenu.h"
#include "stadiumMenu.h"
#include "file.h"
#include "util.h"

constexpr int kStandardFrameOffset = 80;
constexpr int kLargeFrameOffset = 5 * kStandardFrameOffset;
constexpr int kExtraLargeFrameOffset = 60 * kStandardFrameOffset;

enum class Status
{
    kNormal,
    kPaused,
    kAborted,
    kNext,
    kPrevious,
};

static HilV2Header m_header;
static ReplayDataStorage m_replayData;

static bool m_recordingEnabled;

static bool m_replaying;
static bool m_paused;

static bool m_gotReplay;
static bool m_gotHighlight;

static bool m_instantReplay;

static bool m_fastReplay;
static bool m_slowMotion;

static void runReplay(bool inGame, bool isReplay);
static Status replayScene(bool inGame, bool isReplay, bool userRequested = false);
static Status checkReplayControlKeys(bool inGame, bool userRequested);
static void showIntroMenus();
static bool fetchAndRenderFrame(bool isReplay);
static void drawResultAndTime(const ReplayDataStorage::FrameData& frameData, bool replay);
static void drawResult(int leftTeamGoals, int rightTeamGoals, bool showTime);
static void drawOverlay(const ReplayDataStorage::FrameData& frameData, bool isReplay);
static void drawBigRotatingLetterR();
static void drawPercentage(bool isReplay);
static void updateScreen(bool isReplay, bool& doFadeIn, bool aborted);
static bool shouldAbortCurrentScene(Status status);
static bool isReplayAborted(bool blockNonScorerFire);
static bool shouldRecordReplayData();
static void createDirectories();
static void autoSaveReplay();
static int getGameTime();
static void unpackTime(int time, int& digit1, int& digit2, int& digit3);

void initReplays()
{
    createDirectories();

    atexit([]() {
        // only fetch replay data if we're interrupting a game (otherwise it will be caught by a regular call)
        if (isMatchRunning())
            finishCurrentReplay();
    });
}

void initNewReplay()
{
    m_replayData.startRecordingNewReplay();
    m_replaying = false;
    m_gotHighlight = true;
    m_gotReplay = true;
}

// Gets the necessary data from the game to have everything the replay file requires locally. It must be
// called at the proper time, right after the game is finished, so the data is valid and fresh.
void refreshReplayGameData()
{
    m_header.team1 = swos.topTeamIngame;
    m_header.team2 = swos.bottomTeamIngame;

    // using strncpy here so the buffer gets cleared and we don't get any garbage in the file
    strncpy(m_header.gameName, swos.gameName, sizeof(m_header.gameName));
    strncpy(m_header.gameRound, swos.gameRound, sizeof(m_header.gameRound));

    m_header.team1Goals = swos.statsTeam1Goals;
    m_header.team2Goals = swos.statsTeam2Goals;
    m_header.pitchNumber = swos.pitchNumberAndType >> 8;
    m_header.pitchType = swos.pitchNumberAndType & 0xff;
    m_header.numMaxSubstitutes = swos.gameMaxSubstitutes;
}

// Called when a match ends or the whole program exits.
void finishCurrentReplay()
{
    if (!m_replaying) {
        refreshReplayGameData();
        autoSaveReplay();
    }
}

void setReplayRecordingEnabled(bool enabled)
{
    m_recordingEnabled = enabled;
}

void startNewHighlightsFrame()
{
    if (shouldRecordReplayData()) {
        int gameTime = getGameTime();
        ReplayDataStorage::FrameData data(getCameraX(), getCameraY(), swos.team1TotalGoals, swos.team2TotalGoals, gameTime);
        m_replayData.recordFrame(data);
    }
}

void saveCoordinatesForHighlights(int spriteIndex, float x, float y)
{
    assert(spriteIndex >= 0 && spriteIndex < kNumSprites);

    if (shouldRecordReplayData())
        m_replayData.recordSprite(spriteIndex, x, y);
}

void saveStatsForHighlights(const GameStats& stats)
{
    if (shouldRecordReplayData())
        m_replayData.recordStats(stats);
}

void saveSfxForHighlights(int sampleIndex, int volume)
{
    if (shouldRecordReplayData())
        m_replayData.recordSfx(sampleIndex, volume);
}

bool replayingNow()
{
    return m_replaying;
}

bool gotReplay()
{
    return m_gotReplay && !m_replayData.empty();
}

bool gotHighlights()
{
    return m_gotHighlight && !m_replayData.empty();
}

void saveHighlightScene()
{
    if (!m_replaying)
        m_replayData.saveHighlightsScene();
}

void playInstantReplay(bool userRequested)
{
    assert(!m_replaying);

    logInfo("Starting instant replay (%s)...", userRequested ? "user requested" : "auto");

    m_instantReplay = true;
    m_replaying = true;
    m_slowMotion = false;

    m_replayData.setupForCurrentSceneReplay();
    replayScene(true, false, userRequested);

    m_replaying = false;
    m_instantReplay = false;

    markFrameStartTime();

    logInfo("Instant replay done");
}

void playHighlights(bool inGame)
{
    m_instantReplay = false;
    runReplay(inGame, false);
}

void playFullReplay()
{
    m_instantReplay = false;
    runReplay(false, true);
}

FileStatus loadHighlightsFile(const char *path)
{
    auto status = m_replayData.load(path, kHighlightsDir, m_header, false);

    if (status == FileStatus::kOk) {
        m_gotHighlight = true;
        m_gotReplay = false;
    }

    return status;
}

FileStatus loadReplayFile(const char *path)
{
    auto status = m_replayData.load(path, kReplaysDir, m_header, true);

    if (status == FileStatus::kOk) {
        m_gotHighlight = m_replayData.numScenes() > 0;
        m_gotReplay = true;
    }

    return status;
}

bool saveHighlightsFile(const char *path, bool overwrite /* = true */)
{
    return m_replayData.save(path, m_header, false, overwrite);
}

bool saveReplayFile(const char *path, bool overwrite /* = true */)
{
    return m_replayData.save(path, m_header, true, overwrite);
}

static void runReplay(bool inGame, bool isReplay)
{
    assert(!m_replaying && !m_replayData.empty());

    swos.teamsLoaded = 0;
    swos.poolplyrLoaded = 0;

    logInfo("Starting highlights replay...");

    m_paused = false;
    m_replaying = true;

    m_fastReplay = false;
    m_slowMotion = false;

    if (!inGame)
        showIntroMenus();

    unloadMenuBackground();
    initGameAudio();
    playCrowdNoise();

    if (isReplay) {
        m_replayData.setupForFullReplay();
        replayScene(inGame, isReplay);
    } else {
        for (int i = 0; i < m_replayData.numScenes(); i++) {
            m_replayData.setupForStoredSceneReplay(i);
            auto status = replayScene(inGame, isReplay);

            if (status == Status::kAborted) {
                break;
            } else if (status == Status::kNext) {
                assert(i < m_replayData.numScenes() - 1);
            } else if (status == Status::kPrevious) {
                assert(i > 0);
                i -= 2;
            } else {
                assert(status == Status::kNormal);
            }
        }
    }

    setStandardMenuBackgroundImage();

    if (showPreMatchMenus())
        enqueueMenuFadeIn();

    stopAudio();
    startMenuSong();

    m_replaying = false;

    logInfo("Highlights replay done");
}

static Status replayScene(bool inGame, bool isReplay, bool userRequested /* = false */)
{
    bool doFadeIn = true;
    bool aborted = false;
    auto status = Status::kNormal;

    m_paused = false;

    do {
        status = checkReplayControlKeys(inGame, userRequested);
        aborted = shouldAbortCurrentScene(status);

        ReadTimerDelta();
        playEnqueuedSamples();

        // prevent pause on first/last frames when the screen has to fade in or has already faded to black
        if (m_replayData.hasAnotherFullFrame() && !doFadeIn && status == Status::kPaused) {
            SDL_Delay(100);
            continue;
        }

        markFrameStartTime();

        if (!fetchAndRenderFrame(isReplay))
            break;

        updateScreen(isReplay, doFadeIn, aborted);
    } while (!aborted);

    return status;
}

static Status checkReplayControlKeys(bool inGame, bool userRequested)
{
    processControlEvents();

    SDL_Scancode key;
    SDL_Keymod mod;

    if (checkZoomKeys() && m_paused) {
        m_replayData.skipFrames(0);
        return Status::kNormal;
    }

    while (std::get<0>(std::tie(key, mod) = getKeyAndModifier()) != SDL_SCANCODE_UNKNOWN) {
        switch (key) {
        case SDL_SCANCODE_ESCAPE:
            return Status::kAborted;

        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_RIGHT:
            {
                int frameOffset = kStandardFrameOffset;

                if (mod & KMOD_CTRL)
                    frameOffset = mod & KMOD_ALT ? kExtraLargeFrameOffset : kLargeFrameOffset;
                else if (mod & KMOD_SHIFT)
                    frameOffset = 1;

                if (key == SDL_SCANCODE_LEFT)
                    frameOffset = -frameOffset;

                m_replayData.skipFrames(frameOffset);
            }
            return Status::kNormal;

        case SDL_SCANCODE_P:
            return (m_paused = !m_paused) ? Status::kPaused : Status::kNormal;

        case SDL_SCANCODE_PERIOD:
            return Status::kNext;

        case SDL_SCANCODE_COMMA:
            return Status::kPrevious;

        case SDL_SCANCODE_R:
            if (m_slowMotion = !m_slowMotion)
                m_fastReplay = false;
            break;

        case SDL_SCANCODE_I:
            if (!m_instantReplay)
                toggleShowReplayPercentage();
            break;

        case SDL_SCANCODE_F:
            if (!m_instantReplay && (m_fastReplay = !m_fastReplay))
                m_slowMotion = false;
            break;
        }
    }

    if (inGame && isReplayAborted(m_instantReplay && !userRequested))
        return Status::kAborted;

    return m_paused ? Status::kPaused : Status::kNormal;
}

static void showIntroMenus()
{
    startFadingOutMusic();

    menuFadeOut();

    auto currentMenu = getCurrentPackedMenu();
    auto currentEntry = getCurrentMenu()->selectedEntry->ordinal;

    showStadiumScreenAndFadeOutMusic(&m_header.team1, &m_header.team2, m_header.numMaxSubstitutes);

    restoreMenu(currentMenu, currentEntry);
}

static bool fetchAndRenderFrame(bool isReplay)
{
    ReplayDataStorage::FrameData frameData;
    if (!m_replayData.fetchFrameData(frameData))
        return false;

    float xOffset, yOffset;
    std::tie(xOffset, yOffset) = drawPitch(frameData.cameraX, frameData.cameraY);

    GameStats stats;
    bool gotStats = false;
    ReplayDataStorage::Object obj;

    while (m_replayData.fetchObject(obj)) {
        switch (obj.type) {
        case ReplayDataStorage::ObjectType::kSprite:
            if (m_replayData.isLegacyFormat()) {
                const auto& sprite = getSprite(obj.pictureIndex);
                obj.x += sprite.centerXF + sprite.xOffsetF;
                obj.y += sprite.centerYF + sprite.yOffsetF;
            }
            drawSprite(obj.pictureIndex, obj.x, obj.y, true, xOffset, yOffset);
            break;

        case ReplayDataStorage::ObjectType::kStats:
            stats = obj.stats;
            gotStats = true;
            break;

        case ReplayDataStorage::ObjectType::kSfx:
            if (!m_instantReplay)
                playSfx(obj.sampleIndex, obj.volume);
            break;

        default:
            logWarn("Unknown replay object type %d", obj.type);
        }
    }

    if (gotStats)
        drawStats(frameData.team1Goals, frameData.team2Goals, stats);

    drawOverlay(frameData, isReplay);

    return true;
}

static void drawResultAndTime(const ReplayDataStorage::FrameData& frameData, bool isReplay)
{
    bool showTime = isReplay && frameData.gameTime >= 0;

    drawResult(frameData.team1Goals, frameData.team2Goals, showTime);

    if (showTime) {
        int digit1, digit2, digit3;
        unpackTime(frameData.gameTime, digit1, digit2, digit3);
        drawGameTime(digit1, digit2, digit3);
    }
}

static void drawResult(int leftTeamGoals, int rightTeamGoals, bool showTime)
{
    int x = 12;
    int y = 13;

    if (showTime)
        y += 8;

    auto leftScoreDigits = std::div(leftTeamGoals, 10);
    auto leftScoreDigit1 = leftScoreDigits.quot;
    auto leftScoreDigit2 = leftScoreDigits.rem;

    if (leftScoreDigit1) {
        drawMenuSprite(leftScoreDigit1 + kBigZeroSprite, x, y);
        x += 12;
    }

    drawMenuSprite(leftScoreDigit2 + kBigZeroSprite, x, y);

    x += 15;
    y += 8;
    drawMenuSprite(kBigDashSprite, x, y);

    x += 8;
    y -= 8;

    auto rightScoreDigits = std::div(rightTeamGoals, 10);
    auto rightScoreDigit1 = rightScoreDigits.quot;
    auto rightScoreDigit2 = rightScoreDigits.rem;

    if (rightScoreDigit1) {
        drawMenuSprite(rightScoreDigit1 + kBigZero2Sprite, x, y);
        x += 12;
    }

    drawMenuSprite(rightScoreDigit2 + kBigZero2Sprite, x, y);
}

static void drawOverlay(const ReplayDataStorage::FrameData& frameData, bool isReplay)
{
    if (m_instantReplay)
        drawBigRotatingLetterR();
    else
        drawResultAndTime(frameData, isReplay);

    if (!m_instantReplay && getShowReplayPercentage())
        drawPercentage(isReplay);
}

static void drawBigRotatingLetterR()
{
    int spriteIndex = ((swos.stoppageTimer >> 1) & 0x1f) + kReplayFrame00Sprite;
    drawMenuSprite(spriteIndex, 11, 14);
}

static void drawPercentage(bool isReplay)
{
    char buf[32];
    if (isReplay)
        sprintf(buf, "%.2f%%", m_replayData.percentageAt());
    else
        sprintf(buf, "(%d/%d) %.2f%%", m_replayData.currentScene() + 1, m_replayData.numScenes(), m_replayData.percentageAt());
    drawText(isReplay ? 276 : 254, 190, buf, -1, kGrayText);
}

static void updateScreen(bool isReplay, bool& doFadeIn, bool aborted)
{
    bool frameSkip = false;

    if (!m_paused && m_fastReplay) {
        static bool s_frameSkip;
        s_frameSkip = !s_frameSkip;
        frameSkip = s_frameSkip;
    } else if (!m_paused && m_slowMotion) {
        frameDelay();
        markFrameStartTime();
    }

    auto showCurrentFrame = [&]() {
        m_replayData.skipFrames(0);
        fetchAndRenderFrame(isReplay);
    };

    if (doFadeIn) {
        fadeIn(showCurrentFrame);
        doFadeIn = false;
    } else if (!m_replayData.hasAnotherFullFrame() || aborted) {
        fadeOut(showCurrentFrame);
    } else if (!frameSkip) {
        updateScreen(true);
    }
}

static bool shouldAbortCurrentScene(Status status)
{
    switch (status) {
    case Status::kAborted:
        return true;
    case Status::kNext:
        if (m_replayData.currentScene() < m_replayData.numScenes() - 1)
            return true;
        break;
    case Status::kPrevious:
        if (m_replayData.currentScene() > 0)
            return true;
        break;
    }

    return false;
}

static bool isReplayAborted(bool blockNonScorerFire)
{
    int checkWhichPlayerFire = 1 + 2;

    if (blockNonScorerFire) {
        if (!swos.teamScoredDataPtr)
            return false;

        if (swos.teamScoredDataPtr->playerNumber == 1)
            checkWhichPlayerFire = 1;
        else if (swos.teamScoredDataPtr->playerNumber == 2)
            checkWhichPlayerFire = 2;
        else if (swos.teamScoredDataPtr->plCoachNum == 1)
            checkWhichPlayerFire = 1;
        else if (swos.teamScoredDataPtr->plCoachNum == 2)
            checkWhichPlayerFire = 2;
    }

    if ((checkWhichPlayerFire & 1) && isPlayerFiring(kPlayer1) ||
        (checkWhichPlayerFire & 2) && isPlayerFiring(kPlayer2))
        return true;

    return false;
}

static bool shouldRecordReplayData()
{
    return m_recordingEnabled && !m_replaying && !inBenchOrGoingTo();
}

static void createDirectories()
{
    for (auto dir : { kReplaysDir, kHighlightsDir }) {
        auto dirPath = pathInRootDir(dir);
        if (!createDir(dirPath.c_str()))
            errorExit("Failed to create %s directory", dir);
    }
}

static void autoSaveReplay()
{
    if (getAutoSaveReplays() && !m_replaying && !m_replayData.empty()) {
        char filename[256];

        auto t = time(nullptr);
        strftime(filename, sizeof(filename), "%Y-%m-%d-%H-%M-%S-auto-save.rpl", localtime(&t));

        auto path = joinPaths(kReplaysDir, filename);

        // let's not overwrite the file (append to it) if it somehow already exists
        auto success = saveReplayFile(path.c_str(), false);
        logInfo("Automatically saved replay %s, result: %s", filename, success ? "success" : "failure!");
    }
}

static int getGameTime()
{
    int gameTime = -1;

    if (gameTimeShowing()) {
        int digit1, digit2, digit3;
        std::tie(digit1, digit2, digit3) = gameTimeAsBcd();
        gameTime = (digit1 << 16) | (digit2 << 8) | digit3;
    }

    return gameTime;
}

static void unpackTime(int time, int& digit1, int& digit2, int& digit3)
{
    digit1 = (time >> 16) & 0xff;
    digit2 = (time >> 8) & 0xff;
    digit3 = time & 0xff;
}
