#include "menus.h"
#include "menuProc.h"
#include "menuAlloc.h"
#include "menuMouse.h"
#include "menuItemRenderer.h"
#include "render.h"
#include "menuControls.h"
#include "mainMenu.h"

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
    extern void checkMemory();
    checkMemory();
#endif

    if (!swos.menuCycleTimer) {
        timerProc();
        timerProc();    // imitate int 8 ;)
    }
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

static const char *entryText(int entryOrdinal)
{
    auto entry = getMenuEntry(entryOrdinal);

    static char buf[16];

    switch (entry->type) {
    case kEntryString: return entry->string();
    case kEntryMultilineText: return entry->fg.multiLineText.asCharPtr() + 1;
    case kEntrySprite2:
    case kEntryNumber: return SDL_itoa(entry->fg.number, buf, 10);
    default: return "/";
    }
}

// These are macros instead of functions because we need to save these variables for each level of nesting;
// being functions they can't create local variables on callers stack, and then we'd need explicit stacks.
#define saveCurrentMenu() \
    auto currentMenu = getCurrentMenu(); \
    auto savedEntry = currentMenu->selectedEntry ? currentMenu->selectedEntry->ordinal : 0; \
    auto savedMenu = getCurrentPackedMenu(); \

#define restorePreviousMenu() \
    restoreMenu(savedMenu, savedEntry); \

void showMenu(const BaseMenu& menu)
{
    swos.g_scanCode = 0;

    resetControls();

    menuMouseOnAboutToShowNewMenu();

    saveCurrentMenu();
    logInfo("Showing menu %#x [%s], previous menu is %#x", &menu, entryText(savedEntry), savedMenu);

    auto memoryMark = SwosVM::markAllMemory();

    swos.g_exitMenu = 0;
    activateMenu(&menu);

    while (!swos.g_exitMenu) {
        menuProcCycle();
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

// called by SWOS, when we're starting the game
void SWOS::ToMainGameLoop()
{
    logInfo("Starting the game...");

    saveCurrentMenu();
    invokeWithSaved68kRegisters(StartMainGameLoop);
    restorePreviousMenu();
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

void activateMenu(const void *menu)
{
    unpackMenu(menu);

    auto currentMenu = getCurrentMenu();
    auto currentEntry = currentMenu->selectedEntry.getRaw();

    if (currentEntry <= 0xffff)
        currentMenu->selectedEntry = getMenuEntry(currentEntry);

    if (currentMenu->onInit) {
        SDL_UNUSED auto menuMark = menuAlloc(0);

        A6 = swos.g_currentMenu;
        currentMenu->onInit();

        assert(menuMark == menuAlloc(0));
    }

    assert(currentMenu->numEntries < 256);

    cacheMenuItemBackgrounds();

    resetMenuMouseData();
}

bool showContinueAbortPrompt(const char *header, const char *continueText,
    const char *abortText, const std::vector<const char *>& messageLines)
{
    using namespace SwosVM;

    assert(messageLines.size() < 7);
    auto mark = getMemoryMark();

    A0 = allocateString(header);
    A2 = allocateString(continueText);
    A3 = allocateString(abortText);

    constexpr int kBufferSize = 256;
    auto buf = allocateMemory(kBufferSize);

    auto p = buf + 1;
    auto sentinel = buf + kBufferSize - 1;

    for (auto line : messageLines) {
        do {
            if (p >= sentinel)
                break;
            *p++ = *line;
        } while (*line++);

        while (*p == ' ')
            p--;

        assert(p > buf);
    }

    *p = '\0';
    buf[0] = static_cast<char>(messageLines.size());

    A1 = buf;
    assert(p - buf + 1 < kBufferSize);

    DoContinueAbortMenu();

    releaseMemory(mark);
    return D0 == 0;
}
