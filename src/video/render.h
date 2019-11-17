#pragma once

extern int kVgaWidth;
extern int kVgaHeight;
extern int kVgaScreenSize;

extern int kVgaWidthAdder;
extern int kVgaHeightAdder;
extern int kVgaWidthAdderHalf;
extern int kVgaHeightAdderHalf;

constexpr int kVgaPaletteNumColors = 256;
constexpr int kVgaPaletteSize = kVgaPaletteNumColors * 3;

extern int kWindowWidth;
extern int kWindowHeight;

extern int PatternHorzNum_plus4;
extern int PatternVertNum_plus5;
extern int DrawPatternVerticalCount;

struct RGBStruct {
	uint8_t r, g, b;
};

extern uint8_t m_palette[256 * 3];
extern uint8_t AB_FlagTable[200 * 320];
extern uint8_t AB_FlagTable2[200 * 320];
extern char AB_OldVideoMemory[200 * 320];
extern RGBStruct AB_RGBTable[200 * 320];

enum WindowMode { kModeFullScreen, kModeWindow, kModeBorderlessMaximized, kNumWindowModes, };

std::pair<int, int> getWindowSize();
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
SDL_Rect getViewport();

void loadVideoOptions(const CSimpleIni& ini);
void saveVideoOptions(CSimpleIni& ini);

void initRendering();
void finishRendering();
void setPalette(const char *palette, int numColors = kVgaPaletteNumColors);
void getPalette(char *palette);
void clearScreen();
void skipFrameUpdate();
void updateScreen(const char *data = nullptr, int offsetLine = 0, int numLines = kVgaHeight);
void updateScreen_GameScreen(const char *data = nullptr, int offsetLine = 0, int numLines = kVgaHeight);
void updateScreen_MenuScreen(const char *data = nullptr, int offsetLine = 0, int numLines = kVgaHeight);
void frameDelay(double factor = 1.0);
void timerProc();
void fadeIfNeeded();
void fadeIfNeeded2();

void makeScreenshot();

void showVideoOptionsMenu();