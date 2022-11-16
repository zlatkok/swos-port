#include "RecordedDataTest.h"
#include "unitTest.h"
#include "sdlProcs.h"
#include "gameLoop.h"
#include "sprites.h"
#include "gameSprites.h"
#include "updateSprite.h"
#include "updateBench.h"
#include "camera.h"
#include "pitch.h"
#include "gameTime.h"
#include "playerNameDisplay.h"
#include "audio.h"
#include "controls.h"
#include "keyboard.h"
#include "random.h"
#include "windowManager.h"
#include "unpackMenu.h"
#include <dirent.h>

#define MAKE_FULL_VERSION(major, minor) (((major) << 8) | (minor))

// ensure animation and frame tables are contiguous
static_assert(SwosVM::refSecondYellowFrames - SwosVM::frameIndicesTablesStart == 2'328);
static_assert(SwosVM::refSecondYellowAnimTable - SwosVM::animTablesStart == 3'696);

constexpr int kVersionMajor = 1;
constexpr int kVersionMinor = 1;

constexpr char kDataDir[] = "recdata";
constexpr char kExtension[] = ".rgd";

constexpr int kBallSprite = 1;
constexpr int kTeam1CurPlayerNumSprite = 26;
constexpr int kTeam2CurPlayerNumSprite = 27;
constexpr int kPlayerMarkerSprite = 28;
constexpr int kBookedPlayerCardOrNumberSprite = 29;
constexpr int kRefereeSprite = 30;
constexpr int kCornerFlagSprite = 31;
constexpr int kCurrentPlayerNameSprite = 32;
constexpr int kNumCornerFlagSprites = 4;

static const DefaultKeySet kPlayer1Keys = {
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_RCTRL, SDL_SCANCODE_RSHIFT,
};
static const DefaultKeySet kPlayer2Keys = {
    SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_GRAVE,
};

struct {
    int speed;
    int x;
    int y;
    int destX;
    int destY;
    int angle;
    unsigned deltaX;
    unsigned deltaY;
} static const kCalculateDeltaXAndYTestData[] = {
    { 1168, 396, 373, 757, 447, 0x46, 0x17048, 0x349d },
    { 1168, 757, 447, 396, 373, 0xc6, 0xfffe8ccd, 0xffffc878 },
    { 0, 396, 373, 757, 447, 0x46, 0, 0 },
    { 0, 757, 447, 396, 373, 0xc6, 0, 0 },
    { 1168, 396, 373, 396, 373, -1, 0, 0 },
    { 1168, 396, 373, 396, 373, -1, 0, 0 },
    { 500, 200, 300, 200, 100, 0, 0, 0xffff5fd8 },
    { 500, 200, 100, 200, 300, 0x80, 0, 0x00009ee9 },
    { 1024, 0, 0, 1, 1, 0x60, 0x0000e6a0, 0x0000e6a0 },
    { 1024, 1, 1, 0, 0, 0xe0, 0xffff16d0, 0xffff16d0 },
};

static RecordedDataTest t;

void RecordedDataTest::init()
{
    m_files = findResFiles(kExtension);
    auto files = findFiles(kExtension, kDataDir);
    std::for_each(files.begin(), files.end(), [this](const auto& file) {
        const auto& path = joinPaths(kDataDir, file.name.c_str());
        m_files.emplace_back(path);
    });
    if (!m_files.empty())
        m_files.push_back(m_files.front());

    disableRendering();
    killSdlDelay();
    takeOverInput();

    setGameLoopStartHook(std::bind(&RecordedDataTest::setFrameInput, this));
    setGameLoopEndHook(std::bind(&RecordedDataTest::verifyFrame, this));
    SWOS::setRandHook([this]() {
        return static_cast<int>(m_header.fixedRandomValue);
    });

    setPl1Controls(kKeyboard1);
    setPl2Controls(kKeyboard2);

    std::tie(m_screenWidth, m_screenHeight) = getWindowSize();
    setWindowSize(320, 200);    // must run in VGA resolution to match the game logic (visible sprites)
    setZoomFactor(1);
}

void RecordedDataTest::finish()
{
    enableRendering();
    resurrectSdlDelay();

    SWOS_UnitTest::setMenuCallback(nullptr);
    setWindowSize(m_screenWidth, m_screenHeight);

    swos.g_allowShorterMenuItemsWithFrames = 0;
}

const char *RecordedDataTest::name() const
{
    return "rec-data";
}

const char *RecordedDataTest::displayName() const
{
    return "recorded game data";
}

auto RecordedDataTest::getCases() -> CaseList
{
    return {
        { "test calculateDeltaXAndY()", "calculateDeltaXAndY", nullptr,
            bind(&RecordedDataTest::verifyCalculateDeltaXandY), std::size(kCalculateDeltaXAndYTestData), false },
        { "verify recorded game data", "verify-rec-data", bind(&RecordedDataTest::setupRecordedDataVerification),
            bind(&RecordedDataTest::verifyRecordedData), m_files.size(), false },
    };
}

void RecordedDataTest::setupRecordedDataVerification()
{
    assert(!m_files.empty());

    setSoundEnabled(false);
    resetKeyboardInput();

    setDefaultKeyPackForKeyboard(Keyboard::kSet1, kPlayer1Keys);
    setDefaultKeyPackForKeyboard(Keyboard::kSet2, kPlayer2Keys);

    // make initial referee sprite fields the same as in SWOS
    SWOS::RemoveReferee();

    m_lastGameTick = 0;
    m_firstFrame = true;
}

void RecordedDataTest::verifyRecordedData()
{
    std::tie(m_dataFile, m_header) = openDataFile(m_files[m_currentDataIndex]);

    m_frameSize = MAKE_FULL_VERSION(m_header.major, m_header.minor) >= MAKE_FULL_VERSION(1, 3) ?
        sizeof(FrameV1p3) : sizeof(FrameV1p0);

    assert(m_header.inputControls == kSwosKeyboardOnly || m_header.inputControls == kSwosKeyboardAndJoypad ||
        m_header.inputControls == kSwosJoypadOnly);

    swos.pitchTypeOrSeason = m_header.pitchTypeOrSeason;
    swos.g_pitchType = m_header.pitchType;
    swos.g_gameLength = m_header.gameLength;
    swos.g_autoReplays = m_header.autoReplays;
    swos.g_allPlayerTeamsEqual = m_header.allPlayerTeamsEqual;
    swos.friendlySeason = m_header.season;
    swos.minSubstitutes = m_header.minSubstitutes;
    swos.maxSubstitutes = m_header.maxSubstitutes;

    // loading career file for training game might mess some things up
    swos.g_autoSaveHighlights = 0;

    memcpy(&swos.USER_A, m_header.userTactics, sizeof(m_header.userTactics));

	static int timesCalled;
    timesCalled = 0;
    SWOS_UnitTest::setMenuCallback([this] {
        if (getCurrentPackedMenu() != swos.playMatchMenu)
            return true;

        int plNum = 0;
        if (!timesCalled) {
            if (m_header.topTeamFile.teamControls != kComputerTeam)
                plNum = m_header.topTeamFile.teamControls == kCoach ? m_header.topTeamCoachNo : m_header.topTeamPlayerNo;
            else if (m_header.bottomTeamFile.teamControls != kComputerTeam)
                plNum = m_header.bottomTeamFile.teamControls == kCoach ? m_header.bottomTeamCoachNo : m_header.bottomTeamPlayerNo;
        } else {
            assert(timesCalled == 1);
            assert(m_header.topTeamFile.teamControls != kComputerTeam && m_header.bottomTeamFile.teamControls != kComputerTeam);
			plNum = m_header.bottomTeamFile.teamControls == kCoach ? m_header.bottomTeamCoachNo : m_header.bottomTeamPlayerNo;
        }
        timesCalled++;
        swos.playerNumThatStarted = plNum;
        return true;
    });

    if (m_header.trainingGameCareerSize) {
        memcpy(swos.careerTeam, &m_header.topTeamFile, sizeof(m_header.topTeamFile));
        memcpy(&swos.currentTeam, &m_header.bottomTeamFile, sizeof(m_header.bottomTeamFile));
        swos.selTeamsPtr = SwosVM::ptrToOffset(&swos.g_selectedTeams);
        PlayTrainingGame();
    } else {
        memcpy(swos.g_selectedTeams, &m_header.topTeamFile, sizeof(m_header.topTeamFile));
        memcpy(swos.g_selectedTeams + sizeof(m_header.topTeamFile), &m_header.bottomTeamFile, sizeof(m_header.bottomTeamFile));
        assert(reinterpret_cast<intptr_t>(&swos.g_numSelectedTeams) % 2);
        store(&swos.g_numSelectedTeams, 2);
        A1 = swos.g_selectedTeams;
        A2 = swos.g_selectedTeams + sizeof(TeamFile);
        InitAndPlayGame();
    }

    auto currentOffset = SDL_RWseek(m_dataFile, 0, RW_SEEK_CUR);
    auto endOffset = SDL_RWseek(m_dataFile, 0, RW_SEEK_END);
    assertEqual(currentOffset, endOffset);

    SDL_RWclose(m_dataFile);
}

void RecordedDataTest::verifyCalculateDeltaXandY()
{
    const auto& data = kCalculateDeltaXAndYTestData[m_currentDataIndex];
    Sprite s;
    s.speed = data.speed;
    s.x = data.x;
    s.y = data.y;
    s.destX = data.destX;
    s.destY = data.destY;
    updateSpriteDirectionAndDeltas(s);
    assertEqual(s.fullDirection, (data.angle & 0xffff));
    assertEqual(s.deltaX.raw(), data.deltaX);
    assertEqual(s.deltaY.raw(), data.deltaY);
}

void RecordedDataTest::finalizeRecordedDataVerification()
{
    setGameLoopStartHook(nullptr);
    setGameLoopEndHook(nullptr);
}

void RecordedDataTest::setFrameInput()
{
    m_frame = readNextFrame(m_dataFile);

    if (m_firstFrame) {
        swos.extraTimeState = m_header.extraTime;
        swos.penaltiesState = m_header.penaltyShootout;
        swos.topTeamInGame.markedPlayer = m_header.topTeamMarkedPlayer;
        swos.bottomTeamInGame.markedPlayer = m_header.bottomTeamMarkedPlayer;
        m_firstFrame = false;
    }

    m_player1Keys = controlFlagsToKeys(m_frame.player1Controls, kPlayer1Keys);
    m_player2Keys = controlFlagsToKeys(m_frame.player2Controls, kPlayer2Keys);

    for (auto vec : { &m_player1Keys, &m_player2Keys })
        for (auto key : *vec)
            key == SDL_SCANCODE_ESCAPE ? queueSdlKeyDown(key) : setSdlKeyDown(key);
}

void RecordedDataTest::verifyFrame()
{
    for (auto vec : { &m_player1Keys, &m_player2Keys })
        for (auto key : *vec)
            setSdlKeyUp(key);

    if (memcmp(&m_frame.topTeamGame, &swos.topTeamInGame, sizeof(m_frame.topTeamGame)))
        verifyTeamGame(m_frame.topTeamGame, swos.topTeamInGame);
    if (memcmp(&m_frame.bottomTeamGame, &swos.bottomTeamInGame, sizeof(m_frame.bottomTeamGame)))
        verifyTeamGame(m_frame.bottomTeamGame, swos.bottomTeamInGame);
    assertMemEqual(&m_frame.topTeamStats, &swos.team1StatsData);
    assertMemEqual(&m_frame.bottomTeamStats, &swos.team2StatsData);

    verifyTeam(m_frame.topTeam, swos.topTeamData);
    verifyTeam(m_frame.bottomTeam, swos.bottomTeamData);

    verifyPlayerSpriteOrder(m_frame.topTeamPlayers, swos.topTeamData);
    verifyPlayerSpriteOrder(m_frame.bottomTeamPlayers, swos.bottomTeamData);

    verifySprites(m_frame.sprites);

    assertEqual(m_frame.cameraX, getCameraX());
    assertEqual(m_frame.cameraY, getCameraY());

    assertEqual(m_frame.cameraXVelocity, swos.cameraXVelocity);
    assertEqual(m_frame.cameraYVelocity, swos.cameraYVelocity);

    int gameTime = m_frame.gameTime[3] + 10 * m_frame.gameTime[2] + 100 * m_frame.gameTime[1];
    assertEqual(gameTime, gameTimeInMinutes());
    assertEqual(m_frame.gameTime[0], 0);

    assertEqual(m_frame.team1Goals, swos.statsTeam1Goals);
    assertEqual(m_frame.team2Goals, swos.statsTeam2Goals);

    assertEqual(m_frame.gameState, static_cast<int>(swos.gameState));
    assertEqual(m_frame.gameStatePl, static_cast<int>(swos.gameStatePl));

    if (MAKE_FULL_VERSION(m_header.major, m_header.minor) >= MAKE_FULL_VERSION(1, 3))
        verifyBench();

    // SWOS resets game tick to 0 when fading in, and that happens when the game ends; this is just a
    // minor implementation detail, this procedure executes *after* reset and SWOS++'s *before*,
    // so we have to compensate like this (not to lose a frame with tick zero after the reset)
    if (swos.currentGameTick || !m_lastGameTick) {
        m_lastGameTick = swos.currentGameTick;
        swos.currentGameTick++;
    } else {
        m_lastGameTick = 0;
    }

    swos.currentTick++;
}

void RecordedDataTest::verifyTeam(const TeamGeneralInfo& recTeam, const TeamGeneralInfo& team)
{
    assert(recTeam.opponentTeam.getRaw() == 0 || recTeam.opponentTeam.getRaw() == 1);
    assert(team.opponentTeam ==
        (recTeam.opponentTeam.getRaw() == 0 ? &swos.topTeamData : &swos.bottomTeamData));

    auto inGameTeamPtr = recTeam.inGameTeamPtr.asAlignedDword();
    assert(inGameTeamPtr == 0 || inGameTeamPtr == 1);
    assert(team.inGameTeamPtr.asAligned() ==
        (inGameTeamPtr == 0 ? &swos.topTeamInGame : &swos.bottomTeamInGame));

    auto teamStatsPtr = recTeam.teamStatsPtr.asAlignedDword();
    assert(teamStatsPtr == 0 || teamStatsPtr == 1);
    assert(team.teamStatsPtr.asAligned() ==
        (teamStatsPtr == 0 ? &swos.team1StatsData : &swos.team2StatsData));

    assert(recTeam.players.getRaw() == 0 || recTeam.players.getRaw() == 1);
    assert(team.players ==
        (recTeam.players.getRaw() == 0 ? swos.team1SpritesTable : swos.team2SpritesTable));

    verifyShotChanceTable(recTeam.shotChanceTable.getRaw(), team.shotChanceTable);

    verifySpritePointer(recTeam.controlledPlayerSprite, team.controlledPlayerSprite);
    verifySpritePointer(recTeam.passToPlayerPtr, team.passToPlayerPtr);
    verifySpritePointer(recTeam.lastHeadingPlayer, team.lastHeadingPlayer);
    verifySpritePointer(recTeam.passingKickingPlayer, team.passingKickingPlayer);

    assertEqual(recTeam.playerNumber, team.playerNumber);
    assertEqual(recTeam.playerCoachNumber, team.playerCoachNumber);
    assertEqual(recTeam.isPlCoach, team.isPlCoach);
    assertEqual(recTeam.teamNumber, team.teamNumber);
    assertEqual(recTeam.tactics, team.tactics);
    assertEqual(recTeam.updatePlayerIndex, team.updatePlayerIndex);
    assertEqual(recTeam.playerHasBall, team.playerHasBall);
    assertEqual(recTeam.playerHadBall, team.playerHadBall);
    assertEqual(recTeam.currentAllowedDirection, team.currentAllowedDirection);
    assertEqual(recTeam.direction, team.direction);
    assertEqual(recTeam.quickFire, team.quickFire);
    assertEqual(recTeam.normalFire, team.normalFire);
    assertEqual(recTeam.firePressed, team.firePressed);
    assertEqual(!!recTeam.fireThisFrame, !!team.fireThisFrame);
    assertEqual(recTeam.headerOrTackle, team.headerOrTackle);
    assertEqual(recTeam.fireCounter, team.fireCounter);
    assertEqual(recTeam.allowedPlDirection, team.allowedPlDirection);
    assertEqual(recTeam.shooting, team.shooting);
    assertEqual(recTeam.ofs60, team.ofs60);
    assertEqual(recTeam.plVeryCloseToBall, team.plVeryCloseToBall);
    assertEqual(recTeam.plCloseToBall, team.plCloseToBall);
    assertEqual(recTeam.plNotFarFromBall, team.plNotFarFromBall);
    assertEqual(recTeam.ballLessEqual4, team.ballLessEqual4);
    assertEqual(recTeam.ball4To8, team.ball4To8);
    assertEqual(recTeam.ball8To12, team.ball8To12);
    assertEqual(recTeam.ball12To17, team.ball12To17);
    assertEqual(recTeam.ballAbove17, team.ballAbove17);
    assertEqual(recTeam.prevPlVeryCloseToBall, team.prevPlVeryCloseToBall);
    assertEqual(recTeam.ofs70, team.ofs70);
    assertEqual(recTeam.goalkeeperSavedCommentTimer, team.goalkeeperSavedCommentTimer);
    assertEqual(recTeam.ofs78, team.ofs78);
    assertEqual(recTeam.goalkeeperJumpingRight, team.goalkeeperJumpingRight);
    assertEqual(recTeam.goalkeeperJumpingLeft, team.goalkeeperJumpingLeft);
    assertEqual(recTeam.ballOutOfPlayOrKeeper, team.ballOutOfPlayOrKeeper);
    assertEqual(recTeam.goaliePlayingOrOut, team.goaliePlayingOrOut);
    assertEqual(recTeam.passingBall, team.passingBall);
    assertEqual(recTeam.passingToPlayer, team.passingToPlayer);
    assertEqual(recTeam.playerSwitchTimer, team.playerSwitchTimer);
    assertEqual(recTeam.ballInPlay, team.ballInPlay);
    assertEqual(recTeam.ballOutOfPlay, team.ballOutOfPlay);
    assertEqual(recTeam.ballX, team.ballX);
    assertEqual(recTeam.ballY, team.ballY);
    assertEqual(recTeam.passKickTimer, team.passKickTimer);
    assertEqual(recTeam.ofs108, team.ofs108);
    assertEqual(recTeam.ballCanBeControlled, team.ballCanBeControlled);
    assertEqual(recTeam.ballControllingPlayerDirection, team.ballControllingPlayerDirection);
    assertEqual(recTeam.ofs114, team.ofs114);
    assertEqual(recTeam.ofs116, team.ofs116);
    assertEqual(recTeam.spinTimer, team.spinTimer);
    assertEqual(recTeam.leftSpin, team.leftSpin);
    assertEqual(recTeam.rightSpin, team.rightSpin);
    assertEqual(recTeam.longPass, team.longPass);
    assertEqual(recTeam.longSpinPass, team.longSpinPass);
    assertEqual(recTeam.passInProgress, team.passInProgress);
    assertEqual(recTeam.AITimer, team.AITimer);
    assertEqual(recTeam.ofs134, team.ofs134);
    assertEqual(recTeam.ofs136, team.ofs136);
    assertEqual(recTeam.ofs138, team.ofs138);
    assertEqual(recTeam.unkTimer, team.unkTimer);
    assertEqual(recTeam.goalkeeperPlaying, team.goalkeeperPlaying);
    assertEqual(recTeam.resetControls, team.resetControls);
    assertEqual(!!recTeam.secondaryFire, !!team.secondaryFire);
}

void RecordedDataTest::verifyTeamGame(const TeamGame& recTeam, const TeamGame& team)
{
    assertEqual(recTeam.prShirtType, team.prShirtType);
    assertEqual(recTeam.prShirtCol, team.prShirtCol);
    assertEqual(recTeam.prStripesCol, team.prStripesCol);
    assertEqual(recTeam.prShortsCol, team.prShortsCol);
    assertEqual(recTeam.prSocksCol, team.prSocksCol);
    assertEqual(recTeam.secShirtType, team.secShirtType);
    assertEqual(recTeam.secShirtCol, team.secShirtCol);
    assertEqual(recTeam.secStripesCol, team.secStripesCol);
    assertEqual(recTeam.secShortsCol, team.secShortsCol);
    assertEqual(recTeam.secSocksCol, team.secSocksCol);
    assertEqual(recTeam.markedPlayer, team.markedPlayer);
    assertMemEqual(recTeam.teamName, team.teamName);
    assertEqual(recTeam.unk_1, team.unk_1);
    assertEqual(recTeam.numOwnGoals, team.numOwnGoals);
    assertEqual(recTeam.unk_2, team.unk_2);
    assertMemEqual(recTeam.unknownTail, team.unknownTail);
    for (size_t i = 0; i < std::size(recTeam.players); i++) {
        auto& recPlayer = recTeam.players[i];
        auto& player = team.players[i];
        assertEqual(recPlayer.substituted, player.substituted);
        assertEqual(recPlayer.index, player.index);
        assertEqual(recPlayer.goalsScored, player.goalsScored);
        assertEqual(recPlayer.shirtNumber, player.shirtNumber);
        assertEqual(static_cast<int>(recPlayer.position), static_cast<int>(player.position));
        assertEqual(recPlayer.face, player.face);
        assertEqual(recPlayer.isInjured, player.isInjured);
        assertEqual(recPlayer.field_7, player.field_7);
        assertEqual(recPlayer.field_8, player.field_8);
        assertEqual(recPlayer.field_9, player.field_9);
        assertEqual(recPlayer.cards, player.cards);
        assertEqual(recPlayer.field_B, player.field_B);
        assertMemEqual(recPlayer.shortName, player.shortName);
        assertEqual(recPlayer.passing, player.passing);
        assertEqual(recPlayer.shooting, player.shooting);
        assertEqual(recPlayer.heading, player.heading);
        assertEqual(recPlayer.tackling, player.tackling);
        assertEqual(recPlayer.ballControl, player.ballControl);
        assertEqual(recPlayer.speed, player.speed);
        assertEqual(recPlayer.finishing, player.finishing);
        assertEqual(recPlayer.goalieDirection, player.goalieDirection);
        assertEqual(recPlayer.injuriesBitfield, player.injuriesBitfield);
        assertEqual(recPlayer.halfPlayed, player.halfPlayed);
        assertEqual(recPlayer.face2, player.face2);
        assertMemEqual(recPlayer.fullName, player.fullName);
    }
}

void RecordedDataTest::verifySprites(const Sprite *sprites)
{
    for (int i = 0; i < kFrameNumSprites; i++) {
        const auto& sprite1 = sprites[i];
        int spriteIndex = i;
        if (i == kCornerFlagSprite) {
            constexpr FixedPoint kCenterX = 176;
            constexpr FixedPoint kCenterY = 349;
            if (getCameraX() >= kCenterX)
                spriteIndex += getCameraY() >= kCenterY ? 3 : 1;
            else if (getCameraY() >= kCenterY)
                spriteIndex += 2;
        } else if (i == kCurrentPlayerNameSprite) {
            auto [actualTopTeam, actualPlayerNumber] = getDisplayedPlayerNumberAndTeam();
            bool playerNameDisplayed = actualPlayerNumber >= 0;
            bool spriteVisible = sprite1.visible && sprite1.hasImage() &&
                sprite1.x < 25'000 && sprite1.y < 25'000 && sprite1.z < 25'000;
            if (spriteVisible) {
                assertTrue(playerNameDisplayed);
                int imageIndex = sprite1.imageIndex;
                assert(imageIndex >= kTeam1PlayerNamesStartSprite && imageIndex <= kTeam1PlayerNamesEndSprite ||
                    imageIndex >= kTeam2PlayerNamesStartSprite && imageIndex <= kTeam2PlayerNamesEndSprite);
                bool topTeam = imageIndex <= kTeam1PlayerNamesEndSprite;
                int playerNumber = imageIndex - (topTeam ? kTeam1PlayerNamesStartSprite : kTeam2PlayerNamesStartSprite);
                assertEqual(topTeam, actualTopTeam);
                assertEqual(playerNumber, actualPlayerNumber);
            } else {
                assertFalse(playerNameDisplayed);
            }
            continue;
        } else if (i > kCornerFlagSprite) {
            // for later, currently not used
            spriteIndex += kNumCornerFlagSprites - 1;
        }
        const auto& sprite2 = *spriteAt(spriteIndex);

        assertEqual(sprite1.teamNumber, sprite2.teamNumber);
        assertEqual(sprite1.playerOrdinal, sprite2.playerOrdinal);
        assertEqual(sprite1.frameOffset, sprite2.frameOffset);
        assertEqual(sprite1.startingDirection, sprite2.startingDirection);
        assert(sprite1.state == sprite2.state);
        assertEqual(sprite1.playerDownTimer, sprite2.playerDownTimer);
        assertEqual(sprite1.unk001, sprite2.unk001);
        assertEqual(sprite1.unk002, sprite2.unk002);
        assertEqual(sprite1.x, sprite2.x);
        assertEqual(sprite1.y, sprite2.y);
        assertEqual(sprite1.z, sprite2.z);
        assertEqual(sprite1.direction, sprite2.direction);
        assertEqual(sprite1.speed, sprite2.speed);
        assertEqual(sprite1.deltaX, sprite2.deltaX);
        assertEqual(sprite1.deltaY, sprite2.deltaY);
        assertEqual(sprite1.deltaZ, sprite2.deltaZ);
        assertEqual(sprite1.destX, sprite2.destX);
        assertEqual(sprite1.destY, sprite2.destY);
        assertMemEqual(sprite1.unk003, sprite2.unk003);
        assertEqual(sprite1.visible, sprite2.visible);
        assertEqual(sprite1.saveSprite, sprite2.saveSprite);
        assertEqual(sprite1.ballDistance, sprite2.ballDistance);
        assertEqual(sprite1.unk004, sprite2.unk004);
        assertEqual(sprite1.unk005, sprite2.unk005);
        assertEqual(sprite1.fullDirection, sprite2.fullDirection);
        auto animTable2 = sprite2.animTablePtr.asAligned();
        if (!animTable2)
            animTable2 = reinterpret_cast<PlayerAnimationTable *>(-1);
        assertEqual(convertAnimationTable(sprite1.animTablePtr.getRaw()), animTable2);
        assertEqual(convertFrameIndicesTable(sprite1.frameIndicesTable.getRaw()), sprite2.frameIndicesTable.asAligned());
        // in order for these sprite frame related tests to be successful game resolution must be set
        // to 320x200, and zoom to 1; otherwise sprites randomly appear on and off screen and animation
        // frames state diverges beyond reconciliation
        assertEqual(sprite1.onScreen, sprite2.onScreen);
        assertEqual(sprite1.frameIndex, sprite2.frameIndex);
        assertEqual(sprite1.frameDelay, sprite2.frameDelay);
        assertEqual(sprite1.cycleFramesTimer, sprite2.cycleFramesTimer);
        assertEqual(sprite1.frameSwitchCounter, sprite2.frameSwitchCounter);
        if (sprite1.onScreen || i != kCornerFlagSprite)
			assertEqual(sprite1.imageIndex, sprite2.imageIndex);
        assertEqual(sprite1.unk006, sprite2.unk006);
        assertEqual(sprite1.unk007, sprite2.unk007);
        assertEqual(sprite1.unk008, sprite2.unk008);
        assertEqual(sprite1.playerDirection, sprite2.playerDirection);
        assertEqual(sprite1.isMoving, sprite2.isMoving);
        assertEqual(sprite1.tackleState, sprite2.tackleState);
        assertEqual(sprite1.unk009, sprite2.unk009);
        assertEqual(static_cast<int>(sprite1.destReachedState), static_cast<int>(sprite2.destReachedState));
        assertEqual(sprite1.cards, sprite2.cards);
        assertEqual(sprite1.injuryLevel, sprite2.injuryLevel);
        assertEqual(sprite1.tacklingTimer, sprite2.tacklingTimer);
        assertEqual(sprite1.sentAway, sprite2.sentAway);
    }
}

void RecordedDataTest::verifyBench()
{
    BenchData data;
    fillBenchData(data);
    assertEqual(data.pl1TapCount, m_frame.bench.pl1TapCount);
    assertEqual(data.pl2TapCount, m_frame.bench.pl2TapCount);
    assertEqual(data.pl1PreviousDirection, m_frame.bench.pl1PreviousDirection);
    assertEqual(data.pl2PreviousDirection, m_frame.bench.pl2PreviousDirection);
    assertEqual(data.pl1BlockWhileHoldingDirection, m_frame.bench.pl1BlockWhileHoldingDirection);
    assertEqual(data.pl2BlockWhileHoldingDirection, m_frame.bench.pl2BlockWhileHoldingDirection);
    assertEqual(data.controls, m_frame.bench.controls);
    // we have to draw the bench all the time, that's why bench pointer is always set to something
    // just skip the check if SWOS hasn't set the bench team pointer yet
    if (m_frame.bench.teamData >= 0)
        assertEqual(data.teamData, m_frame.bench.teamData);
    if (m_frame.bench.teamGame >= 0)
        assertEqual(data.teamGame, m_frame.bench.teamGame);
    assertEqual(data.state, m_frame.bench.state);
    assertEqual(data.goToBenchTimer, m_frame.bench.goToBenchTimer);
    assertEqual(data.bench1Called, m_frame.bench.bench1Called);
    assertEqual(data.bench2Called, m_frame.bench.bench2Called);
    assertEqual(data.blockDirections, m_frame.bench.blockDirections);
    assertEqual(data.fireTimer, m_frame.bench.fireTimer);
    assertEqual(data.blockFire, m_frame.bench.blockFire);
    assertEqual(data.lastDirection, m_frame.bench.lastDirection);
    assertEqual(data.movementDelayTimer, m_frame.bench.movementDelayTimer);
    assertEqual(data.trainingTopTeam, m_frame.bench.trainingTopTeam);
    assertEqual(data.teamsSwapped, m_frame.bench.teamsSwapped);
    assertEqual(data.alternateTeamsTimer, m_frame.bench.alternateTeamsTimer);
    assertEqual(data.arrowPlayerIndex, m_frame.bench.arrowPlayerIndex);
    assertEqual(data.selectedMenuPlayerIndex, m_frame.bench.selectedMenuPlayerIndex);
    assertEqual(data.playerToEnterGameIndex, m_frame.bench.playerToEnterGameIndex);
    assertEqual(data.playerToBeSubstitutedPos, m_frame.bench.playerToBeSubstitutedPos);
    assertEqual(data.playerToBeSubstitutedOrd, m_frame.bench.playerToBeSubstitutedOrd);
    assertEqual(data.selectedFormationEntry, m_frame.bench.selectedFormationEntry);
    assertMemEqual(data.shirtNumberTable, m_frame.bench.shirtNumberTable);
}

PlayerAnimationTable *RecordedDataTest::convertAnimationTable(int offset)
{
    constexpr int kAnimTablesAreaLength = SwosVM::animTablesEnd - SwosVM::animTablesStart;
    assert(offset == -1 || offset < kAnimTablesAreaLength);
    return reinterpret_cast<PlayerAnimationTable *>(offset + (offset == -1 ? 0 : swos.animTablesStart));
}

int16_t *RecordedDataTest::convertFrameIndicesTable(int offset)
{
    constexpr int kHeaderFramesAreaLength = SwosVM::strongHeaderUpFrames - SwosVM::choosingPreset;
    if (offset == -1) {
        return nullptr;
    } else if (offset == -2) {
        return swos.ballMovingFrameIndices;
    } else if (offset == -3) {
        return swos.ballStaticFrameIndices;
    } else if (offset & 0x80000000) {
        return reinterpret_cast<int16_t *>(swos.strongHeaderUpFrames + (offset & 0x7fffffff));
    } else {
        constexpr int kFrameIndicesTablesAreaLength = SwosVM::animTablesStart - SwosVM::frameIndicesTablesStart;
        assert(static_cast<unsigned>(offset) < kFrameIndicesTablesAreaLength);
        return reinterpret_cast<int16_t *>(swos.frameIndicesTablesStart + offset);
    }
}

void RecordedDataTest::verifyPlayerSpriteOrder(char *players, const TeamGeneralInfo& team)
{
    for (int i = 0; i < 11; i++) {
        int index = players[i];
        auto sprite = team.players[i];
        assert(index >= 4 && index <= 25);
        assert(spriteAt(index) == sprite);
    }
}

void RecordedDataTest::verifyShotChanceTable(int recOffset, SwosDataPointer<int16_t> table)
{
    if (recOffset == -1)
        assert(table.getRaw() == 0 || table.getRaw() == -1);
    else if (recOffset == 0)
        assert(table == swos.playerShotChanceTable);
    else {
        auto it = std::find(std::begin(swos.goalieSkillTables), std::end(swos.goalieSkillTables), table.asPtr());
        assertNotEqual(it, std::end(swos.goalieSkillTables));
        int index = it - std::begin(swos.goalieSkillTables);
        assertEqual(index, recOffset - 1);
    }
}

void RecordedDataTest::verifySpritePointer(SwosDataPointer<Sprite> recSprite, SwosDataPointer<Sprite> sprite)
{
    int index = recSprite.getRaw();
    if (index == -1) {
        assert(!sprite);
    } else {
        assert(index >= 4 && index <= 25);
        assert(spriteAt(index) == sprite);
    }
}

auto RecordedDataTest::openDataFile(const std::string& file) -> std::pair<SDL_RWops *, HeaderV1p2>
{
    auto f = openResFile(file.c_str());
    assertMessage(f, "Failed to open file "s + file);

    HeaderV1p2 h;
    auto objectsRead = SDL_RWread(f, &h, sizeof(Header), 1);
    assertMessage(objectsRead == 1, "Failed to read header from file "s + file);

    if (h.major > 1 || h.minor >= 2) {
        objectsRead = SDL_RWread(f, &h.trainingGameCareerSize, sizeof(HeaderV1p2) - sizeof(Header), 1);
        assertMessage(objectsRead == 1, "Failed to read v1.2 header from file "s + file);
        if (h.trainingGameCareerSize) {
            objectsRead = SDL_RWread(f, &swos.careerFileBuffer, h.trainingGameCareerSize, 1);
            assertMessage(objectsRead == 1, "Failed to read training game career data from file "s + file);
        }
    } else {
        h.trainingGameCareerSize = 0;
        h.season = 8;
        h.minSubstitutes = 2;
        h.maxSubstitutes = 5;
    }

    return { f, h };
}

auto RecordedDataTest::readNextFrame(SDL_RWops *f) -> Frame
{
    Frame frame;
    auto objectsRead = SDL_RWread(f, &frame, m_frameSize, 1);
    assertMessage(objectsRead == 1, "Failed to read data frame");

    return frame;
}

std::vector<SDL_Scancode> RecordedDataTest::controlFlagsToKeys(ControlFlags flags, const DefaultKeySet& keySet)
{
    std::vector<SDL_Scancode> keys;

    // test escape first, so handle keys routine gets it (tests 1st)
    if (flags & kEscape)
        keys.push_back(SDL_SCANCODE_ESCAPE);
    if (flags & kUp)
        keys.push_back(keySet[0]);
    if (flags & kDown)
        keys.push_back(keySet[1]);
    if (flags & kLeft)
        keys.push_back(keySet[2]);
    if (flags & kRight)
        keys.push_back(keySet[3]);
    if (flags & kFire)
        keys.push_back(keySet[4]);
    if (flags & kBench)
        keys.push_back(keySet[5]);

    return keys;
}
