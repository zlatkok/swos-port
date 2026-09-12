#pragma once

void stopAllPlayers();
void initPlayerShotChanceTables();
void updatePlayerShotChanceTable(TeamGeneralInfo& team, const Sprite& player);

#ifdef SWOS_TEST
const ShotChanceTable *getPlayerShotChanceTable();
int getGoalieShotChanceTableIndex(const ShotChanceTable *ptr);
#endif
