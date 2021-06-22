//
// Replay/Exit menu after the end of a friendly game
//

#include "game.h"
#include "drawMenu.h"
#include "render.h"
#include "replays.h"
#include "replayExit.mnu.h"

void SWOS::ReplayExitMenuAfterFriendly()
{
    showMenu(replayExitMenu);
}

static void replayExitMenuOnInit()
{
    swos.replaySelected = 0;

    drawMenu();     // redraw menu so it's ready for the fade-in
    fadeIfNeeded();

    swos.g_cameraX = 0;
    swos.g_cameraY = 0;

    SDL_ShowCursor(SDL_ENABLE);
}

static void replayExitMenuDone(bool replay = false)
{
    swos.replaySelected = replay;
    FadeOutToBlack();
    SetExitMenuFlag();

    if (replay)
        initNewReplay();

    updateCursor(replay);
}

static void replayOnSelect()
{
    logInfo("Going for another one!");
    replayExitMenuDone(true);
}

static void exitOnSelect()
{
    replayExitMenuDone();
}
