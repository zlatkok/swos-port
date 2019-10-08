#include "render.h"
#include "mockRender.h"

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

static UpdateHook m_updateHook;

std::pair<int, int> getWindowSize()
{
    return { m_windowWidth, m_windowHeight };
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

void setUpdateHook(UpdateHook updateHook)
{
    m_updateHook = updateHook;
}

std::pair<int, int> getFullScreenDimensions()
{
    return { m_displayWidth, m_displayHeight };
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

SDL_Rect getViewport()
{
    return { 0, 0, m_windowWidth, m_windowHeight };
}

void updateScreen(const char *, int, int)
{
    if (m_updateHook)
        m_updateHook();
}

void loadVideoOptions(const CSimpleIniA&) {}
void saveVideoOptions(CSimpleIniA&) {}
void initRendering() {}
void finishRendering() {}
void setPalette(const char *, int) {}
void getPalette(char *) {}
void clearScreen() {}
void skipFrameUpdate() {}
void frameDelay(double) {}
void timerProc() {}

void SWOS::Flip() {}
