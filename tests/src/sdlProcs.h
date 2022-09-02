#pragma once

enum SdlApiIndex : size_t {
    SDL_LogWarnIndex = 5,
    SDL_GetErrorIndex = 76,
    SDL_PumpEventsIndex = 79,
    SDL_PeepEventsIndex = 80,
    SDL_PollEventIndex = 85,
    SDL_WaitEventIndex = 86,
    SDL_NumJoysticksIndex = 158,
    SDL_JoystickNameForIndexIndex = 159,
    SDL_JoystickOpenIndex = 160,
    SDL_JoystickNameIndex = 161,
    SDL_JoystickGetGUIDIndex = 163,
    SDL_JoystickInstanceIDIndex = 167,
    SDL_JoystickNumAxesIndex = 168,
    SDL_JoystickNumBallsIndex = 169,
    SDL_JoystickNumHatsIndex = 170,
    SDL_JoystickNumButtonsIndex = 171,
    SDL_JoystickGetButtonIndex = 177,
    SDL_JoystickCloseIndex = 178,
    SDL_GetKeyboardStateIndex = 180,
    SDL_GetMouseStateIndex = 209,
    SDL_SetCursorIndex = 217,
    SDL_SetTextureColorModIndex = 272,
    SDL_GetTicksIndex = 444,
    SDL_DelayIndex = 447,
    SDL_GetNumDisplayModesIndex = 465,
    SDL_GetDisplayModeIndex = 466,
    SDL_RenderCopyFIndex = 664,
};

bool initSdlApiTable();
void *getSdlProc(SdlApiIndex index);
void *setSdlProc(SdlApiIndex index, void *hook);
void restoreSdlProc(SdlApiIndex index);
void restoreOriginalSdlFunctionTable();
void takeOverInput();

void queueSdlEvent(const SDL_Event& event);
void queueSdlMouseWheelEvent(int direction);
void queueSdlMouseMotionEvent(int x, int y);
void queueSdlMouseButtonEvent(bool mouseUp = false, int button = 1);
void queueSdlKeyDown(SDL_Scancode keyCode);
void queueSdlKeyUp(SDL_Scancode keyCode);

void setSdlMouseState(int x, int y, bool leftClick = false, bool rightClick = false);
void bumpMouse();
void resetSdlInput();
void killSdlDelay();

void disableRendering();
void enableRendering();

void setFakeDisplayModes(const std::vector<SDL_DisplayMode>& displayModes);
void restoreRealDisplayModes();

void clearTextureColorMods();
bool hadTextureColorMod(int r, int g, int b);

void setSetTicksDelta(int delta);
void freezeSdlTime();

struct FakeJoypad
{
    FakeJoypad(const char *name) : name(name) {
        memset(guid.data, 0, sizeof(guid.data));
        for (size_t i = 0; i < sizeof(guid.data) && name[i]; i++)
            guid.data[i] = name[i];
    }

    const char *name;
    intptr_t id;
    SDL_JoystickGUID guid;
};

using FakeJoypadList = std::vector<FakeJoypad>;

void setJoypads(const FakeJoypadList& joypads);
void connectJoypad(const FakeJoypad& joypad);
void disconnectJoypad(int index);
