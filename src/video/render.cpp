#include "render.h"
#include "timer.h"
#include "overlay.h"
#include "sprites.h"
#include "colorizeSprites.h"
#include "windowManager.h"
#include "joypads.h"
#include "VirtualJoypad.h"
#include "text.h"
#include "dump.h"
#include "file.h"
#include "util.h"

static SDL_Renderer *m_renderer;
static Uint32 m_windowPixelFormat;

static bool m_useLinearFiltering = true;
static bool m_clearScreen = true;

static bool m_pendingScreenshot;

static void fade(bool fadeOut, std::function<void()> render);
static void doMakeScreenshot();
static SDL_Surface *getScreenSurface();

void initRendering()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        sdlErrorExit("Could not initialize SDL");

    auto window = createWindow();
    if (!window)
        sdlErrorExit("Could not create main window");

    m_windowPixelFormat = SDL_GetWindowPixelFormat(window);
    if (m_windowPixelFormat == SDL_PIXELFORMAT_UNKNOWN)
        sdlErrorExit("Failed to query window pixel format");

    m_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer)
        sdlErrorExit("Could not create renderer");

    SDL_RendererInfo info;
    SDL_GetRendererInfo(m_renderer, &info);
    logInfo("Renderer \"%s\" created successfully, maximum texture width: %d, height: %d",
        info.name, info.max_texture_width, info.max_texture_height);

    logInfo("Supported texture formats:");
    for (size_t i = 0; i < info.num_texture_formats; i++)
        logInfo("    %s", SDL_GetPixelFormatName(info.texture_formats[i]));

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

    initWindow();   // must have renderer alive and well first to update the logical size

#ifdef VIRTUAL_JOYPAD
    getVirtualJoypad().setRenderer(m_renderer);
#endif
}

void finishRendering()
{
    deinitWindow();

    finishSpriteColorizer();
    finishSprites();

    if (m_renderer)
        SDL_DestroyRenderer(m_renderer);

    destroyWindow();

    SDL_Quit();
}

SDL_Renderer *getRenderer()
{
    assert(m_renderer);
    return m_renderer;
}

SDL_Rect getViewport()
{
    SDL_Rect viewport;
    SDL_RenderGetViewport(m_renderer, &viewport);
    return viewport;
}

void updateScreen(bool delay /* = false */)
{
#ifdef VIRTUAL_JOYPAD
    getVirtualJoypad().render(m_renderer);
#endif

#ifdef DEBUG
    dumpVariables();
#endif

    if (m_pendingScreenshot) {
        doMakeScreenshot();
        m_pendingScreenshot = false;
    }

    // important call to keep the FPS stable
    SDL_RenderFlush(m_renderer);

    showOverlay();

    if (delay)
        gameFrameDelay();

    measureRendering([]() {
        SDL_RenderPresent(m_renderer);
    });

    // must clear the renderer or there will be display artifacts on Samsung phone
    // still, let the user decide which way is better on their machine
    if (m_clearScreen) {
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        SDL_RenderClear(m_renderer);
    }
}

void fadeIn(std::function<void()> render)
{
    fade(false, render);
}

void fadeOut(std::function<void()> render)
{
    fade(true, render);
}

bool getLinearFiltering()
{
    return m_useLinearFiltering;
}

void setLinearFiltering(bool useLinearFiltering)
{
    if (useLinearFiltering != m_useLinearFiltering) {
        if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, useLinearFiltering ? "1" : "0"))
            m_useLinearFiltering = useLinearFiltering;
        else
            logWarn("Couldn't %s linear filtering", useLinearFiltering ? "enable" : "disable");
    }
}

bool getClearScreen()
{
    return m_clearScreen;
}

void setClearScreen(bool clearScreen)
{
    m_clearScreen = clearScreen;
}

std::string ensureScreenshotsDirectory()
{
    auto path = pathInRootDir("screenshots");
    createDir(path.c_str());
    return path;
}

void makeScreenshot()
{
    m_pendingScreenshot = true;
}

static void fade(bool fadeOut, std::function<void()> render)
{
    constexpr int kFadeDelayMs = 900;
    constexpr int kMaxAlpha = 255;

    auto numSteps = std::ceil(kFadeDelayMs * targetFps() / 1'000);
    auto alphaPerFrame = kMaxAlpha / numSteps;

    for (auto i = .0; i <= numSteps; i++) {
        markFrameStartTime();

        render();

        auto alpha = fadeOut ? i * alphaPerFrame : kMaxAlpha - i * alphaPerFrame;
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, static_cast<int>(std::round(alpha)));
        SDL_RenderFillRect(m_renderer, nullptr);

        updateScreen(true);
    }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
}

static void doMakeScreenshot()
{
    char filename[256];

    auto t = time(nullptr);
    auto len = strftime(filename, sizeof(filename), "screenshot-%Y-%m-%d-%H-%M-%S-", localtime(&t));

    auto ms = SDL_GetTicks() % 1'000;
    sprintf(filename + len, "%03d.png", ms);

    const auto& screenshotsPath = ensureScreenshotsDirectory();
    const auto& path = joinPaths(screenshotsPath.c_str(), filename);

    if (auto surface = getScreenSurface()) {
        if (IMG_SavePNG(surface, path.c_str()) >= 0) {
            enqueueInfoMessage("Screenshot created: %s", filename);
            logInfo("Screenshot created: %s", filename);
        } else {
            enqueueInfoMessage("Error saving screenshot");
            logWarn("Failed to save screenshot %s: %s", filename, IMG_GetError());
        }

        SDL_FreeSurface(surface);
    } else {
        logWarn("Error creating screenshot");
    }
}

static SDL_Surface *getScreenSurface()
{
    SDL_Surface *surface = nullptr;
    auto viewPort = getViewport();

    if (surface = SDL_CreateRGBSurfaceWithFormat(0, viewPort.w, viewPort.h, 32, m_windowPixelFormat)) {
        if (SDL_RenderReadPixels(m_renderer, nullptr, m_windowPixelFormat, surface->pixels, surface->pitch) < 0) {
            SDL_FreeSurface(surface);
            surface = nullptr;
            logWarn("Failed to read the pixels from the renderer: %s", SDL_GetError());
        }
    } else {
        logWarn("Failed to create a surface for the screen: %s", SDL_GetError());
    }

    return surface;
}
