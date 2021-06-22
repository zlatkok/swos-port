#include "sprites.h"
#include "spriteDatabase.h"
#include "colorizeSprites.h"
#include "loadTexture.h"
#include "camera.h"
#include "gameTime.h"
#include "replays.h"
#include "game.h"
#include "file.h"
#include "util.h"

constexpr int kZeroSpriteSize = 6;

static const TeamGame *m_topTeam;
static const TeamGame *m_bottomTeam;

static int m_res;
static int m_atlasVgaScale;

static void drawDarkRectangle(const FixedRect& darkRect, int screenWidth, int screenHeight);
static void updateLegacySprites();
static const PackedSprite& getSprite(int index);
static SDL_Texture *getTexture(const PackedSprite& sprite);
static void loadTextureFile(int textureIndex, int fileIndex);
int atlasVgaScale();
static void loadSprites(AssetResolution oldResolution, AssetResolution newResolution);

void initSprites()
{
    m_res = static_cast<int>(getAssetResolution());
    m_atlasVgaScale = atlasVgaScale();
    registerAssetResolutionChangeHandler(loadSprites);
    initMenuSprites();
    initSpriteColorizer(m_res);
}

void initMenuSprites()
{
    for (size_t i = 0; i < kTextureToFile[m_res].size(); i++) {
        int textureFileIndex = kTextureToFile[m_res][i];
        if (textureFileIndex >= 0) {
            auto filename = kTextureFilenames[textureFileIndex];
            if (!strncmp(filename, "charset", 7))
                loadTextureFile(i, textureFileIndex);
        }
    }
}

static void initGameSprites()
{
    colorizeGameSprites(m_res, m_topTeam, m_bottomTeam);
    //ConvertColorsAndTextSprites
    //CopyAdvertisementsData
    //MakeSpritesFromPlayerNames
    initializePlayerSpriteFrameIndices();
    updateLegacySprites();
}

void initGameSprites(const TeamGame *topTeam, const TeamGame *bottomTeam)
{
    m_topTeam = topTeam;
    m_bottomTeam = bottomTeam;

    initGameSprites();
}

static void drawTeamNameSprites(int spriteIndex, FixedPoint x, FixedPoint y)
{
    assert(spriteIndex == kTeam1NameSprite || spriteIndex == kTeam2NameSprite);

    constexpr int k2ndTeamOffset = 1'536;
    constexpr int kNumLines = 11;
    constexpr int kLineLength = 136;

    int fieldWidth = getVisibleFieldWidth();

    auto src = swos.dosMemOfs4fc00h + kVirtualScreenSize;
    if (spriteIndex == kTeam2NameSprite)
        src += k2ndTeamOffset;

    //for (int i = 0; i < kNumLines; i++) {
    //    for (int j = 0; j < kLineLength; j++)
    //        if (src[j])
    //            dest[j] = src[j];
    //
    //    src += kLineLength;
    //    dest += fieldWidth;
    //}
}

static const PackedSprite& drawSprite(int pictureIndex, float x, float y, int screenWidth, int screenHeight)
{
    const auto& sprite = getSprite(pictureIndex);

    auto xScale = getXScale();
    auto yScale = getYScale();

    x = x - sprite.centerXF + sprite.xOffsetF;
    auto xDest = x * xScale;
    y = y - sprite.centerYF + sprite.yOffsetF;
    auto yDest = y * yScale;

    if (sprite.rotated)
        std::swap(xScale, yScale);

    auto destWidth = sprite.widthF * xScale;
    auto destHeight = sprite.heightF * yScale;

    auto texture = getTexture(sprite);
    auto renderer = getRenderer();

    SDL_FRect dst{ xDest, yDest, destWidth, destHeight };

    if (sprite.rotated) {
        dst.x += dst.h / 2 - dst.w / 2;
        dst.y += dst.w / 2 - dst.h / 2;
        SDL_RenderCopyExF(renderer, texture, &sprite.frame, &dst, -90.0, nullptr, SDL_FLIP_NONE);
    } else {
        SDL_RenderCopyF(renderer, texture, &sprite.frame, &dst);
    }

    return sprite;
}

void drawSprite(int spriteIndex, int x, int y)
{
    if (spriteIndex == kTeam1NameSprite || spriteIndex == kTeam2NameSprite) {
        drawTeamNameSprites(spriteIndex, x, y);
        return;
    }

    const auto& sprite = getSprite(spriteIndex);
    auto texture = getTexture(sprite);
//sprite 226

    int screenWidth, screenHeight;
    std::tie(screenWidth, screenHeight) = getWindowSize();

    drawSprite(spriteIndex, static_cast<float>(x), static_cast<float>(y), screenWidth, screenHeight);
}

void drawCharSprite(int spriteIndex, int x, int y, int color)
{
//color
    drawSprite(spriteIndex, x, y);
}

void copySprite(int sourceSpriteIndex, int destSpriteIndex, int xOffset, int yOffset)
{
//    auto srcSprite = getSprite(sourceSpriteIndex);
//    auto dstSprite = getSprite(destSpriteIndex);
//
//    assert(xOffset + srcSprite->width <= dstSprite->width);
//    assert(yOffset + srcSprite->height <= dstSprite->height);
//
//    auto src = srcSprite->data;
//    auto dst = dstSprite->data + yOffset * dstSprite->bytesPerLine() + xOffset / 2;
//
//    for (int i = 0; i < srcSprite->height; i++) {
//        for (int j = 0; j < srcSprite->width; j++) {
//            int index = j / 2;
//            dst[index] = src[index] & 0xf0;
//
//            if (++j >= srcSprite->width)
//                break;
//
//            dst[index] |= src[index] & 0x0f;
//        }
//
//        src += srcSprite->bytesPerLine();
//        dst += dstSprite->bytesPerLine();
//    }
}

std::pair<int, int> getSpriteDimensions(int spriteIndex)
{
    assert(getSprite(0).originalWidth % kZeroSpriteSize == 0 && getSprite(0).originalHeight % kZeroSpriteSize == 0);

    const auto& sprite = getSprite(spriteIndex);
    assert(sprite.originalWidth % m_atlasVgaScale == 0 && sprite.originalHeight % m_atlasVgaScale == 0);

    return { sprite.originalWidth / m_atlasVgaScale, sprite.originalHeight / m_atlasVgaScale };
}

// Integrates given sprites into the sprites array, so they can be later displayed by index.
static void fillSprites(int startSprite, int textureIndex, SDL_Texture **topTeamTextures, SDL_Texture **bottomTeamTextures,
    int numTextures, const PackedSprite *sprites, int numSprites)
{
    for (auto textures : { topTeamTextures, bottomTeamTextures }) {
        for (int i = 0; i < numTextures; i++) {
            auto texture = textures[i];
            m_textures[m_res][textureIndex] = texture;

            if (texture) {
                for (int j = 0; j < numSprites; j++) {
                    auto& sprite = m_packedSprites[m_res][startSprite + j];
                    sprite = sprites[j];
                    sprite.texture = textureIndex;
                }
            }
            textureIndex++;
            startSprite += numSprites;
        }
    }
}

void fillPlayerSprites(SDL_Texture **topTeamTextures, SDL_Texture **bottomTeamTextures, int numTextures,
    const PackedSprite *sprites, int numSprites)
{
    fillSprites(kTeam1PlayerSpriteStart, kNumBasicTextures, topTeamTextures, bottomTeamTextures, numTextures, sprites, numSprites);
}

void fillGoalkeeperSprites(SDL_Texture **topTeamTextures, SDL_Texture **bottomTeamTextures, int numTextures,
    const PackedSprite *sprites, int numSprites)
{
    fillSprites(kTeam1GoalkeeperSpriteStart, kNumBasicTextures + 6, topTeamTextures, bottomTeamTextures, numTextures, sprites, numSprites);
}

void fillBenchSprites(SDL_Texture *topTeamTexture, SDL_Texture *bottomTeamTexture, const PackedSprite *sprites, int numSprites)
{
    fillSprites(kTeam1BenchPlayerSpriteStart, kNumBasicTextures + 10, &topTeamTexture, &bottomTeamTexture, 1, sprites, numSprites);
}

static void growRectangle(FixedRect& rect, float x, float y, int screenWidth, int screenHeight)
{
    constexpr int kGridWidth = 168;
    constexpr int kGridHeight = 32;

    x = x * screenWidth / kVgaWidth;
    y = y * screenHeight / kVgaHeight;

    auto width =  static_cast<float>(kGridWidth) * screenWidth / kVgaWidth;
    auto height = static_cast<float>(kGridHeight) * screenHeight / kVgaHeight;

    if (x < rect.xStart)
        rect.xStart = x;
    if (y < rect.yStart)
        rect.yStart = y;
    if (x + width > rect.xEnd)
        rect.xEnd = x + width;
    if (y + height > rect.yEnd)
        rect.yEnd = y + height;
}

static std::pair<float, float> transformCamera(const Sprite *sprite, float cameraX, float cameraY)
{
    float x = sprite->x;
    float y = sprite->y;
    float z = sprite->z;

    x -= cameraX;
    y -= cameraY + z;

    return { x, y };
}

// SWOS draws darkened rectangles using special sprite which has a fixed width and height. That's why this
// sprite is drawn many times to cover larger areas. The problem is that these areas overlap in the game
// which causes problems since we're drawing it in blended mode (while they simply set high bit in the pixel
// to take advantage of specially constructed palette). The solution is to combine the area into a single
// rect first, and then draw this rect. It has to be done immediately and not at the end since it would
// disrupt z-order.
static void drawDarkRectangles(size_t& i, int screenWidth, int screenHeight, float cameraX, float cameraY)
{
    assert(swos.sortedSprites[i]->pictureIndex == kSquareGridForResultSprite);

    FixedRect darkRectangle;

    for (; i < swos.numSpritesToRender; i++) {
        auto sprite = swos.sortedSprites[i].asPtr();
        if (sprite->pictureIndex != kSquareGridForResultSprite)
            break;

        float x, y;
        std::tie(x, y) = transformCamera(sprite, cameraX, cameraY);

        growRectangle(darkRectangle, x, y, screenWidth, screenHeight);
    }

    assert(darkRectangle.valid());

    drawDarkRectangle(darkRectangle, screenWidth, screenHeight);
}

static void drawGameTime(const Sprite *sprite, const TimeDigitSprites& timeDigitSprites, int screenWidth,
    int screenHeight, float cameraX, float cameraY)
{
    float xOffset = 0;
    auto kDigitWidth = getSprite(kBigTimeDigitSprite0 + 8).width();

    float x, y;
    std::tie(x, y) = transformCamera(sprite, cameraX, cameraY);

    for (size_t i = 0; i < timeDigitSprites.size() && timeDigitSprites[i] >= 0; i++) {

        const auto& spriteImage = getSprite(timeDigitSprites[i]);

        float smallDigitFiller = 0;
        if (spriteImage.width() < kDigitWidth)
            smallDigitFiller = (kDigitWidth - spriteImage.width()) / 2;

        drawSprite(timeDigitSprites[i], x + xOffset + smallDigitFiller, y, screenWidth, screenHeight);

        xOffset += kDigitWidth;
    }

    drawSprite(kTimeSprite8Mins, x + xOffset, y, screenWidth, screenHeight);
}

static bool handleSpecialSprites(const Sprite *sprite, size_t& i, int screenWidth, int screenHeight, float cameraX, float cameraY)
{
    assert(sprite);

    if (sprite->pictureIndex < 0) {
        return true;
    } else if (sprite == &swos.currentTimeSprite && sprite->visible) {
        auto timeDigitSprites = getGameTime();
        drawGameTime(sprite, timeDigitSprites, screenWidth, screenHeight, cameraX, cameraY);
        return true;
    } else if (sprite->pictureIndex == kSquareGridForResultSprite) {
        drawDarkRectangles(i, screenWidth, screenHeight, cameraX, cameraY);
        return true;
    }

    return false;
}

static void drawSprites()
{
    SortSprites();

    int screenWidth, screenHeight;
    std::tie(screenWidth, screenHeight) = getWindowSize();

    auto cameraX = getCameraX();
    auto cameraY = getCameraY();

    for (size_t i = 0; i < swos.numSpritesToRender; i++) {
        auto sprite = swos.sortedSprites[i].asPtr();

        assert(sprite->pictureIndex == -1 || sprite->pictureIndex > kMaxMenuSprite && sprite->pictureIndex < kNumSprites);

        if (handleSpecialSprites(sprite, i, screenWidth, screenHeight, cameraX, cameraY))
            continue;

        float x, y;
        std::tie(x, y) = transformCamera(sprite, cameraX, cameraY);

        const auto& spriteImage = drawSprite(sprite->pictureIndex, x, y, screenWidth, screenHeight);

        // since screen can potentially be huge don't reject any sprites for highlights, just dump them all there
        if (sprite != &swos.big_S_Sprite && sprite->teamNumber)
            saveCoordinatesForHighlights(sprite->pictureIndex, std::lround(x), std::lround(y));

        int iX = std::lround(x);
        int iY = std::lround(y);

        // check if this is correct
        if (iX < 336 && iY < 200 && iX > -spriteImage.originalWidth && iY > -spriteImage.originalHeight)
            sprite->beenDrawn = 1;
        else
            sprite->beenDrawn = 0;
    }
}

void SWOS::DrawSprites()
{
    drawSprites();
}

void SWOS::DrawSpriteInColor()
{
}

static SDL_Renderer *prepareRendererForDarkenedRectangles()
{
    auto renderer = getRenderer();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 127);
    return renderer;
}

static void drawDarkRectangle(const FixedRect& darkRect, int screenWidth, int screenHeight)
{
    assert(darkRect.valid());

    auto renderer = prepareRendererForDarkenedRectangles();

    auto width = darkRect.xEnd - darkRect.xStart;
    auto height = darkRect.yEnd - darkRect.yStart;

    SDL_FRect rect{ darkRect.xStart, darkRect.yStart, width, height };

    SDL_RenderFillRectF(renderer, &rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void darkenRectangles(const SDL_Rect *rects, int numRects)
{
    auto renderer = prepareRendererForDarkenedRectangles();
    SDL_RenderFillRects(renderer, rects, numRects);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void drawStatistics()
{
    static const std::array<SDL_Rect, 10> kDarkRects = {{
        { 80, 19, 160, 16 }, { 16, 39, 288, 16 }, { 16, 57, 288, 16 }, { 16, 75, 288, 16 },
        { 16, 93, 288, 16 }, { 16, 111, 288, 16 }, { 16, 129, 288, 16 }, { 16, 147, 288, 16 },
        { 16, 165, 288, 16 }, { 16, 183, 288, 16 },
    }};

    int width, height;
    std::tie(width, height) = getWindowSize();

    auto rects = kDarkRects;

    for (auto& rect : rects) {
        rect.x = rect.x * width / kVgaWidth;
        rect.y = rect.y * height / kVgaHeight;
        rect.w = (rect.w * width + kVgaWidth / 2) / kVgaWidth;
        rect.h = (rect.h * height + kVgaHeight / 2) / kVgaHeight;
    }

    darkenRectangles(rects.data(), rects.size());
}

void SWOS::DrawStatistics()
{
    drawStatistics();
}

void SWOS::DrawGrid()
{
}

// Since there are still parts of code that refer to the old sprites array, we need to maintain it for a bit longer
// Only update the dimensions without data pointers; we will use the pitch patterns buffer for this sinister purpose
static void updateLegacySprites()
{
    auto sprites = *swos.g_spriteGraphicsPtr;

    for (int i = 0; i < kNumSprites; i++) {
        auto& sprite = sprites[i];
        swos.spritesIndex[i] = &sprite;

        const auto& srcSprite = m_packedSprites[m_res][i];
        sprite.centerX = static_cast<int16_t>(std::lround(srcSprite.centerXF));
        sprite.centerY = static_cast<int16_t>(std::lround(srcSprite.centerYF));
        sprite.width = static_cast<int16_t>(srcSprite.width());
        sprite.height = static_cast<int16_t>(srcSprite.height());
        sprite.wquads = sprite.width / 16;
        sprite.nlinesDiv4 = sprite.height / 4;
        sprite.ordinal = i;
        sprite.data.set(nullptr);
    }
}

static const PackedSprite& getSprite(int index)
{
    assert(static_cast<size_t>(index) < m_packedSprites[m_res].size());
    return m_packedSprites[m_res][index];
}

static SDL_Texture *getTexture(const PackedSprite& sprite)
{
    assert(static_cast<size_t>(sprite.texture) < m_textures[m_res].size());

    auto& texture = m_textures[m_res][sprite.texture];
    if (!texture)
        loadTextureFile(sprite.texture, kTextureToFile[m_res][sprite.texture]);
    if (!texture)
        errorExit("Can't access sprite from unloaded texture %s", kTextureFilenames[kTextureToFile[m_res][sprite.texture]]);

    return texture;
}

static void loadTextureFile(int textureIndex, int fileIndex)
{
    assert(static_cast<size_t>(textureIndex) < m_textures[m_res].size());
    assert(static_cast<size_t>(fileIndex) < kTextureFilenames.size());

    auto& texture = m_textures[m_res][textureIndex];

    if (!texture)
        texture = loadTexture(kTextureFilenames[fileIndex]);
}

int atlasVgaScale()
{
    assert(getSprite(0).originalWidth % kZeroSpriteSize == 0);
    return getSprite(0).originalWidth / kZeroSpriteSize;
}

static void loadSprites(AssetResolution oldResolution, AssetResolution newResolution)
{
    if (oldResolution != AssetResolution::kInvalid) {
        for (auto& texture : m_textures[static_cast<int>(oldResolution)]) {
            if (texture) {
                SDL_DestroyTexture(texture);
                texture = nullptr;
            }
        }
    }

    m_res = static_cast<int>(newResolution);

    if (newResolution != AssetResolution::kInvalid) {
        initMenuSprites();
        if (isMatchRunning())
            initGameSprites();
        m_atlasVgaScale = atlasVgaScale();
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

// These are only used by bench routines, remove when they are converted.
// Notice we assume save sprite background is true since we know where the calls are coming from and
// the headache from dealing with ebp can be avoided.
void SWOS::DrawSpriteInCameraView()
{
//    int spriteIndex = D0.asWord();
//    int x = D1.asWord();
//    int y = D2.asWord();
//
//    const auto sprite = getSprite(spriteIndex);
//    x -= sprite->centerX + swos.g_cameraX;
//    y -= sprite->centerY + swos.g_cameraY;
//
//    drawSprite(spriteIndex, x, y);
}

void SWOS::DrawSprite16Pixels()
{
    drawSprite(D0.asWord(), D1.asWord(), D2.asWord());
}
