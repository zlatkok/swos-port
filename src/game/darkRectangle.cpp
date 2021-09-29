#include "darkRectangle.h"
#include "render.h"
#include "windowManager.h"

static SDL_Renderer *prepareRendererForDarkenedRectangles();

void drawDarkRectangle(const SDL_FRect& rect)
{
    auto renderer = prepareRendererForDarkenedRectangles();
    SDL_RenderFillRectF(renderer, &rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_RenderSetScale(renderer, 1, 1);
}

void darkenRectangles(const SDL_FRect *rects, int numRects)
{
    auto xOffset = getScreenXOffset() / getScale();
    std::vector<SDL_FRect> centeredRects(rects, rects + numRects);
    for (auto& rect : centeredRects)
        rect.x += xOffset;

    auto renderer = prepareRendererForDarkenedRectangles();
    SDL_RenderFillRectsF(renderer, centeredRects.data(), numRects);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_RenderSetScale(renderer, 1, 1);
}

static SDL_Renderer *prepareRendererForDarkenedRectangles()
{
    auto renderer = getRenderer();
    auto scale = getScale();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 127);
    SDL_RenderSetScale(renderer, scale, scale);
    return renderer;
}
