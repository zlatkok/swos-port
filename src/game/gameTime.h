#pragma once

void resetGameTime();
bool gameTimeShowing();
void updateGameTime();
void drawGameTime();
void drawGameTime(int digit1, int digit2, int digit3);
dword gameTimeInMinutes();
std::tuple<int, int, int> gameTimeAsBcd();
bool gameAtZeroMinute();
