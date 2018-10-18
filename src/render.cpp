#include "render.h"
#include "log.h"
#include "util.h"
#include "controls.h"
#include "videoOptions.mnu.h"

static SDL_Window *m_window;
static SDL_Renderer *m_renderer;
static SDL_Texture *m_texture;
static SDL_PixelFormat *m_pixelFormat;

constexpr int kNumColors = 256;
constexpr int kPaletteSize = 3 * kNumColors;
static uint8_t m_palette[kPaletteSize];

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 960;  // create a bit taller window to get black bars, easier for the eyes

static Uint32 m_lastRenderStartTime;
static Uint32 m_lastRenderEndTime;

static bool m_delay;
static std::deque<int> m_lastFramesDelay;
static constexpr int kMaxLastFrames = 16;

static WindowMode m_windowMode = kModeWindow;
static int m_windowWidth;
static int m_windowHeight;
static int m_displayWidth;
static int m_displayHeight;
static int16_t m_windowResizable = 1;

static bool setDisplayMode(int width, int height, bool menusRunning)
{
    assert(m_window);

    SDL_DisplayMode mode;
    mode.w = width;
    mode.h = height;
    mode.refresh_rate = 0;
    mode.driverdata = nullptr;
    mode.format = SDL_PIXELFORMAT_RGB888;

    return SDL_SetWindowDisplayMode(m_window, &mode) == 0 && SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN) == 0;
}

WindowMode getWindowMode()
{
    return m_windowMode;
}

void setWindowMode(WindowMode newMode, bool apply /* = true */)
{
    bool success = true;
    auto mode = "standard windowed";

    if (apply) {
        assert(m_window);

        logInfo("Window mode change requested, from %d to %d", m_windowMode, newMode);

        switch (newMode) {
            case kModeWindow:
                success = SDL_SetWindowFullscreen(m_window, 0) == 0;
                SDL_SetWindowSize(m_window, m_windowWidth, m_windowHeight);
                break;

            case kModeBorderlessMaximized:
                success = SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0;
                mode = "borderless maximized";
                break;

            case kModeFullScreen:
                logInfo("Trying to switch to %d x %d", m_displayWidth, m_displayHeight);
                success = SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN) == 0;
                success &= setDisplayMode(m_displayWidth, m_displayHeight, false);
                success ? logInfo("Full screen switch succeeded") : logWarn("Full screen switch failed");
                mode = "full screen";
                break;

            default:
                assert(false);
        }
    }

    m_windowMode = newMode;

    if (!success) {
        logWarn("Failed to change window mode to %s", mode);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Resolution switch failure", "Failed to change window mode", m_window);

        int flags = SDL_GetWindowFlags(m_window);
        m_windowMode = kModeWindow;

        if (flags & SDL_WINDOW_FULLSCREEN)
            m_windowMode = kModeFullScreen;
        else if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
            m_windowMode = kModeBorderlessMaximized;
    } else if (apply) {
        logInfo("Successfully switched into %s mode", mode);
    }
}

std::pair<int, int> getWindowSize()
{
    assert(m_window);

    if (m_windowMode == kModeWindow)
        SDL_GetWindowSize(m_window, &m_windowWidth, &m_windowHeight);

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
        }
    }
}

void setWindowSize(int width, int height, bool apply /* = true */)
{
    if (!width || !height) {
        width = kWindowWidth;
        height = kWindowHeight;
    }

    if (apply) {
        assert(m_window);

        clampWindowSize(width, height);
        SDL_SetWindowSize(m_window, width, height);
        SDL_GetWindowSize(m_window, &m_windowWidth, &m_windowHeight);
    } else {
        m_windowWidth = width;
        m_windowHeight = height;
    }
}

bool isWindowResizable()
{
    return m_windowResizable != 0;
}

void setWindowResizable(bool resizable, bool apply /* = true */)
{
    if (apply) {
        assert(m_window);
        SDL_SetWindowResizable(m_window, resizable ? SDL_TRUE : SDL_FALSE);
    }

    m_windowResizable = resizable;
}

std::pair<int, int> getFullScreenResolution()
{
    return { m_displayWidth, m_displayHeight };
}

bool setFullScreenResolution(int width, int height, bool apply /* = true */)
{
    bool result = true;

    m_displayWidth = width;
    m_displayHeight = height;

    if (apply) {
        result = setDisplayMode(width, height, apply);
        if (result)
            m_windowMode = kModeFullScreen;
    }

    return result;
}

void toggleBorderlessMaximizedMode()
{
    if (m_windowMode == kModeFullScreen || m_windowMode == kModeBorderlessMaximized)
        setWindowMode(kModeWindow);
    else
        setWindowMode(kModeBorderlessMaximized);
}

bool hasMouseFocus()
{
    assert(m_window);
    return SDL_GetMouseFocus() == m_window;
}

void initRendering()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        sdlErrorExit("Could not initialize SDL");

    int flags = SDL_WINDOW_SHOWN;
    if (m_windowResizable)
        flags |= SDL_WINDOW_RESIZABLE;

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
    auto delay = m_lastRenderEndTime < m_lastRenderStartTime || m_lastRenderEndTime - m_lastRenderStartTime < 12;

    if (m_lastFramesDelay.size() >= kMaxLastFrames)
        m_lastFramesDelay.pop_front();

    m_lastFramesDelay.push_back(delay);

    size_t sum = std::accumulate(m_lastFramesDelay.begin(), m_lastFramesDelay.end(), 0);
    m_delay = 2 * sum >= m_lastFramesDelay.size();
}

void clearScreen()
{
    Uint32 *pixels;
    int pitch;

    if (SDL_LockTexture(m_texture, nullptr, (void **)&pixels, &pitch)) {
        logWarn("Failed to lock drawing texture");
        return;
    }

    auto color = &m_palette[0];
    auto rgbaColor = SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);

    for (int y = 0; y < kVgaHeight; y++) {
        for (int x = 0; x < kVgaWidth; x++) {
            pixels[y * pitch / sizeof(Uint32) + x] = rgbaColor;
        }
    }

    SDL_UnlockTexture(m_texture);
}

void skipFrameUpdate()
{
    m_lastRenderStartTime = m_lastRenderEndTime = SDL_GetTicks();
}

void updateScreen(const char *inData /* = nullptr */, int offsetLine /* = 0 */, int numLines /* = kVgaHeight */)
{
    assert(offsetLine >= 0 && offsetLine <= kVgaHeight);
    assert(numLines >= 0 && numLines <= kVgaHeight);
    assert(offsetLine + numLines <= kVgaHeight);

    m_lastRenderStartTime = SDL_GetTicks();

    Uint32 *pixels;
    int pitch;

    auto data = reinterpret_cast<const uint8_t *>(inData ? inData : (vsPtr ? vsPtr : linAdr384k));

    if (SDL_LockTexture(m_texture, nullptr, (void **)&pixels, &pitch)) {
        logWarn("Failed to lock drawing texture");
        return;
    }

    for (int y = offsetLine; y < numLines; y++) {
        for (int x = 0; x < kVgaWidth; x++) {
            auto color = &m_palette[data[y * screenWidth + x] * 3];
            pixels[y * pitch / sizeof(Uint32) + x] = SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);
        }
    }

    SDL_UnlockTexture(m_texture);
    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);

    m_lastRenderEndTime = SDL_GetTicks();

    determineIfDelayNeeded();
}

// simulate SWOS procedure executed at each interrupt 8 tick
static void timerProc()
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

void frameDelay(float factor /* = 1.0 */)
{
    timerProc();

    if (!m_delay) {
        SDL_Delay(3);
        return;
    }

    constexpr int kNumFramesForSlackValue = 32;
    static std::array<int, kNumFramesForSlackValue> slackValues;
    static int slackValueIndex;

    int slackValue = std::accumulate(std::begin(slackValues), std::end(slackValues), 0);
    slackValue = (slackValue + kNumFramesForSlackValue / 2) / kNumFramesForSlackValue;

    constexpr Uint32 kFrameDelay = 15;
    Uint32 delay = std::lround(kFrameDelay * factor);

    auto startTicks = SDL_GetTicks();
    auto diff = startTicks - m_lastRenderStartTime;

    if (diff < delay) {
        if (static_cast<int>(delay - diff) > slackValue) {
            auto intendedDelay = delay - diff - slackValue;
            SDL_Delay(intendedDelay);
            auto newTicks = SDL_GetTicks();

            if (newTicks >= startTicks) {
                auto actualDelay = newTicks - startTicks;
                slackValues[slackValueIndex] = static_cast<int>(actualDelay - intendedDelay);
                slackValueIndex = (slackValueIndex + 1) % kNumFramesForSlackValue;
            }
        }

        do {
            startTicks = SDL_GetTicks();
        } while (m_lastRenderStartTime + delay > startTicks);
    }
}

void SWOS::Flip()
{
    updateControls();
    frameDelay();
    updateScreen();
}

//
// video options menu
//

static bool m_menuShown;

void showVideoOptionsMenu()
{
    m_menuShown = false;
    showMenu(videoOptionsMenu);
}

static void updateWindowSize()
{
    using namespace VideoOptionsMenu;
    assert(m_window);

    int width = m_windowWidth;
    int height = m_windowHeight;

    if (m_windowMode == kModeWindow)
        SDL_GetWindowSize(m_window, &width, &height);

    auto widthEntry = getMenuEntryAddress(Entries::customWidth);
    widthEntry->u2.number = width;

    auto heightEntry = getMenuEntryAddress(Entries::customHeight);
    heightEntry->u2.number = height;

    auto windowFlags = SDL_GetWindowFlags(m_window);
    m_windowResizable = (windowFlags & SDL_WINDOW_RESIZABLE) != 0;
}

using DisplayModeList = std::vector<std::pair<int, int>>;

static DisplayModeList getDisplayModes(int displayIndex)
{
    assert(displayIndex >= 0);

    int numModes = SDL_GetNumDisplayModes(displayIndex);
    if (numModes < 1)
        logWarn("No display modes, SDL reported: %s", SDL_GetError());
    else
        logInfo("Enumerating display modes, %d found", numModes);

    if (numModes > 20) {
        D1 = 55;
        D2 = 70;
        D3 = kYellowText;
        A1 = smallCharsTable;
        A0 = "PLEASE WAIT, ENUMERATING GRAPHICS MODES...";
        DrawMenuText();
        updateScreen();
    }

    DisplayModeList result;

    for (int i = 0; i < numModes; i++) {
        SDL_DisplayMode mode;
        if (!SDL_GetDisplayMode(displayIndex, i, &mode)) {
            logInfo("  %2d %d x %d, format: %x, refresh rate: %d", i, mode.w, mode.h, mode.format, mode.refresh_rate);

            // disallow any silly display mode
            switch (mode.format) {
            case SDL_PIXELFORMAT_INDEX1LSB:
            case SDL_PIXELFORMAT_INDEX1MSB:
            case SDL_PIXELFORMAT_INDEX4LSB:
            case SDL_PIXELFORMAT_INDEX4MSB:
            case SDL_PIXELFORMAT_INDEX8:
                continue;
            default:
                bool different = result.empty() ? true : result.back().first != mode.w || result.back().second != mode.h;
                bool acceptable = mode.w >= 640 && 10 * mode.w == 16 * mode.h || mode.w % 1920 == 0 && mode.h % 1080 == 0;

                if (different && acceptable)
                    result.emplace_back(mode.w, mode.h);
            }
        } else {
            logWarn("Failed to retrieve mode %d info, SDL reported: %s", i, SDL_GetError());
        }
    }

    logInfo("Accepted display modes:");
    for (size_t i = 0; i < result.size(); i++)
        logInfo("  %2d %d x %d", i, result[i].first, result[i].second);

    std::reverse(result.begin(), result.end());
    return result;
}

static DisplayModeList m_resolutions;
static int m_resListOffset;

static void fillResolutionListUi()
{
    using namespace VideoOptionsMenu;

    int currentEntry = getCurrentEntry();

    for (int i = resolutionField0; i < resolutionField0 + kNumResolutionFields; i++)
        getMenuEntryAddress(i)->invisible = 1;

    int visibleFields = m_resolutions.size() - m_resListOffset;
    for (int i = m_resListOffset; i < m_resListOffset + std::min(visibleFields, kNumResolutionFields); i++) {
        auto entry = getMenuEntryAddress(resolutionField0 + i - m_resListOffset);
        entry->invisible = 0;

        auto width = m_resolutions[i].first;
        auto height = m_resolutions[i].second;
        sprintf_s(entry->u2.string, 30, "%d X %d", width, height);

        if (m_windowMode == kModeFullScreen && m_displayWidth == width && m_displayHeight == height)
            entry->u1.entryColor = kPurple;
    }

    for (int i = resolutionField0; i < resolutionField0 + kNumResolutionFields; i++)
        if (i == currentEntry && getMenuEntryAddress(i)->invisible)
            setCurrentEntry(VideoOptionsMenu::exit);
}

static void scrollResolutionsDownSelected()
{
    using namespace VideoOptionsMenu;
    assert(m_resolutions.size() > kNumResolutionFields);
    m_resListOffset = std::min(m_resListOffset + 1, static_cast<int>(m_resolutions.size()) - kNumResolutionFields);
}

static void scrollResolutionsUpSelected()
{
    m_resListOffset = std::max(0, m_resListOffset - 1);
}

static void updateScrollArrows()
{
    using namespace VideoOptionsMenu;

    int hide = m_resolutions.size() <= kNumResolutionFields;

    int selectedEntry = getCurrentEntry();

    for (auto i : { scrollUpArrow, scrollDownArrow }) {
        auto entry = getMenuEntryAddress(i);
        entry->invisible = hide;

        if (hide && i == selectedEntry)
            setCurrentEntry(resolutionField0);
    }
}

static void toggleWindowResizable()
{
    if (m_windowMode != kModeWindow)
        setWindowMode(kModeWindow);

    setWindowResizable(!m_windowResizable);
}

static void changeResolutionSelected()
{
    auto entry = A5.as<MenuEntry *>();

    int i = entry->ordinal - VideoOptionsMenu::resolutionField0 + m_resListOffset;
    assert(i >= 0 && i < static_cast<int>(m_resolutions.size()));

    if (!setFullScreenResolution(m_resolutions[i].first, m_resolutions[i].second)) {
        logWarn("Failed to switch to %s", entry->u2.string);
        char buffer[64];
        sprintf_s(buffer, "FAILED TO SWITCH TO %s", entry->u2.string);
        showError(buffer);
    } else {
        logInfo("Successfully switched to %s", entry->u2.string);
    }
}

static std::pair<bool, int> inputNumber(MenuEntry *entry)
{
    assert(entry->type2 == ENTRY_NUMBER);

    char inputBuffer[32];
    int bufferLen = 0;
    constexpr int kMaxDigits = 4;
    int value = 0;

    auto originalNumber = entry->u2.number;
    entry->type2 = ENTRY_STRING;

    while (true) {
        SWOS::GetKey();

        if (bufferLen < kMaxDigits && lastKey >= 2 && lastKey <= 11) {
            int digit = lastKey == 11 ? 0 : lastKey - 1;
            value = value * 10 + digit;
            inputBuffer[bufferLen++] = '0' + digit;
        } else if (lastKey == 0xe) {
            if (bufferLen > 0)
                bufferLen--;
        } else if (lastKey == 0x1c) {
            entry->type2 = ENTRY_NUMBER;
            entry->u2.number = value;
            return { true, value };
            break;
        } else if (lastKey == 0x01) {
            entry->type2 = ENTRY_NUMBER;
            entry->u2.number = originalNumber;
            return { false, 0 };
        }

        inputBuffer[bufferLen] = '\0';
        entry->u2.string = inputBuffer;

        drawMenuItem(entry);

        SWOS::FlipInMenu();
    }
}

static void inputWindowWidth()
{
    if (m_windowMode == kModeWindow) {
        assert(m_window);

        auto entry = A5.as<MenuEntry *>();
        auto result = inputNumber(entry);
        auto width = result.second;

        if (result.first && width != m_windowWidth && width >= kVgaWidth) {
            setWindowSize(width, m_windowHeight);
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }
}

static void inputWindowHeight()
{
    if (m_windowMode == kModeWindow) {
        assert(m_window);

        auto entry = A5.as<MenuEntry *>();
        auto result = inputNumber(entry);
        auto height = result.second;

        if (result.first && height != m_windowHeight && height >= kVgaHeight) {
            setWindowSize(m_windowWidth, height);
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }
}

static void enableCustomSizeFields(bool enabled)
{
    using namespace VideoOptionsMenu;

    for (int i : { windowed, resizable, })
        getMenuEntryAddress(i)->textColor = enabled ? kWhiteText : kGrayText;

    for (int i : { customWidth, customHeight, resizableOnOff, })
        getMenuEntryAddress(i)->u1.entryColor = enabled ? kLightBlue : kGray;
}

static void enableBorderlessFields(bool enabled)
{
    auto color = enabled ? kWhiteText : kGrayText;
    getMenuEntryAddress(VideoOptionsMenu::borderlessMaximized)->textColor = color;
}

static void enableFullScreenFields(bool enabled)
{
    using namespace VideoOptionsMenu;

    int textColor = enabled ? kWhiteText : kGrayText;
    int backColor = enabled ? kLightBlue : kGray;

    getMenuEntryAddress(fullScreen)->textColor = textColor;

    for (int i = resolutionField0; i < resolutionField0 + kNumResolutionFields; i++)
        getMenuEntryAddress(i)->u1.entryColor = backColor;
}

void highlightCurrentMode()
{
    using namespace VideoOptionsMenu;

    static const std::array<int, 3> kHighlightArrows = {
        customSizeArrow, borderlessMaximizedArrow, fullScreenArrow,
    };

    for (int index : kHighlightArrows)
        getMenuEntryAddress(index)->invisible = 1;

    int showIndex = customSizeArrow;
    bool enableCustomSize = false, enableBorderless = false, enableFullScreen = false;

    switch (m_windowMode) {
    case kModeWindow:
        showIndex = customSizeArrow;
        enableCustomSize = true;
        break;

    case kModeBorderlessMaximized:
        showIndex = borderlessMaximizedArrow;
        enableBorderless = true;
        break;

    case kModeFullScreen:
        showIndex = fullScreenArrow;
        enableFullScreen = true;
        break;

    default:
        assert(false);
    }

    getMenuEntryAddress(showIndex)->invisible = 0;

    enableCustomSizeFields(enableCustomSize);
    enableBorderlessFields(enableBorderless);
    enableFullScreenFields(enableFullScreen);
}

static void videoOptionsMenuOnDraw()
{
    static int lastDisplayIndex = -1;

    updateWindowSize();
    highlightCurrentMode();

    int displayIndex = SDL_GetWindowDisplayIndex(m_window);
    if (displayIndex >= 0) {
        if (displayIndex != lastDisplayIndex) {
            m_resolutions = getDisplayModes(displayIndex);
            lastDisplayIndex = displayIndex;
            m_resListOffset = 0;
        }

        fillResolutionListUi();
        updateScrollArrows();
    } else {
        if (!m_menuShown)
            logWarn("Failed to get window display index, SDL reported: %s", SDL_GetError());
    }

    m_menuShown = true;
}

static void switchToWindow()
{
    if (m_windowMode != kModeWindow)
        setWindowMode(kModeWindow);
}

static void switchToBorderlessMaximized()
{
    if (m_windowMode != kModeBorderlessMaximized)
        setWindowMode(kModeBorderlessMaximized);
}
