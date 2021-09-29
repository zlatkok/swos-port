#include "drawBench.h"
#include "bench.h"
#include "benchControls.h"
#include "camera.h"
#include "sprites.h"
#include "renderSprites.h"
#include "text.h"
#include "render.h"
#include "color.h"

constexpr int kBenchNotVisibleCameraX = 35;
constexpr int kBenchReservePlayersX = 27;
constexpr int kBenchPlayerHeight = 7;
constexpr int kMaxBenchPlayerIndex = 6;

constexpr int kMenuX = 98;
constexpr int kShadowOffset = 2;

constexpr int kHeaderY = 8;

constexpr int kSubsMenuEntryWidth = 126;
constexpr int kSubsMenuEntryHeight = 11;

constexpr int kFormationEntryWidth = 60;
constexpr int kFormationEntryHeight = 9;

constexpr int kFormationIndex = -1;

static float m_cameraX;
static float m_cameraY;

static float m_xOffset;
static float m_yOffset;

enum MenuType
{
    kSubstitutesMenu, kFormationMenu
};

struct ColorSet
{
    int background;
    int highlight;
    int entryFrame;
};

static ColorSet m_topTeamColors;
static ColorSet m_bottomTeamColors;
static const ColorSet *m_colors;

static void drawBenchPlayerArrow();
static void drawOpponentsBench();
static void drawBenchPlayersAndCoach(BenchState state, int y, const TeamGeneralInfo& team, const TeamGame& teamData, bool drawTrainingTopHalf);
static void drawBenchPlayers(const PlayerGame *players, int reservePlayersFrameTeamOffset, int y, BenchState state, bool drawTrainingTopHalf);
static bool isPlayerStanding(BenchState state, int currentPlayerIndex);
static void drawFormationMenu();
static void drawSubstitutesMenu();
static void drawSubstitutesMenuHeader();
static void drawSubstitutesMenuPlayers();
static void drawSubstitutesMenuEntry(int y, int playerIndex);
static void drawFormationEntry(int i, int y);
static void drawEntryHighlight(int y, int pos);
static void drawLegend(MenuType menuType);
static bool isBenchVisible();
static bool getPlayerArrowCoordinates(int& x, int& y);
static int getCoachY(int y, bool drawTrainingTopHalf);
static int getCoachSpriteIndex(BenchState state, const TeamGeneralInfo& team);
static void determineMenuTeamColors();
static void updateMenuTeamColorsPointer();
static const Color& getSelectedPlayerHighlightColor(int pos);
static void drawBenchSprite(int spriteIndex, int x, int y);
static void drawRectWithShadow(int x, int y, int width, int height, const Color& color);
static void updateCameraCoordinates();

void initBenchMenusBeforeMatch()
{
    determineMenuTeamColors();
}

void drawBench(float xOffset, float yOffset)
{
    if (!isBenchVisible())
        return;

    m_xOffset = xOffset;
    m_yOffset = yOffset;

    updateCameraCoordinates();

    if (!swos.g_trainingGame || !trainingTopTeam())
        drawOpponentsBench();

    drawBenchPlayerArrow();
    drawBenchPlayersAndCoach(getBenchState(), getBenchY(), *getBenchTeam(), *getBenchTeamData(), trainingTopTeam());

    if (swos.g_trainingGame && trainingTopTeam())
        drawOpponentsBench();

    switch (getBenchState()) {
    case BenchState::kAboutToSubstitute:
    case BenchState::kMarkingPlayers:
        drawSubstitutesMenu();
        break;
    case BenchState::kFormationMenu:
        drawFormationMenu();
        break;
    }
}

static void drawBenchPlayerArrow()
{
    int x, y;
    if (getPlayerArrowCoordinates(x, y))
        drawBenchSprite(kSubstitutesArrowSprite, x, y);
}

static void drawOpponentsBench()
{
    auto team = getBenchTeam()->opponentsTeam;
    auto teamData = getBenchTeamData() == &swos.topTeamIngame ? &swos.bottomTeamIngame : &swos.topTeamIngame;
    int y = getOpponentBenchY();

    drawBenchPlayersAndCoach(BenchState::kOpponentsBench, y, *team, *teamData, !trainingTopTeam());
}

static void drawBenchPlayersAndCoach(BenchState state, int y, const TeamGeneralInfo& team, const TeamGame& teamData, bool drawTrainingTopHalf)
{
    assert(team.teamNumber == 1 || team.teamNumber == 2);

    int coachFrameTeamOffset = 0;
    int reservePlayersFrameTeamOffset = 0;

    if (team.teamNumber != 1) {
        coachFrameTeamOffset = kCoach2SittingStartSprite - kCoach1SittingStartSprite;
        reservePlayersFrameTeamOffset = kTeam2BenchPlayerSpriteStart - kTeam1BenchPlayerSpriteStart;
    }

    constexpr int kReservePlayersTopOffset = 15;

    y -= kReservePlayersTopOffset;
    if (swos.g_trainingGame)
        y += kBenchPlayerHeight;

    int coachY = getCoachY(y, drawTrainingTopHalf);
    if (coachY > 0) {
        int coachFrame = getCoachSpriteIndex(state, team);
        drawBenchSprite(coachFrame, kBenchReservePlayersX, coachY);
    }

    y += kBenchPlayerHeight;

    auto players = &teamData.players[11];   // player 12 (first reserve)
    if (swos.g_trainingGame && drawTrainingTopHalf) {
        y -= 8 * kBenchPlayerHeight;
        players += 4;   // player 16 (last reserve, will go backwards)
    }

    drawBenchPlayers(players, reservePlayersFrameTeamOffset, y, state, drawTrainingTopHalf);
}

static void drawBenchPlayers(const PlayerGame *players, int reservePlayersFrameTeamOffset, int y, BenchState state, bool drawTrainingTopHalf)
{
    for (int currentPlayerIndex = 1; currentPlayerIndex < kMaxBenchPlayerIndex; currentPlayerIndex++) {
        const auto& player = *players++;
        if (player.canBeSubstituted()) {
            int spriteIndex = reservePlayersFrameTeamOffset;

            bool isGoalkeeper = player.position == PlayerPosition::kGoalkeeper;
            bool firstPlayer = currentPlayerIndex == 1;

            // 5 should be maximum substitutes SWOS supports, but I've kept the test for now
            bool useGoalkeeperSprite = swos.gameMaxSubstitutes <= 5 ? isGoalkeeper : firstPlayer;

            if (isPlayerStanding(state, currentPlayerIndex))
                spriteIndex += useGoalkeeperSprite ? kBenchGoalkeeperStandingWhite : kBenchPlayerStandingWhite;
            else
                spriteIndex += useGoalkeeperSprite ? kBenchGoalkeeperSittingWhite : kBenchPlayerSittingWhite;

            assert(player.face <= 2);
            spriteIndex += player.face;

            drawBenchSprite(spriteIndex, kBenchReservePlayersX, y);
        }

        y += kBenchPlayerHeight;

        // go backwards with players for training game top bench half
        if (swos.g_trainingGame && drawTrainingTopHalf)
            players -= 2;
    }
}

static bool isPlayerStanding(BenchState state, int currentPlayerIndex)
{
    if (state == BenchState::kOpponentsBench)
        return false;

    if (substituteInProgress())
        return playerToEnterGameIndex() - 10 == currentPlayerIndex;
    else
        return state == BenchState::kAboutToSubstitute && getBenchPlayerIndex() == currentPlayerIndex;
}

static void drawSubstitutesMenu()
{
    updateMenuTeamColorsPointer();
    drawSubstitutesMenuHeader();
    drawSubstitutesMenuPlayers();
}

static void drawSubstitutesMenuHeader()
{
    Color headerFrameColor;
    int playerIndex;

    switch (getBenchState()) {
    case BenchState::kMarkingPlayers:
        {
            int colorIndex = playerToBeSubstitutedIndex() >= 0 ? m_colors->background : m_colors->highlight;
            headerFrameColor = kGamePalette[colorIndex];
            playerIndex = kFormationIndex;
        }
        break;
    default:
        headerFrameColor = kGamePalette[m_colors->background];
        playerIndex = playerToEnterGameIndex();
        break;
    }

    drawRectWithShadow(kMenuX, kHeaderY, kSubsMenuEntryWidth, kSubsMenuEntryHeight, headerFrameColor);
    drawSubstitutesMenuEntry(kHeaderY, playerIndex);

    drawLegend(kSubstitutesMenu);
}

static void drawSubstitutesMenuPlayers()
{
    constexpr int kPlayerListY = 60;
    constexpr int kPlayerEntryHeight = 10;

    drawRectWithShadow(kMenuX, kPlayerListY, kSubsMenuEntryWidth, 11 * kPlayerEntryHeight + 1, kGamePalette[m_colors->background]);

    int y = kPlayerListY;
    for (int i = 0; i < 11; i++) {
        drawSubstitutesMenuEntry(y, i);
        y += kPlayerEntryHeight;
    }
}

static void drawFormationMenu()
{
    constexpr int kFormationMenuY = 37;

    updateMenuTeamColorsPointer();
    drawLegend(kFormationMenu);

    drawRectWithShadow(kMenuX, kFormationMenuY, kFormationEntryWidth,
        kNumFormationEntries * (kFormationEntryHeight - 1) + 1, kGamePalette[m_colors->background]);

    int y = kFormationMenuY;
    for (int i = 0; i < kNumFormationEntries; i++) {
        drawFormationEntry(i, y);
        y += kFormationEntryHeight - 1; // overlap so the horizontal frame is 1 pixel thick
    }
}

static bool benchVisibleByX()
{
    return getCameraX() < kBenchNotVisibleCameraX;
}

static void drawPlayerFaceIcon(int y, int playerIndex)
{
    constexpr int kFaceX = kMenuX + kShadowOffset;

    auto teamData = getBenchTeamData();
    if (teamData->markedPlayer != playerIndex || swos.frameCount & 4) {
        assert(playerIndex >= 0 && playerIndex < kNumPlayersInTeam);
        const auto& player = teamData->players[playerIndex];
        static const std::array<int, 3> kPlayerFaces = { kWhiteFaceSprite, kGingerFaceSprite, kBlackFaceSprite };
        int faceSpriteIndex = kPlayerFaces[player.face];
        drawMenuSprite(faceSpriteIndex, kFaceX, y);
    }
}

static void drawShirtNumber(int y, const PlayerGame& player)
{
    constexpr int kNameX = kMenuX + kShadowOffset + 13;

    char shirtNumBuf[16];
    SDL_itoa(player.shirtNumber, shirtNumBuf, 10);
    drawTextCentered(kNameX, y + 1, shirtNumBuf);
}

static void drawCards(int y, int playerIndex, const Sprite *playerSprite)
{
    constexpr int kCardX = kMenuX + kShadowOffset + 9;

    if (playerIndex < 11 && playerSprite->cards) {
        int cardSprite = playerSprite->cards < 0 ? kRedCardSprite : kYellowCardSprite;
        drawMenuSprite(cardSprite, kCardX, y);
    }
}

static void drawName(int y, const PlayerGame& player)
{
    constexpr int kNameX = kMenuX + kShadowOffset + 18;

    auto name = player.shortName;
    drawText(kNameX, y + 1, name);
}

static void drawPosition(int y, const PlayerGame& player)
{
    constexpr int kPositionX = kMenuX + kShadowOffset + 116;

    int pos = static_cast<int>(player.position);
    assert(pos >= 0 && pos <= 8);

    auto posString = swos.playerPositionsStringTable[pos];
    drawTextCentered(kPositionX, y + 1, posString);
}

static void drawInjuryIcon(int y, const PlayerGame& player)
{
    constexpr int kInjuryIconX = kMenuX + kShadowOffset + 101;

    if (auto injuries = player.injuriesBitfield) {
        int injuryLevel = (injuries & 0xe0) >> 5;
        if (!(injuries & 0x20) || injuryLevel >= (swos.frameCount & 7))
            drawMenuSprite(kRedCrossInjurySprite, kInjuryIconX, y);
    }
}

static void drawSubstitutesMenuEntry(int y, int playerIndex)
{
    assert(playerIndex >= 0 && playerIndex < kNumPlayersInTeam || playerIndex == -1);

    // the original game accesses invalid sprite when showing top entry for bench player
    // in that case use negative value that won't match and become highlighted
    int teamPos = -2;
    // however bench player is 12+ and must not be converted to tactics position (only has 11 slots)
    if (playerIndex >= 0)
        teamPos = playerIndex >= 0 && playerIndex < 11 ? getBenchPlayerPosition(playerIndex) : playerIndex;

    drawEntryHighlight(y, teamPos);

    const auto& frameColor = kGamePalette[m_colors->entryFrame];
    drawRectangle(kMenuX, y, kSubsMenuEntryWidth, kSubsMenuEntryHeight, frameColor);

    y += kShadowOffset;

    if (playerIndex == kFormationIndex) {
        drawTextCentered(kMenuX + kSubsMenuEntryWidth / 2, y + 1, "FORMATION");
    } else {
        drawPlayerFaceIcon(y, teamPos);

        Sprite *playerSprite = teamPos >= 0 && teamPos < 11 ? getBenchTeam()->players[teamPos].asPtr() : nullptr;
        const auto& player = getBenchTeamData()->players[teamPos];

        if (playerSprite && playerSprite->injuryLevel == -2) {
            // never seen value -2 but including it just in case
            constexpr int kBigInjuryX = kMenuX + kShadowOffset + 9;
            drawMenuSprite(kRedCrossInjurySprite, kBigInjuryX, y);
        } else {
            drawShirtNumber(y, player);
            if (playerSprite)
                drawCards(y, playerIndex, playerSprite);
        }

        drawName(y, player);
        drawPosition(y, player);
        drawInjuryIcon(y, player);
    }
}

static void drawFormationEntry(int i, int y)
{
    constexpr int kTextCenterX = kMenuX + 30;

    assert(i >= 0 && i < kNumFormationEntries);

    if (i == getSelectedFormationEntry()) {
        auto renderer = getRenderer();
        const auto& highlightColor = kGamePalette[m_colors->highlight];
        SDL_SetRenderDrawColor(renderer, highlightColor.r, highlightColor.g, highlightColor.b, 255);
        const auto& highlightRect = mapRect(kMenuX, y, kFormationEntryWidth, kFormationEntryHeight);
        SDL_RenderFillRectF(renderer, &highlightRect);
    }

    drawRectangle(kMenuX, y, kFormationEntryWidth, kFormationEntryHeight, kGamePalette[m_colors->entryFrame]);

    static const std::array<const char *, 12> kTacticNames = {{
        "4-4-2", "5-4-1", "4-5-1",
        "5-3-2", "3-5-2", "4-3-3",
        "4-2-4", "3-4-3", "SWEEP",
        "5-2-3", "ATTACK", "DEFEND",
    }};

    auto name = static_cast<size_t>(i) < kTacticNames.size() ? kTacticNames[i] : swos.g_tacticsTable[i]->name;
    drawTextCentered(kTextCenterX, y + 2, name);
}

static void drawEntryHighlight(int y, int pos)
{
    if (pos == playerToBeSubstitutedPos() || getBenchState() == BenchState::kMarkingPlayers && pos == getBenchMenuSelectedPlayer()) {
        auto highlightColor = kGamePalette[m_colors->highlight];
        if (pos == getBenchMenuSelectedPlayer())
            highlightColor = getSelectedPlayerHighlightColor(pos);

        auto renderer = getRenderer();
        SDL_SetRenderDrawColor(renderer, highlightColor.r, highlightColor.g, highlightColor.b, 255);
        const auto& highlightRect = mapRect(kMenuX, y, kSubsMenuEntryWidth, kSubsMenuEntryHeight);
        SDL_RenderFillRectF(renderer, &highlightRect);
    }
}

static void drawLegend(MenuType menuType)
{
    constexpr int kFormationLegendX = 108;
    constexpr int kSubstitutesLegendX = 141;

    constexpr int kFormationLegendY = 0;
    constexpr int kSubstitutesLegendY = 23;

    constexpr int kLegendWidth = 41;
    constexpr int kLegendHeight = 33;

    int spriteIndex = menuType == kSubstitutesMenu ? kSubstitutesLegendSprite : kTacticsLegendSprite;
    const auto& legendSprite = getSprite(spriteIndex);

    assert(legendSprite.width == (legendSprite.rotated ? legendSprite.heightF : legendSprite.widthF));
    assert(kLegendWidth >= legendSprite.width && kLegendHeight >= legendSprite.height);

    int x = menuType == kSubstitutesMenu ? kSubstitutesLegendX : kFormationLegendX;
    int y = menuType == kSubstitutesMenu ? kSubstitutesLegendY : kFormationLegendY;

    drawRectWithShadow(x, y, kLegendWidth, kLegendHeight, kGamePalette[m_colors->highlight]);
    drawRectangle(x, y, kLegendWidth, kLegendHeight, kGamePalette[m_colors->background]);

    int spriteX = x + (kLegendWidth - legendSprite.width) / 2;
    int spriteY = y + (kLegendHeight - legendSprite.height) / 2;
    drawMenuSprite(spriteIndex, spriteX, spriteY);
}

static bool isBenchVisible()
{
    if (!inBench() && (!isCameraLeavingBench() || !benchVisibleByX())) {
        clearCameraLeavingBench();
        return false;
    }

    return benchVisibleByX();
}

static bool getPlayerArrowCoordinates(int& x, int& y)
{
    constexpr int kBenchArrowXOffset = 15;
    constexpr int kBenchArrowTopOffset = 17;

    if (goingToBenchDelay() || substituteInProgress() || getBenchState() != BenchState::kInitial)
        return false;

    x = kBenchArrowXOffset;

    int yOffset = getBenchPlayerIndex() * kBenchPlayerHeight;

    if (swos.g_trainingGame) {
        if (trainingTopTeam())
            yOffset = -yOffset;
        else
            yOffset += 2 * kBenchPlayerHeight;
        yOffset -= kBenchPlayerHeight;
    }

    y = getBenchY() - kBenchArrowTopOffset + yOffset;

    return true;
}

static int getCoachY(int y, bool drawTrainingTopHalf)
{
    if (swos.g_trainingGame) {
        if (drawTrainingTopHalf)
            return -1;
        else {
            if (trainingTopTeam())
                return y - 2 * kBenchPlayerHeight;
        }
    }

    return y;
}

static int getCoachSpriteIndex(BenchState state, const TeamGeneralInfo& team)
{
    int coachFrameTeamOffset = 0;
    if (team.teamNumber != 1)
        coachFrameTeamOffset = kCoach2SittingStartSprite - kCoach1SittingStartSprite;

    int coachBaseFrame = kCoach1SittingStartSprite;
    switch (getBenchState()) {
    case BenchState::kFormationMenu:
    case BenchState::kMarkingPlayers:
        coachBaseFrame = kCoach1StandingStartSprite;
    }

    static const std::array<byte, 16> kOurCoachFrames = { 2, 1, 0, 2, 0, 1, 0, 1, 2, 2, 1, 1, 0, 2, 1, 0 };
    static const std::array<byte, 16> kOpponentCoachFrames = { 2, 1, 2, 2, 1, 2, 0, 1, 2, 0, 1, 1, 1, 0, 1, 2 };

    int index = (swos.stoppageTimer & 0x1e0) >> 5;
    const auto& frames = state != BenchState::kOpponentsBench ? kOurCoachFrames : kOpponentCoachFrames;

    return coachBaseFrame + coachFrameTeamOffset + frames[index];
}

static void determineMenuTeamColors()
{
    const std::array<std::pair<ColorSet&, const TeamGame *>, 2> kTeamColorData = {{
        { std::ref(m_topTeamColors), &swos.topTeamIngame },
        { std::ref(m_bottomTeamColors), &swos.bottomTeamIngame },
    }};

    for (auto& colorData : kTeamColorData) {
        auto& colors = colorData.first;
        auto team = colorData.second;

        const std::array<int, 9> availableColors = {
            team->prShirtCol, team->prStripesCol, team->prShortsCol, team->prSocksCol,
            kGameColorBlack, kGameColorGray, kGameColorBlack, kGameColorGray, -1,
        };

        auto findColor = [&availableColors](auto condition) {
            for (auto color : availableColors)
                if (condition(color))
                    return color;

            return -1;
        };
        auto isBrightColor = [](int color) { return color == kGameColorWhite || color == kGameColorYellow; };

        colors.background = findColor([&](int color) { return !isBrightColor(color); });
        colors.highlight = findColor([&](int color) { return !isBrightColor(color) && color != colors.background; });
        colors.entryFrame = findColor([&colors](int color) { return color != colors.background && color != colors.highlight; });

        assert(colors.background >= 0 && colors.highlight >= 0);

        if (colors.entryFrame < 0)
            colors.entryFrame = colors.highlight;
    }
}

static void updateMenuTeamColorsPointer()
{
    m_colors = getBenchTeam()->teamNumber == 1 ? &m_topTeamColors : &m_bottomTeamColors;

    assert(static_cast<unsigned>(m_colors->background) < 16 &&
        static_cast<unsigned>(m_colors->entryFrame) < 16 &&
        static_cast<unsigned>(m_colors->highlight) < 16);
}

static const Color& getSelectedPlayerHighlightColor(int pos)
{
    int highlightColorIndex = m_colors->highlight;

    if (getBenchState() == BenchState::kMarkingPlayers && pos == getBenchMenuSelectedPlayer()) {
        highlightColorIndex = kGameColorRed;
        if (m_colors->background == kGameColorRed || m_colors->highlight == kGameColorRed) {
            highlightColorIndex = kGameColorBlue;
            if (m_colors->background == kGameColorBlue || m_colors->highlight == kGameColorBlue)
                highlightColorIndex = kGameColorGreen;
        }
    }

    return kGamePalette[highlightColorIndex];
}

static void drawBenchSprite(int spriteIndex, int x, int y)
{
    drawSprite(spriteIndex, x - m_cameraX, y - m_cameraY, true, m_xOffset, m_yOffset);
}

static void drawRectWithShadow(int x, int y, int width, int height, const Color& baseColor)
{
    const auto& shadowColor = kGamePalette[kGameColorGreenishBlack];

    auto renderer = getRenderer();

    SDL_SetRenderDrawColor(renderer, shadowColor.r, shadowColor.g, shadowColor.b, 255);
    const auto& shadowRect = mapRect(x + kShadowOffset, y + kShadowOffset, width, height);
    SDL_RenderFillRectF(renderer, &shadowRect);

    SDL_SetRenderDrawColor(renderer, baseColor.r, baseColor.g, baseColor.b, 255);
    const auto& rect = mapRect(x, y, width, height);
    SDL_RenderFillRectF(renderer, &rect);
}

void updateCameraCoordinates()
{
    m_cameraX = getCameraX();
    m_cameraY = getCameraY();
}
