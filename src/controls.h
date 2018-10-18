#pragma once

void initJoypads();
void finishJoypads();
void normalizeInput();
bool anyInputActive();
void pressFire();
void releaseFire();
void updateControls();
std::tuple<int, int, int, int> joypad1DeadZone();
std::tuple<int, int, int, int> joypad2DeadZone();
void setJoypad1DeadZone(int xPos, int xNeg, int yPos, int yNeg);
void setJoypad2DeadZone(int xPos, int xNeg, int yPos, int yNeg);
