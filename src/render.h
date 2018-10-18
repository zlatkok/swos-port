#pragma once

constexpr int kVgaWidth = 320;
constexpr int kVgaHeight = 200;
constexpr int kVgaScreenSize = kVgaWidth * kVgaHeight;

constexpr int kVgaPaletteNumColors = 256;
constexpr int kVgaPaletteSize = kVgaPaletteNumColors * 3;

constexpr int kGameScreenWidth = 384;

enum WindowMode { kModeFullScreen, kModeWindow, kModeBorderlessMaximized, kMaxWindowMode, };

WindowMode getWindowMode();
void setWindowMode(WindowMode newMode, bool apply = true);
std::pair<int, int> getWindowSize();
void setWindowSize(int width, int height, bool apply = true);
bool isWindowResizable();
void setWindowResizable(bool resizable, bool apply = true);
std::pair<int, int> getFullScreenResolution();
bool setFullScreenResolution(int width, int height, bool apply = true);
void toggleBorderlessMaximizedMode();

bool hasMouseFocus();

void initRendering();
void finishRendering();
void setPalette(const char *palette, int numColors = kVgaPaletteNumColors);
void getPalette(char *palette);
void clearScreen();
void skipFrameUpdate();
void updateScreen(const char *data = nullptr, int offsetLine = 0, int numLines = kVgaHeight);
void frameDelay(float factor = 1.0);

void showVideoOptionsMenu();
