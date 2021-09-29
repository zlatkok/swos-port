#include "stadiumMenu.h"
#include "menuBackground.h"
#include "gameSprites.h"
#include "sprites.h"
#include "colorizeSprites.h"
#include "controls.h"
#include "drawMenu.h"
#include "versusMenu.h"
#include "stadiumSprites.h"
#include "stadium.mnu.h"
#include "color.h"

constexpr Uint32 kMinimumPreMatchScreenLengthMs = 800;

using PlayerTextures = std::array<SDL_Texture *, kNumFaces>;

static const TeamGame *m_team1;
static const TeamGame *m_team2;
static int m_maxSubstitutes;
static std::function<void()> m_callback;

static Uint32 m_startTick;

static bool m_texturesLoaded;
static PlayerTextures m_team1PlayerTextures;
static PlayerTextures m_team2PlayerTextures;
static SDL_Texture *m_goalkeeper1Texture;
static SDL_Texture *m_goalkeeper2Texture;

static void setupTeamNames();
static void setupPlayerSprites();
static void setupPlayerNames();
static void setupTeamPlayerSprites(const FacesArray& playerSprites, int goalkeeperSprite, MenuEntry *entry, const TeamGame *team);
static void insertPauseBeforeTheGame();
static PlayerTextures createPlayerSprites(SDL_Surface *spriteAtlas, const TeamGame *team);
static SDL_Texture *createGoalkeeperSprite(SDL_Surface *spriteAtlas, const TeamGame *team);
static SDL_Surface *createPlayerBackground(int res, SDL_Surface *spriteAtlas);
static void pastePlayerLayers(int face, int res, const TeamGame *team, SDL_Surface *spriteAtlas, SDL_Surface *backSurface);
static void pasteShirtLayer(int res, const TeamGame *team, SDL_Surface *spriteAtlas, SDL_Surface *backSurface);

using namespace StadiumMenu;

void showStadiumMenu(const TeamGame *team1, const TeamGame *team2, int maxSubstitutes, std::function<void()> callback)
{
    assert(team1 && team2);

    m_team1 = team1;
    m_team2 = team2;
    m_maxSubstitutes = maxSubstitutes;
    m_callback = callback;

    showMenu(stadiumMenu);
}

void loadStadiumSprites(const TeamGame *team1, const TeamGame *team2)
{
    auto spriteAtlas = loadSurface(kStadiumSpritesAtlas);

    m_team1PlayerTextures = createPlayerSprites(spriteAtlas, team1);
    m_team2PlayerTextures = createPlayerSprites(spriteAtlas, team2);
    m_goalkeeper1Texture = createGoalkeeperSprite(spriteAtlas, team1);
    m_goalkeeper2Texture = createGoalkeeperSprite(spriteAtlas, team2);

    m_texturesLoaded = true;
}

static void stadiumMenuOnInit()
{
    constexpr char kBackgroundImageBasename[] = "stad";

    setMenuBackgroundImage(kBackgroundImageBasename);

    if (!m_texturesLoaded)
        loadStadiumSprites(m_team1, m_team2);

    setupTeamNames();
    setupPlayerSprites();
    setupPlayerNames();

    menuFadeIn();
    processControlEvents();

    m_startTick = SDL_GetTicks();

    if (m_callback)
        m_callback();

    insertPauseBeforeTheGame();
    processControlEvents();

    menuFadeOut();

    SetExitMenuFlag();
}

static void stadiumMenuOnDestroy()
{
    m_texturesLoaded = false;
}

static void setupTeamNames()
{
    setTeamNameAndColor(leftTeamNameEntry, *m_team1, swos.leftTeamCoachNo, swos.leftTeamPlayerNo);
    setTeamNameAndColor(rightTeamNameEntry, *m_team2, swos.rightTeamCoachNo, swos.rightTeamPlayerNo);
}

static void setupPlayerSprites()
{
    assert(m_texturesLoaded);

    FacesArray team1SpriteIndices{ -1, -1, -1 };
    FacesArray team2SpriteIndices{ -1, -1, -1 };

    int res = static_cast<int>(getAssetResolution());
    auto width = m_stadiumSprites[res][kPlayerBackground].width;
    auto height = m_stadiumSprites[res][kPlayerBackground].height;

    int currentSpriteIndex = 0;

    for (size_t i = 0; i < m_team1PlayerTextures.size(); i++) {
        registerMenuLocalSprite(width, height, m_team1PlayerTextures[i]);
        team1SpriteIndices[i] = currentSpriteIndex++;
    }
    for (size_t i = 0; i < m_team2PlayerTextures.size(); i++) {
        registerMenuLocalSprite(width, height, m_team2PlayerTextures[i]);
        team2SpriteIndices[i] = currentSpriteIndex++;
    }
    registerMenuLocalSprite(width, height, m_goalkeeper1Texture);
    registerMenuLocalSprite(width, height, m_goalkeeper2Texture);

    setupTeamPlayerSprites(team1SpriteIndices, currentSpriteIndex, &leftTeamPlayerSpritesStartEntry, m_team1);
    setupTeamPlayerSprites(team2SpriteIndices, currentSpriteIndex + 1, &rightTeamPlayerSpritesStartEntry, m_team1);
}

static void setupPlayerNames()
{
    static_assert(StadiumMenu::kNumPlayerNames <= sizeofarray(m_team1->players), "Too many names!");

    static const auto kTeamData = {
        std::make_pair(&lefTeamPlayerNamesStartEntry, m_team1),
        std::make_pair(&rightTeamPlayerNamesStartEntry, m_team2),
    };

    for (const auto& teamData : kTeamData) {
        auto entry = teamData.first;
        auto team = teamData.second;
        for (int i = 0; i < StadiumMenu::kNumPlayerNames; i++) {
            const auto& player = team->players[i];
            entry++->copyString(player.fullName);
        }

        // hide extraneous reserves (based on gameMaxSubstitues)
        auto lastPlayer = entry - 1;
        for (int i = 5 - m_maxSubstitutes; i > 0; i--)
            lastPlayer--->hide();
    }
}

static void setupTeamPlayerSprites(const FacesArray& playerSprites, int goalkeeperSprite, MenuEntry *entry, const TeamGame *team)
{
    entry++->setSprite(goalkeeperSprite);

    for (int i = 1; i < kNumPlayerSprites; i++) {
        const auto& player = team->players[i];
        int spriteIndex = playerSprites[player.face2];
        entry++->setSprite(spriteIndex);
    }
}

static void insertPauseBeforeTheGame()
{
    auto diff = SDL_GetTicks() - m_startTick;
    if (diff < kMinimumPreMatchScreenLengthMs) {
        auto pause = kMinimumPreMatchScreenLengthMs - diff;
        logInfo("Pausing at versus menu for %d milliseconds (max: %d)", pause, kMinimumPreMatchScreenLengthMs);
        SDL_Delay(pause);
    }
}

static PlayerTextures createPlayerSprites(SDL_Surface *spriteAtlas, const TeamGame *team)
{
    PlayerTextures result{};

    int res = static_cast<int>(getAssetResolution());
    auto faceTypes = faceTypesInTeam(team, false);
    auto renderer = getRenderer();

    for (size_t i = 0; i < faceTypes.size(); i++) {
        if (faceTypes[i]) {
            auto backSurface = createPlayerBackground(res, spriteAtlas);
            pastePlayerLayers(i, res, team, spriteAtlas, backSurface);
            pasteShirtLayer(res, team, spriteAtlas, backSurface);
            result[i] = SDL_CreateTextureFromSurface(renderer, backSurface);
            if (!result[i])
                sdlErrorExit("Failed to create stadium player texture");
            SDL_FreeSurface(backSurface);
        }
    }

    return result;
}

static SDL_Texture *createGoalkeeperSprite(SDL_Surface *spriteAtlas, const TeamGame *team)
{
    int res = static_cast<int>(getAssetResolution());
    auto renderer = getRenderer();

    auto backSurface = createPlayerBackground(res, spriteAtlas);
    auto face = team->players[0].face2;
    pastePlayerLayers(face, res, team, spriteAtlas, backSurface);

    auto result = SDL_CreateTextureFromSurface(renderer, backSurface);
    if (!result)
        sdlErrorExit("Failed to create stadium goalkeeper texture");
    SDL_FreeSurface(backSurface);

    return result;
}

static SDL_Surface *createPlayerBackground(int res, SDL_Surface *spriteAtlas)
{
    SDL_SetSurfaceColorMod(spriteAtlas, 255, 255, 255);
    auto& back = m_stadiumSprites[res][kPlayerBackground];
    auto backSurface = SDL_CreateRGBSurfaceWithFormat(0, back.width, back.height,
        spriteAtlas->format->BitsPerPixel, spriteAtlas->format->format);
    SDL_Rect backRect = { back.xOffset, back.yOffset, back.frame.w, back.frame.h };
    SDL_LowerBlit(spriteAtlas, const_cast<SDL_Rect *>(&back.frame), backSurface, &backRect);

    return backSurface;
}

static void pastePlayerLayers(int face, int res, const TeamGame *team, SDL_Surface *spriteAtlas, SDL_Surface *backSurface)
{
    struct Layer
    {
        Color color;
        int index;
    } const kPlayerLayers[] = {
        kSkinColor[face], kPlayerSkin,
        kHairColor[face], kPlayerHair,
        kGamePalette[team->prShortsCol], kPlayerShorts,
        kGamePalette[team->prSocksCol], kPlayerSocks,
    };

    for (const auto& layer : kPlayerLayers) {
        SDL_SetSurfaceColorMod(spriteAtlas, layer.color.r, layer.color.g, layer.color.b);
        const auto& src = m_stadiumSprites[res][layer.index];
        SDL_Rect dst{ src.xOffset, src.yOffset, src.frame.w, src.frame.h };
        SDL_LowerBlit(spriteAtlas, const_cast<SDL_Rect *>(&src.frame), backSurface, &dst);
    }
}

static void pasteShirtLayer(int res, const TeamGame *team, SDL_Surface *spriteAtlas, SDL_Surface *backSurface)
{
    static const std::array<int, kNumShirtTypes> kShirtLayers = {
        kPlayerShirtVerticalStripes,
        kPlayerShirtColoredSleeves,
        kPlayerShirtVerticalStripes,
        kPlayerShirtHorizontalStripes,
    };
    auto& back = m_stadiumSprites[res][kPlayerBackground];
    PackedSprite backLayerSprite{ { 0, 0, back.width, back.height }, back.xOffset, back.yOffset, 0, 0, back.width, back.height };
    const auto& shirtRect = m_stadiumSprites[res][kShirtLayers[team->prShirtType]];

    auto shirtColor = team->prShirtCol;
    auto stripesColor = team->prStripesCol;
    if (team->prShirtType == kShirtVerticalStripes)
        std::swap(shirtColor, stripesColor);

    copyShirtPixels(shirtColor, stripesColor, backLayerSprite, shirtRect, backSurface, spriteAtlas);
}
