#include "versusMenu.h"
#include "menus.h"
#include "drawMenu.h"
#include "menuAlloc.h"
#include "render.h"
#include "controls.h"
#include "replays.h"
#include "versus.mnu.h"

static const TeamGame *m_team1;
static const TeamGame *m_team2;
static const char *m_gameName;
static const char *m_gameRound;
static std::function<void()> m_callback;

using namespace VersusMenu;

static void invokeCallbackWithTimeout();

void showVersusMenu(const TeamGame *team1, const TeamGame *team2,
    const char *gameName, const char *gameRound, std::function<void()> callback)
{
    assert(team1 && team2);

    m_team1 = team1;
    m_team2 = team2;
    m_gameName = gameName;
    m_gameRound = gameRound;
    m_callback = callback;

    showMenu(versusMenu);
}

void setTeamNameAndColor(MenuEntry& entry, const TeamGame& team, word coachNo, word playerNo)
{
    entry.copyString(team.teamName);
    strcat(entry.string(), " ");

    if (!replayingNow()) {
        int color = kLightBlue;

        if (!coachNo) {
            color = kRed;
            if (playerNo)
                color = kPurple;
        }

        entry.setBackgroundColor(color);
    }
}

static void versusMenuOnInit()
{
    gameNameEntry.copyString(m_gameName);
    if (m_gameRound[0]) {
        gameRoundEntry.copyString(m_gameRound);
    } else {
        gameRoundEntry.hide();
        gameNameEntry.y += 5;
    }

    setTeamNameAndColor(leftTeamNameEntry, *m_team1, swos.leftTeamCoachNo, swos.leftTeamPlayerNo);
    setTeamNameAndColor(rightTeamNameEntry, *m_team2, swos.rightTeamCoachNo, swos.rightTeamPlayerNo);

    drawMenu(false);

    menuFadeIn();

    invokeCallbackWithTimeout();

    processControlEvents();
    menuFadeOut();

    SetExitMenuFlag();
}

static void invokeCallbackWithTimeout()
{
    constexpr int kDelayTicks = 300;

    auto startTicks = SDL_GetTicks();

    if (m_callback)
        m_callback();

    auto elapsed = SDL_GetTicks() - startTicks;
    if (elapsed < kDelayTicks)
        SDL_Delay(kDelayTicks - elapsed);
}
