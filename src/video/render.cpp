#include "render.h"
#include "log.h"
#include "util.h"
#include "file.h"
#include "controls.h"
#include "dump.h"
#include "options.h"
#include <SDL_image.h>

int kVgaWidth = 320;
int kVgaHeight = 200;
int kVgaScreenSize = kVgaWidth * kVgaHeight;

int kVgaWidthAdder      = kVgaWidth - kMenuScreenWidth;
int kVgaHeightAdder     = kVgaHeight - kMenuScreenHeight;
int kVgaWidthAdderHalf  = kVgaWidthAdder  / 2;
int kVgaHeightAdderHalf = kVgaHeightAdder / 2;

int kWindowWidth = 4 * kVgaWidth;
int kWindowHeight = 4 * kVgaHeight;

int kMenuScreenWidth = 320;
int kGameScreenWidth = 384;
int kMenuScreenHeight = 200;
int DrawPatternVerticalCount = 2;

static SDL_Window *m_window;
static SDL_Renderer *m_renderer;
static SDL_Texture *m_texture;
static SDL_PixelFormat *m_pixelFormat;

constexpr int kNumColors = 256;
constexpr int kPaletteSize = 3 * kNumColors;
uint8_t m_palette[kPaletteSize];

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

uint8_t AB_FlagTable[200 * 320];
uint8_t AB_FlagTable2[200 * 320];
char AB_OldVideoMemory[200 * 320];
RGBStruct AB_RGBTable[200 * 320];

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

    uint8_t _gap = 0;
    if (kVgaWidth == 480) _gap = 1;
    logInfo("Creating %dx%d window, flags: %d", m_windowWidth, m_windowHeight-_gap*2, flags);
    m_window = SDL_CreateWindow("SWOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_windowWidth, m_windowHeight-_gap * 2, flags);
    if (!m_window)
        sdlErrorExit("Could not create window");

    setWindowMode(m_windowMode);

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer)
        sdlErrorExit("Could not create renderer");

    auto format = SDL_PIXELFORMAT_RGBA8888;
	m_texture = SDL_CreateTexture(m_renderer, format, SDL_TEXTUREACCESS_STREAMING, kVgaWidth, kVgaHeight-_gap * 2);
    if (!m_texture)
        sdlErrorExit("Could not create surface");

    m_pixelFormat = SDL_AllocFormat(format);
    if (!m_pixelFormat)
        sdlErrorExit("Could not allocate pixel format");

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(m_renderer, kVgaWidth, kVgaHeight-_gap * 2);
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

void updateScreen_MenuScreen(const char *inData /* = nullptr */, int offsetLine /* = 0 */, int numLines /* = kVgaHeight */)
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
		for (int y = 0; y < kMenuScreenHeight; y++) {
        	for (int x = 0; x < kMenuScreenWidth; x++) {
				//if (AB_FlagTable[y * kMenuScreenWidth + x] == 0 || AB_FlagTable2[y * kMenuScreenWidth + x] == 0) {
				if (AB_FlagTable[y * kMenuScreenWidth + x] == 0) {
					auto color = &m_palette[data[y * kMenuScreenWidth + x] * 3];
				    pixels[(y + kVgaHeightAdderHalf) * pitch / sizeof(Uint32) + (x + kVgaWidthAdderHalf)] =
						SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);
				}
				else {
					/*
					auto color_old = &m_palette[AB_OldVideoMemory[y * kMenuScreenWidth + x] * 3];
					auto color_new = &m_palette[data[y * kMenuScreenWidth + x] * 3];

					// If background not changed, draw Alpha-blending pixels
					if (color_old[0] == color_new[0] && color_old[1] == color_new[1] && color_old[2] == color_new[2]) {
						RGBStruct ab;
						uint8_t color[3];

						ab = AB_RGBTable[y * kMenuScreenWidth + x];
						color[0] = ab.r;
						color[1] = ab.g;
						color[2] = ab.b;
						pixels[(y + kVgaHeightAdderHalf) * pitch / sizeof(Uint32) + (x + kVgaWidthAdderHalf)] =
							SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);
					}
					// If background changed, draw latest from latest video memory
					else {
						auto color = &m_palette[data[y * kMenuScreenWidth + x] * 3];

						pixels[(y + kVgaHeightAdderHalf) * pitch / sizeof(Uint32) + (x + kVgaWidthAdderHalf)] =
							SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);
					}
					*/

					///*
					auto color = &m_palette[data[y * kMenuScreenWidth + x] * 3];

					if (
						(color[0] == 255 && color[1] == 255 && color[2] == 255) ||
						(color[0] ==  36 && color[1] ==  36 && color[2] ==  36) ||
						(color[0] == 182 && color[1] == 182 && color[2] == 182)
						) {
						pixels[(y + kVgaHeightAdderHalf) * pitch / sizeof(Uint32) + (x + kVgaWidthAdderHalf)] =
							SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);
					}
					else {
						RGBStruct ab;
						uint8_t _color[3];

						ab = AB_RGBTable[y * kMenuScreenWidth + x];
						_color[0] = ab.r;
						_color[1] = ab.g;
						_color[2] = ab.b;
						pixels[(y + kVgaHeightAdderHalf) * pitch / sizeof(Uint32) + (x + kVgaWidthAdderHalf)] =
							SDL_MapRGBA(m_pixelFormat, _color[0], _color[1], _color[2], 255);
					}
					//*/
				}
			}
    	}
    });

    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);

    m_lastRenderEndTime = SDL_GetPerformanceCounter();

    determineIfDelayNeeded();
}

void putPixel(Uint32 *pixels, int pitch, uint8_t *data, int x, int y, int p)
{
    auto color = &m_palette[p * 3];
    pixels[y * pitch / sizeof(Uint32) + x] = SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);
}


void putDarkenPixel(Uint32 *pixels, int pitch, uint8_t *data, int x, int y)
{
    int p = data[y * kGameScreenWidth + x];
    int p2 = 0;

    if (p < 128) {
        p2 = p + 128;
    }
	else {
		p2 = p;
	}

	/*
    if (p == 2) p2 = 2;
    if (p == 160) p2 = 160;
    if (p == 81) p2 = 81;

	if (p == 101) p2 = 101;
	if (p == 189) p2 = 189;
	*/
    auto color = &m_palette[p2 * 3];
    pixels[y * pitch / sizeof(Uint32) + x] = SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);
}


void drawHorzLine(Uint32 *pixels, int pitch, uint8_t *data, int x, int y, int w, int p)
{
    int i;
    for (i = 0; i <= w; i++) {
        putPixel(pixels, pitch, data, x + i, y, p);
    }
}


void drawVertLine(Uint32 *pixels, int pitch, uint8_t *data, int x, int y, int h, int p)
{
    int i;
    for (i = 0; i <= h; i++) {
        putPixel(pixels, pitch, data, x, y + i, p);
    }
}


void drawBox(Uint32 *pixels, int pitch, uint8_t *data, int x, int y, int w, int h, int p)
{
    drawHorzLine(pixels, pitch, data, x, y,     w, p);
    drawHorzLine(pixels, pitch, data, x, y + h, w, p);
    drawVertLine(pixels, pitch, data, x, y,     h, p);
    drawVertLine(pixels, pitch, data, x + w, y, h, p);
}


void drawFillBox(Uint32 *pixels, int pitch, uint8_t *data, int x, int y, int w, int h, int p)
{
    int i;
    for (i = 0; i <= h; i++) {
        drawHorzLine(pixels, pitch, data, x, y + i, w, p);
    }
}


void drawDarkenRect(Uint32 *pixels, int pitch, uint8_t *data, int x, int y, int w, int h)
{
    int i, j;
    for (j = y; j <= y + h; j++) {
        for (i = x; i <= x + w; i++) {
            putDarkenPixel(pixels, pitch, data, i, j);
        }
    }
}


int getRandomInt(int a, int b)
{
    return a + (std::rand() % (b - a + 1));
}

char *uniform0[8] = {
    "........",
    ".12221..",
    ".12221+.",
    "..222++.",
    "..333+..",
    "..333+..",
    "...+++..",
    "........",
};
char *uniform1[8] = {
    "........",
    ".12221..",
    ".12221+.",
    "..222++.",
    "..333+..",
    "..333+..",
    "...+++..",
    "........",
};
char *uniform2[8] = {
    "........",
    ".12121..",
    ".12121+.",
    "..212++.",
    "..333+..",
    "..333+..",
    "...+++..",
    "........",
};
char *uniform3[8] = {
    "........",
    ".11111..",
    ".12221+.",
    "..111++.",
    "..333+..",
    "..333+..",
    "...+++..",
    "........",
};


void showUniform(Uint32 *pixels, int pitch, uint8_t *data, int mode, int x, int y, int scale)
{
    int i, j, _i, _j;
    char c;
    int col;
    TeamGame team;
    
    if (mode == 0) {
        team = leftTeamIngame;
    }
    else {
        team = rightTeamIngame;
    }

    for (j = 0; j <= 7 * scale; j++) {
        for (i = 0; i <= 7 * scale; i++) {
            _j = j / scale;
            _i = i / scale;

            if (team.prShirtType == 0) {
                c = uniform0[_j][_i];
            }
            else if (team.prShirtType == 1) {
                c = uniform1[_j][_i];
            }
            else if (team.prShirtType == 2) {
                c = uniform2[_j][_i];
            }
            else if (team.prShirtType == 3) {
                c = uniform3[_j][_i];
            }

            col = -1;
            if (c == '1') {
                col = team.prShirtCol;
            }
            else if (c == '2') {
                col = team.prStripesCol;
            }
            else if (c == '3') {
                col = team.prShortsCol;
            }
            else if (c == '+') {
                col = 3; //50; //80;
            }
            if (col != -1) putPixel(pixels, pitch, data, x + i, y + j, col);
        }
    }
}

extern word g_scoresMode;
extern int g_replayViewLoop;
int showTeamIcons(Uint32 *pixels, int pitch, uint8_t *data) 
{
    //drawDarkenRect(pixels, pitch, data, 0, 100, 100, 50 * (g_viewLoop + 1));
    if (g_replayViewLoop == 1) return 0;
    if (g_scoresMode == 1 || g_scoresMode == 2) {
        if (resultTimer != 0) return 0;
    }
    if (replayState != 0) return 0;
    if (g_scoresMode == dseg_168941) return 0;

    int ox, oy, x, y;

    if (g_scoresMode == 4) {
        ox = 10;             oy = 5;
    }
    else if (g_scoresMode == 1) {
        ox = 10;             oy = kVgaHeight - 5 - 8;
    }
    else if (g_scoresMode == 2) {
        ox = kVgaWidth - 10 - 70;  oy = kVgaHeight - 5 - 8;
    }
    else if (g_scoresMode == 3) {
        ox = kVgaWidth - 10 - 70;  oy = 5;
    }
    
    drawDarkenRect(pixels, pitch, data, ox, oy, 70, 8);

    x = ox + 23; y = oy + 1;
    showUniform(pixels, pitch, data, 0, x, y, 1);   
    x = ox + 63; y = oy + 1;
    showUniform(pixels, pitch, data, 1, x, y, 1);

    return -1;
}


void updateScreen_GameScreen(const char *inData /* = nullptr */, int offsetLine /* = 0 */, int numLines /* = kVgaHeight */)
{
    assert(offsetLine >= 0 && offsetLine <= kVgaHeight);
    assert(numLines >= 0 && numLines <= kVgaHeight);
    assert(offsetLine + numLines <= kVgaHeight);

    m_lastRenderStartTime = SDL_GetPerformanceCounter();


#ifdef DEBUG
    if (screenWidth == kGameScreenWidth)
        dumpVariables();
#endif

    requestPixelAccess([&](Uint32 *pixels, int pitch) {
		uint8_t *data;
    	data = (uint8_t *)vsPtr;		
		uint8_t _gap = 0;
	    if (kVgaWidth == 480) _gap = 1;
    	for (int y = offsetLine+_gap; y < numLines-_gap; y++) {
        	for (int x = 0; x < kVgaWidth; x++) {
            	auto color = &m_palette[data[y * kGameScreenWidth + x] * 3];
            	pixels[(y-_gap) * pitch / sizeof(Uint32) + x] = SDL_MapRGBA(m_pixelFormat, color[0], color[1], color[2], 255);
        	}
    	}
		showTeamIcons(pixels, pitch, data);
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

    double kTargetFps = 70;
	if (getGameStyle() == 1) kTargetFps = 50;

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
        SWOS::FadeIn();
        skipFade = -1;
    }
}

void fadeIfNeeded2()
{
    if (!skipFade) {
        SWOS::FadeIn2();
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
    updateScreen_GameScreen();
}

void drawEmptyMenuItemOriginal()
{
	word x, y, width, height, color;
	word xi, yi;
	dword w, h, ww;
	char c; //, c0, c2;
	word i, j;
	word index;
	dword where_index, fill_index;
	char* fill256 = (char*)linAdr384k + 65536;

	x = (word)D0;
	y = (word)D1;
	width = (word)D2;
	height = (word)D3;
	color = (word)D4;

	if ((width == 1) || (height == 1)) {
		ww = 320 - width;
		h = height;
		c = color - deltaColor;

		index = y * 320 + x;
		for (j = 0; j <= height - 1; j++) {
			for (i = 0; i <= width - 1; i++) {
				linAdr384k[index] = c;
				index++;
			}
			index += ww;
		}
	}
	else {
		color = menuItemColorTable[color];
		where_index = y * 320 + x;
		fill_index = y * 320 + x;;

		ww = 320 - width;
		w = width;
		h = height;

		for (j = 0; j <= h - 1; j++) {
			for (i = 0; i <= w - 1; i++) {
				xi = x + i;
				yi = y + j;

				c = fill256[fill_index];
				c += color;

				linAdr384k[where_index] = c;

				where_index++;
				fill_index++;
			}
			where_index += ww;
			fill_index += ww;
		}
	}
}

void drawEmptyMenuItemAlphaBlending()
{
	word x, y, width, height, color;
	word xi, yi;
	dword w, h, ww;
	byte c;
	char backColor, foreColor;
	word i, j;
	word index;
	dword where_index, fill_index;
	char* fill256 = (char*)linAdr384k + 65536;
	byte ab_ratio;
	
	ab_ratio = getAlphaBlendingMenu();

	x = (word)D0;
	y = (word)D1;
	width = (word)D2;
	height = (word)D3;
	color = (word)D4;

	if ((width == 1) || (height == 1)) {
		ww = 320 - width;
		h = height;
		c = color - deltaColor;

		index = y * 320 + x;
		for (j = 0; j <= height - 1; j++) {
			for (i = 0; i <= width - 1; i++) {
				linAdr384k[index] = c;
				index++;
			}
			index += ww;
		}
	}
	else {
		color = menuItemColorTable[color];
		where_index = y * 320 + x;
		fill_index = y * 320 + x;;

		ww = 320 - width;
		w = width;
		h = height;

		for (j = 0; j <= h - 1; j++) {
			for (i = 0; i <= w - 1; i++) {
				if (j > 0 && j <= h - 2 && i > 0 && i <= w - 2) {
					AB_FlagTable[where_index] = 1;
					//AB_FlagTable2[where_index] = 1;
				}

				RGBStruct f, b, ab;

				xi = x + i;
				yi = y + j;

				// Get RGB color of background (image)
				backColor = linAdr384k[where_index];
				auto color_b = &m_palette[linAdr384k[where_index] * 3];
				b.r = color_b[0];
				b.g = color_b[1];
				b.b = color_b[2];

				// Get RGB color of foreground (menu item)
				c = fill256[fill_index];
				c += color;
				foreColor = c;
				//foreColor = color;
				linAdr384k[where_index] = foreColor;
				AB_OldVideoMemory[where_index] = foreColor;
				auto color_f = &m_palette[linAdr384k[where_index] * 3];
				f.r = color_f[0];
				f.g = color_f[1];
				f.b = color_f[2];

				// Make alpha blending
				ab.r = (uint8_t) (b.r * (100. - ab_ratio) / 100. + f.r * ab_ratio / 100.);
				ab.g = (uint8_t) (b.g * (100. - ab_ratio) / 100. + f.g * ab_ratio / 100.);
				ab.b = (uint8_t) (b.b * (100. - ab_ratio) / 100. + f.b * ab_ratio / 100.);

				//ab.r = 128;
				//ab.g = 128;
				//ab.b = 128;

				// Put RGB Table
				AB_RGBTable[where_index] = ab;

				where_index++;
				fill_index++;
			}
			where_index += ww;
			fill_index += ww;
		}
	}
}

void SWOS::DrawEmptyMenuItem()
{
	if (!isAlphaBlendingMenu()) {
		SAFE_INVOKE(drawEmptyMenuItemOriginal);
	}
	else {
		SAFE_INVOKE(drawEmptyMenuItemAlphaBlending);
	}
}

void SWOS::DrawMenu_1()
{
	int i, j;
	for (j = 0; j <= 199; j++) {
		for (i = 0; i <= 319; i++) {
			AB_FlagTable[j * 320 + i] = 0;
		}
	}
	int k = 0;
}

/*
void SWOS::DrawMenuItem_1()
{
	int i, j;
	for (j = 0; j <= 199; j++) {
		for (i = 0; i <= 319; i++) {
			AB_FlagTable2[j * 320 + i] = 0;
		}
	}
	int k = 0;
}
*/