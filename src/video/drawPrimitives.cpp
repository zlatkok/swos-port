#include "drawPrimitives.h"
#include "gameFieldMapping.h"
#include "render.h"
#include "color.h"

static void drawStraightLine(int x, int y, int width, int height, const Color& color);

void drawHorizontalLine(int x, int y, int width, const Color& color)
{
    drawStraightLine(x, y, width + 1, 1, color);
}

void drawVerticalLine(int x, int y, int height, const Color& color)
{
    drawStraightLine(x, y, 1, height + 1, color);
}

void drawRectangle(int x, int y, int width, int height, const Color& color)
{
    SDL_SetRenderDrawColor(getRenderer(), color.r, color.g, color.b, 255);
    auto scale = getGameScale();

    auto x1 = x * scale + getGameScreenOffsetX() - scale / 2;
    auto y1 = y * scale + getGameScreenOffsetY();
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
    SDL_RenderFillRectsF(getRenderer(), rects, std::size(rects));
}

static void drawStraightLine(int x, int y, int width, int height, const Color& color)
{
    SDL_SetRenderDrawColor(getRenderer(), color.r, color.g, color.b, 255);
    auto scale = getGameScale();

    auto xF = x * scale + getGameScreenOffsetX() - scale / 2;
    auto yF = y * scale + getGameScreenOffsetY() - scale / 2;
    auto w = width * scale;
    auto h = height * scale;

    SDL_FRect rect{ xF, yF, w, h };
    SDL_RenderFillRectF(getRenderer(), &rect);
}
