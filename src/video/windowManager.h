#pragma once

#include "render.h"

constexpr int kWindowWidth = 4 * kVgaWidth;
constexpr int kWindowHeight = 4 * kVgaHeight;

enum WindowMode
{
    kModeFullScreen, kModeWindow, kModeBorderlessMaximized, kNumWindowModes,
};

enum class AssetResolution {
    k4k, kHD, kLowRes, kNumResolutions, kInvalid = kNumResolutions,
};

constexpr auto kNumAssetResolutions = static_cast<size_t>(AssetResolution::kNumResolutions);

using AssetResolutionChangeHandler = std::function<void(AssetResolution, AssetResolution)>;

SDL_Window *createWindow();
void destroyWindow();
std::pair<int, int> getWindowSize();
AssetResolution getAssetResolution();
void registerAssetResolutionChangeHandler(AssetResolutionChangeHandler handler);
const char *getAssetDir();
void setWindowSize(int width, int height);
bool getWindowResizable();
WindowMode getWindowMode();
int getWindowDisplayIndex();
bool setFullScreenResolution(int width, int height);
bool isInFullScreenMode();
std::pair<int, int> getFullScreenDimensions();
std::pair<int, int> getVisibleFieldSize();
int getVisibleFieldWidth();

void switchToWindow();
void switchToBorderlessMaximized();

void toggleBorderlessMaximizedMode();
void toggleFullScreenMode();
void toggleWindowResizable();
void centerWindow();

bool hasMouseFocus();
bool mapCoordinatesToGameArea(int& x, int& y);
float getXScale();
float getYScale();
SDL_Rect mapRect(int x, int y, int width, int height);

#ifdef VIRTUAL_JOYPAD
bool getShowTouchTrails();
void setShowTouchTrails(bool showTouchTrails);
bool getTransparentVirtualJoypadButtons();
void setTransparentVirtualJoypadButtons(bool transparentButtons);
#endif
bool cursorFlashingEnabled();
void setFlashMenuCursor(bool flashMenuCursor);

void loadVideoOptions(const CSimpleIni& ini);
void saveVideoOptions(CSimpleIni& ini);
