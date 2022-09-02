#include "testEnvironment.h"
#include "render.h"
#include "windowManager.h"
#include "mockWindowManager.h"
#include "init.h"
#include "mainMenu.h"
#include "exceptions.h"
#include "mockRender.h"
#include "mockRenderSprites.h"
#include "sdlProcs.h"

using RenderCopyType = int (*)(SDL_Renderer *, SDL_Texture *, const SDL_Rect *, const SDL_FRect *);

static RenderCopyType m_realRenderCopyF;
static bool m_menuDrawn;

static void initMenuBackgroundDrawHook();
static int hookRenderCopyF(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcRect, const SDL_FRect *dstRect);

void initializeTestEnvironment()
{
    try {
        initMenuBackgroundDrawHook();
        initRendering();
        setWindowSize(kDefaultWindowWidth, kDefaultWindowHeight);
        initMockSpriteRenderer();
        initMainMenuGlobals();
        SDL_StopTextInput();
        startMainMenuLoop();
    } catch (const SWOS_UnitTest::BaseException& e) {
        std::cout << "Initialization error: " << e.error();
        exit(1);
    }
}

void resetMenuDrawn()
{
    m_menuDrawn = false;
}

bool wasMenuDrawn()
{
    return m_menuDrawn;
}

static void initMenuBackgroundDrawHook()
{
    m_realRenderCopyF = reinterpret_cast<RenderCopyType>(setSdlProc(SDL_RenderCopyFIndex, reinterpret_cast<void *>(hookRenderCopyF)));
}

static int hookRenderCopyF(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcRect, const SDL_FRect *dstRect)
{
    auto [width, height] = getWindowSize();

    // crude heuristics
    if (!dstRect || dstRect->w >= width || dstRect->h >= height)
        m_menuDrawn = true;

    return m_realRenderCopyF(renderer, texture, srcRect, dstRect);
}
