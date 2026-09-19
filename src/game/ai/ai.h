#pragma once

struct Sprite;
struct TeamGeneralInfo;

void updateCpuPlayerControls(TeamGeneralInfo& team);
void cpuPlayerAttemptTackle(TeamGeneralInfo& team, const Sprite& player);
void resetCpuResumePlayTurnDirection();
