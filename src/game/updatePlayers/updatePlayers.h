#pragma once

void updatePlayers(TeamGeneralInfo *team);
Sprite *getLastPlayerPlayed();
void resetLastPlayerPlayed();
Sprite *getLastPlayerBeforeGoalkeeper();
void setLastPlayerBeforeGoalkeeper(Sprite *player);
void resetLastPlayerBeforeGoalkeeper();
void setPlayerDownTacklingInterval(int interval);
void setPlayerDownHeadingInterval(int interval);
