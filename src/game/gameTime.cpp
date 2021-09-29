#include "gameTime.h"
#include "gameSprites.h"
#include "sprites.h"
#include "renderSprites.h"
#include "pitchConstants.h"
#include "sfx.h"

constexpr int kTimeX = 20 - 6;
constexpr int kTimeY = 9;

using LastPeriodMinuteHandler = void (*)();
using TimeDigitSprites = std::array<int, 4>;

// format: not used, digit 1, digit 2, digit 3 (e.g. 115 = 0115)
using GameTime = std::array<int, 4>;

static GameTime m_gameTime;
static int m_endGameCounter;

static int m_timeDelta;
static int m_gameSeconds;
static int m_secondsSwitchAccumulator;

static bool m_showTime;

static void initTimeDelta();
static bool isGameAtMinute(dword minute);
static bool prolongLastMinute();
static void drawGameTime(const GameTime& gameTime);
static TimeDigitSprites getGameTimeSprites(const GameTime& gameTime);
static LastPeriodMinuteHandler getPeriodEndHandler();
static bool isNextMinuteLastInPeriod();
static void bumpGameTime();
static void setupLastMinuteSwitchNextFrame();

void resetGameTime()
{
    m_showTime = false;

    m_gameSeconds = 0;
    m_gameTime.fill(0);
    m_secondsSwitchAccumulator = 0;

    initTimeDelta();
}

bool gameTimeShowing()
{
    return m_showTime;
}

void updateGameTime()
{
    m_showTime = true;

    bool minuteSwitchAboutToHappen = m_gameSeconds < 0;

    if (minuteSwitchAboutToHappen) {
        m_endGameCounter -= swos.timerDifference;
        if (m_endGameCounter < 0) {
            if (auto periodEndHandler = getPeriodEndHandler()) {
                m_gameSeconds = 0;
                swos.stateGoal = 0;
                playEndGameWhistleSample();
                periodEndHandler();
            }
        } else if (prolongLastMinute()) {
            setupLastMinuteSwitchNextFrame();
        }
    } else if (swos.gameStatePl == GameState::kInProgress && !swos.playingPenalties) {
        m_secondsSwitchAccumulator -= m_timeDelta;
        if (m_secondsSwitchAccumulator < 0) {
            m_secondsSwitchAccumulator += 54;
            m_gameSeconds += swos.timerDifference;

            if (m_gameSeconds >= 60) {
                m_gameSeconds = 0;
                bumpGameTime();

                if (isGameAtMinute(1) || isGameAtMinute(46))
                    BumpAllPlayersLastPlayedHalfAtGameStart();

                if (isNextMinuteLastInPeriod())
                    setupLastMinuteSwitchNextFrame();
            }
        }
    }
}

void drawGameTime()
{
    if (m_showTime)
        drawGameTime(m_gameTime);
}

void drawGameTime(int digit1, int digit2, int digit3)
{
    assert(digit1 >= 0 && digit2 >= 0 && digit3 >= 0 && digit1 <= 1 && digit2 <= 9 && digit3 <= 9);
    drawGameTime({ 0, digit1, digit2, digit3 });
}

dword gameTimeInMinutes()
{
    return m_gameTime[1] * 100 + m_gameTime[2] * 10 + m_gameTime[3];
}

std::tuple<int, int, int> gameTimeAsBcd()
{
    return { m_gameTime[1], m_gameTime[2], m_gameTime[3] };
}

bool gameAtZeroMinute()
{
    return m_gameTime[1] == 0 && m_gameTime[2] == 0 && m_gameTime[3] == 0;
}

static void initTimeDelta()
{
    static const int kGameLenSecondsTable[] = { 30, 18, 12, 9 };
    assert(swos.ingameGameLength <= 3);
    m_timeDelta = kGameLenSecondsTable[swos.ingameGameLength];
}

static bool isGameAtMinute(dword minute)
{
    return minute == gameTimeInMinutes();
}

static void endFirstHalf()
{
    EndFirstHalf();
    BumpAllPlayersLastPlayedHalfPostGame();
}

static void endSecondHalf()
{
    BumpAllPlayersLastPlayedHalfPostGame();
    swos.statsTeam1GoalsCopy = swos.statsTeam1Goals;
    swos.statsTeam2GoalsCopy = swos.statsTeam2Goals;

    int totalTeam1Goals = swos.team1TotalGoals;
    int totalTeam2Goals = swos.team2TotalGoals;

    if (totalTeam1Goals == totalTeam2Goals) {
        totalTeam1Goals = swos.statsTeam1Goals + 2 * swos.team1GoalsFirstLeg;
        totalTeam2Goals = swos.statsTeam2Goals + 2 * swos.team2GoalsFirstLeg;

        bool gameTied = !swos.secondLeg || swos.playing2ndGame != 1 || totalTeam1Goals == totalTeam2Goals;
        if (gameTied) {
            if (swos.extraTimeState) {
                swos.extraTimeState = -1;
                StartFirstExtraTime();
            } else if (swos.penaltiesState) {
                swos.penaltiesState = -1;
                StartPenalties();
            } else {
                swos.winningTeamPtr = nullptr;
                EndOfGame();
            }
            return;
        }
    }

    swos.winningTeamPtr = totalTeam1Goals > totalTeam2Goals ? &swos.topTeamIngame : &swos.bottomTeamIngame;
    EndOfGame();
}

static bool prolongLastMinute()
{
    if (swos.gameStatePl != GameState::kInProgress)
        return true;

    constexpr int kUpperPenaltyAreaLowerLine = 216;
    constexpr int kLowerPenaltyAreaUpperLine = 682;

    auto ballY = swos.ballSprite.y;
    auto ballInsidePenaltyArea = ballY <= kUpperPenaltyAreaLowerLine || ballY > kLowerPenaltyAreaUpperLine;
    auto attackingTeam = ballY > kPitchCenterY ? &swos.topTeamData : &swos.bottomTeamData;
    auto attackInProgress = swos.lastTeamPlayed == attackingTeam;

    return ballInsidePenaltyArea || attackInProgress;
}

static void endFirstExtraTime()
{
    EndFirstExtraTime();
}

static void endSecondExtraTime()
{
    int totalTeam1Goals = swos.team1TotalGoals;
    int totalTeam2Goals = swos.team2TotalGoals;

    if (totalTeam1Goals == totalTeam2Goals) {
        totalTeam1Goals = swos.statsTeam1Goals + 2 * swos.team1GoalsFirstLeg;
        totalTeam2Goals = swos.statsTeam2Goals + 2 * swos.team2GoalsFirstLeg;

        bool gameTied = !swos.secondLeg || !swos.playing2ndGame || totalTeam1Goals == totalTeam2Goals;
        if (gameTied) {
            if (swos.penaltiesState) {
                swos.penaltiesState = -1;
                StartPenalties();
            } else {
                swos.winningTeamPtr = nullptr;
                EndOfGame();
            }
            return;
        }
    }

    swos.winningTeamPtr = totalTeam1Goals > totalTeam2Goals ? &swos.topTeamIngame : &swos.bottomTeamIngame;
    EndOfGame();
}

static TimeDigitSprites getGameTimeSprites(const GameTime& gameTime)
{
    TimeDigitSprites sprites = { -1, -1, -1, -1 };

    if (gameTime[1]) {
        sprites[0] = gameTime[1] + kBigTimeDigitSprite0;
        sprites[1] = gameTime[2] + kBigTimeDigitSprite0;
        sprites[2] = gameTime[3] + kBigTimeDigitSprite0;
    } else if (gameTime[2]) {
        sprites[0] = gameTime[2] + kBigTimeDigitSprite0;
        sprites[1] = gameTime[3] + kBigTimeDigitSprite0;
    } else {
        sprites[0] = gameTime[3] + kBigTimeDigitSprite0;
    }

    return sprites;
}

static LastPeriodMinuteHandler getPeriodEndHandler()
{
    switch (gameTimeInMinutes()) {
    case 45:  return endFirstHalf;
    case 90:  return endSecondHalf;
    case 105: return endFirstExtraTime;
    case 120: return endSecondExtraTime;
    default: return nullptr;
    }
}

static bool isNextMinuteLastInPeriod()
{
    return getPeriodEndHandler() != nullptr;
}

static void drawGameTime(const GameTime& gameTime)
{
    auto kDigitWidth = getSprite(kBigTimeDigitSprite0 + 8).width;
    auto timeDigitSprites = getGameTimeSprites(gameTime);

    int xOffset = 0;
    for (size_t i = 0; i < timeDigitSprites.size() && timeDigitSprites[i] >= 0; i++) {
        drawMenuSprite(timeDigitSprites[i], kTimeX + xOffset, kTimeY);
        xOffset += kDigitWidth;
    }

    drawMenuSprite(kTimeSprite8Mins, kTimeX + xOffset, kTimeY);
}

static void bumpGameTime()
{
    if (++m_gameTime[3] >= 10) {
        m_gameTime[3] = 0;
        if (++m_gameTime[2] >= 10) {
            m_gameTime[2] = 0;
            if (m_gameTime[1] < 9)
                m_gameTime[1]++;
        }
    }
}

static void setupLastMinuteSwitchNextFrame()
{
    m_gameSeconds = -1;
    m_endGameCounter = 55;
}
