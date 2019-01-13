#pragma once

#include "scanCodes.h"

enum PlayerNumber {
    kNoPlayer = -1,
    kPlayer1 = 0,
    kPlayer2 = 1,
};

enum Controls {
    kNone,
    kKeyboard1,
    kKeyboard2,
    kJoypad,
    kMouse,                 // :D
    kNumControls,
    kNumUniqueControls = kNumControls - 1,
};

Controls getPl1Controls();
Controls getPl2Controls();

void setPl1Controls(Controls controls, int joypadIndex = -1);
void setPl2Controls(Controls controls, int joypadIndex = -1);
void setPl1GameControls(Controls controls);
void setPl2GameControls(Controls controls);

ScanCodes& getPl1ScanCodes();
ScanCodes& getPl2ScanCodes();

bool pl1KeyboardNullScancode();
bool pl2KeyboardNullScancode();

void normalizeInput();
bool anyInputActive();
void pressFire();
void releaseFire();
int mouseWheelAmount();

void clearKeyInput();
void clearPlayer1State();

void waitForKeyPress();
void updateControls();

void loadControlOptions(const CSimpleIni& ini);
void saveControlOptions(CSimpleIni& ini);

void initGameControls();
void finishGameControls();

void updateMatchControls();
int matchControlsSelected(const MenuEntry *activeEntry);
void resetMatchControls();
bool gotMousePlayer();
