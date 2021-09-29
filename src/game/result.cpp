#include "result.h"
#include "gameTime.h"
#include "windowManager.h"
#include "text.h"
#include "renderSprites.h"
#include "darkRectangle.h"

constexpr int kMaxScorersForDisplay = 8;
constexpr int kMaxGoalsPerScorer = 10;

constexpr int kMaxScorerLineWidth = 120;
constexpr int kScorerLineHeight = 7;

constexpr int kCharactersPerLine = 61;  // theoretical maximum (all characters 2 pixels)

constexpr int kResultX = 160;
constexpr int kDashX = 157;
constexpr int kDashY = 185;

constexpr int kLeftMargin = 128;
constexpr int kRightMargin = 192;

constexpr int kBigLeftResultDigitX = 143;
constexpr int kBigResultDigitY = 177;

constexpr int kSmallLeftResultDigitX = 150;
constexpr int kSmallRightResultDigitX = 163;
constexpr int kBigRightResultSecondDigitX = 165;

constexpr int kFirstLineBelowResultY = 199;
constexpr int kTeamNameY = 182;
constexpr int kGridTopY = kTeamNameY - 15;
constexpr int kPeriodEndSpriteY = kGridTopY - 10;

constexpr int kResultBigDigitWidth = 12;
constexpr int kResultSmallDigitWidth = 6;
constexpr int kResultDigit1Offset = 4;

constexpr int kResultAtHalfTimeLength = 275;
constexpr int kResultAtGameBreakLength = 265;

constexpr int kEndOfHalfResult = 30'000;
constexpr int kGameBreakResult = 31'000;
constexpr int kMaxResultTicks = 32'000;
constexpr int kResultTickClamped = 29'000;

struct GoalInfo
{
    GoalType type;
    char time[3];

    void update(GoalType goalType, const std::tuple<int, int, int>& gameTime) {
        type = goalType;
        time[0] = std::get<0>(gameTime) + '0';
        time[1] = std::get<1>(gameTime) + '0';
        time[2] = std::get<2>(gameTime) + '0';
    }
    int storeTime(char *buf) const {
        int i = 0;

        if (time[0] != '0')
            buf[i++] = time[0];
        if (time[1] != '0')
            buf[i++] = time[1];
        buf[i++] = time[2];

        return i;
    }
};

struct ScorerInfo
{
    int shirtNum;
    int numGoals;
    int numLines;
    std::array<GoalInfo, kMaxGoalsPerScorer> goals;
};

static std::array<ScorerInfo, kMaxScorersForDisplay> m_team1Scorers;
static std::array<ScorerInfo, kMaxScorersForDisplay> m_team2Scorers;

using ScorerLines = std::array<std::array<char, kCharactersPerLine>, kMaxScorersForDisplay>;
static ScorerLines m_team1ScorerLines;
static ScorerLines m_team2ScorerLines;

static const char *m_team1Name;
static const char *m_team2Name;
static int m_team1NameLength;

static bool m_showResult;

static void resetResultTimer();
static void drawGrid(int scorerListOffsetY);
static void drawTeamNames(int scorerListOffsetY);
static void drawCurrentResult(int scorerListOffsetY);
static void draw1stLegResult(int scorerListOffsetY);
static void drawScorerList(int scorerListOffsetY);
static void drawHalfAndFullTimeSprites(int scorerListOffsetY);
static int getScorerListOffsetY();
static void updateScorersText(const Sprite& scorer, const TeamGame& team, int teamNum, ScorerInfo& scorerInfo, int currentLine);

void resetResult(const char *team1Name, const char *team2Name)
{
    m_showResult = false;

    for (int i = 0; i < kMaxScorersForDisplay; i++) {
        m_team1Scorers[i].shirtNum = 0;
        m_team2Scorers[i].shirtNum = 0;
        m_team1ScorerLines[i][0] = '\0';
        m_team2ScorerLines[i][0] = '\0';
    }

    m_team1Name = team1Name;
    m_team2Name = team2Name;
    m_team1NameLength = getStringPixelLength(team1Name, true);
}

void updateResult()
{
    if (swos.resultTimer < 0) {
        hideResult();
    } else if (swos.resultTimer > 0) {
        if (swos.resultTimer == kEndOfHalfResult || swos.resultTimer == kGameBreakResult || swos.resultTimer == kMaxResultTicks) {
            resetResultTimer();
            m_showResult = true;
        }

        swos.resultTimer -= swos.timerDifference;
        if (swos.resultTimer <= 0) {
            if (swos.gameState == GameState::kResultOnHalftime || swos.gameState == GameState::kResultAfterTheGame)
                swos.statsTimer = kMaxResultTicks;
            hideResult();
        }
    }
}

void hideResult()
{
    swos.resultTimer = 0;
    m_showResult = false;
}

void drawResult()
{
    if (m_showResult) {
        int scorerListOffsetY = getScorerListOffsetY();
        drawGrid(scorerListOffsetY);
        drawTeamNames(scorerListOffsetY);
        drawCurrentResult(scorerListOffsetY);
        draw1stLegResult(scorerListOffsetY);
        drawScorerList(scorerListOffsetY);
        drawHalfAndFullTimeSprites(scorerListOffsetY);
    }
}

void registerScorer(const Sprite& scorer, int teamNum, GoalType goalType)
{
    assert(swos.topTeamPtr && swos.bottomTeamPtr);

    ScorerInfo *scorers;
    TeamGame *scoringTeam, *concedingTeam;

    if (teamNum == 1) {
        scorers = m_team1Scorers.data();
        scoringTeam = swos.topTeamPtr;
        concedingTeam = swos.bottomTeamPtr;
    } else {
        scorers = m_team2Scorers.data();
        scoringTeam = swos.bottomTeamPtr;
        concedingTeam = swos.topTeamPtr;
    }

    auto& player = scoringTeam->players[scorer.playerOrdinal - 1];
    int shirtNum = player.shirtNumber;

    if (goalType == GoalType::kOwnGoal) {
        std::swap(scoringTeam, concedingTeam);
        concedingTeam->numOwnGoals++;
        shirtNum += 1'000;  // this is to differentiate a player that scored own goal from a guy with the same
                            // number in the opposite team (since own-goals appear under the opponent's goals)
    } else {
        player.goalsScored++;
    }

    int currentSlot = 0;
    int currentLine = 0;

    while (currentSlot < kMaxScorersForDisplay && currentLine < kMaxScorersForDisplay) {
        auto& scorerInfo = scorers[currentSlot];
        if (!scorerInfo.shirtNum || scorerInfo.shirtNum == shirtNum) {
            if (!scorerInfo.shirtNum) {
                scorerInfo.numGoals = 0;
                scorerInfo.numLines = 1;
            }
            scorerInfo.shirtNum = shirtNum;
            if (scorerInfo.numGoals != kMaxGoalsPerScorer) {
                auto& goal = scorerInfo.goals[scorerInfo.numGoals++];
                const auto& gameTime = gameTimeAsBcd();
                goal.update(goalType, gameTime);
                updateScorersText(scorer, *scoringTeam, teamNum, scorerInfo, currentLine);
            }
            break;
        } else {
            currentSlot++;
            currentLine += scorerInfo.numLines;
        }
    }
}

static void resetResultTimer()
{
    assert(swos.resultTimer == kEndOfHalfResult || swos.resultTimer == kGameBreakResult || swos.resultTimer == kMaxResultTicks);

    switch (swos.resultTimer) {
    case kEndOfHalfResult:
        swos.resultTimer = kResultAtHalfTimeLength;
        break;
    case kGameBreakResult:
        swos.resultTimer = kResultAtGameBreakLength;
        break;
    case kMaxResultTicks:
        swos.resultTimer = kResultTickClamped;
        break;
    }
}

static void drawGrid(int scorerListOffsetY)
{
    float width = kVgaWidth + 2 * getScreenXOffset();
    float y = static_cast<float>(kGridTopY + scorerListOffsetY);
    float height = kVgaHeight - y;

    drawDarkRectangle({ 0, y, width, height });
}

static void drawTeamNames(int scorerListOffsetY)
{
    int team1X = kLeftMargin - m_team1NameLength;
    int team2X = kRightMargin;
    int y = kTeamNameY + scorerListOffsetY;

    drawText(team1X, y, m_team1Name, -1, kWhiteText, true);
    drawText(team2X, y, m_team2Name, -1, kWhiteText, true);
}

static void drawCurrentResult(int scorerListOffsetY)
{
    int x = kBigLeftResultDigitX;
    int y = kBigResultDigitY + scorerListOffsetY;

    drawMenuSprite(kBigZeroSprite + swos.team1GoalsDigit2, x, y);
    if (swos.team1GoalsDigit1)
        drawMenuSprite(kBigZeroSprite + swos.team1GoalsDigit1, x - kResultBigDigitWidth, y);

    drawMenuSprite(kBigDashSprite, kDashX, kDashY + scorerListOffsetY);

    x = kBigRightResultSecondDigitX;

    drawMenuSprite(kBigZeroSprite + swos.team2GoalsDigit2, x, y);
    if (swos.team2GoalsDigit1)
        drawMenuSprite(kBigZeroSprite + swos.team2GoalsDigit1, x + kResultBigDigitWidth, y);
}

static void draw1stLegResult(int scorerListOffsetY)
{
    if (swos.dontShowScorers || swos.secondLeg) {
        int team1GoalsDigit1 = swos.team1TotalGoals / 10;
        int team1GoalsDigit2 = swos.team1TotalGoals % 10;

        int x = kSmallLeftResultDigitX;
        int y = kFirstLineBelowResultY + scorerListOffsetY;

        if (team1GoalsDigit2 == 1)
            x += kResultDigit1Offset;
        drawMenuSprite(kSmallZeroSprite + team1GoalsDigit2, x, y);

        if (team1GoalsDigit1) {
            x -= kResultSmallDigitWidth;
            if (team1GoalsDigit1 == 1)
                x += kResultDigit1Offset;
            drawMenuSprite(kSmallZeroSprite + team1GoalsDigit1, x, y);
        }

        int team2GoalsDigit1 = swos.team2TotalGoals / 10;
        int team2GoalsDigit2 = swos.team2TotalGoals % 10;

        x = kSmallRightResultDigitX;
        drawMenuSprite(kSmallZeroSprite + team2GoalsDigit2, x, y);

        if (team2GoalsDigit1) {
            x += kResultSmallDigitWidth;
            if (team2GoalsDigit1 == 1)
                x -= kResultDigit1Offset;
            drawMenuSprite(kSmallZeroSprite + team2GoalsDigit1, x, y);
        }
    }
}

static void drawScorerList(int scorerListOffsetY)
{
    if (!swos.dontShowScorers) {
        static const auto kTeamScorerData = {
            std::make_tuple(std::cref(m_team1ScorerLines), kLeftMargin, drawTextRightAligned),
            std::make_tuple(std::cref(m_team2ScorerLines), kRightMargin, drawText),
        };

        for (const auto& scorerData : kTeamScorerData) {
            const auto& lines = std::get<0>(scorerData);
            auto drawTextRoutine = std::get<2>(scorerData);

            int x = std::get<1>(scorerData);
            int y = kFirstLineBelowResultY + scorerListOffsetY;

            for (size_t i = 0; i < kMaxScorersForDisplay; i++) {
                if (lines[i][0]) {
                    drawTextRoutine(x, y, lines[i].data(), -1, kWhiteText, false);
                    y += kScorerLineHeight;
                }
            }
        }
    }
}

static void drawHalfAndFullTimeSprites(int scorerListOffsetY)
{
    if (swos.gameState == GameState::kResultOnHalftime)
        drawMenuSprite(kHalftimeSprite, kResultX, kPeriodEndSpriteY + scorerListOffsetY);
    else if (swos.gameState == GameState::kResultAfterTheGame)
        drawMenuSprite(kFullTimeSprite, kResultX, kPeriodEndSpriteY + scorerListOffsetY);
}

static int getScorerListOffsetY()
{
    int scorerListOffsetY = 0;

    // move team name up 7 pixels for each scorer line
    for (size_t i = 0; i < m_team1ScorerLines.size() && (m_team1ScorerLines[i][0] || m_team2ScorerLines[i][0]); i++)
        scorerListOffsetY -= kScorerLineHeight;

    // force at least 14 pixels between team name and scorer list
    return std::min(scorerListOffsetY, -2 * kScorerLineHeight);
}

static const char *getPlayerName(int playerOrdinal, const TeamGame& team)
{
    auto name = team.players[playerOrdinal].shortName;
    int offset = name[0] && name[1] && name[1] == '.' && name[2] == ' ' ? 3 : 0;
    return name + offset;
}

static bool shiftLinesDown(ScorerLines& lines, int lineToFree)
{
    if (lineToFree >= kMaxScorersForDisplay)
        return false;

    memmove(lines[lineToFree + 1].data(), lines[lineToFree].data(), (lines.size() - lineToFree - 1) * lines[0].size());

    return true;
}

static void updateScorersText(const Sprite& scorer, const TeamGame& team, int teamNum, ScorerInfo& scorerInfo, int currentLine)
{
    assert(currentLine < kMaxScorersForDisplay);

    auto name = getPlayerName(scorer.playerOrdinal - 1, team);
    auto& lines = teamNum == 1 ? m_team1ScorerLines : m_team2ScorerLines;
    auto line = lines[currentLine].data();

    int i = 0;
    while (line[i++] = *name++)
        ;

    line[i - 1] = '\t'; // preserve 5 pixels spacing as in the original
    line[i] = '\0';

    int width = getStringPixelLength(line);
    line += i;

    int currentScorerLine = 0;
    char goalMinuteBuf[16];

    for (int i = 0; i < scorerInfo.numGoals; i++) {
        const auto& goal = scorerInfo.goals[i];
        int minuteBufLen = goal.storeTime(goalMinuteBuf);

        switch (goal.type) {
        case GoalType::kPenalty:
            strcpy(&goalMinuteBuf[minuteBufLen], "(PEN)");
            minuteBufLen += 5;
            break;
        case GoalType::kOwnGoal:
            strcpy(&goalMinuteBuf[minuteBufLen], "(OG)");
            minuteBufLen += 4;
            break;
        }

        if (i != scorerInfo.numGoals - 1)
            goalMinuteBuf[minuteBufLen++] = ',';

        goalMinuteBuf[minuteBufLen] = '\0';
        int goalMinuteLen = getStringPixelLength(goalMinuteBuf);

        if (width + goalMinuteLen <= kMaxScorerLineWidth) {
            width += goalMinuteLen;
        } else {
            if (++currentLine >= kMaxScorersForDisplay)
                break;

            if (++currentScorerLine >= scorerInfo.numLines) {
                if (!shiftLinesDown(lines, currentLine))
                    break;
                scorerInfo.numLines++;
            }

            *line = '\0';
            line = lines[currentLine].data();
            width = goalMinuteLen;
        }

        assert(line + minuteBufLen < lines[currentLine].data() + kCharactersPerLine);

        memcpy(line, goalMinuteBuf, minuteBufLen);
        line += minuteBufLen;
    }

    *line = '\0';
}
