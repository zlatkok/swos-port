#include "darkRectangle.h"
#include "render.h"
#include "gameFieldMapping.h"

class ScopedGameScaler
{
public:
    ScopedGameScaler() {
        m_renderer = getRenderer();
        SDL_RenderGetScale(m_renderer, &m_originalScaleX, &m_originalScaleY);
        auto gameScale = getGameScale();
        SDL_RenderSetScale(m_renderer, gameScale, gameScale);
    }
    ~ScopedGameScaler() {
        SDL_RenderSetScale(m_renderer, m_originalScaleX, m_originalScaleY);
    }
private:
    SDL_Renderer *m_renderer;
    float m_originalScaleX;
    float m_originalScaleY;
};

static SDL_Renderer *prepareRendererForDarkenedRectangles();

void drawDarkRectangle(const SDL_FRect& rect, bool offsetX /* = true */)
{
    darkenRectangles(&rect, 1, offsetX);
}

void darkenRectangles(const SDL_FRect *rects, int numRects, bool offsetX /* = true */)
{
    auto gameScale = getGameScale();
    auto xOffset = getGameScreenOffsetX() / gameScale;
    auto yOffset = getGameScreenOffsetY() / gameScale;

    std::vector<SDL_FRect> centeredRects(rects, rects + numRects);
    for (auto& rect : centeredRects) {
        if (offsetX)
            rect.x += xOffset;
        rect.y += yOffset;
    }

    ScopedGameScaler gs;
    auto renderer = prepareRendererForDarkenedRectangles();
    SDL_RenderFillRectsF(renderer, centeredRects.data(), numRects);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static SDL_Renderer *prepareRendererForDarkenedRectangles()
{
    auto renderer = getRenderer();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 127);
    return renderer;
}
