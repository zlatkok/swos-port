#include "windowManager.h"
#include "assetManager.h"
#include "gameFieldMapping.h"
#include "render.h"
#include "util.h"

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;

constexpr int kDefaultFullScreenWidth = 640;
constexpr int kDefaultFullScreenHeight = 480;

constexpr int kMinimumWindowWidth= 640;
constexpr int kMinimumWindowHeight = 400;

static SDL_Window *m_window;
static auto m_windowMode =
#ifdef __ANDROID__
    kModeFullScreen;
#else
    kModeWindow;
#endif

static bool m_maximized;

static int m_windowWidth;
static int m_windowHeight;
static int m_nonMaximizedWidth;
static int m_nonMaximizedHeight;

// full-screen dimensions
static int m_displayWidth;
static int m_displayHeight;
static bool m_windowResizable = true;

static const char kWindowSection[] = "window";
#ifndef __ANDROID__
static const char kWindowModeKey[] = "windowMode";
static const char kWindowMaximizedKey[] = "windowMaximized";
static const char kWindowResizableKey[] = "windowResizable";
#endif
static const char kWindowWidthKey[] = "windowWidth";
static const char kWindowHeightKey[] = "windowHeight";
static const char kFullScreenWidthKey[] = "fullScreenWidth";
static const char kFullScreenHeightKey[] = "fullScreenHeight";

static int handleSizeChanged(void *, SDL_Event *event);
static void updateWindowDimensions(int width, int height, bool init = false);
static void normalizeWindowSize(int& width, int& height, int defaultWidth, int defaultHeight);
static void setWindowMode(WindowMode newMode);

SDL_Window *createWindow()
{
    int flags = SDL_WINDOW_SHOWN;
    if (m_windowResizable)
        flags |= SDL_WINDOW_RESIZABLE;

    logInfo("Creating %dx%d window, flags: %d", m_windowWidth, m_windowHeight, flags);
    m_window = SDL_CreateWindow("SWOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_windowWidth, m_windowHeight, flags);
    if (!m_window)
        sdlErrorExit("Could not create window");

    setWindowMode(m_windowMode);

    SDL_AddEventWatch(handleSizeChanged, nullptr);

    return m_window;
}

void destroyWindow()
{
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void initWindow()
{
    if (m_maximized)
        SDL_MaximizeWindow(m_window);
    else
        updateLogicalScreenSize();
}

void deinitWindow()
{
    SDL_DelEventWatch(handleSizeChanged, nullptr);
}

SDL_Window *getWindow()
{
    return m_window;
}

static int handleSizeChanged(void *, SDL_Event *event)
{
    if (event->type == SDL_WINDOWEVENT) {
        switch (event->window.event) {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            updateWindowDimensions(event->window.data1, event->window.data2);
            break;
        case SDL_WINDOWEVENT_MAXIMIZED:
            m_maximized = true;
            break;
        case SDL_WINDOWEVENT_RESTORED:
            m_maximized = false;
            m_nonMaximizedWidth = m_windowWidth;
            m_nonMaximizedHeight = m_windowHeight;
            break;
        }
    }

    return 0;
}

static void updateWindowDimensions(int width, int height, bool init /* = false */)
{
    if (width != m_windowWidth || height != m_windowHeight || init) {
        m_windowWidth = width;
        m_windowHeight = height;
        updateAssetResolution(width, height);
        updateGameScaleFactor(width, height);
        if (!init)
            updateLogicalScreenSize();
    }
}

static void clampWindowSize(int& width, int& height)
{
    assert(m_window);

    auto displayIndex = SDL_GetWindowDisplayIndex(m_window);
    if (displayIndex >= 0) {
        SDL_Rect rect;
        if (SDL_GetDisplayBounds(displayIndex, &rect) == 0) {
            width = std::min(width, rect.w);
            height = std::min(height, rect.h);
            logInfo("Clamping window size to %dx%d", width, height);
        }
    }
}

static void normalizeWindowSize(int& width, int& height, int defaultWidth, int defaultHeight)
{
    if (width < kMinimumWindowWidth || height <= kMinimumWindowHeight) {
        logInfo("Got invalid width/height (%dx%d), setting to default (%dx%d)", width, height, defaultWidth, defaultHeight);
        width = defaultWidth;
        height = defaultHeight;
    }
}

std::pair<int, int> getWindowSize()
{
    assert(m_window);

    if (m_window && (m_windowMode == kModeWindow || m_windowMode == kModeBorderlessMaximized)) {
        int width = -1, height = -1;
        SDL_GetWindowSize(m_window, &width, &height);

        if (width > 0 && height > 0) {
            if (m_windowMode == kModeWindow)
                updateWindowDimensions(width, height);

            return { width, height };
        }
    }

    return { m_windowWidth, m_windowHeight };
}

std::pair<int, int> getWindowRestoredSize()
{
    int width, height;

    if (m_windowMode == kModeWindow && m_maximized) {
        width = m_nonMaximizedWidth;
        height = m_nonMaximizedHeight;
    } else {
        std::tie(width, height) = getWindowSize();
    }

    return { width, height };
}

void initWindowSize(int width, int height)
{
    m_windowWidth = width;
    m_windowHeight = height;
}

void setWindowSize(int width, int height)
{
    normalizeWindowSize(width, height, kWindowWidth, kWindowHeight);

    assert(m_window);

    clampWindowSize(width, height);

    logInfo("Setting new window size: %dx%d", width, height);

    SDL_SetWindowSize(m_window, width, height);
    SDL_GetWindowSize(m_window, &m_windowWidth, &m_windowHeight);

    updateWindowDimensions(width, height);
}

void initWindowResizable(bool resizable)
{
    m_windowResizable = resizable;
}

bool getWindowResizable()
{
    return m_window ? (SDL_GetWindowFlags(m_window) & SDL_WINDOW_RESIZABLE) != 0 : m_windowResizable;
}

bool getWindowMaximized()
{
    return m_window ? (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0 : m_maximized;
}

WindowMode getWindowMode()
{
    return m_windowMode;
}

int getWindowDisplayIndex()
{
    assert(m_window);
    return SDL_GetWindowDisplayIndex(m_window);
}

bool isInFullScreenMode()
{
    return m_windowMode == kModeFullScreen;
}

std::pair<int, int> getFullScreenDimensions()
{
    return { m_displayWidth, m_displayHeight };
}

static bool setDisplayMode(int width, int height)
{
    assert(m_window);

    logInfo("Trying to switch to %dx%d", width, height);

    SDL_DisplayMode mode;
    mode.w = width;
    mode.h = height;
    mode.refresh_rate = 0;
    mode.driverdata = nullptr;
    mode.format = SDL_PIXELFORMAT_RGB888;

    int err = SDL_SetWindowDisplayMode(m_window, &mode);
    if (err) {
        logWarn("SDL_SetWindowDisplayMode() failed with code: %d, reason: %s", err, SDL_GetError());
        return false;
    }

    if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN)
        SDL_SetWindowFullscreen(m_window, 0);

    err = SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN);

    if (!err) {
        if (SDL_GetWindowDisplayMode(m_window, &mode) == 0 && (mode.w != width || mode.h != height))
            logWarn("Couldn't switch to %dx%d, switched to %dx%d instead", width, height, mode.w, mode.h);

        m_displayWidth = mode.w;
        m_displayHeight = mode.h;
    } else {
        logWarn("SDL_SetWindowFullscreen() failed with code: %d, reason: %s", err, SDL_GetError());
    }

    return err == 0;
}

static void setWindowMode(WindowMode newMode)
{
    bool success = true;
    auto mode = "standard windowed";

    assert(m_window);

    if (m_windowMode != newMode)
        logInfo("Window mode change requested, from %d to %d", m_windowMode, newMode);

    switch (newMode) {
    case kModeWindow:
        success = SDL_SetWindowFullscreen(m_window, 0) == 0;
        if (success)
            SDL_SetWindowSize(m_window, m_windowWidth, m_windowHeight);
        break;

    case kModeBorderlessMaximized:
        success = SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0;
        mode = "borderless maximized";
        break;

    case kModeFullScreen:
        success = setDisplayMode(m_displayWidth, m_displayHeight);
        mode = "full screen";
        break;

    default:
        assert(false);
    }

    m_windowMode = newMode;

    if (success) {
        logInfo("Successfully switched into %s mode", mode);
    } else {
        logWarn("Failed to change window mode to %s", mode);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Resolution switch failure", "Failed to change window mode", m_window);

        int flags = SDL_GetWindowFlags(m_window);
        m_windowMode = kModeWindow;

        if (flags & SDL_WINDOW_FULLSCREEN)
            m_windowMode = kModeFullScreen;
        else if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
            m_windowMode = kModeBorderlessMaximized;
    }
}

void switchToWindow()
{
    if (m_windowMode != kModeWindow)
        setWindowMode(kModeWindow);
}

void switchToBorderlessMaximized()
{
    if (m_windowMode != kModeBorderlessMaximized)
        setWindowMode(kModeBorderlessMaximized);
}

void setWindowResizable(bool resizable)
{
    assert(m_window);

    SDL_SetWindowResizable(m_window, resizable ? SDL_TRUE : SDL_FALSE);
    m_windowResizable = resizable;
}

void initFullScreenResolution(int width, int height)
{
    m_displayWidth = width;
    m_displayHeight = height;
}

bool setFullScreenResolution(int width, int height)
{
    bool result = setDisplayMode(width, height);
    if (result)
        m_windowMode = kModeFullScreen;

    return result;
}

void toggleBorderlessMaximizedMode()
{
    if (m_windowMode == kModeFullScreen || m_windowMode == kModeBorderlessMaximized)
        setWindowMode(kModeWindow);
    else
        setWindowMode(kModeBorderlessMaximized);
}

static std::pair<int, int> getDesktopResolution()
{
    auto displayIndex = SDL_GetWindowDisplayIndex(m_window);
    if (displayIndex >= 0) {
        SDL_DisplayMode mode;
        if (!SDL_GetDesktopDisplayMode(displayIndex, &mode))
            return { mode.w, mode.h };
    }

    return { kDefaultFullScreenWidth, kDefaultFullScreenHeight };
}

void toggleFullScreenMode()
{
    if (!m_displayWidth || !m_displayHeight)
        std::tie(m_displayWidth, m_displayHeight) = getDesktopResolution();

    if (m_windowMode == kModeFullScreen || m_windowMode == kModeBorderlessMaximized)
        setWindowMode(kModeWindow);
    else
        setWindowMode(kModeFullScreen);
}

void toggleWindowResizable()
{
    if (m_windowMode != kModeWindow)
        setWindowMode(kModeWindow);

    setWindowResizable(!m_windowResizable);
}

void toggleWindowMaximized()
{
    assert(m_window);

    if (getWindowMaximized())
        SDL_RestoreWindow(m_window);
    else
        SDL_MaximizeWindow(m_window);
}

void centerWindow()
{
    assert(m_window);
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

bool hasMouseFocus()
{
    assert(m_window);
    return SDL_GetMouseFocus() == m_window;
}

void loadWindowOptions(const CSimpleIni& ini)
{
#ifdef __ANDROID__
    m_windowMode = kModeFullScreen;
#else
    m_windowMode = static_cast<WindowMode>(ini.GetLongValue(kWindowSection, kWindowModeKey, kModeWindow));

    if (m_windowMode >= kNumWindowModes)
        m_windowMode = kModeWindow;

    m_maximized = ini.GetBoolValue(kWindowSection, kWindowMaximizedKey);
    m_windowResizable = ini.GetBoolValue(kWindowSection, kWindowResizableKey, true);
#endif

    int width = ini.GetLongValue(kWindowSection, kWindowWidthKey, kWindowWidth);
    int height = ini.GetLongValue(kWindowSection, kWindowHeightKey, kWindowHeight);

    normalizeWindowSize(width, height, kWindowWidth, kWindowHeight);
    updateWindowDimensions(width, height, true);

    m_nonMaximizedWidth = m_windowWidth;
    m_nonMaximizedHeight = m_windowHeight;

    m_displayWidth = ini.GetLongValue(kWindowSection, kFullScreenWidthKey);
    m_displayHeight = ini.GetLongValue(kWindowSection, kFullScreenHeightKey);

    normalizeWindowSize(m_displayWidth, m_displayHeight, kDefaultFullScreenWidth, kDefaultFullScreenHeight);
}

void saveWindowOptions(CSimpleIni& ini)
{
#ifndef __ANDROID__
    ini.SetLongValue(kWindowSection, kWindowModeKey, static_cast<long>(m_windowMode));
    ini.SetBoolValue(kWindowSection, kWindowMaximizedKey, m_maximized);
    ini.SetLongValue(kWindowSection, kWindowResizableKey, m_windowResizable);
#endif

    int windowWidth, windowHeight;

    if (m_windowMode == kModeWindow && m_maximized) {
        windowWidth = m_nonMaximizedWidth;
        windowHeight = m_nonMaximizedHeight;
    } else {
        std::tie(windowWidth, windowHeight) = getWindowSize();
    }

    ini.SetLongValue(kWindowSection, kWindowWidthKey, windowWidth);
    ini.SetLongValue(kWindowSection, kWindowHeightKey, windowHeight);

    ini.SetLongValue(kWindowSection, kFullScreenWidthKey, m_displayWidth);
    ini.SetLongValue(kWindowSection, kFullScreenHeightKey, m_displayHeight);
}
