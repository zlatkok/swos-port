#include "render.h"
#include "util.h"
#include "file.h"
#include "controls.h"
#include "dump.h"
#include <SDL_image.h>

static SDL_Window *m_window;
static SDL_Renderer *m_renderer;
static SDL_Texture *m_texture;
static SDL_PixelFormat *m_pixelFormat;

constexpr int kNumColors = 256;
constexpr int kPaletteSize = 3 * kNumColors;
static uint8_t m_palette[kPaletteSize];

static Uint64 m_lastRenderStartTime;
static Uint64 m_lastRenderEndTime;

static bool m_delay;
static std::deque<int> m_lastFramesDelay;
static constexpr int kMaxLastFrames = 16;

static auto m_windowMode = kModeWindow;

constexpr int kDefaultFullScreenWidth = 640;
constexpr int kDefaultFullScreenHeight = 480;

static int m_windowWidth = kWindowWidth;
static int m_windowHeight = kWindowHeight;
static int m_fieldWidth = kGameScreenWidth;
static int m_fieldHeight = kVgaHeight;

// full-screen dimensions
static int m_displayWidth = kDefaultFullScreenWidth;
static int m_displayHeight = kDefaultFullScreenHeight;
static bool m_windowResizable = true;

// video options
const char kVideoSection[] = "video";
const char kWindowMode[] = "windowMode";
const char kWindowWidthKey[] = "windowWidth";
const char kWindowHeightKey[] = "windowHeight";
const char kFieldWidthKey[] = "fieldWidth";
const char kFieldHeightKey[] = "fieldHeight";
const char kWindowResizable[] = "windowResizable";
const char kFullScreenWidth[] = "fullScreenWidth";
const char kFullScreenHeight[] = "fullScreenHeight";

std::pair<int, int> getWindowSize()
{
    assert(m_window);

   int width = -1, height = -1;

    if (m_windowMode == kModeWindow || m_windowMode == kModeBorderlessMaximized) {
        SDL_GetWindowSize(m_window, &width, &height);

        if (width > 0 && height > 0) {
            if (m_windowMode == kModeWindow) {
                m_windowWidth = width;
                m_windowHeight = height;
            }

            return { width, height };
        }
    }

    return { m_windowWidth, m_windowHeight };
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

static void normalizeWindowSize(int& width, int& height)
{
    if (width <= 0 || height <= 0) {
        logInfo("Got invalid width/height (%dx%d), setting to default", width, height);
        width = kWindowWidth;
        height = kWindowHeight;
    }
}

void setWindowSize(int width, int height)
{
    normalizeWindowSize(width, height);

    assert(m_window);

    clampWindowSize(width, height);

    logInfo("Setting new window size: %dx%d", width, height);

    SDL_SetWindowSize(m_window, width, height);
    SDL_GetWindowSize(m_window, &m_windowWidth, &m_windowHeight);

    m_windowWidth = width;
    m_windowHeight = height;
}

bool getWindowResizable()
{
    assert(m_window);
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_RESIZABLE) != 0;
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

std::pair<int, int> getVisibleFieldSize()
{
    return { m_fieldWidth, m_fieldHeight };
}

int getVisibleFieldWidth()
{
    return m_fieldWidth;
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
    int errorCode = 0;
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

SDL_Rect getViewport()
{
    SDL_Rect rect;
    SDL_RenderGetViewport(m_renderer, &rect);

    return rect;
}

void loadVideoOptions(const CSimpleIniA& ini)
{
    m_windowMode = static_cast<WindowMode>(ini.GetLongValue(kVideoSection, kWindowMode, -1));

    if (m_windowMode < 0 || m_windowMode >= kNumWindowModes)
        m_windowMode = kModeWindow;

    m_windowWidth = ini.GetLongValue(kVideoSection, kWindowWidthKey, kWindowWidth);
    m_windowHeight = ini.GetLongValue(kVideoSection, kWindowHeightKey, kWindowHeight);

    m_fieldWidth = ini.GetLongValue(kVideoSection, kFieldWidthKey, kGameScreenWidth);
    m_fieldHeight = ini.GetLongValue(kVideoSection, kFieldHeightKey, kVgaHeight);

    normalizeWindowSize(m_windowWidth, m_windowHeight);

    m_displayWidth = ini.GetLongValue(kVideoSection, kFullScreenWidth);
    m_displayHeight = ini.GetLongValue(kVideoSection, kFullScreenHeight);

    if (!m_displayWidth)
        m_displayWidth = kDefaultFullScreenWidth;
    if (!m_displayHeight)
        m_displayHeight = kDefaultFullScreenHeight;

    m_windowResizable = ini.GetLongValue(kVideoSection, kWindowResizable, 1) != 0;
}

void saveVideoOptions(CSimpleIniA& ini)
{
    ini.SetLongValue(kVideoSection, kWindowMode, static_cast<long>(m_windowMode));

    int windowWidth, windowHeight;
    std::tie(windowWidth, windowHeight) = getWindowSize();

    ini.SetLongValue(kVideoSection, kWindowWidthKey, windowWidth);
    ini.SetLongValue(kVideoSection, kWindowHeightKey, windowHeight);

    ini.SetLongValue(kVideoSection, kFullScreenWidth, m_displayWidth);
    ini.SetLongValue(kVideoSection, kFullScreenHeight, m_displayHeight);

    ini.SetLongValue(kVideoSection, kWindowResizable, m_windowResizable);
}

void initRendering()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        sdlErrorExit("Could not initialize SDL");

    int flags = SDL_WINDOW_SHOWN;
    if (m_windowResizable)
        flags |= SDL_WINDOW_RESIZABLE;

    logInfo("Creating %dx%d window, flags: %d", m_windowWidth, m_windowHeight, flags);
    m_window = SDL_CreateWindow("SWOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_windowWidth, m_windowHeight, flags);
    if (!m_window)
        sdlErrorExit("Could not create window");

    setWindowMode(m_windowMode);

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer)
        sdlErrorExit("Could not create renderer");

    auto format = SDL_PIXELFORMAT_RGBA8888;
    m_texture = SDL_CreateTexture(m_renderer, format, SDL_TEXTUREACCESS_STREAMING, kVgaWidth, kVgaHeight);
    if (!m_texture)
        sdlErrorExit("Could not create surface");

    m_pixelFormat = SDL_AllocFormat(format);
    if (!m_pixelFormat)
        sdlErrorExit("Could not allocate pixel format");

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(m_renderer, kVgaWidth, kVgaHeight);
}

void finishRendering()
{
    if (m_pixelFormat)
        SDL_FreeFormat(m_pixelFormat);

    if (m_texture)
        SDL_DestroyTexture(m_texture);

    if (m_renderer)
        SDL_DestroyRenderer(m_renderer);

    if (m_window)
        SDL_DestroyWindow(m_window);

    SDL_Quit();
}

void setPalette(const char *palette, int numColors /* = kVgaPaletteNumColors */)
{
    assert(numColors >= 0 && numColors <= kVgaPaletteNumColors);

    for (int i = 0; i < numColors * 3; i++) {
        assert(palette[i] >= 0 && palette[i] < 64);
        m_palette[i] = palette[i] * 255 / 63;
    }
}

void getPalette(char *palette)
{
    for (int i = 0; i < kPaletteSize; i++)
        palette[i] = m_palette[i] * 63 / 255;
}

static void determineIfDelayNeeded()
{
    auto delay = m_lastRenderEndTime > m_lastRenderStartTime && m_lastRenderEndTime - m_lastRenderStartTime >= 10;

    if (m_lastFramesDelay.size() >= kMaxLastFrames)
        m_lastFramesDelay.pop_front();

    m_lastFramesDelay.push_back(delay);

    size_t sum = std::accumulate(m_lastFramesDelay.begin(), m_lastFramesDelay.end(), 0);
    m_delay = 2 * sum >= m_lastFramesDelay.size();
}

template <typename F>
static void requestPixelAccess(F drawFunction)
{
    Uint32 *pixels;
    int pitch;

    if (SDL_LockTexture(m_texture, nullptr, (void **)&pixels, &pitch)) {
        logWarn("Failed to lock drawing texture");
        return;
    }

    drawFunction(pixels, pitch);

    SDL_UnlockTexture(m_texture);
}

void clearScreen()
{
    requestPixelAccess([](Uint32 *pixels, int pitch) {
        auto color = &m_palette[0];
        auto rgbaColor = SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);

        for (int y = 0; y < kVgaHeight; y++) {
            for (int x = 0; x < kVgaWidth; x++) {
                pixels[y * pitch / sizeof(Uint32) + x] = rgbaColor;
            }
        }
    });
}

void skipFrameUpdate()
{
    m_lastRenderStartTime = m_lastRenderEndTime = SDL_GetPerformanceCounter();
}

void updateScreen(const char *inData /* = nullptr */, int offsetLine /* = 0 */, int numLines /* = kVgaHeight */)
{
    assert(offsetLine >= 0 && offsetLine <= kVgaHeight);
    assert(numLines >= 0 && numLines <= kVgaHeight);
    assert(offsetLine + numLines <= kVgaHeight);

    m_lastRenderStartTime = SDL_GetPerformanceCounter();

    auto data = reinterpret_cast<const uint8_t *>(inData ? inData : (vsPtr ? vsPtr : linAdr384k));

#ifdef DEBUG
    if (screenWidth == kGameScreenWidth)
        dumpVariables();
#endif

    requestPixelAccess([&](Uint32 *pixels, int pitch) {
        for (int y = offsetLine; y < numLines; y++) {
            for (int x = 0; x < kVgaWidth; x++) {
                auto color = &m_palette[data[y * screenWidth + x] * 3];
                pixels[y * pitch / sizeof(Uint32) + x] = SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);
            }
        }
    });

    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);

    m_lastRenderEndTime = SDL_GetPerformanceCounter();

    determineIfDelayNeeded();
}

void frameDelay(double factor /* = 1.0 */)
{
    timerProc();

    if (!m_delay) {
        SDL_Delay(3);
        return;
    }

    constexpr double kTargetFps = 70;

    if (factor > 1.0) {
        // don't use busy wait in menus
        auto delay = std::lround(1'000 * factor / kTargetFps);
        SDL_Delay(delay);
        return;
    }

    static const Sint64 kFrequency = SDL_GetPerformanceFrequency();

    Uint64 delay = std::llround(kFrequency * factor / kTargetFps);

    auto startTicks = SDL_GetPerformanceCounter();
    auto diff = startTicks - m_lastRenderStartTime;

    if (diff < delay) {
        constexpr int kNumFramesForSlackValue = 64;
        static std::array<Uint64, kNumFramesForSlackValue> slackValues;
        static int slackValueIndex;

        auto slackValue = std::accumulate(std::begin(slackValues), std::end(slackValues), 0LL);
        slackValue = (slackValue + (slackValue > 0 ? kNumFramesForSlackValue : -kNumFramesForSlackValue) / 2) / kNumFramesForSlackValue;

        if (static_cast<Sint64>(delay - diff) > slackValue) {
            auto intendedDelay = 1'000 * (delay - diff - slackValue) / kFrequency;
            auto delayStart = SDL_GetPerformanceCounter();
            SDL_Delay(static_cast<Uint32>(intendedDelay));

            auto actualDelay = SDL_GetPerformanceCounter() - delayStart;
            slackValues[slackValueIndex] = actualDelay - intendedDelay * kFrequency / 1'000;
            slackValueIndex = (slackValueIndex + 1) % kNumFramesForSlackValue;
        }

        do {
            startTicks = SDL_GetPerformanceCounter();
        } while (m_lastRenderStartTime + delay > startTicks);
    }
}

// simulate SWOS procedure executed at each interrupt 8 tick
void timerProc()
{
    currentTick++;
    menuCycleTimer++;
    if (!paused) {
        stoppageTimer++;
        timerBoolean = (timerBoolean + 1) & 1;
        if (!timerBoolean)
            bumpBigSFrame = -1;
    }
}

void fadeIfNeeded()
{
    if (!skipFade) {
        FadeIn();
        skipFade = -1;
    }
}

std::string ensureScreenshotsDirectory()
{
    auto path = pathInRootDir("screenshots");
    createDir(path.c_str());
    return path;
}

void makeScreenshot()
{
    char filename[256];

    auto t = time(nullptr);
    auto len = strftime(filename, sizeof(filename), "screenshot-%Y-%m-%d-%H-%M-%S-", localtime(&t));

    auto ms = SDL_GetTicks() % 1'000;
    sprintf(filename + len, "%03d.png", ms);

    const auto& screenshotsPath = ensureScreenshotsDirectory();
    const auto& path = joinPaths(screenshotsPath.c_str(), filename);

    requestPixelAccess([&](Uint32 *pixels, int pitch) {
        Uint32 rMask, gMask, bMask, aMask;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        rMask = 0xff000000;
        gMask = 0x00ff0000;
        bMask = 0x0000ff00;
        aMask = 0x000000ff;
#else
        rMask = 0x000000ff;
        gMask = 0x0000ff00;
        bMask = 0x00ff0000;
        aMask = 0xff000000;
#endif

        auto surface = SDL_CreateRGBSurfaceFrom(pixels, kVgaWidth, kVgaHeight, 32, pitch, rMask, gMask, bMask, aMask);
        bool surfaceLocked = surface ? SDL_LockSurface(surface) >= 0 : false;

        if (surface && surfaceLocked && IMG_SavePNG(surface, path.c_str()) >= 0)
            logInfo("Created screenshot %s", filename);
        else
            logWarn("Failed to save screenshot %s", filename);

        if (surfaceLocked)
            SDL_UnlockSurface(surface);

        SDL_FreeSurface(surface);
    });
}

void SWOS::Flip()
{
    updateControls();
    frameDelay();
    updateScreen();
}
