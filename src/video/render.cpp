#include "render.h"
#include "windowManager.h"
#include "util.h"
#include "file.h"
#include "dump.h"
#include "joypads.h"
#include "VirtualJoypad.h"

static SDL_Renderer *m_renderer;

static bool m_useLinearFiltering;

static Uint64 m_lastRenderStartTime;
static Uint64 m_lastRenderEndTime;

static bool m_delay;
static std::deque<int> m_lastFramesDelay;
static constexpr int kMaxLastFrames = 16;

void initRendering()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        sdlErrorExit("Could not initialize SDL");

    auto window = createWindow();

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

static void determineIfDelayNeeded()
{
    auto delay = m_lastRenderEndTime > m_lastRenderStartTime && m_lastRenderEndTime - m_lastRenderStartTime >= 10;

    if (m_lastFramesDelay.size() >= kMaxLastFrames)
        m_lastFramesDelay.pop_front();

    m_lastFramesDelay.push_back(delay);

    size_t sum = std::accumulate(m_lastFramesDelay.begin(), m_lastFramesDelay.end(), 0);
    m_delay = 2 * sum >= m_lastFramesDelay.size();
}

void skipFrameUpdate()
{
    m_lastRenderStartTime = m_lastRenderEndTime = SDL_GetPerformanceCounter();
}

void updateScreen()
{
    m_lastRenderStartTime = SDL_GetPerformanceCounter();

#ifdef VIRTUAL_JOYPAD
    getVirtualJoypad().render(m_renderer);
#endif
    SDL_RenderPresent(m_renderer);
    // must clear the renderer or there will be display artefacts on Samsung phone
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);

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
    swos.currentTick++;
    swos.menuCycleTimer++;
    if (!swos.paused) {
        swos.stoppageTimer++;
        swos.timerBoolean = (swos.timerBoolean + 1) & 1;
        if (!swos.timerBoolean)
            swos.bumpBigSFrame = -1;
    }
}

void fadeIfNeeded()
{
    if (!swos.skipFade) {
        FadeIn();
        swos.skipFade = -1;
    }
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
    char filename[256];

    auto t = time(nullptr);
    auto len = strftime(filename, sizeof(filename), "screenshot-%Y-%m-%d-%H-%M-%S-", localtime(&t));

    auto ms = SDL_GetTicks() % 1'000;
    sprintf(filename + len, "%03d.png", ms);

    const auto& screenshotsPath = ensureScreenshotsDirectory();
    const auto& path = joinPaths(screenshotsPath.c_str(), filename);

    auto viewPort = getViewport();

    if (auto surface = SDL_CreateRGBSurface(0, viewPort.w, viewPort.h, 32, 0, 0, 0, 0)) {
        if (SDL_RenderReadPixels(m_renderer, nullptr, surface->format->format, surface->pixels, surface->pitch)) {
            if (IMG_SavePNG(surface, path.c_str()) >= 0)
                logInfo("Screenshot created: %s", filename);
            else
                logWarn("Failed to save screenshot %s: %s", filename, IMG_GetError());
        } else {
            logWarn("Failed to read the data from the renderer: %s", SDL_GetError());
        }
        SDL_FreeSurface(surface);
    } else {
        logWarn("Failed to create a surface for the screenshot: %s", SDL_GetError());
    }
}

void SWOS::Flip()
{
    processControlEvents();
    frameDelay();
    updateScreen();
}
