// Handling of current player indicator displayed top-left (number + surname).
#include "playerNameDisplay.h"
#include "updateBench.h"
#include "referee.h"
#include "camera.h"
#include "text.h"
#include "updatePlayers.h"

constexpr int kPlayerNameX = 12;
constexpr int kPlayerNameY = 0;

static bool m_topTeam;
static int m_playerOrdinal;

static void getPlayerNumberAndSurname(char *buf, const PlayerInfo& player);
static void hideCurrentPlayerName();
static void showNameBlinking(const Sprite *lastPlayer, const TeamGeneralInfo *lastTeam);
static void prolongLastPlayersName(const Sprite *lastPlayer, const TeamGeneralInfo *lastTeam);
static void resetNobodysBallTimer();

void updateCurrentPlayerName()
{
    static int s_nobodysBallLastFrame;

    swos.currentPlayerNameSprite.show();

    if (swos.currentScorer) {
        showNameBlinking(swos.currentScorer, swos.lastTeamScored);
    } else if (cardHandingInProgress()) {
        showNameBlinking(swos.bookedPlayer, swos.lastTeamBooked);
    } else if (auto lastPlayerBeforeGoalkeeper = getLastPlayerBeforeGoalkeeper()) {
        prolongLastPlayersName(lastPlayerBeforeGoalkeeper, swos.lastTeamScored);
    } else {
        auto lastPlayer = getLastPlayerPlayed();
        auto lastTeam = swos.lastTeamPlayed;

        if (swos.gameStatePl == GameState::kInProgress) {
            if (lastPlayer) {
                if (lastTeam->playerHasBall)
                    resetNobodysBallTimer();
                prolongLastPlayersName(lastPlayer, lastTeam);
            } else {
                hideCurrentPlayerName();
            }
        } else if (!lastPlayer || !lastTeam->controlledPlayer) {
            s_nobodysBallLastFrame = 1;
            hideCurrentPlayerName();
        } else if (s_nobodysBallLastFrame) {
            s_nobodysBallLastFrame--;
            hideCurrentPlayerName();
        } else {
            resetNobodysBallTimer();
            prolongLastPlayersName(lastPlayer, lastTeam);
        }
    }
}

void drawPlayerName()
{
    if (m_playerOrdinal >= 0) {
        assert(m_playerOrdinal >= 0 && m_playerOrdinal <= 16);

        auto team = m_topTeam ? swos.topTeamPtr : swos.bottomTeamPtr;
        const auto& player = team->players[m_playerOrdinal];

        char nameBuf[64];
        getPlayerNumberAndSurname(nameBuf, player);

        drawText(kPlayerNameX, kPlayerNameY, nameBuf);
    }
}

#ifdef SWOS_TEST
std::pair<bool, int> getDisplayedPlayerNumberAndTeam()
{
    return { m_topTeam, m_playerOrdinal };
}
#endif

static void getPlayerNumberAndSurname(char *buf, const PlayerInfo& player)
{
    SDL_itoa(player.shirtNumber, buf, 10);
    int shirtNumberLen = strlen(buf);
    buf[shirtNumberLen] = ' ';

    A0 = player.shortName;
    D0 = 0x4000 | 11;   // copy surname only, 11 characters max.
    ExtractSurname();

    strcpy(buf + shirtNumberLen + 1, A0.asPtr());
}

static void showCurrentPlayerName(const Sprite *lastPlayer, const TeamGeneralInfo *lastTeam)
{
    assert(lastPlayer && lastTeam);

    m_topTeam = lastTeam->inGameTeamPtr.asAligned() == &swos.topTeamInGame;
    m_playerOrdinal = lastPlayer->playerOrdinal - 1;
}

static void hideCurrentPlayerName()
{
    m_playerOrdinal = -1;
    resetLastPlayerBeforeGoalkeeper();
}

static void showNameBlinking(const Sprite *lastPlayer, const TeamGeneralInfo *lastTeam)
{
    bool showName = (swos.currentGameTick & 8) != 0;
    showName ? showCurrentPlayerName(lastPlayer, lastTeam) : hideCurrentPlayerName();
}

static void prolongLastPlayersName(const Sprite *lastPlayer, const TeamGeneralInfo *lastTeam)
{
    if (!swos.nobodysBallTimer) {
        hideCurrentPlayerName();
    } else {
        swos.nobodysBallTimer--;
        showCurrentPlayerName(lastPlayer, lastTeam);
    }
}

static void resetNobodysBallTimer()
{
    constexpr int kFramesBeforeNobodysBall = 50;

    // draw controlled player name for some time after the ball's gone
    swos.nobodysBallTimer = kFramesBeforeNobodysBall;
}
