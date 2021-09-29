#pragma once

constexpr int kNumFormationEntries = 18;

void initBenchBeforeMatch();
void updateBench();
bool inBench();
bool inBenchMenus();
bool isCameraLeavingBench();
void clearCameraLeavingBench();
void setCameraLeavingBench();
int getBenchY();
int getOpponentBenchY();
void swapBenchWithOpponent();
bool getBenchOff();
void setBenchOff();
float benchCameraX();
const PlayerGame& getBenchPlayer(int index);
int getBenchPlayerPosition(int index);
