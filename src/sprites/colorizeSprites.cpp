#include "colorizeSprites.h"
#include "variableSprites.h"
#include "render.h"
#include "sprites.h"
#include "file.h"
#include "util.h"
#include "color.h"

static_assert(kPlayerBackground.size() == kNumFaces, "The air is dry");

static FacesArray m_topTeamGoalkeeperFaceToIndex;
static FacesArray m_bottomTeamGoalkeeperFaceToIndex;

using PlayerSurfaces = std::array<SDL_Surface *, kNumFaces>;
using GoalKeeperSurfaces = std::array<SDL_Surface *, 2>;
using GoalkeeperFaces = std::array<int, 2>;

using PlayerTextures = std::array<SDL_Texture *, kNumFaces>;
using GoalKeeperTextures = std::array<SDL_Texture *, 2>;

static PlayerTextures m_topTeamPlayerTextures;
static PlayerTextures m_bottomTeamPlayerTextures;
static GoalKeeperTextures m_topTeamGoalkeeperTextures;
static GoalKeeperTextures m_bottomTeamGoalkeeperTextures;
static SDL_Texture *m_topTeamBenchPlayersTexture;
static SDL_Texture *m_bottomTeamBenchPlayersTexture;

static int m_res;

static const TeamGame *m_topTeam;
static const TeamGame *m_bottomTeam;

static void colorizePlayers();
static void colorizeGoalkeepers();
static void colorizeBenchPlayers();
static bool gotPlayerTextures();
static bool gotGoalkeeperTextures();
static bool gotBenchPlayerTextures();
static PlayerSurfaces createPlayerFaceSurfaces(const FacesArray& faces);
template <size_t N> static SDL_Surface *loadSurface(const std::array<std::array<PackedSprite, N>, kNumAssetResolutions>& sprites);
template <size_t N> static void pastePlayerLayer(SDL_Surface *dstSurface, SDL_Surface *srcSurface,
    const std::array<std::array<PackedSprite, N>, kNumAssetResolutions>& background,
    const std::array<std::array<PackedSprite, N>, kNumAssetResolutions>& sprites);
static void pastePlayerShirtLayer(const TeamGame *team, SDL_Surface *dstSurface, SDL_Surface *srcSurface);
static void convertTextures(SDL_Texture **textures, SDL_Surface **surfaces, int numTextures, bool free = true);
GoalkeeperFaces determineGoalkeeperFaces(const TeamGame *team, FacesArray& faceToGoalkeeper);
static GoalKeeperSurfaces createGoalkeeperSurfaces(const GoalkeeperFaces& faces);
static void freeGoalkeeperTextures(GoalKeeperTextures& textures);
static void freeGoalkeeperSurfaces(GoalKeeperSurfaces& surfaces);

void initSpriteColorizer(int res)
{
    m_res = res;
}

// Do not deallocate, just clear the pointers.
void clearMatchSpriteCache()
{
    struct {
        SDL_Texture **textures;
        size_t numTextures;
    } static const kTextures[] = {
        { m_topTeamPlayerTextures.data(), m_topTeamPlayerTextures.size() },
        { m_bottomTeamPlayerTextures.data(), m_bottomTeamPlayerTextures.size() },
        { m_topTeamGoalkeeperTextures.data(), m_topTeamGoalkeeperTextures.size() },
        { m_bottomTeamGoalkeeperTextures.data(), m_bottomTeamGoalkeeperTextures.size() },
        { &m_topTeamBenchPlayersTexture, 1 },
        { &m_bottomTeamBenchPlayersTexture, 1 },
    };

    for (const auto& texturePack : kTextures)
        for (size_t i = 0; i < texturePack.numTextures; i++)
            texturePack.textures[i] = nullptr;
}

void colorizeGameSprites(int res, const TeamGame *topTeam, const TeamGame *bottomTeam)
{
    m_res = res;
    m_topTeam = topTeam;
    m_bottomTeam = bottomTeam;

    if (!gotPlayerTextures())
        colorizePlayers();
    fillPlayerSprites(&m_topTeamPlayerTextures[0], &m_bottomTeamPlayerTextures[0], m_topTeamPlayerTextures.size(),
        kPlayerBackground[m_res].data(), kPlayerBackground[m_res].size());

    if (!gotGoalkeeperTextures())
        colorizeGoalkeepers();
    fillGoalkeeperSprites(m_topTeamGoalkeeperTextures.data(), m_bottomTeamGoalkeeperTextures.data(),
        m_topTeamGoalkeeperTextures.size(), kGoalkeeperBackground[m_res].data(), kGoalkeeperBackground[m_res].size());

    if (!gotBenchPlayerTextures())
        colorizeBenchPlayers();
    fillBenchSprites(m_topTeamBenchPlayersTexture, m_bottomTeamBenchPlayersTexture,
        kBenchBackground[m_res].data(), kBenchBackground[m_res].size());
}

int getGoalkeeperIndexFromFace(bool topTeam, int face)
{
    assert(face >= 0 && face <= 2);
    return topTeam ? m_topTeamGoalkeeperFaceToIndex[face] : m_bottomTeamGoalkeeperFaceToIndex[face];
}

SDL_Surface *loadSurface(const char *filename)
{
    auto path = joinPaths(getAssetDir(), filename);
    if (auto f = openFile(path.c_str())) {
        auto surface = IMG_Load_RW(f, 1);
        if (!surface)
            errorExit("Failed to load surface file %s: %s", path.c_str(), IMG_GetError());
        return surface;
    } else {
        errorExit("Failed to open surface file %s", path.c_str());
    }

    return nullptr;
}

FacesArray faceTypesInTeam(const TeamGame *team, bool forGame)
{
    FacesArray result{};

    auto allFound = [&result]() {
        return std::all_of(result.begin(), result.end(), [](auto flag) { return flag; });
    };

    for (int i = 0; !allFound() && i < kNumPlayersInTeam; i++) {
        auto face = forGame ? team->players[i].face : team->players[i].face2;
        result[face] = 1;
    }

    return result;
}

void copyShirtPixels(int baseColor, int stripesColor, const PackedSprite& back, const PackedSprite& shirt,
    const SDL_Surface *backSurface, const SDL_Surface *shirtSurface)
{
    assert(back.width == shirt.width && back.height == shirt.height);
    assert(shirt.xOffset < back.frame.w && shirt.yOffset < back.frame.h);
    assert(back.frame.x + shirt.xOffset + shirt.frame.w <= backSurface->w &&
        back.frame.y + shirt.yOffset + shirt.frame.h <= backSurface->h);
    assert(backSurface->pitch % 4 == 0);
    assert(shirtSurface->pitch % 4 == 0);

    auto src = reinterpret_cast<Uint32 *>(shirtSurface->pixels) + shirtSurface->pitch / 4 * shirt.frame.y + shirt.frame.x;
    auto dst = reinterpret_cast<Uint32 *>(backSurface->pixels) + backSurface->pitch / 4 * (back.frame.y + shirt.yOffset) + back.frame.x + shirt.xOffset;

    for (int y = 0; y < shirt.frame.h; y++) {
        for (int x = 0; x < shirt.frame.w; x++) {
            Uint8 rComponent, gComponent, bComponent, alpha;
            auto pixel = src[x];
            SDL_GetRGBA(pixel, shirtSurface->format, &rComponent, &gComponent, &bComponent, &alpha);

            if (auto total = rComponent + bComponent) {
                const auto& baseColorRgb = kTeamPalette[baseColor];
                const auto& stripesColorRgb = kTeamPalette[stripesColor];

                Uint32 r = (baseColorRgb.r * rComponent + stripesColorRgb.r * bComponent + total / 2) / total;
                Uint32 g = (baseColorRgb.g * rComponent + stripesColorRgb.g * bComponent + total / 2) / total;
                Uint32 b = (baseColorRgb.b * rComponent + stripesColorRgb.b * bComponent + total / 2) / total;

                dst[x] = SDL_MapRGBA(backSurface->format, r, g, b, alpha);
            }
        }

        dst += backSurface->pitch / 4;
        src += shirtSurface->pitch / 4;
    }
}

static void colorizePlayers()
{
    assert(kPlayerBackground[m_res].size() == kPlayerSkin[m_res].size());

    auto kTeamData = {
        std::make_pair(m_topTeam, std::ref(m_topTeamPlayerTextures)),
        std::make_pair(m_bottomTeam, std::ref(m_bottomTeamPlayerTextures)),
    };

    for (auto& teamData : kTeamData) {
        auto team = teamData.first;
        auto& textures = teamData.second;

        auto faces = faceTypesInTeam(teamData.first, true);
        auto surfaces = createPlayerFaceSurfaces(faces);

        for (int i = 0; i < kNumFaces; i++) {
            if (faces[i]) {
                const std::pair<const Color&, const decltype(kPlayerSkin)&> kLayers[] = {
                    { kSkinColor[i], kPlayerSkin },
                    { kHairColor[i], kPlayerHair },
                    { kGamePalette[team->prShortsCol], kPlayerShorts },
                    { kGamePalette[team->prSocksCol], kPlayerSocks },
                };

                for (const auto& layer : kLayers) {
                    auto layerSurface = loadSurface(layer.second);
                    const auto& color = layer.first;
                    SDL_SetSurfaceColorMod(layerSurface, color.r, color.g, color.b);
                    pastePlayerLayer(surfaces[i], layerSurface, kPlayerBackground, layer.second);
                    SDL_FreeSurface(layerSurface);
                }

                auto shirtSurface = loadSurface(kPlayerShirt);
                pastePlayerShirtLayer(team, surfaces[i], shirtSurface);
                SDL_FreeSurface(shirtSurface);
            }
        }
        convertTextures(textures.data(), surfaces.data(), textures.size());
    }
}

static void colorizeGoalkeepers()
{
    auto topTeamFaces = determineGoalkeeperFaces(m_topTeam, m_topTeamGoalkeeperFaceToIndex);
    auto bottomTeamFaces = determineGoalkeeperFaces(m_bottomTeam, m_bottomTeamGoalkeeperFaceToIndex);

    assert(topTeamFaces[0] != -1 && bottomTeamFaces[0] != -1);

    const auto kTeamData = { std::make_tuple(m_topTeam, std::cref(topTeamFaces), std::ref(m_topTeamGoalkeeperTextures)),
        std::make_tuple(m_bottomTeam, std::cref(bottomTeamFaces), std::ref(m_bottomTeamGoalkeeperTextures)) };

    for (const auto& teamData : kTeamData) {
        auto team = std::get<0>(teamData);
        const auto& faces = std::get<1>(teamData);

        auto surfaces = createGoalkeeperSurfaces(faces);

        for (size_t i = 0; i < faces.size(); i++) {
            if (faces[i] < 0)
                continue;

            assert(faces[i] <= 3);

            const std::pair<const Color&, const decltype(kGoalkeeperSkin)&> kLayers[] = {
                { kSkinColor[faces[i]], kGoalkeeperSkin },
                { kHairColor[faces[i]], kGoalkeeperHair },
                { kGamePalette[team->prShortsCol], kGoalkeeperShorts },
                { kGamePalette[team->prSocksCol], kGoalkeeperSocks },
            };

            for (const auto& layer : kLayers) {
                auto layerSurface = loadSurface(layer.second);
                const auto& color = layer.first;
                SDL_SetSurfaceColorMod(layerSurface, color.r, color.g, color.b);
                pastePlayerLayer(surfaces[i], layerSurface, kGoalkeeperBackground, layer.second);
                SDL_FreeSurface(layerSurface);
            }
        }

        auto& textures = std::get<2>(teamData);
        freeGoalkeeperTextures(textures);

        convertTextures(textures.data(), surfaces.data(), textures.size(), false);

        freeGoalkeeperSurfaces(surfaces);
    }
}

void colorizeBenchPlayers()
{
    assert(kBenchBackground[m_res].size() == kBenchShirt[m_res].size());

    auto surface1 = loadSurface(kBenchBackground);
    auto surface2 = SDL_DuplicateSurface(surface1);
    auto shirtSurface = loadSurface(kBenchShirt);

    if (m_topTeamBenchPlayersTexture)
        SDL_DestroyTexture(m_topTeamBenchPlayersTexture);
    if (m_bottomTeamBenchPlayersTexture)
        SDL_DestroyTexture(m_bottomTeamBenchPlayersTexture);

    const auto kTeamData = {
        std::make_tuple(m_topTeam, surface1, &m_topTeamBenchPlayersTexture),
        std::make_tuple(m_bottomTeam, surface2, &m_bottomTeamBenchPlayersTexture),
    };

    for (const auto& teamData : kTeamData) {
        auto team = std::get<0>(teamData);
        auto backSurface = std::get<1>(teamData);
        auto texture = std::get<2>(teamData);

        for (size_t i = 0; i < kBenchBackground[m_res].size(); i++) {
            const auto& back = kBenchBackground[m_res][i];
            const auto& shirt = kBenchShirt[m_res][i];
            copyShirtPixels(team->prShirtCol, team->prStripesCol, back, shirt, backSurface, shirtSurface);
        }

        convertTextures(texture, &backSurface, 1, false);
    }

    SDL_FreeSurface(surface1);
    SDL_FreeSurface(shirtSurface);
}

static bool gotPlayerTextures()
{
    const auto& topTeamFaces = faceTypesInTeam(m_topTeam, true);
    for (size_t i = 0; i < topTeamFaces.size(); i++)
        if (topTeamFaces[i] && !m_topTeamPlayerTextures[i])
            return false;

    const auto& bottomTeamFaces = faceTypesInTeam(m_bottomTeam, true);
    for (size_t i = 0; i < bottomTeamFaces.size(); i++)
        if (bottomTeamFaces[i] && !m_bottomTeamPlayerTextures[i])
            return false;

    return true;
}

static bool gotGoalkeeperTextures()
{
    return m_topTeamGoalkeeperTextures[0] && m_topTeamGoalkeeperTextures[1] &&
        m_bottomTeamGoalkeeperTextures[0] && m_bottomTeamGoalkeeperTextures[1];
}

static bool gotBenchPlayerTextures()
{
    return m_topTeamBenchPlayersTexture && m_bottomTeamBenchPlayersTexture;
}

template <size_t N>
static SDL_Surface *loadSurface(const std::array<std::array<PackedSprite, N>, kNumAssetResolutions>& sprites)
{
    assert(std::all_of(sprites[m_res].begin(), sprites[m_res].end(), [&](const auto& sprite) {
        return sprite.texture == sprites[m_res][0].texture; }));

    int fileIndex = kTextureToFile[m_res][sprites[m_res][0].texture];

    assert(static_cast<size_t>(fileIndex) < kTextureFilenames.size());

    return loadSurface(kTextureFilenames[fileIndex]);
}

static SDL_Surface *loadPlayerBackgroundSurface()
{
    return loadSurface(kPlayerBackground);
}

static SDL_Surface *loadGoalkeeperBackgroundSurface()
{
    return loadSurface(kGoalkeeperBackground);
}

static PlayerSurfaces createPlayerFaceSurfaces(const FacesArray& faces)
{
    PlayerSurfaces surfaces{};

    auto surface = loadPlayerBackgroundSurface();
    bool copy = false;

    for (size_t i = 0; i < faces.size(); i++) {
        if (faces[i]) {
            if (copy) {
                surfaces[i] = SDL_DuplicateSurface(surface);
                if (!surfaces[i])
                    sdlErrorExit("Failed to duplicate player surface");
            } else {
                surfaces[i] = surface;
                copy = true;
            }
        }
    }

    return surfaces;
}

template <size_t N>
static void pastePlayerLayer(SDL_Surface *dstSurface, SDL_Surface *srcSurface,
    const std::array<std::array<PackedSprite, N>, kNumAssetResolutions>& background,
    const std::array<std::array<PackedSprite, N>, kNumAssetResolutions>& sprites)
{
    assert(background[m_res].size() == sprites[m_res].size());

    for (size_t i = 0; i < sprites[m_res].size(); i++) {
        const auto& back = background[m_res][i];
        const auto& skin = sprites[m_res][i];

        assert(!back.rotated && !skin.rotated);

        SDL_Rect src{ skin.frame };
        SDL_Rect dest{ back.frame.x + skin.xOffset, back.frame.y + skin.yOffset, skin.frame.w, skin.frame.h };

        assert(src.x >= 0 && src.y >= 0 && src.x + src.w <= srcSurface->w && src.y + src.h <= srcSurface->h);
        assert(dest.x >= 0 && dest.y >= 0 && dest.x + dest.w <= dstSurface->w && dest.y + dest.h <= dstSurface->h);

        SDL_LowerBlit(srcSurface, &src, dstSurface, &dest);
    }
}

static void pastePlayerShirtLayer(const TeamGame *team, SDL_Surface *backSurface, SDL_Surface *shirtSurface)
{
    assert(!SDL_MUSTLOCK(backSurface) && !SDL_MUSTLOCK(shirtSurface));
    assert(shirtSurface->format->BytesPerPixel == 4 && shirtSurface->format->format == backSurface->format->format);
    assert(3 * kPlayerBackground[m_res].size() == kPlayerShirt[m_res].size());

    int baseColor = team->prShirtCol;
    int stripesColor = team->prStripesCol;
    int shirtOffset = 0;

    switch (team->prShirtType) {
    case kShirtHorizontalStripes:
        shirtOffset = kPlayerShirt[m_res].size() / 3;
        std::swap(baseColor, stripesColor);
        break;
    case kShirtColoredSleeves:
        shirtOffset = 2 * kPlayerShirt[m_res].size() / 3;
        break;
    }

    for (size_t i = 0; i < kPlayerBackground[m_res].size(); i++) {
        const auto& back = kPlayerBackground[m_res][i];
        const auto& shirt = kPlayerShirt[m_res][i + shirtOffset];
        copyShirtPixels(baseColor, stripesColor, back, shirt, backSurface, shirtSurface);
    }
}

static void convertTextures(SDL_Texture **textures, SDL_Surface **surfaces, int numTextures, bool free /* = true */)
{
    for (int i = 0; i < numTextures; i++) {
        if (surfaces[i]) {
            if (free && textures[i])
                SDL_DestroyTexture(textures[i]);
            textures[i] = SDL_CreateTextureFromSurface(getRenderer(), surfaces[i]);
            if (!textures[i])
                sdlErrorExit("Error creating player textures");
            if (free)
                SDL_FreeSurface(surfaces[i]);
        }
    }
}

GoalkeeperFaces determineGoalkeeperFaces(const TeamGame *team, FacesArray& faceToGoalkeeper)
{
    int goalieIndex = 0;

    GoalkeeperFaces faces = { -1, -1 };
    faceToGoalkeeper.fill(0);

    for (int i = 0; i < 16; i++) {
        const auto& player = team->players[i];
        int playerPos = static_cast<int>(player.position);
        if (playerPos >= 0) {
            auto face = player.face;
            bool mainGoalkeeper = playerPos == 0;

            if (face != faces[0] && (mainGoalkeeper || face != faces[1])) {
                assert(face <= 3);
                faceToGoalkeeper[face] = goalieIndex;
                faces[goalieIndex] = face;
                if ((i == 0 || mainGoalkeeper) && ++goalieIndex == 2)
                    break;
            }
        }
    }

    return faces;
}

static GoalKeeperSurfaces createGoalkeeperSurfaces(const GoalkeeperFaces& faces)
{
    GoalKeeperSurfaces surfaces;

    surfaces[0] = loadGoalkeeperBackgroundSurface();
    surfaces[1] = faces[0] == faces[1] ? surfaces[0] : SDL_DuplicateSurface(surfaces[0]);
    if (!surfaces[1])
        sdlErrorExit("Failed to copy goalkeeper surface");

    return surfaces;
}

static void freeGoalkeeperTextures(GoalKeeperTextures& textures)
{
    if (textures[0])
        SDL_DestroyTexture(textures[0]);
    if (textures[1] && textures[1] != textures[0])
        SDL_DestroyTexture(textures[1]);
}

static void freeGoalkeeperSurfaces(GoalKeeperSurfaces& surfaces)
{
    SDL_FreeSurface(surfaces[0]);
    if (surfaces[1] && surfaces[1] != surfaces[0])
        SDL_FreeSurface(surfaces[1]);
}
