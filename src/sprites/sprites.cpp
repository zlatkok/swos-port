#include "sprites.h"
#include "spriteDatabase.h"
#include "gameSprites.h"
#include "colorizeSprites.h"
#include "loadTexture.h"
#include "darkRectangle.h"
#include "camera.h"
#include "gameTime.h"
#include "replays.h"
#include "game.h"
#include "text.h"
#include "file.h"
#include "util.h"
#include "color.h"

static const TeamGame *m_topTeam;
static const TeamGame *m_bottomTeam;

static int m_res;

static Color m_textColor{ 255, 255, 255 };

static void updateLegacySprites();
static void loadTextureFile(int textureIndex, int fileIndex);
static void loadSprites(AssetResolution oldResolution, AssetResolution newResolution);

void initSprites()
{
    m_res = static_cast<int>(getAssetResolution());
    registerAssetResolutionChangeHandler(loadSprites);
    initMenuSprites();
    initSpriteColorizer(m_res);
}

static void initMatchSprites()
{
    colorizeGameSprites(m_res, m_topTeam, m_bottomTeam);
    //CopyAdvertisementsData
    initGameSprites(m_topTeam, m_bottomTeam);
    updateLegacySprites();
}

void initMatchSprites(const TeamGame *topTeam, const TeamGame *bottomTeam)
{
    m_topTeam = topTeam;
    m_bottomTeam = bottomTeam;

    initMatchSprites();
}

static void traverseMenuTextures(std::function<void(int, int)> f)
{
    for (size_t i = 0; i < kTextureToFile[m_res].size(); i++) {
        int textureFileIndex = kTextureToFile[m_res][i];
        if (textureFileIndex >= 0) {
            auto filename = kTextureFilenames[textureFileIndex];
            if (!strncmp(filename, "charset", 7))
                f(i, textureFileIndex);
        }
    }
}

void initMenuSprites()
{
    traverseMenuTextures([](int textureIndex, int fileIndex) {
        loadTextureFile(textureIndex, fileIndex);
    });
}

const PackedSprite& getSprite(int index)
{
    assert(static_cast<size_t>(index) < m_packedSprites[m_res].size());
//fixme
    assert(index==225||
        m_packedSprites[m_res][index].widthF && m_packedSprites[m_res][index].heightF);

    return m_packedSprites[m_res][index];
}

SDL_Texture *getTexture(const PackedSprite& sprite)
{
    assert(static_cast<size_t>(sprite.texture) < m_textures[m_res].size());

    auto& texture = m_textures[m_res][sprite.texture];
    if (!texture)
        loadTextureFile(sprite.texture, kTextureToFile[m_res][sprite.texture]);
    if (!texture)
        errorExit("Can't access sprite from unloaded texture %s", kTextureFilenames[kTextureToFile[m_res][sprite.texture]]);

    return texture;
}

void setMenuSpritesColor(const Color& color)
{
    if (color != m_textColor) {
        traverseMenuTextures([&color](int textureIndex, int) {
            SDL_SetTextureColorMod(m_textures[m_res][textureIndex], color.r, color.g, color.b);
        });
        m_textColor = color;
    }
    if (isMatchRunning())
        SDL_SetRenderDrawColor(getRenderer(), color.r, color.g, color.b, 255);
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
    fillSprites(kTeam1WhitePlayerSpriteStart, kNumBasicTextures, topTeamTextures, bottomTeamTextures, numTextures, sprites, numSprites);
}

void fillGoalkeeperSprites(SDL_Texture **topTeamTextures, SDL_Texture **bottomTeamTextures, int numTextures,
    const PackedSprite *sprites, int numSprites)
{
    fillSprites(kTeam1MainGoalkeeperSpriteStart, kNumBasicTextures + 6, topTeamTextures, bottomTeamTextures, numTextures, sprites, numSprites);
}

void fillBenchSprites(SDL_Texture *topTeamTexture, SDL_Texture *bottomTeamTexture, const PackedSprite *sprites, int numSprites)
{
    fillSprites(kTeam1BenchPlayerSpriteStart, kNumBasicTextures + 10, &topTeamTexture, &bottomTeamTexture, 1, sprites, numSprites);
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
        sprite.width = srcSprite.width;
        sprite.height = srcSprite.height;
        sprite.wquads = sprite.width / 16;
        sprite.nlinesDiv4 = sprite.height / 4;
        sprite.ordinal = i;
        sprite.data.set(nullptr);
    }
}

static void loadTextureFile(int textureIndex, int fileIndex)
{
    assert(static_cast<size_t>(textureIndex) < m_textures[m_res].size());
    assert(static_cast<size_t>(fileIndex) < kTextureFilenames.size());

    auto& texture = m_textures[m_res][textureIndex];

    if (!texture)
        texture = loadTexture(kTextureFilenames[fileIndex]);
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
            initMatchSprites();
    }
}
