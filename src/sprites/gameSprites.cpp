#include "gameSprites.h"
#include "sprites.h"
#include "renderSprites.h"
#include "colorizeSprites.h"
#include "referee.h"
#include "replays.h"
#include "camera.h"

static Sprite m_cornerFlagSpriteTopLeft, m_cornerFlagSpriteTopRight, m_cornerFlagSpriteBottomLeft, m_cornerFlagSpriteBottomRight;

static Sprite * const kAllSprites[] = {
    &swos.ballShadowSprite,
    &swos.ballSprite,
    &swos.goal1TopSprite,
    &swos.goal2BottomSprite,
    &swos.goalie1Sprite,
    &swos.team1Player2Sprite,
    &swos.team1Player3Sprite,
    &swos.team1Player4Sprite,
    &swos.team1Player5Sprite,
    &swos.team1Player6Sprite,
    &swos.team1Player7Sprite,
    &swos.team1Player8Sprite,
    &swos.team1Player9Sprite,
    &swos.team1Player10Sprite,
    &swos.team1Player11Sprite,
    &swos.goalie2Sprite,
    &swos.team2Player2Sprite,
    &swos.team2Player3Sprite,
    &swos.team2Player4Sprite,
    &swos.team2Player5Sprite,
    &swos.team2Player6Sprite,
    &swos.team2Player7Sprite,
    &swos.team2Player8Sprite,
    &swos.team2Player9Sprite,
    &swos.team2Player10Sprite,
    &swos.team2Player11Sprite,
    &swos.team1CurPlayerNumSprite,
    &swos.team2CurPlayerNumSprite,
    &swos.playerMarkSprite,
    &swos.bookedPlayerCardOrNumberSprite,
    refereeSprite(),
    &m_cornerFlagSpriteTopLeft,
    &m_cornerFlagSpriteTopRight,
    &m_cornerFlagSpriteBottomLeft,
    &m_cornerFlagSpriteBottomRight,
    &swos.currentPlayerNameSprite,
};

static std::array<Sprite *, std::size(kAllSprites)> m_sortedSprites;
static int m_numSpritesToRender;

static const TeamGame *m_topTeam;
static const TeamGame *m_bottomTeam;

static void sortDisplaySprites();
static bool shouldZoomSprite(int pictureIndex);

void initGameSprites(const TeamGame *topTeam, const TeamGame *bottomTeam)
{
    m_topTeam = topTeam;
    m_bottomTeam = bottomTeam;

    initializePlayerSpriteFrameIndices();
}

// Prepares display sprites for rendering. Sorts them by y-axis.
void initDisplaySprites()
{
    m_numSpritesToRender = 0;

    for (auto sprite : kAllSprites)
        if (sprite->visible)
            m_sortedSprites[m_numSpritesToRender++] = sprite;

    sortDisplaySprites();
}

void initializePlayerSpriteFrameIndices()
{
    const auto kTeamData = {
        std::make_tuple(swos.team1SpritesTable, m_topTeam, true),
        std::make_tuple(swos.team2SpritesTable, m_bottomTeam, false)
    };

    for (const auto& teamData : kTeamData) {
        auto team = std::get<1>(teamData);
        auto spriteTable = std::get<0>(teamData);
        bool topTeam = std::get<2>(teamData);

        spriteTable[0]->frameOffset = getGoalkeeperSpriteOffset(topTeam, team[0].players[0].face);

        for (size_t i = 1; i < std::size(swos.team1SpritesTable); i++) {
            auto& player = team->players[i];
            auto& sprite = spriteTable[i];

            assert(player.face <= 3);
            sprite->frameOffset = getPlayerSpriteOffsetFromFace(player.face);
        }
    }
}

#ifdef DEBUG
static void verifySprites()
{
    for (const auto& sprite : kAllSprites) {
        auto assertIn = [sprite](int start, int end, bool allowEmpty = false) {
            if (!allowEmpty || sprite->pictureIndex != -1)
                assert(sprite->pictureIndex >= start && sprite->pictureIndex <= end);
        };

        if (sprite == &swos.ballShadowSprite)
            assert(sprite->pictureIndex == kBallShadowSprite || sprite->pictureIndex == -1);
        else if (sprite == &swos.ballSprite)
            assertIn(kBallSprite1, kBallSprite4, true);
        else if (sprite == &swos.goal1TopSprite)
            assert(sprite->pictureIndex == kTopGoalSprite);
        else if (sprite == &swos.goal2BottomSprite)
            assert(sprite->pictureIndex == kBottomGoalSprite);
        else if (sprite == &swos.goalie1Sprite)
            assertIn(kTeam1MainGoalkeeperSpriteStart, kTeam1ReserveGoalkeeperSpriteEnd);
        else if (sprite == &swos.goalie2Sprite)
            assertIn(kTeam2MainGoalkeeperSpriteStart, kTeam2ReserveGoalkeeperSpriteEnd);
        else if (sprite == &swos.team1Player2Sprite || sprite == &swos.team1Player3Sprite ||
            sprite == &swos.team1Player4Sprite || sprite == &swos.team1Player5Sprite ||
            sprite == &swos.team1Player6Sprite || sprite == &swos.team1Player7Sprite ||
            sprite == &swos.team1Player8Sprite || sprite == &swos.team1Player9Sprite ||
            sprite == &swos.team1Player10Sprite || sprite == &swos.team1Player11Sprite)
            assertIn(kTeam1WhitePlayerSpriteStart, kTeam1BlackPlayerSpriteEnd);
        else if (sprite == &swos.team2Player2Sprite || sprite == &swos.team2Player3Sprite ||
            sprite == &swos.team2Player4Sprite || sprite == &swos.team2Player5Sprite ||
            sprite == &swos.team2Player6Sprite || sprite == &swos.team2Player7Sprite ||
            sprite == &swos.team2Player8Sprite || sprite == &swos.team2Player9Sprite ||
            sprite == &swos.team2Player10Sprite || sprite == &swos.team2Player11Sprite)
            assertIn(kTeam2WhitePlayerSpriteStart, kTeam2BlackPlayerSpriteEnd);
        else if (sprite == &swos.team1CurPlayerNumSprite || sprite == &swos.team2CurPlayerNumSprite)
            assertIn(kSmallDigit1, kSmallDigit16, true);
        else if (sprite == &swos.playerMarkSprite)
            assert(sprite->pictureIndex == kPlayerMarkSprite || sprite->pictureIndex == -1);
        else if (sprite == &swos.bookedPlayerCardOrNumberSprite)
            assert(sprite->pictureIndex >= kSmallDigit1 && sprite->pictureIndex <= kSmallDigit16 ||
                sprite->pictureIndex == kRedCardSprite || sprite->pictureIndex == kYellowCardSprite ||
                sprite->pictureIndex == -1);
        else if (sprite == refereeSprite())
            assertIn(kRefereeSpriteStart, kRefereeSpriteEnd, true);
        else if (sprite == &m_cornerFlagSpriteTopLeft || sprite == &m_cornerFlagSpriteTopRight ||
            sprite == &m_cornerFlagSpriteBottomLeft || sprite == &m_cornerFlagSpriteBottomRight)
            assertIn(kCornerFlagSpriteStart, kCornerFlagSpriteEnd);
        else if (sprite == &swos.currentPlayerNameSprite)
            assertIn(kTeam1PlayerNamesStartSprite, kTeam2PlayerNamesEndSprite, true);
    }
}
#endif

// The place where game sprites get drawn to the screen.
void drawSprites(float xOffset, float yOffset)
{
#ifdef DEBUG
    verifySprites();
#endif

    sortDisplaySprites();

    int screenWidth, screenHeight;
    std::tie(screenWidth, screenHeight) = getWindowSize();

    auto cameraX = getCameraX();
    auto cameraY = getCameraY();

    for (int i = 0; i < m_numSpritesToRender; i++) {
        auto sprite = m_sortedSprites[i];

        if (sprite->pictureIndex == -1)
            continue;

        assert(sprite->visible);

        auto x = sprite->x.asFloat() - cameraX;
        auto y = sprite->y.asFloat() - cameraY - sprite->z.asFloat();

        auto zoom = shouldZoomSprite(sprite->pictureIndex);
        sprite->onScreen = drawSprite(sprite->pictureIndex, x, y, zoom, xOffset, yOffset);

        // since screen can potentially be huge don't reject any sprites for highlights, just dump them all there
        if (sprite->teamNumber)
            saveCoordinatesForHighlights(sprite->pictureIndex, x, y);
    }
}

int getGoalkeeperSpriteOffset(bool topTeam, int face)
{
    constexpr int kNumGoalkeeperSprites = kTeam1ReserveGoalkeeperSpriteStart - kTeam1MainGoalkeeperSpriteStart;

    assert(face >= 0 && face <= 3);

    auto goalie = getGoalkeeperIndexFromFace(topTeam, face);
    assert(goalie == 0 || goalie == 1);

    return goalie * kNumGoalkeeperSprites;
}

int getPlayerSpriteOffsetFromFace(int face)
{
    constexpr int kNumPlayerSprites = kTeam1GingerPlayerSpriteStart - kTeam1WhitePlayerSpriteStart;

    assert(face >= 0 && face <= 3);

    return face * kNumPlayerSprites;
}

void updateCornerFlags()
{
    constexpr int kLeftCornerFlagX = 81;
    constexpr int kRightCornerFlagX = 590;
    constexpr int kTopCornerFlagY = 129;
    constexpr int kBottomCornerFlagY = 769;

    static const std::array<Sprite *, 4> kCornerFlagSprites = {
        &m_cornerFlagSpriteTopLeft, &m_cornerFlagSpriteTopRight,
        &m_cornerFlagSpriteBottomLeft, &m_cornerFlagSpriteBottomRight
    };

    static const std::array<std::pair<int, int>, 4> kCornerFlagCoordinates = {{
        { kLeftCornerFlagX, kTopCornerFlagY }, { kRightCornerFlagX, kTopCornerFlagY },
        { kLeftCornerFlagX, kBottomCornerFlagY }, { kRightCornerFlagX, kBottomCornerFlagY },
    }};

    static const std::array<uint8_t, 32> kCornerFlagFrameOffsets = {
        0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3
    };

    for (size_t i = 0; i < kCornerFlagSprites.size(); i++) {
        auto& sprite = kCornerFlagSprites[i];

        sprite->x = kCornerFlagCoordinates[i].first;
        sprite->y = kCornerFlagCoordinates[i].second;

        int frame = (swos.stoppageTimer >> 1) & 0x1f;
        sprite->pictureIndex = kCornerFlagSpriteStart + kCornerFlagFrameOffsets[frame];

        sprite->show();
    }
}

static void sortDisplaySprites()
{
    std::sort(m_sortedSprites.begin(), m_sortedSprites.begin() + m_numSpritesToRender, [](const auto& spr1, const auto& spr2) {
        return spr1->y < spr2->y;
    });
}

static bool shouldZoomSprite(int pictureIndex)
{
    return pictureIndex >= kTeam1WhitePlayerSpriteStart && pictureIndex <= kBottomGoalSprite ||
        pictureIndex >= kRefereeSpriteStart;
}
