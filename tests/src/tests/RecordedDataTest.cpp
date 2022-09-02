#include "RecordedDataTest.h"
#include "unitTest.h"
#include "sdlProcs.h"
#include "gameLoop.h"
#include "audio.h"
#include "controls.h"
#include "keyboard.h"
#include <dirent.h>

constexpr int kVersionMajor = 1;
constexpr int kVersionMinor = 1;

constexpr char kDataDir[] = "recdata";
constexpr char kExtension[] = ".rgd";

static const DefaultKeySet kPlayer1Keys = {
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_RCTRL, SDL_SCANCODE_RSHIFT,
};
static const DefaultKeySet kPlayer2Keys = {
    SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_GRAVE,
};

static RecordedDataTest t;

void RecordedDataTest::init()
{
    m_files = findFiles(kExtension, kDataDir);
    disableRendering();
    takeOverInput();
}

void RecordedDataTest::finish()
{
    enableRendering();
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
        { "verify recorded game data", "verify-rec-data", bind(&RecordedDataTest::setupRecordedDataVerification),
            bind(&RecordedDataTest::verifyRecordedData), m_files.size(), false },
    };
}

void RecordedDataTest::setupRecordedDataVerification()
{
    assert(!m_files.empty());

    setSoundEnabled(false);

    setGameLoopStartHook(std::bind(&RecordedDataTest::setFrameInput, this));
    setGameLoopEndHook(std::bind(&RecordedDataTest::verifyFrame, this));

    setPl1Controls(kKeyboard1);
    setPl2Controls(kKeyboard2);

    setDefaultKeyPackForKeyboard(Keyboard::kSet1, kPlayer1Keys);
    setDefaultKeyPackForKeyboard(Keyboard::kSet2, kPlayer2Keys);
}

void RecordedDataTest::verifyRecordedData()
{
    std::tie(m_dataFile, m_header) = openDataFile(m_files[m_currentDataIndex].name.c_str());

    assert(m_header.inputControls == kKeyboardOnly || m_header.inputControls == kKeyboardAndJoypad);

    memcpy(swos.g_selectedTeams, &m_header.topTeamFile, sizeof(m_header.topTeamFile));
    memcpy(swos.g_selectedTeams + sizeof(m_header.topTeamFile), &m_header.bottomTeamFile, sizeof(m_header.bottomTeamFile));
    A1 = swos.g_selectedTeams;
    A2 = swos.g_selectedTeams + sizeof(TeamFile);
    InitAndPlayGame();
    //setup teams & all pre-game
    //run zi game
    //verify each frame
}

void RecordedDataTest::finalizeRecordedDataVerification()
{
    setGameLoopStartHook(nullptr);
    setGameLoopEndHook(nullptr);
}

void RecordedDataTest::setFrameInput()
{
    m_player1Keys = controlFlagsToKeys(m_frame.player1Controls, kPlayer1Keys);
    m_player2Keys = controlFlagsToKeys(m_frame.player2Controls, kPlayer2Keys);

    for (auto vec : { &m_player1Keys, &m_player2Keys })
        for (auto key : *vec)
            queueSdlKeyDown(key);
}

void RecordedDataTest::verifyFrame()
{
    //...

    for (auto vec : { &m_player1Keys, &m_player2Keys })
        for (auto key : *vec)
            queueSdlKeyUp(key);

    m_frame = readNextFrame(m_dataFile);
    assertMemEqual(&m_frame.topTeamGame, &swos.topTeamIngame);
    assertMemEqual(&m_frame.bottomTeamGame, &swos.bottomTeamIngame);
    assertMemEqual(&m_frame.topTeamStats, &swos.team1StatsData);
    assertMemEqual(&m_frame.bottomTeamStats, &swos.team2StatsData);

    verifyTeam(m_frame.topTeam, swos.topTeamData);
    verifyTeam(m_frame.bottomTeam, swos.bottomTeamData);

    //FixedPoint cameraX;
    //FixedPoint cameraY;
    //dword gameTime;
    //Sprite sprites[kFrameNumSprites];
}

void RecordedDataTest::verifyTeam(const TeamGeneralInfo& recTeam, const TeamGeneralInfo& team)
{
    assert(recTeam.opponentTeam.getRaw() == 0 || recTeam.opponentTeam.getRaw() == 1);
    assert(recTeam.opponentTeam.getRaw() == 0 ?
        team.opponentTeam == &swos.topTeamData : team.opponentTeam == &swos.bottomTeamData);

    assert(recTeam.inGameTeamPtr.getRaw() == 0 || recTeam.inGameTeamPtr.getRaw() == 1);
    assert(recTeam.inGameTeamPtr.getRaw() == 0 ?
        team.inGameTeamPtr == &swos.topTeamIngame : team.inGameTeamPtr == &swos.bottomTeamIngame);

    assert(recTeam.teamStatsPtr.getRaw() == 0 || recTeam.teamStatsPtr.getRaw() == 1);
    assert(recTeam.teamStatsPtr.getRaw() == 0 ?
        team.teamStatsPtr == &swos.team1StatsData : team.teamStatsPtr == &swos.team2StatsData);

    assert(recTeam.players.getRaw() == 0 || recTeam.players.getRaw() == 1);
    assert(recTeam.players.getRaw() == 0 ?
        team.players == swos.team1SpritesTable : team.players == swos.team2SpritesTable);

    verifyShotChanceTable(recTeam.shotChanceTable.getRaw(), team.shotChanceTable.getRaw());

    assert(!recTeam.players);

    ;
}

void RecordedDataTest::verifyShotChanceTable(int recOffset, int offset)
{
    if (recOffset == -1)
        assert(offset == 0 || offset == -1);
    else if (recOffset == 0)
        assert(offset == offsetof(SwosVM::SwosVariables, playerShotChanceTable));
    else
        assert(offset == 1 + offsetof(SwosVM::SwosVariables, goalieSkill0Table));
}

void RecordedDataTest::verifySpritePointer(SwosDataPointer<Sprite> recSprite, SwosDataPointer<Sprite> sprite)
{
    if (recSprite.getRaw() == -1)
        assert(!sprite);
//    sprite.getRaw() - swos.bigssprie
}

auto RecordedDataTest::openDataFile(const char *path) -> std::pair<SDL_RWops *, Header>
{
    const auto& fullPath = joinPaths(kDataDir, path);
    auto f = openFile(fullPath.c_str());
    assertMessage(f, "Failed to open file "s + fullPath);

    Header h;
    auto objectsRead = SDL_RWread(f, &h, sizeof(h), 1);
    assertMessage(objectsRead == 1, "Failed to read header from file "s + path);

    return { f, h };
}

auto RecordedDataTest::readNextFrame(SDL_RWops *f) -> Frame
{
    Frame frame;
    auto objectsRead = SDL_RWread(f, &frame, sizeof(frame), 1);
    assertMessage(objectsRead == 1, "Failed to read data frame");

    return frame;
}

std::vector<SDL_Scancode> RecordedDataTest::controlFlagsToKeys(ControlFlags flags, const DefaultKeySet& keySet)
{
    std::vector<SDL_Scancode> keys;

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
