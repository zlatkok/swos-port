#include "team.h"
#include "player.h"

static const ShotChanceTable kGoalieSkillTables[8] = {
    { 7, 424, -50, 832, 160, { 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 }, 3, 5, 8, 5, 11, 2, 6, 8, 5 },
    { 6, 588, -4, 864, 176, { 3, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7 }, 4, 5, 7, 6, 10, 3, 6, 7, 6 },
    { 5, 752, 42, 896, 192, { 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 7, 7 }, 5, 5, 6, 7, 9, 4, 6, 6, 7 },
    { 4, 916, 88, 928, 208, { 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 6, 7 }, 6, 5, 5, 8, 8, 5, 6, 5, 8 },
    { 3, 1080, 134, 960, 224, { 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 5, 6 }, 6, 6, 4, 9, 7, 6, 6, 4, 9 },
    { 2, 1244, 180, 992, 240, { 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 4, 5 }, 7, 6, 3, 10, 6, 7, 6, 3, 10 },
    { 1, 1408, 226, 1024, 256, { 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4 }, 8, 6, 2, 11, 5, 8, 6, 2, 11 },
    { 99, 1408, 226, 1024, 256, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 }, 9, 6, 1, 12, 4, 9, 6, 1, 12 },
};
static const ShotChanceTable kPlayerShotChanceTable = {
    8, 1024, 112, 800, 144, { 7, 7, 7, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7 }, 1, 6, 9, 4, 12, 1, 6, 9, 4
};

static SwosDataPointer<ShotChanceTable> m_shotChanceTables;

static void stopPlayers(TeamGeneralInfo& team);

using namespace SwosVM;

void stopAllPlayers()
{
    for (auto team : { &swos.topTeamData, &swos.bottomTeamData }) {
        stopPlayers(*team);
        team->ballInPlay = 0;
        team->ballOutOfPlay = 0;
        team->controlledPlayer.reset();
        team->passToPlayerPtr.reset();
        team->passingBall = 0;
        team->passingToPlayer = 0;
        team->playerSwitchTimer = 0;
        team->passingKickingPlayer.reset();
#ifdef SWOS_TEST
        // original SWOS fails to reset goalkeeperPlaying for top team
        if (team == &swos.bottomTeamData)
#endif
        team->goalkeeperPlaying = 0;
    }
}

static void stopPlayers(TeamGeneralInfo& team)
{
    for (int i = 0; i < 11; i++) {
        auto& player = team.players[i];
        if (player->inNormalState() && !player->sentAway) {
            player->destX = player->x.whole();
            player->destY = player->y.whole();
        }
    }
}

// Remove this when we don't rely on VM for the gameplay anymore.
void initPlayerShotChanceTables()
{
    auto tables = SwosVM::allocateMemory(sizeof(kGoalieSkillTables) + sizeof(kPlayerShotChanceTable));
    memcpy(tables, kGoalieSkillTables, sizeof(kGoalieSkillTables));
    memcpy(tables + sizeof(kGoalieSkillTables), &kPlayerShotChanceTable, sizeof(kPlayerShotChanceTable));
    m_shotChanceTables = tables.as<ShotChanceTable *>();
}

void updatePlayerShotChanceTable(TeamGeneralInfo& team, const Sprite& player)
{
    const auto& playerInfo = getPlayerPointerFromShirtNumber(team, player);
    if (playerInfo.position == PlayerPosition::kGoalkeeper) {
        assert(playerInfo.goalieSkill < 8);
        team.shotChanceTable = &m_shotChanceTables.asPtr()[playerInfo.goalieSkill];
    } else {
        team.shotChanceTable = reinterpret_cast<ShotChanceTable *>(m_shotChanceTables.asCharPtr() + sizeof(kGoalieSkillTables));
    }
}

#ifdef SWOS_TEST
const ShotChanceTable *getPlayerShotChanceTable()
{
    return reinterpret_cast<ShotChanceTable *>(m_shotChanceTables.asCharPtr() + sizeof(kGoalieSkillTables));
}

int getGoalieShotChanceTableIndex(const ShotChanceTable *ptr)
{
    for (size_t i = 0; i < std::size(kGoalieSkillTables); ++i) {
        if (ptr == &m_shotChanceTables.asPtr()[i])
            return static_cast<int>(i);
    }

    return -1;
}
#endif
