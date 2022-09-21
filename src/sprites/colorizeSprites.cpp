#include "colorizeSprites.h"
#include "variableSprites.h"
#include "sprites.h"
#include "SharedTexture.h"
#include "file.h"
#include "util.h"
#include "color.h"

static_assert(kPlayerBackground.size() == kNumFaces, "The air is dry");

static FacesArray m_topTeamGoalkeeperFaceToIndex;
static FacesArray m_bottomTeamGoalkeeperFaceToIndex;

using PlayerSurfaces = std::array<SDL_Surface *, kNumFaces>;
using BenchPlayerSurfaces = std::array<SDL_Surface *, 2>;
using GoalkeeperSurfaces = std::array<SDL_Surface *, 2>;
using GoalkeeperFaces = std::array<int, 2>;
using AllTeamsGoalkeeperFaces = std::pair<GoalkeeperFaces, GoalkeeperFaces>;

using PlayerTextures = std::array<SharedTexture, kNumFaces>;
using GoalkeeperFaceTextures = std::array<SharedTexture, kNumFaces>;
using AllTeamsGoalkeeperFaceTextures = std::pair<GoalkeeperFaceTextures, GoalkeeperFaceTextures>;
using GoalkeeperTextures = std::array<SharedTexture, 2>;
using AllTeamsGoalkeeperTextures = std::pair<GoalkeeperTextures, GoalkeeperTextures>;

static int m_res;

struct TeamTextureCacheKey
{
    TeamTextureCacheKey(const TeamGame *team) :
        shirtType(static_cast<byte>(team->prShirtType)), shirtColor(static_cast<byte>(team->prShirtCol)),
        stripesColor(static_cast<byte>(team->prStripesCol)), shortsColor(static_cast<byte>(team->prShortsCol)),
        socksColor(static_cast<byte>(team->prSocksCol)), res(m_res)
    {}
    bool operator==(const TeamTextureCacheKey& other) const {
        return shirtType == other.shirtType && shirtColor == other.shirtColor &&
            stripesColor == other.stripesColor && shortsColor == other.shortsColor &&
            socksColor == other.socksColor && res == other.res;
    }
    byte shirtType;
    byte shirtColor;
    byte stripesColor;
    byte shortsColor;
    byte socksColor;
    int res;
};

struct TeamTextureCacheValue
{
    void reset() {
        for (auto& texture : playerTextures)
            texture.reset();
        for (auto& texture : goalkeeperTextures)
            texture.reset();
        benchTexture.reset();
    }
    PlayerTextures playerTextures;
    SharedTexture benchTexture;
    GoalkeeperFaceTextures goalkeeperTextures;
};

struct TeamTextureCacheItem
{
    TeamTextureCacheItem(const TeamTextureCacheKey& key) : key(key), value{} {}
    TeamTextureCacheKey key;
    TeamTextureCacheValue value;
};

constexpr int kPlayerTextureCacheSize = 6;
std::list<TeamTextureCacheItem> m_playerTextureCache;

static void colorizePlayers(const TeamGame *topTeam, const TeamGame *bottomTeam,
    PlayerTextures& topTeamTextures, PlayerTextures& bottomTeamTextures);
static AllTeamsGoalkeeperFaces colorizeGoalkeepers(const TeamGame *topTeam, const TeamGame *bottomTeam,
    GoalkeeperFaceTextures& topTeamTextures, GoalkeeperFaceTextures& bottomTeamTextures);
static void colorizeBenchPlayers(const TeamGame *topTeam, const TeamGame *bottomTeam,
    SharedTexture *topTeamTexture, SharedTexture *bottomTeamTexture);
static TeamTextureCacheValue *getTeamTextures(const TeamGame *team);
static void trimCache();
static PlayerSurfaces createPlayerFaceSurfaces(const TeamGame *team, const PlayerTextures& textures);
static BenchPlayerSurfaces createBenchPlayerSurfaces(const SharedTexture& topTeamTexture, const SharedTexture& bottomTeamTexture);
static GoalkeeperFaces determineGoalkeeperFaces(const TeamGame *team, FacesArray& faceToGoalkeeper);
static GoalkeeperSurfaces createGoalkeeperSurfaces(const GoalkeeperFaces& faces, const GoalkeeperFaceTextures& textures);
static void freeGoalkeeperSurfaces(GoalkeeperSurfaces& surfaces);
static AllTeamsGoalkeeperTextures getGoalkeeperTextures(const AllTeamsGoalkeeperFaces& faces, const AllTeamsGoalkeeperFaceTextures& textures);
template <size_t N> static SDL_Surface *loadSurface(const std::array<std::array<PackedSprite, N>, kNumAssetResolutions>& sprites);
template <size_t N> static void pastePlayerLayer(SDL_Surface *dstSurface, SDL_Surface *srcSurface,
    const std::array<std::array<PackedSprite, N>, kNumAssetResolutions>& background,
    const std::array<std::array<PackedSprite, N>, kNumAssetResolutions>& sprites);
static void pastePlayerShirtLayer(const TeamGame *team, SDL_Surface *dstSurface, SDL_Surface *srcSurface);
static void convertTextures(SharedTexture *textures, SDL_Surface **surfaces, int numTextures);

void initSpriteColorizer(int res)
{
    m_res = res;
}

void finishSpriteColorizer()
{
    // must clean up before the renderer is destroyed
    for (auto& texture : m_playerTextureCache)
        texture.value.reset();
}

void colorizeGameSprites(int res, const TeamGame *topTeam, const TeamGame *bottomTeam)
{
    m_res = res;

    auto topTeamTextures = getTeamTextures(topTeam);
    auto bottomTeamTextures = getTeamTextures(bottomTeam);

    colorizePlayers(topTeam, bottomTeam, topTeamTextures->playerTextures, bottomTeamTextures->playerTextures);
    fillPlayerSprites(topTeamTextures->playerTextures.data(), bottomTeamTextures->playerTextures.data(),
        topTeamTextures->playerTextures.size(), kPlayerBackground[m_res].data(), kPlayerBackground[m_res].size());

    const auto& goalkeeperFaces = colorizeGoalkeepers(topTeam, bottomTeam,
        topTeamTextures->goalkeeperTextures, bottomTeamTextures->goalkeeperTextures);
    const auto& goalkeeperPerFaceTextures = std::make_pair(topTeamTextures->goalkeeperTextures, bottomTeamTextures->goalkeeperTextures);
    auto goalkeeperTextures = getGoalkeeperTextures(goalkeeperFaces, goalkeeperPerFaceTextures);
    fillGoalkeeperSprites(goalkeeperTextures.first.data(), goalkeeperTextures.second.data(),
        goalkeeperTextures.first.size(), kGoalkeeperBackground[m_res].data(), kGoalkeeperBackground[m_res].size());

    colorizeBenchPlayers(topTeam, bottomTeam, &topTeamTextures->benchTexture, &bottomTeamTextures->benchTexture);
    fillBenchSprites(&topTeamTextures->benchTexture, &bottomTeamTextures->benchTexture,
        kBenchBackground[m_res].data(), kBenchBackground[m_res].size());
}

int getGoalkeeperIndexFromFace(bool topTeam, int face)
{
    assert(face >= 0 && face < kNumFaces);
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

static void colorizePlayers(const TeamGame *topTeam, const TeamGame *bottomTeam,
    PlayerTextures& topTeamTextures, PlayerTextures& bottomTeamTextures)
{
    assert(kPlayerBackground[m_res].size() == kPlayerSkin[m_res].size());

    auto teamsData = {
        std::make_pair(topTeam, std::ref(topTeamTextures)),
        std::make_pair(bottomTeam, std::ref(bottomTeamTextures)),
    };

    for (auto& teamData : teamsData) {
        auto team = teamData.first;
        auto& textures = teamData.second;

        auto surfaces = createPlayerFaceSurfaces(team, textures);

        for (size_t i = 0; i < surfaces.size(); i++) {
            if (surfaces[i]) {
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

static AllTeamsGoalkeeperFaces colorizeGoalkeepers(const TeamGame *topTeam, const TeamGame *bottomTeam,
    GoalkeeperFaceTextures& topTeamTextures, GoalkeeperFaceTextures& bottomTeamTextures)
{
    auto topTeamFaces = determineGoalkeeperFaces(topTeam, m_topTeamGoalkeeperFaceToIndex);
    auto bottomTeamFaces = determineGoalkeeperFaces(bottomTeam, m_bottomTeamGoalkeeperFaceToIndex);

    assert(topTeamFaces[0] != -1 && bottomTeamFaces[0] != -1);

    const auto kTeamData = {
        std::make_tuple(topTeam, std::cref(topTeamFaces), std::ref(topTeamTextures)),
        std::make_tuple(bottomTeam, std::cref(bottomTeamFaces), std::ref(bottomTeamTextures))
    };

    for (const auto& teamData : kTeamData) {
        auto team = std::get<0>(teamData);
        const auto& faces = std::get<1>(teamData);
        auto& textures = std::get<2>(teamData);

        auto surfaces = createGoalkeeperSurfaces(faces, textures);

        for (size_t i = 0; i < faces.size(); i++) {
            if (faces[i] < 0 || textures[faces[i]])
                continue;

            assert(faces[i] <= kNumFaces);

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

            convertTextures(&textures[faces[i]], &surfaces[i], 1);
        }
        freeGoalkeeperSurfaces(surfaces);
    }

    return { topTeamFaces, bottomTeamFaces };
}

void colorizeBenchPlayers(const TeamGame *topTeam, const TeamGame *bottomTeam,
    SharedTexture *topTeamTexture, SharedTexture *bottomTeamTexture)
{
    assert(kBenchBackground[m_res].size() == kBenchShirt[m_res].size());

    if (*topTeamTexture && *bottomTeamTexture)
        return;

    auto shirtSurface = loadSurface(kBenchShirt);
    auto surfaces = createBenchPlayerSurfaces(*topTeamTexture, *bottomTeamTexture);

    const auto teamsData = {
        std::make_tuple(topTeam, surfaces[0], topTeamTexture),
        std::make_tuple(bottomTeam, surfaces[1], bottomTeamTexture),
    };

    for (const auto& teamData : teamsData) {
        auto team = std::get<0>(teamData);
        auto backSurface = std::get<1>(teamData);
        auto texture = std::get<2>(teamData);

        for (size_t i = 0; i < kBenchBackground[m_res].size(); i++) {
            const auto& back = kBenchBackground[m_res][i];
            const auto& shirt = kBenchShirt[m_res][i];
            copyShirtPixels(team->prShirtCol, team->prStripesCol, back, shirt, backSurface, shirtSurface);
        }

        convertTextures(texture, &backSurface, 1);
    }

    for (auto surface : surfaces)
        SDL_FreeSurface(surface);
    SDL_FreeSurface(shirtSurface);
}

static TeamTextureCacheValue *getTeamTextures(const TeamGame *team)
{
    TeamTextureCacheKey key(team);
    auto it = std::find_if(m_playerTextureCache.begin(), m_playerTextureCache.end(), [key](const auto& item) {
        return item.key == key;
    });

    if (it == m_playerTextureCache.end()) {
        m_playerTextureCache.emplace_back(key);
        if (m_playerTextureCache.size() > kPlayerTextureCacheSize)
            m_playerTextureCache.pop_front();
    } else {
        m_playerTextureCache.splice(m_playerTextureCache.end(), m_playerTextureCache, it);
    }

    it = std::prev(m_playerTextureCache.end());
    return &it->value;
}

static void trimCache()
{
    // leave only two current teams, drop everything else
    while (m_playerTextureCache.size() > 2)
        m_playerTextureCache.pop_front();
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

static PlayerSurfaces createPlayerFaceSurfaces(const TeamGame *team, const PlayerTextures& textures)
{
    auto faces = faceTypesInTeam(team, true);

    assert(faces.size() == textures.size());

    PlayerSurfaces surfaces{};
    SDL_Surface *surface{};

    for (size_t i = 0; i < faces.size(); i++) {
        if (faces[i] && !textures[i]) {
            if (surface) {
                surfaces[i] = SDL_DuplicateSurface(surface);
                if (!surfaces[i])
                    sdlErrorExit("Failed to duplicate player surface");
            } else {
                surfaces[i] = surface = loadPlayerBackgroundSurface();
            }
        }
    }

    return surfaces;
}

static BenchPlayerSurfaces createBenchPlayerSurfaces(const SharedTexture& topTeamTexture, const SharedTexture& bottomTeamTexture)
{
    BenchPlayerSurfaces surfaces{};
    auto surface = loadSurface(kBenchBackground);

    if (!topTeamTexture)
        surfaces[0] = surface;
    if (!bottomTeamTexture) {
        surfaces[1] = surface;
        if (surfaces[0]) {
            surfaces[1] = SDL_DuplicateSurface(surface);
            if (!surfaces[1])
                errorExit("Failed to duplicate bench surface %#x", surface);
        }
    }

    return surfaces;
}

static GoalkeeperSurfaces createGoalkeeperSurfaces(const GoalkeeperFaces& faces, const GoalkeeperFaceTextures& textures)
{
    assert(static_cast<unsigned>(faces[0]) < kNumFaces && faces[1] < kNumFaces);

    GoalkeeperSurfaces surfaces{};

    int neededSurfaces = !textures[faces[0]] + (faces[1] >= 0 && !textures[faces[1]]);
    if (neededSurfaces > 0) {
        surfaces[0] = loadGoalkeeperBackgroundSurface();
        if (neededSurfaces > 1) {
            surfaces[1] = SDL_DuplicateSurface(surfaces[0]);
            if (!surfaces[1])
                sdlErrorExit("Failed to duplicate goalkeeper surface %#x", surfaces[0]);
        }
    }

    return surfaces;
}

static void freeGoalkeeperSurfaces(GoalkeeperSurfaces& surfaces)
{
    SDL_FreeSurface(surfaces[0]);
    if (surfaces[1] && surfaces[1] != surfaces[0])
        SDL_FreeSurface(surfaces[1]);
}

static AllTeamsGoalkeeperTextures getGoalkeeperTextures(const AllTeamsGoalkeeperFaces& faces, const AllTeamsGoalkeeperFaceTextures& textures)
{
    AllTeamsGoalkeeperTextures result;
    result.first[0] = textures.first[faces.first[0]];
    result.first[1] = faces.first[1] >= 0 ? textures.first[faces.first[1]] : result.first[0];
    result.second[0] = textures.second[faces.second[0]];
    result.second[1] = faces.second[1] >= 0 ? textures.second[faces.second[1]] : result.second[0];
    return result;
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

static void convertTextures(SharedTexture *textures, SDL_Surface **surfaces, int numTextures)
{
    for (int i = 0; i < numTextures; i++) {
        if (surfaces[i]) {
            textures[i] = SharedTexture::fromSurface(surfaces[i]);
            if (!textures[i]) {
                // try releasing cached textures, maybe there wasn't enough GPU RAM
                trimCache();

                textures[i] = SharedTexture::fromSurface(surfaces[i]);
                if (!textures[i])
                    sdlErrorExit("Error creating texture from surface %#x", surfaces[i]);
            }
        }
    }
}

// Returns face graphic types of main and reserve goalkeeper in a given team.
// Also fill opposite mapping array which maps face type to goalkeeper index (0 = main, 1 = reserve).
GoalkeeperFaces determineGoalkeeperFaces(const TeamGame *team, FacesArray& faceToGoalkeeper)
{
    int goalieIndex = 0;

    GoalkeeperFaces faces = { -1, -1 };
    faceToGoalkeeper.fill(0);

    for (int i = 0; i < kNumPlayersInTeam; i++) {
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
