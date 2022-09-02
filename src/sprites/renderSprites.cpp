#include "renderSprites.h"
#include "render.h"
#include "gameFieldMapping.h"
#include "sprites.h"
#include "pitch.h"
#include "color.h"
#include "PackedSprite.h"

static void setAlpha(SDL_Texture *texture, int alpha);

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
bool drawSprite(int pictureIndex, float x, float y, bool applyZoom, float xOffset, float yOffset,
    bool ignoreCenter /* = false */, int alpha /* = 255 */)
{
    assert(alpha >= 0 && alpha <= 255);

    const auto& sprite = getSprite(pictureIndex);

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

    auto texture = getTexture(sprite);
    auto renderer = getRenderer();

    setAlpha(texture, alpha);

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

    if (sprite.rotated) {
        dst.x += dst.h / 2 - dst.w / 2;
        dst.y += dst.w / 2 - dst.h / 2;
        SDL_RenderCopyExF(renderer, texture, &sprite.frame, &dst, -90.0, nullptr, SDL_FLIP_NONE);
        return dst.x < width && dst.y < height && dst.x > -dst.h && dst.y > -dst.w;
    } else {
        SDL_RenderCopyF(renderer, texture, &sprite.frame, &dst);
        return dst.x < width && dst.y < height && dst.x > -dst.w && dst.y > -dst.h;
    }
}

static void setAlpha(SDL_Texture *texture, int alpha)
{
    Uint8 oldAlpha;
    SDL_GetTextureAlphaMod(texture, &oldAlpha);

    if (alpha != oldAlpha)
        SDL_SetTextureAlphaMod(texture, alpha);
}
