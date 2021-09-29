#include "stats.h"
#include "result.h"
#include "replays.h"
#include "darkRectangle.h"
#include "pitchConstants.h"
#include "text.h"

constexpr int kLeftColumn = 80;
constexpr int kMiddleColumn = 160;
constexpr int kRightColumn = 240;

constexpr int kTeamNamesY = 43;
constexpr int kGoalsY = 61;
constexpr int kGoalAttempsY = 97;

constexpr int kLineSpacing = 18;

static bool m_showStats;
static bool m_showingUserRequestedStats;

static bool m_isGoalAttempt;

static void drawStats();
static void drawDarkRectangles();
static std::pair<const TeamStatsData *, const TeamStatsData *> getTeamStatsPointers();
static void drawText(int team1Goals, int team2Goals, const TeamStatsData *leftTeamStats, const TeamStatsData *rightTeamStats);

void initStats()
{
    m_isGoalAttempt = false;
    m_showStats = false;
}

void toggleStats()
{
    if (m_showingUserRequestedStats) {
        hideStats();
    } else {
        m_showStats = true;
        swos.statsTimer = 1;
        hideResult();
    }
}

void hideStats()
{
    m_showStats = false;
    m_showingUserRequestedStats = false;
    swos.statsTimer = 0;
}

bool statsEnqueued()
{
    return m_showStats;
}

bool showingUserRequestedStats()
{
    return m_showingUserRequestedStats;
}

bool showingPostGameStats()
{
    return swos.statsTimer > 0;
}

void updateStatistics()
{
    if (!m_showingUserRequestedStats && !swos.playingPenalties) {
        if (swos.gameStatePl == GameState::kInProgress) {
            if (swos.lastTeamPlayed == &swos.topTeamData || swos.lastTeamPlayed == &swos.bottomTeamData)
                swos.lastTeamPlayed->teamStatsPtr.asAligned()->ballPossession++;

            auto team = &swos.topTeamData;
            auto stats = team->teamStatsPtr.asAligned();
            auto ballDelta = swos.ballSprite.deltaY;

            if (m_isGoalAttempt) {
                if (swos.ballSprite.y >= kPitchCenterY) {
                    team = &swos.bottomTeamData;
                    ballDelta = -ballDelta;
                }
                if (team == swos.lastTeamPlayed || ballDelta >= 0)
                    m_isGoalAttempt = false;
            } else if (swos.ballInGoalkeeperArea) {
                if (swos.ballSprite.y <= kPitchCenterY) {
                    team = &swos.bottomTeamData;
                    ballDelta = -ballDelta;
                    stats = team->teamStatsPtr.asAligned();
                }
                if (ballDelta > 0 && team == swos.lastTeamPlayed && !team->playerHasBall &&
                    swos.strikeDestX >= kGoalAttemptLeft && swos.strikeDestX <= kGoalAttemptRight) {
                    if (swos.strikeDestX >= kGoalLeft && swos.strikeDestX <= kGoalRight)
                        stats->onTarget++;
                    stats->goalAttempts++;
                    m_isGoalAttempt = true;
                }
            }
        } else {
            m_isGoalAttempt = 0;
        }
    }
}

const GameStats getStats()
{
    GameStats stats;

    const TeamStatsData *leftStats, *rightStats;
    std::tie(leftStats, rightStats) = getTeamStatsPointers();

    stats.team1 = *leftStats;
    stats.team2 = *rightStats;

    return stats;
}

void drawStats(int team1Goals, int team2Goals, const GameStats& stats)
{
    drawDarkRectangles();
    drawText(team1Goals, team2Goals, &stats.team1, &stats.team2);
}

// Called once per frame from main loop.
void drawStatsIfNeeded()
{
    if (m_showStats) {
        m_showStats = false;
        m_showingUserRequestedStats = true;
        swos.statsTimer = 1;
    }

    if (m_showingUserRequestedStats || swos.statsTimer > 0)
        drawStats();
}

static void drawStats()
{
    const TeamStatsData *leftStats, *rightStats;
    std::tie(leftStats, rightStats) = getTeamStatsPointers();

    drawDarkRectangles();
    drawText(swos.statsTeam1Goals, swos.statsTeam2Goals, leftStats, rightStats);

    if (!m_showingUserRequestedStats) {
        GameStats stats{ *leftStats, *rightStats };
        saveStatsForHighlights(stats);
    }
}

static void drawDarkRectangles()
{
    static const std::array<SDL_FRect, 10> kDarkRects = {{
        { 80, 19, 160, 16 }, { 16, 39, 288, 16 }, { 16, 57, 288, 16 }, { 16, 75, 288, 16 },
        { 16, 93, 288, 16 }, { 16, 111, 288, 16 }, { 16, 129, 288, 16 }, { 16, 147, 288, 16 },
        { 16, 165, 288, 16 }, { 16, 183, 288, 16 },
    }};

    auto rects = kDarkRects;
    darkenRectangles(rects.data(), rects.size());
}

static std::pair<const TeamStatsData *, const TeamStatsData *> getTeamStatsPointers()
{
    auto leftTeam = &swos.topTeamData;
    auto rightTeam = &swos.bottomTeamData;

    if (leftTeam->inGameTeamPtr.asAligned() != &swos.topTeamIngame)
        std::swap(leftTeam, rightTeam);

    return { leftTeam->teamStatsPtr.asAligned(), rightTeam->teamStatsPtr.asAligned() };
}

static void drawStatsText(int x, int y, const char *text)
{
    drawTextCentered(x, y, text, -1, kWhiteText, true);
}

static void drawStatsNumber(int x, int y, int value, bool appendPercent = false)
{
    char buf[32];
    SDL_itoa(value, buf, 10);
    if (appendPercent)
        strcat(buf, "%");
    drawStatsText(x, y, buf);
}

static void drawTeamNames()
{
    int leftTeamX = kLeftColumn;
    int rightTeamX = kRightColumn;
    if (swos.g_trainingGame) {
        leftTeamX += 5;
        rightTeamX += 5;
    }

    drawStatsText(leftTeamX, kTeamNamesY, swos.topTeamIngame.teamName);
    drawStatsText(rightTeamX, kTeamNamesY, swos.bottomTeamIngame.teamName);
}

static void drawPossession(const TeamStatsData *leftTeamStats, const TeamStatsData *rightTeamStats)
{
    drawStatsText(160, 79, "POSSESSION");

    if (int totalPossession = leftTeamStats->ballPossession + rightTeamStats->ballPossession) {
        int leftTeamPossessionPercentage = 100 * leftTeamStats->ballPossession / totalPossession;
        drawStatsNumber(80, 79, leftTeamPossessionPercentage, true);
        drawStatsNumber(240, 79, 100 - leftTeamPossessionPercentage, true);
    } else {
        drawStatsText(80, 79, "-");
        drawStatsText(240, 79, "-");
    }
}

static void drawText(int team1Goals, int team2Goals, const TeamStatsData *leftTeamStats, const TeamStatsData *rightTeamStats)
{
    drawStatsText(kMiddleColumn, 23, "M A T C H   S T A T S");

    drawTeamNames();

    drawStatsText(160, kGoalsY, "GOALS");
    drawStatsNumber(80, kGoalsY, team1Goals);
    drawStatsNumber(240, kGoalsY, team2Goals);

    drawPossession(leftTeamStats, rightTeamStats);

    struct {
        const char *description;
        int leftValue;
        int rightValue;
    } const kStatsDisplayInfo[] = {
        "GOAL ATTEMPTS", leftTeamStats->goalAttempts, rightTeamStats->goalAttempts,
        "ON TARGET", leftTeamStats->onTarget, rightTeamStats->onTarget,
        "CORNERS WON", leftTeamStats->cornersWon, rightTeamStats->cornersWon,
        "FOULS CONCEEDED", leftTeamStats->foulsConceded, rightTeamStats->foulsConceded,
        "BOOKINGS", leftTeamStats->bookings, rightTeamStats->bookings,
        "SENDINGS OFF", leftTeamStats->sendingsOff, rightTeamStats->sendingsOff,
    };

    int y = kGoalAttempsY;
    for (const auto& statsInfo : kStatsDisplayInfo) {
        drawStatsText(kMiddleColumn, y, statsInfo.description);
        drawStatsNumber(kLeftColumn, y, statsInfo.leftValue);
        drawStatsNumber(kRightColumn, y, statsInfo.rightValue);
        y += kLineSpacing;
    }
}
