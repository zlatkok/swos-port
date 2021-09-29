#include "bench.h"
#include "benchControls.h"
#include "drawBench.h"
#include "pitchConstants.h"

constexpr float kBenchX = 27;

constexpr int kTopBenchY = 389;
constexpr int kBottomBenchY = 485;

constexpr int kTrainingPitchBenchY = 456;

constexpr int kPlayerGoingInX = 39;
constexpr int kPlayerGoingInY = kPitchCenterY;

static bool m_benchOff;
static bool m_leavingBench;

static int m_benchY;
static int m_opponentBenchY;

static void invokeBench();
static void checkForThrowInAndKeepersBall();
static void checkIfGoalkeeperClaimedTheBall();

void initBenchBeforeMatch()
{
    m_benchOff = false;
    swos.g_inSubstitutesMenu = 0;
    swos.g_cameraLeavingSubsTimer = 0;
    clearCameraLeavingBench();
    initBenchControls();
    initBenchMenusBeforeMatch();
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

bool isCameraLeavingBench()
{
    return m_leavingBench;
}

void clearCameraLeavingBench()
{
    m_leavingBench = false;
}

void setCameraLeavingBench()
{
    m_leavingBench = true;
    checkIfGoalkeeperClaimedTheBall();
    swos.g_inSubstitutesMenu = 0;
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

bool getBenchOff()
{
    return m_benchOff;
}

void setBenchOff()
{
    m_benchOff = false;
}

float benchCameraX()
{
    return kBenchX;
}

const PlayerGame& getBenchPlayer(int index)
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

// Make the bench happen.
static void invokeBench()
{
    swos.stateGoal = 0;
    swos.g_inSubstitutesMenu = 1;
    swos.resultTimer = -swos.resultTimer;
    swos.statsTimer = -swos.statsTimer;

    m_benchY = kTopBenchY;
    m_opponentBenchY = kBottomBenchY;
    bool topTeam = getBenchTeamData() == &swos.topTeamIngame;

    if (swos.g_trainingGame) {
        m_benchY = m_opponentBenchY = kTrainingPitchBenchY;
        // always start from the bottom bench half
        setTrainingTopTeam(topTeam ? 0 : 1);
    } else {
        // bit 2 of the 2nd character of top team name... nice criteria :P
        if (swos.topTeamIngame.teamName[1] & 2)
            std::swap(m_benchY, m_opponentBenchY);
        if (topTeam)
            std::swap(m_benchY, m_opponentBenchY);
    }

    swos.plComingX = kPlayerGoingInX;
    swos.plComingY = kPlayerGoingInY;

    checkForThrowInAndKeepersBall();
}

// If the player is taking a throw-in make him drop the ball and go away.
// (if not it looks weird if the throw-in is near the bench)
static void checkForThrowInAndKeepersBall()
{
    auto player = swos.lastTeamPlayedBeforeBreak->controlledPlayerSprite;
    if (player && player->state == PlayerState::kThrowIn) {
        swos.hideBallShadow = 0;
        player->state = PlayerState::kNormal;
        A0 = &swos.playerNormalStandingAnimTable;
        A1 = player;
        SetPlayerAnimationTable();
    }

    checkIfGoalkeeperClaimedTheBall();
}

static void checkIfGoalkeeperClaimedTheBall()
{
    if (swos.gameState == GameState::kKeeperHoldsTheBall) {
        auto team = swos.lastTeamPlayedBeforeBreak;
        A6 = team;
        A1 = team->players[0];
        A2 = &swos.ballSprite;  // the original game fails to set this
        GoalkeeperClaimedTheBall();
    } else {
        swos.breakCameraMode = -1;
        swos.gameStatePl = GameState::kStopped;
        swos.stoppageTimerTotal = 0;
        swos.stoppageTimerActive = 0;
        StopAllPlayers();
        swos.cameraXVelocity = 0;
        swos.cameraYVelocity = 0;
    }
}
