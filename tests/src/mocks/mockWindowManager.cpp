#include "mockWindowManager.h"
#include "windowManager.h"

constexpr int kDefaultFullScreenWidth = 640;
constexpr int kDefaultFullScreenHeight = 480;

static int m_windowWidth = kWindowWidth;
static int m_windowHeight = kWindowHeight;
static int m_displayWidth = kDefaultFullScreenWidth;
static int m_displayHeight = kDefaultFullScreenHeight;
static bool m_windowResizable = false;

static bool m_isInFullScreen = false;
static bool m_failNextResolutionSwitch;
static int m_windowIndex = 0;
static auto m_windowMode = kModeWindow;

static bool m_flashMenuCursor;
static bool m_showTouchTrails;
static bool m_transparentButtons;

std::pair<int, int> getWindowSize()
{
    return { m_windowWidth, m_windowHeight };
}

AssetResolution getAssetResolution()
{
    return AssetResolution::kInvalid;
}

void registerAssetResolutionChangeHandler(AssetResolutionChangeHandler handler)
{
}

const char *getAssetDir()
{
    return nullptr;
}

std::string getPathInAssetDir(const char *)
{
    return {};
}

void setWindowSize(int width, int height)
{
    m_windowWidth = width;
    m_windowHeight = height;
}

bool getWindowResizable()
{
    return m_windowResizable;
}

void setWindowResizable(bool resizable)
{
    m_windowResizable = resizable;
}

WindowMode getWindowMode()
{
    return m_windowMode;
}

int getWindowDisplayIndex()
{
    return m_windowIndex;
}

void setWindowDisplayIndex(int windowIndex)
{
    m_windowIndex = windowIndex;
}

bool isInFullScreenMode()
{
    return m_isInFullScreen;
}

void setIsInFullScreenMode(bool isInFullScreen)
{
    m_isInFullScreen = isInFullScreen;
    if (isInFullScreen)
        m_windowMode = kModeFullScreen;
}

void failNextDisplayModeSwitch()
{
    m_failNextResolutionSwitch = true;
}

std::pair<int, int> getFullScreenDimensions()
{
    return { m_displayWidth, m_displayHeight };
}

std::pair<int, int> getVisibleFieldSize()
{
    return {};
}

void switchToWindow()
{
    m_windowMode = kModeWindow;
    m_isInFullScreen = false;
}

void switchToBorderlessMaximized()
{
    m_windowMode = kModeBorderlessMaximized;
    m_isInFullScreen = false;
}

bool setFullScreenResolution(int width, int height)
{
    if (m_failNextResolutionSwitch)
        return m_failNextResolutionSwitch = false;

    m_displayWidth = width;
    m_displayHeight = height;
    m_windowMode = kModeFullScreen;
    m_isInFullScreen = true;
    return true;
}

void toggleBorderlessMaximizedMode() {}
void toggleFullScreenMode() {}
void toggleWindowResizable() {}
void centerWindow() {}

bool hasMouseFocus()
{
    return true;
}

bool mapCoordinatesToGameArea(int &x, int &y)
{
    return false;
}

float getScale()
{
    return 1.0;
}

float getScreenXOffset() { return 0; }
float getScreenYOffset() { return 0; }

std::pair<int, int> mapPoint(int x, int y)
{
    return { x, y };
}

std::pair<float, float> mapPoint(float x, float y)
{
    return { x, y };
}

SDL_FRect mapRect(int x, int y, int width, int height)
{
    return {};
}

bool getShowTouchTrails()
{
    return m_showTouchTrails;
}

void setShowTouchTrails(bool showTouchTrails)
{
    m_showTouchTrails = showTouchTrails;
}

bool getTransparentVirtualJoypadButtons()
{
    return m_transparentButtons;
}

void setTransparentVirtualJoypadButtons(bool transparentButtons)
{
    transparentButtons = m_transparentButtons;
}

bool cursorFlashingEnabled()
{
    return m_flashMenuCursor;
}

void setFlashMenuCursor(bool flashMenuCursor)
{
    m_displayHeight = flashMenuCursor;
}

bool getShowFps() { return false; }
void setShowFps(bool showFps) {}

void loadVideoOptions(const CSimpleIniA&) {}
void saveVideoOptions(CSimpleIniA&) {}
