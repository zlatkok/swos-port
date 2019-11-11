#include "sprites.h"
#include "render.h"
#include "util.h"

static SpriteGraphics *getSprite(int index)
{
    assert(index >= 0 && index < kEndSprite);
    return spritesIndex[index];
}

SpriteClipper::SpriteClipper(int spriteIndex, int x, int y)
{
    auto sprite = getSprite(spriteIndex);
    init(sprite, x, y);
}

SpriteClipper::SpriteClipper(const SpriteGraphics *sprite, int x, int y)
{
    assert(sprite);
    init(sprite, x, y);
}

void SpriteClipper::init(const SpriteGraphics *sprite, int x, int y)
{
    width = sprite->width;
    height = sprite->height;
    from = sprite->data;
    pitch = sprite->wquads * 8;

    this->x = x;
    this->y = y;
    odd = false;
}

bool SpriteClipper::clip()
{
    int fieldWidth, fieldHeight;
    std::tie(fieldWidth, fieldHeight) = getVisibleFieldSize();

    if (x < 0) {
        width += x;
        from += -x / 2;
        odd = x & 1;
        x = 0;
    } else if (x + width >= fieldWidth) {
        width = fieldWidth - width;
    }

    if (y < 0) {
        height += y;
        from += -y * pitch;
        y = 0;
    } else if (y + height >= fieldHeight) {
        height = fieldHeight - y;
    }

    return x < fieldWidth && width > 0 && y < fieldHeight && height > 0;
}

void drawTeamNameSprites(int spriteIndex, int x, int y)
{
    assert(spriteIndex == kTeam1NameSprite || spriteIndex == kTeam2NameSprite);

    constexpr int k2ndTeamOffset = 1'536;
    constexpr int kNumLines = 11;
    constexpr int kLineLength = 136;

    int fieldWidth = getVisibleFieldWidth();

    auto src = dosMemOfs4fc00h + kVirtualScreenSize;
    if (spriteIndex == kTeam2NameSprite)
        src += k2ndTeamOffset;

    auto dest = vsPtr + fieldWidth * y + x;

    for (int i = 0; i < kNumLines; i++) {
        for (int j = 0; j < kLineLength; j++)
            if (src[j])
                dest[j] = src[j];

        src += kLineLength;
        dest += fieldWidth;
    }
}

static char *drawPixelRow(SpriteClipper& p, char *dest, int spriteDelta, int fieldDelta)
{
    for (auto width = p.width; width > 0; width--) {
        byte pixelPair = *p.from;

        if (int pixel = (pixelPair >> 4) & 0x0f)
            *dest = pixel | deltaColor;
        dest++;

        if (!--width)
            break;

        if (int pixel = pixelPair & 0x0f)
            *dest = pixel | deltaColor;
        dest++;

        p.from++;
    }

    p.from += spriteDelta;
    dest += fieldDelta;

    return dest;
}

// globals used: [to be removed when possible]
//     deltaColor - OR it with every byte to write
void drawSprite(int spriteIndex, int x, int y, bool saveSpritePixelsFlag /* = true */)
{
    // TODO: remove this
    if (spriteIndex == kTeam1NameSprite || spriteIndex == kTeam2NameSprite) {
        drawTeamNameSprites(spriteIndex, x, y);
        return;
    }

    SpriteClipper clipper(spriteIndex, x, y);

    if (clipper.clip())
        drawSpriteUnclipped(clipper, saveSpritePixelsFlag);
}

void drawSpriteUnclipped(SpriteClipper& c, bool saveSpritePixelsFlag /* = true */)
{
    int fieldWidth = getVisibleFieldWidth();

    char *dest = vsPtr + c.y * fieldWidth + c.x;

    if (saveSpritePixelsFlag) {
        D0 = dest;
        D4 = c.width;
        D5 = c.height;
        SAFE_INVOKE(SavePixelsBehindSprite);    // TODO:convert
    }

    auto spriteDelta = c.pitch - c.width / 2 - c.odd;
    auto fieldDelta = fieldWidth - c.width - c.odd;

    if (c.odd) {
        // a sprite was clipped so that it starts on an odd pixel
        while (c.height--) {
            if (int pixel = *c.from & 0x0f)
                *dest = pixel | deltaColor;

            c.from++;
            dest = drawPixelRow(c, dest + 1, spriteDelta, fieldDelta);
        }
    } else {
        while (c.height--)
            dest = drawPixelRow(c, dest, spriteDelta, fieldDelta);
    }
}

// These are only used by bench routines, remove when they are converted.
// Notice we assume save sprite background is true since we know where the calls are coming from and
// the headache from dealing with ebp can be avoided.
void SWOS::DrawSpriteInCameraView()
{
    int spriteIndex = D0.asWord();
    int x = D1.asWord();
    int y = D2.asWord();

    const auto sprite = getSprite(spriteIndex);
    x -= sprite->centerX + g_cameraX;
    y -= sprite->centerY + g_cameraY;

    drawSprite(spriteIndex, x, y);
}

void SWOS::DrawSprite16Pixels()
{
    drawSprite(D0.asWord(), D1.asWord(), D2.asWord());
}

void copySprite(int sourceSpriteIndex, int destSpriteIndex, int xOffset, int yOffset)
{
    auto srcSprite = getSprite(sourceSpriteIndex);
    auto dstSprite = getSprite(destSpriteIndex);

    assert(xOffset + srcSprite->width <= dstSprite->width);
    assert(yOffset + srcSprite->height <= dstSprite->height);

    auto src = srcSprite->data;
    auto dst = dstSprite->data + yOffset * dstSprite->bytesPerLine() + xOffset / 2;

    for (int i = 0; i < srcSprite->height; i++) {
        for (int j = 0; j < srcSprite->width; j++) {
            int index = j / 2;
            dst[index] = src[index] & 0xf0;

            if (++j >= srcSprite->width)
                break;

            dst[index] |= src[index] & 0x0f;
        }

        src += srcSprite->bytesPerLine();
        dst += dstSprite->bytesPerLine();
    }
}

// Copies source sprite into destination sprite with x and y offsets. Destination sprite must be big enough.
// in:
//      D0 = src sprite
//      D1 = x offset in dest sprite
//      D2 = y offset in dest sprite
//      D3 = dest sprite
//
// Note: x offset can only start from even number (rounded down).
//
void SWOS::CopySprite()
{
    copySprite(D0.asWord(), D3.asWord(), D1.asInt16(), D2.asInt16());
}
