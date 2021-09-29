#include "render.h"
#include "timer.h"
#include "game.h"
#include "windowManager.h"
#include "joypads.h"
#include "VirtualJoypad.h"
#include "dump.h"
#include "file.h"
#include "util.h"
#include "color.h"
#include "text.h"

static SDL_Renderer *m_renderer;
static Uint32 m_windowPixelFormat;

static bool m_useLinearFiltering;

static bool m_pendingScreenshot;

static void showFps();
static void fade(bool fadeOut, std::function<void()> render, double factor);
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

#ifdef VIRTUAL_JOYPAD
    getVirtualJoypad().setRenderer(m_renderer);
#endif
}

void finishRendering()
{
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

    showFps();

    if (delay)
        frameDelay();

    measureRendering([]() {
        SDL_RenderPresent(m_renderer);
    });

    // must clear the renderer or there will be display artifacts on Samsung phone
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
}

void fadeIn(std::function<void()> render, double factor /* = 1.0 */)
{
    fade(false, render, factor);
}

void fadeOut(std::function<void()> render, double factor /* = 1.0 */)
{
    fade(true, render, factor);
}

void drawRectangle(int x, int y, int width, int height, const Color& color)
{
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, 255);
    auto scale = getScale();

    auto x1 = x * scale + getScreenXOffset() - scale / 2;
    auto y1 = y * scale + getScreenYOffset();
    auto w = (width + 1) * scale;
    auto h = height * scale;
    auto x2 = x1 + w - scale;
    auto y2 = y1 + h - scale;

    SDL_FRect rects[4] = {
        { x1, y1, w, scale },
        { x1, y1, scale, h },
        { x2, y1, scale, h },
        { x1, y2, w, scale },
    };
    SDL_RenderFillRectsF(m_renderer, rects, std::size(rects));
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

static void showFps()
{
    if (getShowFps()) {
        constexpr int kNumFramesForFps = 64;

        auto now = SDL_GetPerformanceCounter();

        static Uint64 s_lastFrameTick;
        if (s_lastFrameTick) {
            static std::array<Uint64, kNumFramesForFps> s_renderTimes;
            static int s_renderTimesIndex;

            auto frameTime = now - s_lastFrameTick;

            s_renderTimes[s_renderTimesIndex] = frameTime;
            s_renderTimesIndex = (s_renderTimesIndex + 1) % s_renderTimes.size();
            int numFrames = 0;

            auto totalRenderTime = std::accumulate(s_renderTimes.begin(), s_renderTimes.end(), 0ULL, [&](auto sum, auto current) {
                if (current) {
                    sum += current;
                    numFrames++;
                }
                return sum;
            });

            auto fps = .0;
            if (numFrames)
                fps = 1. / (static_cast<double>(totalRenderTime) / numFrames / SDL_GetPerformanceFrequency());

            char buf[32];
            formatDoubleNoTrailingZeros(fps, buf, sizeof(buf), 2);

            drawText(290, 4, buf);
        }

        s_lastFrameTick = now;
    }
}

static void fade(bool fadeOut, std::function<void()> render, double factor)
{
    constexpr int kFadeDelayMs = 900;
    constexpr int kMaxAlpha = 255;

    auto numSteps = std::ceil(kFadeDelayMs * kTargetFps / factor / 1'000);
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
        if (IMG_SavePNG(surface, path.c_str()) >= 0)
            logInfo("Screenshot created: %s", filename);
        else
            logWarn("Failed to save screenshot %s: %s", filename, IMG_GetError());

        SDL_FreeSurface(surface);
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
