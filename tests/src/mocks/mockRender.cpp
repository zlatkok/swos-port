#include "render.h"
#include "mockRender.h"
#include "windowManager.h"
#include "util.h"

static SDL_Renderer *m_renderer;
static SDL_Texture *m_texture;
static Uint32 m_format;
static std::vector<UpdateHook> m_updateHooks;

void initRendering()
{
    auto window = createWindow();
    auto [width, height] = getWindowSize();

    m_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    assert(m_renderer);

    updateRenderTarget(width, height);

    SDL_RendererInfo info;
    SDL_GetRendererInfo(m_renderer, &info);
    m_format = info.texture_formats[0];
}

void finishRendering() {}

int addUpdateHook(UpdateHook hook)
{
    assert(hook);
    m_updateHooks.push_back(hook);
    return m_updateHooks.size() - 1;
}

void removeUpdateHook(int hookId)
{
    m_updateHooks[hookId] = nullptr;
}

SDL_Surface *getScreenSurface()
{
    auto viewPort = getViewport();
    auto surface = SDL_CreateRGBSurfaceWithFormat(0, viewPort.w, viewPort.h, 32, m_format);
    if (surface)
        SDL_RenderReadPixels(m_renderer, nullptr, surface->format->format, surface->pixels, surface->pitch);

    return surface;
}

void updateRenderTarget(int width, int height)
{
    if (m_texture)
        SDL_DestroyTexture(m_texture);

    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    assert(m_texture);

    int error = SDL_SetRenderTarget(m_renderer, m_texture);
    assert(!error);
}

void updateScreen(bool)
{
    for (const auto& hook : m_updateHooks) {
        if (hook)
            hook();
    }
}

SDL_Renderer *getRenderer()
{
    return m_renderer;
}

SDL_Rect getViewport()
{
    SDL_Rect rect;
    SDL_RenderGetViewport(m_renderer, &rect);
    return rect;
}

void skipFrameUpdate() {}
void frameDelay(double) {}
void fadeIfNeeded() {}
void fadeIn(std::function<void()>, double) {}
void fadeOut(std::function<void()>, double) {}
void makeScreenshot() {}
bool getLinearFiltering() { return false; }
void setLinearFiltering(bool) {}
bool getClearScreen() { return false; }
void setClearScreen(bool) {}
