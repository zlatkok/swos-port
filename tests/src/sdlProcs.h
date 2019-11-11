#pragma once

enum SdlApiIndex : size_t {
    SDL_LogWarnIndex = 5,
    SDL_GetErrorIndex = 76,
    SDL_PumpEventsIndex = 79,
    SDL_PeepEventsIndex = 80,
    SDL_PollEventIndex = 85,
    SDL_WaitEventIndex = 86,
    SDL_NumJoysticksIndex = 158,
    SDL_JoystickGetButtonIndex = 177,
    SDL_GetKeyboardStateIndex = 180,
    SDL_GetMouseStateIndex = 209,
    SDL_SetCursorIndex = 217,
    SDL_GetTicksIndex = 444,
    SDL_DelayIndex = 447,
    SDL_GetNumDisplayModesIndex = 465,
    SDL_GetDisplayModeIndex = 466,
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
void resetSdlInput();
void killSdlDelay();

void setFakeDisplayModes(const std::vector<SDL_DisplayMode>& displayModes);
void restoreRealDisplayModes();

void setSetTicksDelta(int delta);
void freezeSdlTime();
