#include "renderSprites.h"
#include "PackedSprite.h"
#include "sprites.h"
#include "pitch.h"
#include "color.h"

static void drawMenuSprite(int spriteIndex, int x, int y, bool resetColor)
{
    if (resetColor)
        setMenuSpritesColor({ 255, 255, 255 });

    drawSprite(spriteIndex, static_cast<float>(x), static_cast<float>(y), false, 0, 0);
}

void drawMenuSprite(int spriteIndex, int x, int y)
{
    drawMenuSprite(spriteIndex, x, y, true);
}

void drawCharSprite(int spriteIndex, int x, int y)
{
    drawMenuSprite(spriteIndex, x, y, false);
}

// Returns true if the sprite is at least partially on the screen.
bool drawSprite(int pictureIndex, float x, float y, bool applyZoom, float xOffset, float yOffset)
{
    const auto& sprite = getSprite(pictureIndex);

    auto scale = getScale();

    x = x - sprite.centerXF + sprite.xOffsetF - xOffset;
    auto xDest = getScreenXOffset() + x * scale;
    y = y - sprite.centerYF + sprite.yOffsetF - yOffset;
    auto yDest = getScreenYOffset() + y * scale;

    auto destWidth = sprite.widthF * scale;
    auto destHeight = sprite.heightF * scale;

    auto texture = getTexture(sprite);
    auto renderer = getRenderer();

    SDL_FRect dst{ xDest, yDest, destWidth, destHeight };

    int width, height;
    std::tie(width, height) = getWindowSize();

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
