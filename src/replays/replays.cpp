// Implementation of highlights and replays is here. Highlights have a fixed buffer for each scene which holds
// bit-packed coordinates of sprites and camera (and some other things). Replays hook the moment when the buffer
// overflows and refill it with the next data. They are essentially one big highlight scene.

#include "replays.h"
#include "ReplayData.h"
#include "replayOptions.h"
#include "menu.h"
#include "file.h"
#include "sprites.h"
#include "render.h"
#include "controls.h"

enum ReplayState {
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
extern int g_replayViewLoop;

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

    hilNumGoals = 0;

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
    return m_hilValid = loadFile(path, hilFileBuffer, -1, 0, false) != -1;
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
    while (paused) {
        SWOS::GetKey();
        SWOS::MainKeysCheck();
        SWOS::PlayEnqueuedSamples();
    }
}

static bool highlightsAborted()
{
    if (!skipFade)
        return false;

    SWOS::Player1StatusProc();
    if (!pl1Fire) {
        SWOS::Player2StatusProc();
        if (!pl2Fire)
            return false;
    }

    statsFireExit = 1;
    replayState = kNotReplaying;
    paused = 0;
    SAFE_INVOKE(FadeOutToBlack);
    RestoreCameraPosition();

    return true;
}

static void highlightsMoveToNextScene()
{
    goalBasePtr = reinterpret_cast<dword *>(hilStartOfGoalsBuffer + hilSceneNumber * kSingleHighlightBufferSize);
    currentHilPtr = goalBasePtr - 1;
    nextGoalPtr = reinterpret_cast<dword *>(reinterpret_cast<char *>(goalBasePtr) + kSingleHighlightBufferSize);

    HandleInstantReplayStateSwitch();
}

dword *validateHilPointerAdd(dword *ptr)
{
    if (ptr >= nextGoalPtr) {
        // if we are recording the game, do not save the buffer when it overflows
        // during instant replays (when the player presses 'R')
        if (m_playing || replayState == kNotReplaying)
            m_replayData.handleHighglightsBufferOverflow(ptr, m_playing);
        ptr = goalBasePtr;
    }

    return ptr;
}

dword *validateHilPointerSub(dword *ptr)
{
    if (ptr < goalBasePtr)
        ptr = nextGoalPtr;

    return ptr;
}

// Initializes things to start in-game replay, if it was in initializing state.
static void initInstantReplayFrame(dword packedCoordinates)
{
    if (replayState != kFadingIn)
        return;

    replayState = kReplaying;

    savedCameraX = g_cameraX;
    savedCameraY = g_cameraY;

    g_oldCameraX = 0;
    g_cameraX = 176;
    g_oldCameraY = 0;
    g_cameraY = 349;

    SWOS::ResetAnimatedPatternsForBothTeams();

    InitSavedSprites();
    InitBackground();
    SWOS::ScrollToCurrent();

    clearHiBit(packedCoordinates);

    g_cameraX = loWord(packedCoordinates);
    g_cameraY = hiWord(packedCoordinates);

    SWOS::ScrollToCurrent();
}

static void drawResult()
{
    int x = 13;
    int y = 13;

    auto leftScoreDigits = std::div(hilLeftTeamGoals, 10);
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

    auto rightScoreDigits = std::div(hilRightTeamGoals, 10);
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
    while (paused) {
        SWOS::GetKey();
        if (!skipFade)
            break;
        SWOS::MainKeysCheck();
    }
}

static bool instantReplayAborted()
{
    int checkWhichPlayerFire = 1 + 2;

    if (!userRequestedReplay) {
        if (!teamDataPtr)
            return false;

        if (teamDataPtr->playerNumber == 1)
            checkWhichPlayerFire = 1;
        else if (teamDataPtr->playerNumber == 2)
            checkWhichPlayerFire = 2;
        else if (teamDataPtr->plCoachNum == 1)
            checkWhichPlayerFire = 1;
        else if (teamDataPtr->plCoachNum == 2)
            checkWhichPlayerFire = 2;
    }

    if (checkWhichPlayerFire & 1) {
        SWOS::Player1StatusProc();
        if (pl1Fire)
            return true;
    }

    if (checkWhichPlayerFire & 2) {
        SWOS::Player2StatusProc();
        if (pl2Fire)
            return true;
    }

    return false;
}

static void registerPackedCoordinates(dword packedCoordinates)
{
    *currentHilPtr = packedCoordinates;
    if (packedCoordinates != -1)
        currentHilPtr = validateHilPointerAdd(currentHilPtr + 1);
}

// Must do this in order to accept fire to abort highlight replay.
static void setGameControls()
{
    setPl1GameControls(getPl1Controls());
    setPl2GameControls(getPl2Controls());
}

void showHighlights()
{
    assert(hilNumGoals > 0);

    m_playing = true;
    m_fastReplay = false;

    hilStarted = 1;
    gameMaxSubstitutes = hilMaxSubstitutes;
    teamsLoaded = 0;
    poolplyrLoaded = 0;

    SAFE_INVOKE(CopyTeamsAndHeader);

    setGameControls();

    if (g_menuMusic)
        safeInvokeWithSaved68kRegisters(StopMidi);

    safeInvokeWithSaved68kRegisters([] {
        showMenu(stadiumMenu);
    });

    SAFE_INVOKE(ViewHighlightsWrapper);

    SWOS::InitMenuMusic();

    if (g_menuMusic)
        TryMidiPlay();

    SAFE_INVOKE(LoadFillAndSwtitle);
    vsPtr = linAdr384k;
    screenWidth = kMenuScreenWidth;
    g_cameraX = 0;
    g_cameraY = 0;

    resetMatchControls();

    m_playing = false;

    menuFade = 1;
    DrawMenu();
}

void toggleFastReplay()
{
    if (m_playing)
        m_fastReplay ^= 1;
}

static bool inSubstitutes()
{
    // without goToSubsTimer, cutout is too sharp
    return !goToSubsTimer && g_inSubstitutesMenu;
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

        dword packedGoals = (team2Goals << 16) | team1Goals;
        registerPackedCoordinates(packedGoals);

        // repeated two times, probably to keep it aligned
        dword packedAnimPatternsState = (animPatternsState << 16) | animPatternsState;
        registerPackedCoordinates(packedAnimPatternsState);

        registerPackedCoordinates(-1);
    }
}

void SWOS::PlayInstantReplayLoop()
{
    g_replayViewLoop = 1;
    while (true) {
        ReadTimerDelta();

        replayPausedLoop();

        SWOS::GetKey();
        if (skipFade)
            MainKeysCheck();

        if (replayState == kNotReplaying)
            return;

        if (skipFade && instantReplayAborted()) {
            statsFireExit = 1;
            replayState = kNotReplaying;
            break;
        }

        SAFE_INVOKE(ClearBackground);
        ReplayFrame();

        if (replayState == kNotReplaying)
            break;

        if (fadeAndSaveReplay) {
            FadeAndSaveReplay();
            fadeAndSaveReplay = 0;
        }

        if (instantReplayFlag) {
            HandleInstantReplayStateSwitch();
            instantReplayFlag = 0;
        }

        Flip();

        fadeIfNeeded2();
    }

    SAFE_INVOKE(FadeOutToBlack);
    SAFE_INVOKE(ClearBackground);
    RestoreCameraPosition();
    g_replayViewLoop = 0;	
}

// Takes over highlights rendering to avoid clearing background last frame before fade,
// which was causing players to disappear.
void SWOS::PlayHighlightsLoop()
{
    g_replayViewLoop = 1;
    int drawFrame = 1;

    while (true) {
        ReadTimerDelta();

        highlightsPausedLoop();

        GetKey();
        MainKeysCheck();

        if (replayState != kNotReplaying) {
            if (highlightsAborted())
                break;

            SAFE_INVOKE(ClearBackground);
            ReplayFrame();

            if (replayState != kNotReplaying) {
                if (drawFrame ^= m_fastReplay)
                    Flip();
                fadeIfNeeded2();
                continue;
            } else {
                SAFE_INVOKE(FadeOutToBlack);
                RestoreCameraPosition();
                paused = 0;
            }
        }

        if (++hilSceneNumber == numReplaysInHilFile)
            break;

        highlightsMoveToNextScene();
    }

    RestoreHighlightPointers();
    paused = 0;
    g_replayViewLoop = 0;	
}

// Handles initial frame stuff: camera coordinates, result and animated patterns.
static std::pair<dword *, dword> handleCameraPositionAndResult(dword *p, dword d)
{
    if (hiBitSet(d)) {
        clearHiBit(d);
        p = validateHilPointerAdd(p);

        D1 = loWord(d);
        D2 = hiWord(d);
        MoveView();

        d = *p++;
        p = validateHilPointerAdd(p);

        hilLeftTeamGoals = loWord(d);
        hilRightTeamGoals = hiWord(d);

        d = *p++;
        p = validateHilPointerAdd(p);

        animPatternsState = hiWord(d);

        invokeWithSaved68kRegisters(DrawAnimatedPatterns);

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
    deltaColor = 0x70;
    drawSprite(((stoppageTimer >> 1) & 0x1f) + kReplayFrame00Sprite, 11, 14);
    deltaColor = 0;
}

void SWOS::ReplayFrame()
{
    auto p = currentGoalPtr;
    auto d = *p++;

    initInstantReplayFrame(d);

    for (auto func : { handleCameraPositionAndResult, handleSprites }) {
        std::tie(p, d) = func(p, d);

        if (d == -1) {
            replayState = kNotReplaying;
            return;
        }
    }

    p = validateHilPointerSub(p - 1);
    currentGoalPtr = p;

    if (!normalReplayRunning)
        drawBigRotatingLetterR();
    else
        drawResult();

    if (replayState == kSlowMotionPlayback && !paused) {
        frameDelay();
        skipFrameUpdate();
    }
}
