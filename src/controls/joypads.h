#pragma once

#include "controls.h"

constexpr int kDefaultDeadZoneValue = 8'000;

struct JoypadDeadZone {
    bool left(int x) const;
    bool right(int x) const;
    bool up(int y) const;
    bool down(int y) const;

    bool operator==(const JoypadDeadZone& other) const;

    int xPos = kDefaultDeadZoneValue;
    int xNeg = -kDefaultDeadZoneValue;
    int yPos = kDefaultDeadZoneValue;
    int yNeg = -kDefaultDeadZoneValue;
};

struct JoypadConfig {
    JoypadConfig() = default;
    JoypadConfig(const SDL_JoystickGUID& guid): guid(guid) {}

    bool operator==(const JoypadConfig& other) const;
    bool isDefault() const;

    SDL_JoystickGUID guid{};
    int fireButtonIndex = 0;
    int benchButtonIndex = 1;
    JoypadDeadZone deadZone;
};

constexpr int kInvalidJoypadId = -1;

struct JoypadInfo {
    JoypadInfo(SDL_Joystick *handle, SDL_JoystickID id, JoypadConfig config);
    bool tryReopening(int index);
    void buttonStateChanged(int index, bool pressed);
    bool anyButtonDown() const;

    SDL_Joystick *handle = nullptr;
    SDL_JoystickID id = kInvalidJoypadId;
    JoypadConfig config;
    std::vector<std::pair<int, bool>> buttons;
};

struct JoypadValues {
    bool left() const;
    bool right() const;
    bool up() const;
    bool down() const;

    int x;
    int y;
    bool fire;
    bool bench;
    JoypadDeadZone deadZone;
};

void initJoypads();

int getPl1JoypadIndex();
int getPl2JoypadIndex();
int getPl1GameJoypadIndex();
int getPl2GameJoypadIndex();

JoypadInfo& getPl1Joypad();
JoypadInfo& getPl2Joypad();

JoypadInfo& getJoypad(int index);
int getJoypadIndexFromId(SDL_JoystickID id);

bool pl1UsingJoypad();
bool pl2UsingJoypad();

void setPl1JoypadIndex(int index);
void setPl2JoypadIndex(int index);
void setPl1GameJoypadIndex(int index);
void setPl2GameJoypadIndex(int index);

int getNumJoypads();

JoypadValues& getJoypad1Values();
JoypadValues& getJoypad2Values();

void resetMatchJoypads();

void addJoypad(int index);
void removeJoypad(SDL_JoystickID id);
void updateJoypadButtonState(SDL_JoystickID id, int buttonIndex, bool pressed);
void updateJoypadMotion(SDL_JoystickID id, Uint8 axis, Sint16 value);
void propagateJoypadConfig(int index);

int getJoypadWithButtonDown();
std::vector<int> getJoypadButtonsDown(int index);

void clearJoypadInput();
void transferJoypad(PlayerNumber player, int& joypadIndex, int& otherJoypadIndex);
bool selectJoypadControls(PlayerNumber player, int joypadNo);

bool getAutoConnectJoypads();
void setAutoConnectJoypads(bool value);

void loadJoypadOptions(const char *controlsSection, const CSimpleIni& ini);
void saveJoypadOptions(const char *controlsSection, CSimpleIni& ini);
void normalizeJoypadConfig();
