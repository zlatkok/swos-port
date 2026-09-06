#include "bench.h"
#include "updateBench.h"
#include "drawBench.h"
#include "pitchConstants.h"
#include "player.h"
#include "team.h"
#include "game.h"
#include "animation.h"

constexpr FixedPoint kBenchX = 27;

constexpr int kTopBenchY = 389;
constexpr int kBottomBenchY = 485;

constexpr int kTrainingPitchBenchY = 456;

constexpr int kPlayerGoingInX = 26;
constexpr int kPlayerGoingInY = kPitchCenterY;

static int m_benchY;
static int m_opponentBenchY;

static void initBench();
static void invokeBench();
static void checkForThrowInAndKeepersBall();

void initBenchBeforeMatch()
{
    swos.g_inSubstitutesMenu = 0;
    swos.g_cameraLeavingSubsTimer = 0;
    initBenchControls();
    initBenchMenusBeforeMatch();
    initBench();
}

// Main entry point from game loop.
void updateBench()
{
    if (benchCheckControls())
        invokeBench();
}

bool inBench()
{
    return swos.g_inSubstitutesMenu != 0;
}

bool inBenchMenus()
{
    return inBench() && getBenchState() == BenchState::kInitial;
}

int getBenchY()
{
    return m_benchY;
}

int getOpponentBenchY()
{
    return m_opponentBenchY;
}

void swapBenchWithOpponent()
{
    std::swap(m_benchY, m_opponentBenchY);
}

void setBenchOff()
{
    checkIfGoalkeeperClaimedTheBall();
    swos.g_inSubstitutesMenu = 0;
}

FixedPoint benchCameraX()
{
    return kBenchX;
}

const PlayerInfo& getBenchPlayer(int index)
{
    return getBenchTeamData()->players[getBenchPlayerPosition(index)];
}

int getBenchPlayerPosition(int index)
{
    assert(index >= 0 && index < 11);

    auto tactics = getBenchTeam()->teamNumber == 1 ? swos.pl1Tactics : swos.pl2Tactics;
    assert(tactics < swos.positionsTable.size());

    const auto& positions = swos.positionsTable[tactics];

    assert(positions[index] >= 1 && positions[index] <= 11);
    return positions[index] - 1;
}

static void initBench()
{
    swos.stateGoal = 0;
    swos.resultTimer = -swos.resultTimer;
    swos.statsTimer = -swos.statsTimer;

    m_benchY = kTopBenchY;
    m_opponentBenchY = kBottomBenchY;
    bool topTeam = getBenchTeamData() == &swos.topTeamInGame;

    if (swos.g_trainingGame) {
        m_benchY = m_opponentBenchY = kTrainingPitchBenchY;
    } else {
        // bit 2 of the 2nd character of top team name... nice criteria :P
        if (swos.topTeamInGame.teamName[1] & 2)
            std::swap(m_benchY, m_opponentBenchY);
        if (topTeam)
            std::swap(m_benchY, m_opponentBenchY);
    }

    swos.plComingX = kPlayerGoingInX;
    swos.plComingY = kPlayerGoingInY;
}

// Make the bench happen.
static void invokeBench()
{
    initBench();

    swos.g_inSubstitutesMenu = 1;
    checkForThrowInAndKeepersBall();
}

// If the player is taking a throw-in make him drop the ball and go away.
// (if not it looks weird if the throw-in is near the bench)
static void checkForThrowInAndKeepersBall()
{
    auto player = swos.lastTeamPlayedBeforeBreak->controlledPlayer;
    if (player && player->state == PlayerState::kThrowIn) {
        swos.hideBall = 0;
        player->state = PlayerState::kNormal;
        setPlayerAnimationTable(*player, getPlayerNormalStandingAnimTable());
    }

    checkIfGoalkeeperClaimedTheBall();
}
