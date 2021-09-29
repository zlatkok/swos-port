//
// Replay/Exit menu after the end of a friendly game
//

#include "game.h"
#include "drawMenu.h"
#include "render.h"
#include "replays.h"
#include "replayExit.mnu.h"

static bool m_replaySelected;

bool showReplayExitMenuAfterFriendly()
{
    showMenu(replayExitMenu);
    return m_replaySelected;
}

bool replayExitMenuShown()
{
    return getCurrentPackedMenu() == &replayExitMenu;
}

static void replayExitMenuOnInit()
{
    m_replaySelected = false;

    menuFadeIn();

    SDL_ShowCursor(SDL_ENABLE);
}

static void replayExitMenuDone(bool replay)
{
    m_replaySelected = replay;

    if (replay)
        initNewReplay();

    drawMenu(false);
    menuFadeOut();

    SetExitMenuFlag();

    updateCursor(replay);
}

static void replayOnSelect()
{
    logInfo("Going for another one!");
    replayExitMenuDone(true);
}

static void exitOnSelect()
{
    replayExitMenuDone(false);
}
