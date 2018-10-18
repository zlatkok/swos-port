#include "log.h"
#include "util.h"
#include "render.h"
#include "controls.h"
#include "menu.h"

using ReachabilityMap = std::array<bool, 256>;
static ReachabilityMap m_reachableEntries;

void SWOS::FlipInMenu()
{
    frameDelay(2);
    updateScreen();
}

// this is actually a debugging function, but used by everyone to quickly exit SWOS :)
void SWOS::SetPrevVideoModeEndProgram()
{
    std::exit(0);
}

static void menuDelay()
{
#ifndef NDEBUG
    extern void checkMemory();

    assert(_CrtCheckMemory());
    checkMemory();
#endif

    if (!menuCycleTimer) {
        TimerProc();
        TimerProc();    // imitate int 8 ;)
    }
}

static void checkMousePosition()
{
    static int lastX = -1, lastY = -1;
    static bool clickedLastFrame;
    static bool selectedLastFrame;

    int x, y;
    auto buttons = SDL_GetMouseState(&x, &y);

    if (x != lastX || y != lastY)
        selectedLastFrame = false;

    lastX = x;
    lastY = y;

    if (clickedLastFrame) {
        if (!buttons) {
            clickedLastFrame = false;
            selectedLastFrame = false;
        }

        releaseFire();
        return;
    }

    if (selectedLastFrame && !(buttons & SDL_BUTTON(SDL_BUTTON_LEFT)))
        return;

    int windowWidth, windowHeight;
    std::tie(windowWidth, windowHeight) = getWindowSize();

    x = x * kVgaWidth / windowWidth;
    y = y * kVgaHeight / windowHeight;

    if (hasMouseFocus() && vsPtr == linAdr384k) {
        auto currentMenu = reinterpret_cast<Menu *>(g_currentMenu);
        auto entries = reinterpret_cast<MenuEntry *>(g_currentMenu + sizeof(Menu));

        for (size_t i = 0; i < currentMenu->numEntries; i++) {
            auto& entry = entries[i];
            bool insideEntry = x >= entry.x && x < entry.x + entry.width && y >= entry.y && y < entry.y + entry.height;

            if (insideEntry && !entry.invisible && m_reachableEntries[entry.ordinal]) {
                currentMenu->selectedEntry = &entry;
                selectedLastFrame = true;

                bool isPlayMatch = entry.type2 == ENTRY_STRING && entry.u2.string && !strcmp(entry.u2.string, "PLAY MATCH");

                if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT) && !isPlayMatch) {
                    pressFire();
                    clickedLastFrame = true;
                    player1ClearFlag = 1;
                }
            }
        }
    }
}

static void menuProcCycle()
{
    menuDelay();
    checkMousePosition();
    SAFE_INVOKE(MenuProc);
}

void SWOS::WaitRetrace()
{
    menuDelay();
}

void SWOS::MainMenu()
{
    logInfo("Initializing main menu");
    SAFE_INVOKE(InitMainMenu);

    while (true)
        menuProcCycle();
}

static const char *entryText(const MenuEntry *entry)
{
    static char buf[16];
    if (entry) {
        switch (entry->type1) {
        case ENTRY_STRING: return entry->u2.string;
        case ENTRY_MULTI_LINE_TEXT: return reinterpret_cast<char *>(entry->u2.multiLineText) + 1;
        case ENTRY_SPRITE2:
        case ENTRY_NUMBER: return _itoa(entry->u2.number, buf, 10);
        }
    }

    return "/";
}

// Find and mark every reachable item starting from the initial one.
static void determineReachableEntries(const MenuBase *menu)
{
    std::fill(m_reachableEntries.begin(), m_reachableEntries.end(), false);

    static_assert(offsetof(MenuEntry, downEntry) == offsetof(MenuEntry, upEntry) + 1 &&
        offsetof(MenuEntry, upEntry) == offsetof(MenuEntry, rightEntry) + 1 &&
        offsetof(MenuEntry, rightEntry) == offsetof(MenuEntry, leftEntry) + 1 &&
        sizeof(MenuEntry::leftEntry) == 1, "MenuEntry next entries assumptions failed");

    std::vector<std::pair<const MenuEntry *, int>> entryStack;
    entryStack.reserve(256);

    auto currentMenu = reinterpret_cast<Menu *>(g_currentMenu);
    auto currentEntry = currentMenu->selectedEntry;
    auto entries = reinterpret_cast<MenuEntry *>(g_currentMenu + sizeof(Menu));

    entryStack.emplace_back(currentEntry, 0);

    while (!entryStack.empty()) {
        const MenuEntry *entry;
        int direction;

        std::tie(entry, direction) = entryStack.back();

        assert(entry && direction >= 0 && direction <= 4);
        assert(entry->ordinal < 256);

        m_reachableEntries[entry->ordinal] = true;

        auto nextEntries = reinterpret_cast<const uint8_t *>(&entry->leftEntry);

        for (; direction < 4; direction++) {
            auto ord = nextEntries[direction];

            if (ord != 255 && !m_reachableEntries[ord]) {
                entryStack.emplace_back(&entries[ord], 0);
                break;
            }
        }

        if (direction >= 4) {
            entryStack.pop_back();
        } else {
            assert(entryStack.size() >= 2);
            entryStack[entryStack.size() - 2].second = direction + 1;
        }
    }
}

// these are macros instead of functions because we need to save these variables for each level of nesting;
// being functions they can't create local variables on callers stack, and then we'd need explicit stacks
#define saveCurrentMenu() \
    auto currentMenu = reinterpret_cast<Menu *>(g_currentMenu); \
    auto savedEntry = currentMenu->selectedEntry; \
    auto savedMenu = g_currentMenuPtr; \

#define restorePreviousMenu() \
    A6 = savedMenu; \
    A5 = savedEntry; \
    SAFE_INVOKE(RestorePreviousMenu); \

// ShowMenu
//
// in:
//     A6 -> menu to show on screen (packed form)
//
// After showing this menu, returns to previous menu.
//
void SWOS::ShowMenu()
{
    g_scanCode = 0;
    controlWord = 0;
    menuStatus = 0;

    auto newMenu = A6.asPtr();
    saveCurrentMenu();
    logInfo("Showing menu %#x [%s], previous menu is %#x", A6.data, entryText(savedEntry), savedMenu);

    g_exitMenu = 0;
    SAFE_INVOKE(PrepareMenu);

    while (!g_exitMenu)
        menuProcCycle();

    menuStatus = 1;
    g_exitMenu = 0;

    restorePreviousMenu();
    determineReachableEntries(savedMenu);

    logInfo("Menu %#x finished, restoring menu %#x", newMenu, g_currentMenuPtr);
}

// called by SWOS, when we're starting the game
void SWOS::ToMainGameLoop()
{
    logInfo("Starting the game...");

    saveCurrentMenu();
    save68kRegisters();

    SAFE_INVOKE(StartMainGameLoop);

    restore68kRegisters();
    restorePreviousMenu();
}

__declspec(naked) void SWOS::DoUnchainSpriteInMenus_OnEnter()
{
    // only cx is loaded, but later whole ecx is tested; make this right
    _asm {
        xor ecx, ecx
        retn
    }
}

void SWOS::MenuProc_OnEnter()
{
    g_videoSpeedIndex = 50;
}

// PrepareMenu
//
// in:
//     A6 -> menu to prepare (or activate)
// globals:
//     g_currentMenu - destination for unpacked menu.
//
// Unpack given static menu into g_currentMenu. Execute menu init function, if present.
//
void SWOS::PrepareMenu()
{
    g_currentMenuPtr = A6;
    auto packedMenu = A6.as<const MenuBase *>();

    A0 = A6;
    A1 = g_currentMenu;
    ConvertMenu();

    auto currentMenu = reinterpret_cast<Menu *>(g_currentMenu);
    auto currentEntry = reinterpret_cast<uint32_t>(currentMenu->selectedEntry);

    if (currentEntry <= 0xffff)
        currentMenu->selectedEntry = getMenuEntryAddress(currentEntry);

    if (currentMenu->onInit) {
        A6 = g_currentMenu;
        auto proc = currentMenu->onInit;
        SAFE_INVOKE(proc);
    }

    assert(currentMenu->numEntries < 256);

    determineReachableEntries(packedMenu);
}
