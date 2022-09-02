#include "optionsMenu.h"
#include "options.h"
#include "replays.h"
#include "menus.h"
#include "videoOptionsMenu.h"
#include "audioOptionsMenu.h"
#include "controlOptionsMenu.h"

static int16_t m_gameStyle;

#include "options.mnu.h"

void showOptionsMenu()
{
    logInfo("Showing options menu...");

    showMenu(optionsMenu);

    logInfo("Leaving options menu");
    CommonMenuExit();
}

static void exitOptions()
{
    static int s_counter;

    if (swos.controlMask == kShortFireMask) {
        SetExitMenuFlag();
        saveOptions();
    } else if (swos.controlMask == (kShortFireMask | kDownMask) && ++s_counter == 15) {
        auto entry = getMenuEntry(OptionsMenu::Entries::secret);
        entry->show();
    }
}

static void doShowVideoOptionsMenu()
{
    showVideoOptionsMenu();
}

static void doShowAudioOptionsMenu()
{
    showAudioOptionsMenu();
}

static void doShowControlOptionsMenu()
{
    showControlOptionsMenu();
}

static void showGameplayOptions()
{
    showMenu(gameplayOptionsMenu);
}

static void initGamePlayOptions()
{
    m_gameStyle = getGameStyle();
    GameplayOptionsMenu::autoSaveReplaysEntry.copyString(getAutoSaveReplays() ? "ON" : "OFF");
}

static void toggleAutoSaveReplays()
{
    setAutoSaveReplays(!getAutoSaveReplays());
    initGamePlayOptions();
}

static void toggleGameStyle()
{
    setGameStyle(!getGameStyle());
}
