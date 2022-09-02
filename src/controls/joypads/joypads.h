#pragma once

#include "gameControlEvents.h"
#include "controls.h"
#include "JoypadConfig.h"
#include "JoypadElementValue.h"

void initJoypads();

GameControlEvents pl1JoypadEvents();
GameControlEvents pl2JoypadEvents();
GameControlEvents eventsFromAllJoypads();
GameControlEvents joypadEvents(int index);
JoypadElementValueList joypadElementValues(int index);
bool joypadHasBasicBindings(int index);

int getPl1JoypadIndex();
int getPl2JoypadIndex();
int getJoypadIndex(PlayerNumber player);
bool setJoypad(PlayerNumber player, int joypadNo);

#ifdef VIRTUAL_JOYPAD
class VirtualJoypad;

VirtualJoypad& getVirtualJoypad();
bool getTransparentVirtualJoypadButtons();
void setTransparentVirtualJoypadButtons(bool transparentButtons);
bool getShowTouchTrails();
void setShowTouchTrails(bool showTouchTrails);

class VirtualJoypadEnabler
{
public:
    VirtualJoypadEnabler(int index);
    ~VirtualJoypadEnabler();
private:
    int m_index;
};
#endif

int getNumJoypads();
bool joypadDisconnected(SDL_JoystickID id);

JoypadConfig *joypadConfig(int index);
void resetJoypadConfig(int index);
const char *joypadName(int index);
SDL_JoystickGUID joypadGuid(int index);
SDL_JoystickID joypadId(int index);
const char *joypadPowerLevel(int index);

int joypadNumHats(int index);
int joypadNumButtons(int index);
int joypadNumAxes(int index);
int joypadNumBalls(int index);

JoypadConfig::HatBindingList *joypadHatBindings(int joypadIndex, int hatIndex);
JoypadConfig::AxisIntervalList *joypadAxisIntervals(int joypadIndex, int axisIndex);
JoypadConfig::BallBinding& joypadBall(int joypadIndex, int ballIndex);

bool tryReopeningJoypad(int index);

void addNewJoypad(int index);
void removeJoypad(SDL_JoystickID id);

int getJoypadWithButtonDown();
void waitForJoypadButtonsIdle();

bool getAutoConnectJoypads();
void setAutoConnectJoypads(bool value);
bool getEnableMenuControllers();
void setEnableMenuControllers(bool value);
bool getShowSelectMatchControlsMenu();
void setShowSelectMatchControlsMenu(bool value);

void loadJoypadOptions(const char *controlsSection, const CSimpleIni& ini);
void saveJoypadOptions(const char *controlsSection, CSimpleIni& ini);
