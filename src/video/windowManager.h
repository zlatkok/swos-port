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
std::string getPathInAssetDir(const char *path);
void setWindowSize(int width, int height);
bool getWindowResizable();
WindowMode getWindowMode();
int getWindowDisplayIndex();
bool setFullScreenResolution(int width, int height);
bool isInFullScreenMode();
std::pair<int, int> getFullScreenDimensions();

void switchToWindow();
void switchToBorderlessMaximized();

void toggleBorderlessMaximizedMode();
void toggleFullScreenMode();
void toggleWindowResizable();
void centerWindow();

bool hasMouseFocus();
bool mapCoordinatesToGameArea(int& x, int& y);
float getScale();
float getScreenXOffset();
float getScreenYOffset();
SDL_FRect mapRect(int x, int y, int width, int height);

#ifdef VIRTUAL_JOYPAD
bool getShowTouchTrails();
void setShowTouchTrails(bool showTouchTrails);
bool getTransparentVirtualJoypadButtons();
void setTransparentVirtualJoypadButtons(bool transparentButtons);
#endif
bool cursorFlashingEnabled();
void setFlashMenuCursor(bool flashMenuCursor);
bool getShowFps();
void setShowFps(bool showFps);

void loadVideoOptions(const CSimpleIni& ini);
void saveVideoOptions(CSimpleIni& ini);
