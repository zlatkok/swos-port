#include "renderSprites.h"
#include "render.h"
#include "gameFieldMapping.h"
#include "sprites.h"
#include "pitch.h"
#include "color.h"
#include "PackedSprite.h"

static void setAlpha(SDL_Texture *texture, int alpha);
static void drawInPlayMatchPitch(float x, float y, float width, float height);
static void drawTacticsPitch(float x, float y, float width, float height);

static void drawMenuSprite(int spriteIndex, int x, int y, bool resetColor, int alpha = 255)
{
    if (resetColor)
        setMenuSpritesColor({ 255, 255, 255 });

    drawSprite(spriteIndex, static_cast<float>(x), static_cast<float>(y), false, 0, 0, true, alpha);
}

void drawMenuSprite(int spriteIndex, int x, int y)
{
    drawMenuSprite(spriteIndex, x, y, true);
}

void drawGameSprite(int spriteIndex, int x, int y)
{
    drawSprite(spriteIndex, static_cast<float>(x), static_cast<float>(y), false, 0, 0);
}

void drawCharSprite(int spriteIndex, int x, int y, int alpha /* = 255 */)
{
    drawMenuSprite(spriteIndex, x, y, false, alpha);
}

// Returns true if the sprite is at least partially on the screen.
bool drawSprite(int imageIndex, float x, float y, bool applyZoom, float xOffset, float yOffset,
    bool ignoreCenter /* = false */, int alpha /* = 255 */)
{
    assert(alpha >= 0 && alpha <= 255);

    const auto& sprite = getSprite(imageIndex);

    auto scale = getGameScale();

    x = x + sprite.xOffsetF - xOffset;
    if (!ignoreCenter)
        x -= sprite.centerXF;
    auto xDest = getGameScreenOffsetX() + x * scale;

    y = y + sprite.yOffsetF - yOffset;
    if (!ignoreCenter)
        y -= sprite.centerYF;
    auto yDest = getGameScreenOffsetY() + y * scale;

    auto destWidth = sprite.widthF * scale;
    auto destHeight = sprite.heightF * scale;

    SDL_FRect dst{ xDest, yDest, destWidth, destHeight };

    auto viewPort = getViewport();
    int width = viewPort.w;
    int height = viewPort.h;

    if (applyZoom) {
        auto zoom = getZoomFactor();
        dst.x -= (dst.x - static_cast<float>(width) / 2) * (1 - zoom);
        dst.y -= (dst.y - static_cast<float>(height) / 2) * (1 - zoom);
        dst.w *= zoom;
        dst.h *= zoom;
    }

    bool onScreen = dst.x < width && dst.y < height && dst.x > -dst.w && dst.y > -dst.h;
    if (imageIndex == kInPlayMatchPitchSprite) {
        drawInPlayMatchPitch(dst.x, dst.y, dst.w, dst.h);
        return onScreen;
    } else if (imageIndex == kTacticsPitchSprite) {
        drawTacticsPitch(dst.x, dst.y, dst.w, dst.h);
        return onScreen;
    }

    auto texture = getTexture(sprite);
    auto renderer = getRenderer();

    setAlpha(texture, alpha);

    if (sprite.rotated) {
        dst.x += dst.h / 2 - dst.w / 2;
        dst.y += dst.w / 2 - dst.h / 2;
        SDL_RenderCopyExF(renderer, texture, &sprite.frame, &dst, -90.0, nullptr, SDL_FLIP_NONE);
    } else {
        SDL_RenderCopyF(renderer, texture, &sprite.frame, &dst);
    }
    return onScreen;
}

struct PitchCanvas {
    SDL_FRect destination;
    float logicalWidth;
    float logicalHeight;
};

static void fillPitchRect(SDL_Renderer *renderer, const PitchCanvas& pitch, float x, float y, float width,
    float height)
{
    SDL_FRect rect{
        pitch.destination.x + x * pitch.destination.w / pitch.logicalWidth,
        pitch.destination.y + y * pitch.destination.h / pitch.logicalHeight,
        width * pitch.destination.w / pitch.logicalWidth,
        height * pitch.destination.h / pitch.logicalHeight,
    };
    SDL_RenderFillRectF(renderer, &rect);
}

static void drawPitchArc(SDL_Renderer *renderer, const PitchCanvas& pitch, float centerX, float centerY,
    float radius, float startAngle, float endAngle, float lineWidth)
{
    constexpr int kSegments = 32;
    constexpr float kPi = 3.14159265358979323846f;

    auto scaleX = pitch.destination.w / pitch.logicalWidth;
    auto scaleY = pitch.destination.h / pitch.logicalHeight;
    auto thickness = std::max(1, static_cast<int>(std::lround(lineWidth * std::min(scaleX, scaleY))));

    for (int line = 0; line < thickness; line++) {
        auto radiusOffset = (line - (thickness - 1) / 2.0f) / std::min(scaleX, scaleY);
        SDL_FPoint points[kSegments + 1];
        for (int i = 0; i <= kSegments; i++) {
            auto angle = (startAngle + (endAngle - startAngle) * i / kSegments) * kPi / 180.0f;
            points[i].x = pitch.destination.x + (centerX + (radius + radiusOffset) * std::cos(angle)) * scaleX;
            points[i].y = pitch.destination.y + (centerY + (radius + radiusOffset) * std::sin(angle)) * scaleY;
        }
        SDL_RenderDrawLinesF(renderer, points, std::size(points));
    }
}

static void drawPitchLine(SDL_Renderer *renderer, const PitchCanvas& pitch,
    float x, float y, float width, float height)
{
    fillPitchRect(renderer, pitch, x, y, std::max(width, 0.75f), std::max(height, 0.75f));
}

static void drawPitchBox(SDL_Renderer *renderer, const PitchCanvas& pitch, float x, float y, float width,
    float height)
{
    drawPitchLine(renderer, pitch, x, y, width, 0.75f);
    drawPitchLine(renderer, pitch, x, y, 0.75f, height);
    drawPitchLine(renderer, pitch, x + width - 0.75f, y, 0.75f, height);
    drawPitchLine(renderer, pitch, x, y + height - 0.75f, width, 0.75f);
}

static void drawInPlayMatchPitch(float x, float y, float width, float height)
{
    auto renderer = getRenderer();
    PitchCanvas pitch{ { x, y, width, height }, 132, 115 };

    SDL_SetRenderDrawColor(renderer, 43, 125, 38, 255);
    SDL_RenderFillRectF(renderer, &pitch.destination);

    // Subtle mowing bands provide texture without storing a full-pitch image.
    SDL_SetRenderDrawColor(renderer, 48, 132, 42, 255);
    for (int band = 0; band < 8; band += 2)
        fillPitchRect(renderer, pitch, 0, band * 115.0f / 8, 132, 115.0f / 8);

    SDL_SetRenderDrawColor(renderer, 240, 239, 195, 255);

    drawPitchBox(renderer, pitch, 1, 1, 130, 113);
    drawPitchLine(renderer, pitch, 1, 57, 130, 0.75f);

    drawPitchBox(renderer, pitch, 25, 1, 82, 18);
    drawPitchBox(renderer, pitch, 48, 1, 36, 6);
    drawPitchBox(renderer, pitch, 25, 96, 82, 18);
    drawPitchBox(renderer, pitch, 48, 109, 36, 5);

    drawPitchArc(renderer, pitch, 66, 57.5f, 16, 0, 360, 0.75f);
    // The endpoints are the intersections between each 13-unit arc and the
    // penalty-area line, seven units away from its penalty spot.
    drawPitchArc(renderer, pitch, 66, 12, 13, 32.58f, 147.42f, 0.75f);
    drawPitchArc(renderer, pitch, 66, 103, 13, 212.58f, 327.42f, 0.75f);

    drawPitchArc(renderer, pitch, 1, 1, 3, 0, 90, 0.75f);
    drawPitchArc(renderer, pitch, 131, 1, 3, 90, 180, 0.75f);
    drawPitchArc(renderer, pitch, 1, 114, 3, 270, 360, 0.75f);
    drawPitchArc(renderer, pitch, 131, 114, 3, 180, 270, 0.75f);

    fillPitchRect(renderer, pitch, 65.5f, 57, 1, 1);
    fillPitchRect(renderer, pitch, 65.5f, 12, 1, 1);
    fillPitchRect(renderer, pitch, 65.5f, 102, 1, 1);
}

static void drawTacticsPitch(float x, float y, float width, float height)
{
    auto renderer = getRenderer();
    PitchCanvas pitch{ { x, y, width, height }, 130, 200 };

    SDL_SetRenderDrawColor(renderer, 36, 144, 0, 255);
    SDL_RenderFillRectF(renderer, &pitch.destination);

    SDL_SetRenderDrawColor(renderer, 41, 151, 4, 255);
    for (int band = 0; band < 8; band += 2)
        fillPitchRect(renderer, pitch, 0, band * 200.0f / 8, 130, 200.0f / 8);

    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);

    drawPitchBox(renderer, pitch, 1, 1, 128, 198);
    drawPitchLine(renderer, pitch, 1, 100, 128, 1);

    drawPitchBox(renderer, pitch, 28, 1, 74, 23);
    drawPitchBox(renderer, pitch, 49, 1, 32, 8);
    drawPitchBox(renderer, pitch, 28, 176, 74, 23);
    drawPitchBox(renderer, pitch, 49, 191, 32, 8);

    drawPitchArc(renderer, pitch, 65, 100, 15, 0, 360, 1);
    // Each arc intersects its penalty-area line four units from the arc center:
    // asin(4 / 12) = 19.47 degrees.
    drawPitchArc(renderer, pitch, 65, 20, 12, 19.47f, 160.53f, 1);
    drawPitchArc(renderer, pitch, 65, 180, 12, 199.47f, 340.53f, 1);

    drawPitchArc(renderer, pitch, 1, 1, 4, 0, 90, 1);
    drawPitchArc(renderer, pitch, 129, 1, 4, 90, 180, 1);
    drawPitchArc(renderer, pitch, 1, 199, 4, 270, 360, 1);
    drawPitchArc(renderer, pitch, 129, 199, 4, 180, 270, 1);

    fillPitchRect(renderer, pitch, 64, 99, 2, 2);
}

static void setAlpha(SDL_Texture *texture, int alpha)
{
    Uint8 oldAlpha;
    SDL_GetTextureAlphaMod(texture, &oldAlpha);

    if (alpha != oldAlpha)
        SDL_SetTextureAlphaMod(texture, alpha);
}
