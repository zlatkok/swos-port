#pragma once

#include "gameControlEvents.h"
#include "controls.h"
#include "direction.h"

void resetGameControls();
bool updateFireBlocked();
TeamGeneralInfo *selectTeamForUpdate();
void updateTeamControls(TeamGeneralInfo *team);
void postUpdateTeamControls(TeamGeneralInfo *team);
GameControlEvents getPlayerEvents(PlayerNumber player);
bool isPlayerFiring(PlayerNumber player);
bool getFireStartedAndBumpFireCounter(bool currentFire, PlayerNumber player = kPlayer1);
Direction eventsToDirection(GameControlEvents events);
GameControlEvents directionToEvents(Direction direction);
bool isAnyPlayerFiring();
