#pragma once

bool amigaModeActive();
void setAmigaModeEnabled(bool enable);
void checkForAmigaModeDirectionFlipBan(const Sprite *sprite);
void applyAmigaModeDirectionFlipBan(TeamGeneralInfo *team);
