#include "mockWindowManager.h"
#include "windowManager.h"
#include "render.h"
#include "mockRender.h"
#include "gameFieldMapping.h"
#include <SDL_syswm.h>

static int m_windowWidth = kDefaultWindowWidth;
static int m_windowHeight = kDefaultWindowHeight;
static int m_displayWidth = kDefaultWindowWidth;
static int m_displayHeight = kDefaultWindowHeight;
static bool m_windowResizable = false;

static SDL_Window *m_window;
static bool m_isInFullScreen = false;
static bool m_failNextResolutionSwitch;
static int m_windowIndex = 0;
static auto m_windowMode = kModeWindow;

static void sendWindowToBack();

SDL_Window *createWindow()
{
    m_window = SDL_CreateWindow("test", 0, 0, m_windowWidth, m_windowHeight, 0);
    assert(m_window);

    sendWindowToBack();

    return m_window;
}

// If the window is hidden or minimized the renderer won't be rendering anything to our texture,
// so we do the next best thing and send the window away so it's not occluding anything.
void sendWindowToBack()
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(m_window, &wmInfo);
    ::SetWindowPos(wmInfo.info.win.window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
}

std::pair<int, int> getWindowSize()
{
    return { m_windowWidth, m_windowHeight };
}

void setWindowSize(int width, int height)
{
    m_windowWidth = width;
    m_windowHeight = height;
    SDL_SetWindowSize(m_window, m_windowWidth, m_windowHeight);
    updateGameScaleFactor(width, height);
    updateLogicalScreenSize();
    updateRenderTarget(width, height);
}

bool getWindowResizable()
{
    return m_windowResizable;
}

bool getWindowMaximized()
{
    return false;
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
void toggleWindowMaximized() {}
void centerWindow() {}

bool hasMouseFocus()
{
    return true;
}

bool getShowFps() { return false; }
void setShowFps(bool showFps) {}

void loadVideoOptions(const CSimpleIniA&) {}
void saveVideoOptions(CSimpleIniA&) {}
