#pragma once

enum WindowMode
{
    kModeFullScreen, kModeWindow, kModeBorderlessMaximized, kNumWindowModes,
};

SDL_Window *createWindow();
void destroyWindow();
void initWindow();
void deinitWindow();
std::pair<int, int> getWindowSize();
std::pair<int, int> getWindowRestoredSize();
void initWindowSize(int width, int height);
void setWindowSize(int width, int height);
void initWindowResizable(bool resizable);
bool getWindowResizable();
bool getWindowMaximized();
WindowMode getWindowMode();
int getWindowDisplayIndex();
void initFullScreenResolution(int width, int height);
bool setFullScreenResolution(int width, int height);
bool isInFullScreenMode();
std::pair<int, int> getFullScreenDimensions();

void switchToWindow();
void switchToBorderlessMaximized();

void toggleBorderlessMaximizedMode();
void toggleFullScreenMode();
void toggleWindowResizable();
void toggleWindowMaximized();
void centerWindow();

bool hasMouseFocus();

void loadWindowOptions(const CSimpleIni& ini);
void saveWindowOptions(CSimpleIni& ini);
