#include "menus.h"
#include "unpackMenu.h"
#include "menuProc.h"
#include "menuAlloc.h"
#include "menuMouse.h"
#include "menuItemRenderer.h"
#include "timer.h"
#include "menuControls.h"
#include "mainMenu.h"
#include "game.h"
#include "init.h"

#ifdef SWOS_TEST
# include "unitTest.h"
#endif

void setEntriesVisibility(const std::vector<int>& entryIndices, bool visible)
{
    auto currentMenu = getCurrentMenu();

    for (auto entryIndex : entryIndices) {
        assert(entryIndex >= 0 && entryIndex < currentMenu->numEntries);
        currentMenu->entries()[entryIndex].setVisible(visible);
    }
}

void selectEntry(MenuEntry *entry, int controlMask /* = kShortFireMask */)
{
    assert(entry && entry->onSelect);
    if (entry && entry->onSelect) {
        save68kRegisters();

        swos.controlMask = controlMask;

        A5 = entry;
        entry->onSelect();

        restore68kRegisters();
    }
}

void selectEntry(int ordinal)
{
    assert(ordinal >= 0 && ordinal < 255 && ordinal < getCurrentMenu()->numEntries);

    if (ordinal >= 0 && ordinal < 255) {
        auto entry = getMenuEntry(ordinal);
        selectEntry(entry);
    }
}

static void menuDelay()
{
#ifdef DEBUG
# ifdef _WIN32
    assert(_CrtCheckMemory());
# endif
    checkMemory();
#endif
}

static void menuProcCycle()
{
    menuDelay();
    menuProc();
}

void SWOS::WaitRetrace()
{
    menuDelay();
}

// Tries to discover menu title (or something characteristic) using some simple heuristics.
static const char *discoverMenuTitle()
{
    static char buf[40];

    auto entryText = [](const MenuEntry *entry) -> const char * {
        if (entry->type == kEntryString)
            return entry->string();
        else if (entry->type == kEntryMultilineText)
            return entry->fg.multilineText.asCharPtr() + 1;
        else
            return nullptr;
    };

    auto hasText = [entryText](const MenuEntry *entry) {
        const char *str = entryText(entry);
        return str && *str;
    };

    if (mainMenuActive())
        return "MAIN MENU";

    auto currentMenu = getCurrentMenu();

    const MenuEntry *colorMatch = nullptr;
    const MenuEntry *dimensionsMatch = nullptr;
    const MenuEntry *firstText = nullptr;
    const MenuEntry *firstSprite = nullptr;
    const MenuEntry *firstNumeric = nullptr;

    for (auto entry = currentMenu->entries(); entry < currentMenu->sentinelEntry(); entry++) {
        if (!colorMatch && entry->bg.entryColor == (kGray | kGrayFrame) && hasText(entry)) // title color
            colorMatch = entry;
        else if (!dimensionsMatch && entry->width > 250 && entry->height > 12 && hasText(entry)) // very wide entry
            dimensionsMatch = entry;
        else if (!firstText && hasText(entry))
            firstText = entry;
        else if (!firstSprite && entry->type == kEntrySprite2)
            firstSprite = entry;
        else if (!firstNumeric && entry->type == kEntryNumber)
            firstNumeric = entry;
    }

    if (currentMenu->onReturn.index() == static_cast<int>(SwosVM::Procs::PlayMatchMenuReinit)) {
        sprintf(buf, "PLAY MATCH MENU (%s)", entryText(firstText));
        return buf;
    }

    if (colorMatch) {
        return colorMatch->string();
    } else if (dimensionsMatch) {
        return dimensionsMatch->string();
    } else if (firstText) {
        return entryText(firstText);
    } else if (firstSprite || firstNumeric) {
        memcpy(buf, firstSprite ? "SPRITE " : "NUMBER ", 7);
        auto entry = firstSprite ? firstSprite : firstNumeric;
        SDL_itoa(entry->fg.number, buf + 7, 10);
        return buf;
    }

    sprintf(buf, "%d-[%d,%d,%d]-%d", currentMenu->numEntries, currentMenu->onInit.index(), currentMenu->onReturn.index(),
        currentMenu->onDraw.index(), currentMenu->selectedEntry ? currentMenu->selectedEntry->ordinal : -1);

    return buf;
}

// These are macros instead of functions because we need to save the variables for each level of nesting;
// being functions they can't create local variables on callers stack, and then we'd need explicit stacks.
#define saveCurrentMenu() \
    auto currentMenu = getCurrentMenu(); \
    auto savedEntry = currentMenu->selectedEntry ? currentMenu->selectedEntry->ordinal : 0; \
    auto savedMenu = getCurrentPackedMenu(); \

#define restorePreviousMenu() \
    restoreMenu(savedMenu, savedEntry); \

void showMenu(const BaseMenu& menu)
{
    resetControls();

    menuMouseOnAboutToShowNewMenu();

    saveCurrentMenu();
    logInfo("About to activate menu %#x", &menu);

    auto memoryMark = SwosVM::markAllMemory();

    swos.g_exitMenu = 0;
    activateMenu(&menu);

    logInfo("Showing menu: \"%s\", previous menu is %#x", discoverMenuTitle(), savedMenu);

    while (!swos.g_exitMenu) {
        menuProcCycle();
        markFrameStartTime();
#ifdef SWOS_TEST
        // ask unit tests how long they want to run menu proc
        if (SWOS_UnitTest::exitMenuProc())
            return;
#endif
    }

    swos.g_exitMenu = 0;
    SwosVM::releaseAllMemory(memoryMark);

    restorePreviousMenu();

    menuMouseOnOldMenuRestored();

    logInfo("Menu %#x finished, restoring menu %#x", &menu, savedMenu);
}

void saveCurrentMenuAndStartGameLoop()
{
    logInfo("Starting the game...");
    logInfo("Top team: %s, bottom team: %s", swos.topTeamIngame.teamName, swos.bottomTeamIngame.teamName);

    saveCurrentMenu();
    startMainGameLoop();
    restorePreviousMenu();
}

void exitCurrentMenu()
{
    swos.chosenTactics = -1;    // edit tactics menu
    swos.teamsType = -1;        // select files menu
    if (getCurrentPackedMenu() == &swos.diyCompetitionMenu)
        swos.diySelected = -1;
    ExitSelectTeams();
    if (swos.choosingPreset)    // preset competition must be aborted at any level
        swos.abortSelectTeams = -1;
    swos.g_exitGameFlag = -1;   // play match menu
    swos.gameCanceled = 1;
    if (mainMenuActive())
        activateExitGameButton();
}

char *copyStringToMenuBuffer(const char *str)
{
    auto buf = getCurrentMenu()->endOfMenuPtr.asAlignedCharPtr();
    strcpy(buf, str);
    assert(buf + strlen(str) <= swos.g_currentMenu + sizeof(swos.g_currentMenu));
    return buf;
}

char *getMenuTempBuffer()
{
    auto tempBuf = getCurrentMenu()->endOfMenuPtr.asAlignedCharPtr();
    assert(tempBuf + 256 < swos.g_currentMenu + sizeof(swos.g_currentMenu));
    return tempBuf;
}

// ShowMenu
//
// in:
//     A6 -> menu to show on screen (packed form)
//
// Shows this menu and blocks, returns only when it's exited.
// And then returns to the previous menu and it becomes the active menu.
//
void SWOS::ShowMenu()
{
    auto menu = A6.as<BaseMenu *>();
    showMenu(*menu);
}

// ActivateMenu
//
// in:
//     A6 -> menu to activate
// globals:
//     g_currentMenu - destination for unpacked menu.
//
// Unpacks given static menu into g_currentMenu. Executes menu initialization function, if present.
// Sometimes called directly by some menu functions, causing it to skip the menu stack system, and the
// newly shown menu will return to the callers caller.
//
void SWOS::ActivateMenu()
{
    activateMenu(A6.asPtr());
}

// SetCurrentEntry
//
//  in:
//      D0 - number of menu entry
//
//  Sets current entry in the current menu.
//
void SWOS::SetCurrentEntry()
{
    auto currentMenu = getCurrentMenu();
    assert(D0.asWord() < currentMenu->numEntries);

    auto newSelectedEntry = getMenuEntry(D0.asWord());
    currentMenu->selectedEntry = newSelectedEntry;;
}
