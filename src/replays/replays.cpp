// Implementation of highlights and replays is here. Highlights have a fixed buffer for each scene which holds
// bit-packed coordinates of sprites and camera (and some other things). Replays hook the moment when the buffer
// overflows and refill it with the next data. They are essentially one big highlight scene.

#include "replays.h"
#include "ReplayData.h"
#include "replayOptions.h"
#include "menus.h"
#include "drawMenu.h"
#include "game.h"
#include "file.h"
#include "sprites.h"
#include "render.h"
#include "controls.h"

enum ReplayState
{
    kNotReplaying = 0,
    kFadingIn = 1,
    kReplaying = 2,
    kSlowMotionPlayback = 3,
};

static ReplayData m_replayData;
static bool m_hilValid;
static bool m_playing;
static int m_fastReplay;

static void autoSaveReplay();

void initReplays()
{
    auto dirPath = pathInRootDir(ReplayData::kReplaysDir);
    if (!createDir(dirPath.c_str()))
        errorExit("Failed to create %s directory", ReplayData::kReplaysDir);

    atexit([]() {
        // only fetch replay data if we're interrupting a game (otherwise it will be caught by a regular call)
        if (isMatchRunning()) {
            SetHilFileHeader();     // this may have not had a chance to execute
            finishCurrentReplay();
        }
    });
}

void initNewReplay()
{
    m_replayData.startNewReplay();
    swos.replayState = kNotReplaying;
    swos.currentHilPtr = swos.goalBasePtr;
}

// Called when game or program end.
void finishCurrentReplay()
{
    if (!m_playing) {
        m_replayData.finishCurrentReplay();

        if (getAutoSaveReplays() && m_replayData.gotReplay())
            autoSaveReplay();

        m_hilValid = true;
    }
}

void startReplay()
{
    logInfo("Starting replay");

    m_replayData.replayStarting();
    showHighlights();

    swos.hilNumGoals = 0;

    logInfo("Replay done");
}

bool gotReplay()
{
    return m_replayData.gotReplay();
}

bool highlightsValid()
{
    return m_hilValid;
}

bool loadReplayFile(const char *path)
{
    return m_replayData.load(path);
}

bool saveReplayFile(const char *path, bool overwrite /* = true */)
{
    return m_replayData.save(path, overwrite);
}

bool loadHighlightsFile(const char *path)
{
    return m_hilValid = loadFile(path, swos.hilFileBuffer, -1, 0, false) != -1;
}

static void autoSaveReplay()
{
    if (getAutoSaveReplays() && !m_playing && m_replayData.gotReplay()) {
        char filename[256];

        auto t = time(nullptr);
        strftime(filename, sizeof(filename), "%Y-%m-%d-%H-%M-%S-auto-save.rpl", localtime(&t));

        // let's not overwrite the file (append to it) if it somehow already exists
        auto success = saveReplayFile(filename, false);
        logInfo("Automatically saved replay %s, result: %s", filename, success ? "success" : "failure!");
    }
}

void saveCoordinatesForHighlights(int spriteIndex, int x, int y)
{
    D0 = spriteIndex;
    D4 = x;
    D5 = y;
    SWOS::SaveCoordinatesForHighlights();
}

static void highlightsPausedLoop()
{
    while (swos.paused) {
        SWOS::GetKey();
        MainKeysCheck();
        SWOS::PlayEnqueuedSamples();
    }
}

static bool highlightsAborted()
{
    if (!swos.skipFade)
        return false;

    SWOS::Player1StatusProc();
    if (!swos.pl1Fire) {
        SWOS::Player2StatusProc();
        if (!swos.pl2Fire)
            return false;
    }

    swos.statsFireExit = 1;
    swos.replayState = kNotReplaying;
    swos.paused = 0;
    FadeOutToBlack();
    RestoreCameraPosition();

    return true;
}

static void highlightsMoveToNextScene()
{
    swos.goalBasePtr = reinterpret_cast<dword *>(swos.hilStartOfGoalsBuffer + swos.hilSceneNumber * kSingleHighlightBufferSize);
    swos.currentHilPtr = swos.goalBasePtr - 1;
    swos.nextGoalPtr = reinterpret_cast<dword *>(swos.goalBasePtr.asCharPtr() + kSingleHighlightBufferSize);

    HandleInstantReplayStateSwitch();
}

dword *validateHilPointerAdd(dword *ptr)
{
    if (ptr >= swos.nextGoalPtr) {
        // if we are recording the game, do not save the buffer when it overflows
        // during instant replays (when the player presses 'R')
        if (m_playing || swos.replayState == kNotReplaying)
            m_replayData.handleHighglightsBufferOverflow(ptr, m_playing);
        ptr = swos.goalBasePtr;
    }

    return ptr;
}

dword *validateHilPointerSub(dword *ptr)
{
    if (ptr < swos.goalBasePtr)
        ptr = swos.nextGoalPtr;

    return ptr;
}

// Initializes things to start in-game replay, if it was in initializing state.
static void initInstantReplayFrame(dword packedCoordinates)
{
    if (swos.replayState != kFadingIn)
        return;

    swos.replayState = kReplaying;

//TODO: save full 32-bit coordinates
    swos.savedCameraX = swos.g_cameraX;
    swos.savedCameraY = swos.g_cameraY;

    swos.g_oldCameraX = 0;
    swos.g_cameraX = 176;
    swos.g_oldCameraY = 0;
    swos.g_cameraY = 349;

//    SWOS::ResetAnimatedPatternsForBothTeams();

    SWOS::ScrollToCurrent();

    clearHiBit(packedCoordinates);

    swos.g_cameraX = loWord(packedCoordinates);
    swos.g_cameraY = hiWord(packedCoordinates);

    SWOS::ScrollToCurrent();
}

static void drawResult()
{
    int x = 13;
    int y = 13;

    auto leftScoreDigits = std::div(swos.hilLeftTeamGoals, 10);
    auto leftScoreDigit1 = leftScoreDigits.quot;
    auto leftScoreDigit2 = leftScoreDigits.rem;

    if (leftScoreDigit1) {
        drawSprite(leftScoreDigit1 + kBigZeroSprite, x, y);
        x += 12;
    }

    drawSprite(leftScoreDigit2 + kBigZeroSprite, x, y);

    x += 15;
    y += 8;
    drawSprite(kBigDashSprite, x, y);

    x += 8;
    y -= 8;

    auto rightScoreDigits = std::div(swos.hilRightTeamGoals, 10);
    auto rightScoreDigit1 = rightScoreDigits.quot;
    auto rightScoreDigit2 = rightScoreDigits.rem;

    if (rightScoreDigit1) {
        drawSprite(rightScoreDigit1 + kBigZero2Sprite, x, y);
        x += 12;
    }

    drawSprite(rightScoreDigit2 + kBigZero2Sprite, x, y);
}

static void replayPausedLoop()
{
    while (swos.paused) {
        SWOS::GetKey();
        if (!swos.skipFade)
            break;
        MainKeysCheck();
    }
}

static bool instantReplayAborted()
{
    int checkWhichPlayerFire = 1 + 2;

    if (!swos.userRequestedReplay) {
        if (!swos.teamDataPtr)
            return false;

        if (swos.teamDataPtr->playerNumber == 1)
            checkWhichPlayerFire = 1;
        else if (swos.teamDataPtr->playerNumber == 2)
            checkWhichPlayerFire = 2;
        else if (swos.teamDataPtr->plCoachNum == 1)
            checkWhichPlayerFire = 1;
        else if (swos.teamDataPtr->plCoachNum == 2)
            checkWhichPlayerFire = 2;
    }

    if (checkWhichPlayerFire & 1) {
        SWOS::Player1StatusProc();
        if (swos.pl1Fire)
            return true;
    }

    if (checkWhichPlayerFire & 2) {
        SWOS::Player2StatusProc();
        if (swos.pl2Fire)
            return true;
    }

    return false;
}

static void registerPackedCoordinates(dword packedCoordinates)
{
    *swos.currentHilPtr = packedCoordinates;
    if (packedCoordinates != -1)
        swos.currentHilPtr = validateHilPointerAdd(swos.currentHilPtr + 1);
}

void showHighlights()
{
    assert(swos.hilNumGoals > 0);

    m_playing = true;
    m_fastReplay = 0;

    swos.hilStarted = 1;
    swos.gameMaxSubstitutes = swos.hilMaxSubstitutes;
    swos.teamsLoaded = 0;
    swos.poolplyrLoaded = 0;

    CopyTeamsAndHeader();

    if (swos.g_menuMusic)
        invokeWithSaved68kRegisters(StopMidi);

    invokeWithSaved68kRegisters([] {
        showMenu(swos.stadiumMenu);
    });

    SWOS::ViewHighlightsWrapper();
    SWOS::InitMenuMusic();

    if (swos.g_menuMusic)
        TryMidiPlay();

    swos.screenWidth = kMenuScreenWidth;
    swos.g_cameraX = 0;
    swos.g_cameraY = 0;

    m_playing = false;

    swos.menuFade = 1;
    drawMenu();
}

void toggleFastReplay()
{
    if (m_playing)
        m_fastReplay ^= 1;
}

static bool inSubstitutes()
{
    // without goToSubsTimer, cutout is too sharp
    return !swos.goToSubsTimer && swos.g_inSubstitutesMenu;
}

void SWOS::ViewHighlightsWrapper()
{
    save68kRegisters();
    auto currentMenu = getCurrentPackedMenu();
    auto currentEntry = getCurrentMenu()->selectedEntry->ordinal;

    ViewHighlights();
    restore68kRegisters();

    restoreMenu(currentMenu, currentEntry);
}

void SWOS::SaveCoordinatesForHighlights()
{
    // reject save coordinates requests while in substitutes menu
    if (inSubstitutes())
        return;

    auto spriteIndex = D0.asInt16();
    auto x = D4.asInt16();
    auto y = D5.asInt16();

    if (spriteIndex >= 0) {
        dword packedCoordinates = spriteIndex << 10;
        packedCoordinates |= x & 0x3ff;
        packedCoordinates <<= 10;
        packedCoordinates |= y & 0x3ff;
        clearHiBit(packedCoordinates);
        registerPackedCoordinates(packedCoordinates);
    } else if (spriteIndex == -2) {
        registerPackedCoordinates(-1);
    } else {
        dword packedCoordinates = (y << 16) | x | 0x80000000;
        registerPackedCoordinates(packedCoordinates);

        dword packedGoals = (swos.team2TotalGoals << 16) | swos.team1TotalGoals;
        registerPackedCoordinates(packedGoals);

        // repeated two times, probably to keep it aligned
        dword packedAnimPatternsState = (swos.animPatternsState << 16) | swos.animPatternsState;
        registerPackedCoordinates(packedAnimPatternsState);

        registerPackedCoordinates(-1);
    }
}

void SWOS::PlayInstantReplayLoop()
{
    while (true) {
        ReadTimerDelta();

        replayPausedLoop();

        SWOS::GetKey();
        if (swos.skipFade)
            MainKeysCheck();

        if (swos.replayState == kNotReplaying)
            return;

        if (swos.skipFade && instantReplayAborted()) {
            swos.statsFireExit = 1;
            swos.replayState = kNotReplaying;
            break;
        }

        ReplayFrame();

        if (swos.replayState == kNotReplaying)
            break;

        if (swos.fadeAndSaveReplay) {
            FadeAndSaveReplay();
            swos.fadeAndSaveReplay = 0;
        }

        if (swos.instantReplayFlag) {
            HandleInstantReplayStateSwitch();
            swos.instantReplayFlag = 0;
        }

        Flip();

        fadeIfNeeded();
    }

    FadeOutToBlack();
    RestoreCameraPosition();
}

// Takes over highlights rendering to avoid clearing background last frame before fade,
// which was causing players to disappear.
void SWOS::PlayHighlightsLoop()
{
    int drawFrame = 1;

    while (true) {
        ReadTimerDelta();

        highlightsPausedLoop();

        GetKey();
        MainKeysCheck();

        if (swos.replayState != kNotReplaying) {
            if (highlightsAborted())
                break;

            ReplayFrame();

            if (swos.replayState != kNotReplaying) {
                if (drawFrame ^= m_fastReplay)
                    Flip();
                fadeIfNeeded();
                continue;
            } else {
                FadeOutToBlack();
                RestoreCameraPosition();
                swos.paused = 0;
            }
        }

        if (++swos.hilSceneNumber == swos.numReplaysInHilFile)
            break;

        highlightsMoveToNextScene();
    }

    RestoreHighlightPointers();
    swos.paused = 0;
}

// Handles initial frame stuff: camera coordinates, result and animated patterns.
static std::pair<dword *, dword> handleCameraPositionAndResult(dword *p, dword d)
{
    if (hiBitSet(d)) {
        clearHiBit(d);
        p = validateHilPointerAdd(p);

        D1 = loWord(d);
        D2 = hiWord(d);
//move camera

        d = *p++;
        p = validateHilPointerAdd(p);

        swos.hilLeftTeamGoals = loWord(d);
        swos.hilRightTeamGoals = hiWord(d);

        d = *p++;
        p = validateHilPointerAdd(p);

        swos.animPatternsState = hiWord(d);

        //invokeWithSaved68kRegisters(DrawAnimatedPatterns);

        d = *p++;
    }

    return { p, d };
}

static std::pair<dword *, dword> handleSprites(dword *p, dword d)
{
    while (!hiBitSet(d)) {
        p = validateHilPointerAdd(p);
        auto spriteIndex = d >> 20;
        int x = static_cast<int>(d << 12) >> 22;
        int y = static_cast<int>(d << 22) >> 22;

        drawSprite(spriteIndex, x, y);

        d = *p++;
    }

    return { p, d };
}

static void drawBigRotatingLetterR()
{
    drawSprite(((swos.stoppageTimer >> 1) & 0x1f) + kReplayFrame00Sprite, 11, 14);
}

void SWOS::ReplayFrame()
{
    auto p = swos.currentGoalPtr.asPtr();
    auto d = *p++;

    initInstantReplayFrame(d);

    for (auto func : { handleCameraPositionAndResult, handleSprites }) {
        std::tie(p, d) = func(p, d);

        if (d == -1) {
            swos.replayState = kNotReplaying;
            return;
        }
    }

    p = validateHilPointerSub(p - 1);
    swos.currentGoalPtr = p;

    if (!swos.normalReplayRunning)
        drawBigRotatingLetterR();
    else
        drawResult();

    if (swos.replayState == kSlowMotionPlayback && !swos.paused) {
        frameDelay();
        skipFrameUpdate();
    }
}
