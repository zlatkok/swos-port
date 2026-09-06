#include "MenuWalkerTest.h"
#include "mainMenu.h"
#include "menus.h"
#include "unpackMenu.h"
#include "sdlProcs.h"
#include "unitTest.h"
#include "mockLog.h"

static MenuWalkerTest t;

struct MainMenuDestination
{
    int entry;
    const char *name;
};

static constexpr MainMenuDestination kMainMenuDestinations[] = {
    { 0, "edit tactics" },
    { 1, "edit custom teams" },
    { 2, "replays" },
    { 3, "options" },
    { 5, "friendly" },
    { 6, "DIY competition" },
    { 7, "preset competition" },
    { 8, "season" },
    { 9, "career" },
    { 12, "load old competition" },
    { 13, "quit" },
};

static constexpr int kEditCustomTeamsEntries[] = {
    0, 3,                         // team and coach names
    5, 6, 7, 9, 10, 11,          // kit colors
    12, 13, 14, 15, 16, 17, 18, 19, // shirt styles
    22,                           // undo
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, // faces
    39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, // player names
};

void MenuWalkerTest::init()
{
    takeOverInput();
}

void MenuWalkerTest::finish()
{
    SWOS_UnitTest::setMenuCallback();
}

void MenuWalkerTest::defaultCaseInit()
{
}

const char *MenuWalkerTest::name() const
{
    return "menu-walker";
}

const char *MenuWalkerTest::displayName() const
{
    return "automatic menu walker";
}

auto MenuWalkerTest::getCases() -> CaseList
{
    return {
        { "inventory main menu", "menu-walker-main-inventory", bind(&MenuWalkerTest::openMainMenu),
            bind(&MenuWalkerTest::inventoryCurrentMenu) },
        { "main menu submenu round trip", "menu-walker-main-round-trip", bind(&MenuWalkerTest::openMainMenu),
            bind(&MenuWalkerTest::testMainMenuRoundTrip), std::size(kMainMenuDestinations), false },
        { "inventory edit custom teams menu", "menu-walker-edit-custom-teams-inventory",
            bind(&MenuWalkerTest::openEditCustomTeamsMenu), bind(&MenuWalkerTest::inventoryEditCustomTeamsMenu) },
        { "walk edit custom teams entries", "menu-walker-edit-custom-teams-entries",
            bind(&MenuWalkerTest::openEditCustomTeamsMenu), bind(&MenuWalkerTest::walkEditCustomTeamsEntry),
            std::size(kEditCustomTeamsEntries), false },
        { "walk edit custom teams import", "menu-walker-edit-custom-teams-import",
            bind(&MenuWalkerTest::openEditCustomTeamsMenu), bind(&MenuWalkerTest::walkEditCustomTeamsImport), 1, false },
        { "walk edit custom teams OK", "menu-walker-edit-custom-teams-ok",
            bind(&MenuWalkerTest::openEditCustomTeamsMenu), bind(&MenuWalkerTest::walkEditCustomTeamsOk), 1, false },
    };
}

void MenuWalkerTest::openMainMenu()
{
    showMainMenu();
}

void MenuWalkerTest::inventoryCurrentMenu()
{
    auto menu = getCurrentMenu();
    assertTrue(menu);
    assertTrue(mainMenuActive());
    assertTrue(menu->numEntries > 0);

    int visibleEntries = 0;
    int callbacks = 0;
    for (int i = 0; i < menu->numEntries; i++) {
        auto entry = &menu->entries()[i];
        assertEqual(entry->ordinal, i);
        assertTrue(entry->background >= kEntryNoBackground && entry->background <= kEntrySprite1);
        assertTrue(entry->type >= kEntryNoForeground && entry->type <= kEntryBoolOption);

        if (!entry->invisible) {
            visibleEntries++;
            if (!entry->disabled && entry->onSelect)
                callbacks++;
        }
    }

    assertTrue(visibleEntries > 0);
    assertTrue(callbacks > 0);
}

void MenuWalkerTest::testMainMenuRoundTrip()
{
    const auto& destination = kMainMenuDestinations[m_currentDataIndex];
    std::cout << "\n  main menu -> " << destination.name << std::flush;
    auto mainMenu = getCurrentPackedMenu();
    int menusVisited = 0;

    assertTrue(mainMenuActive());
    auto entry = getMenuEntry(destination.entry);
    assertFalse(entry->invisible);
    assertFalse(entry->disabled);
    assertTrue(static_cast<bool>(entry->onSelect));

    SWOS_UnitTest::setMenuCallback([&] {
        assertFalse(mainMenuActive());
        assertNotEqual(getCurrentPackedMenu(), mainMenu);
        assertTrue(getCurrentMenu()->numEntries > 0);
        menusVisited++;

        // Leave every menu entered by the main-menu callback. Some competition
        // callbacks open a confirmation menu before their actual setup menu.
        exitCurrentMenu();
        SetExitMenuFlag();
        return false;
    });

    selectItem(entry);
    SWOS_UnitTest::setMenuCallback();

    assertTrue(menusVisited > 0);
    assertTrue(mainMenuActive());
    assertEqual(getCurrentPackedMenu(), mainMenu);
}

void MenuWalkerTest::openEditCustomTeamsMenu()
{
    LogSilencer silenceUnsupportedLegacySprites;
    showMainMenu();

    const void *editTeamsMenu = nullptr;
    SWOS_UnitTest::setMenuCallback([&] {
        auto menu = getCurrentMenu();
        if (menu->numEntries == 61) {
            editTeamsMenu = getCurrentPackedMenu();
            exitCurrentMenu();
            SetExitMenuFlag();
            return false;
        }

        // The initial confirmation has CONTINUE at ordinal 2. Subsequent
        // team-selection menus initialize their preferred selectable entry.
        MenuEntry *entry = menu->numEntries == 4 ? getMenuEntry(2) : menu->selectedEntry.asPtr();
        assertTrue(entry);
        assertFalse(entry->invisible);
        assertFalse(entry->disabled);
        assertTrue(static_cast<bool>(entry->onSelect));
        selectItem(entry);
        return false;
    });

    selectItem(1);
    SWOS_UnitTest::setMenuCallback();
    assertTrue(editTeamsMenu);
    activateMenu(editTeamsMenu);
}

void MenuWalkerTest::inventoryEditCustomTeamsMenu()
{
    auto menu = getCurrentMenu();
    assertEqual(menu->numEntries, 61);

    int visibleEntries = 0;
    int selectableEntries = 0;
    for (int i = 0; i < menu->numEntries; i++) {
        auto entry = getMenuEntry(i);
        assertEqual(entry->ordinal, i);
        if (!entry->invisible) {
            visibleEntries++;
            if (!entry->disabled && entry->onSelect)
                selectableEntries++;
        }
    }

    assertEqual(visibleEntries, 60);
    assertTrue(selectableEntries >= 40);

    exitCurrentMenu();
    SetExitMenuFlag();
    swos.g_exitMenu = 0;
    showMainMenu();
    assertTrue(mainMenuActive());
}

void MenuWalkerTest::walkEditCustomTeamsEntry()
{
    LogSilencer silenceUnsupportedLegacySprites;
    auto ordinal = kEditCustomTeamsEntries[m_currentDataIndex];
    auto entry = getMenuEntry(ordinal);

    assertFalse(entry->invisible);
    assertFalse(entry->disabled);
    assertTrue(static_cast<bool>(entry->onSelect));

    bool textEntry = ordinal == 0 || ordinal == 3 || ordinal >= 39;
    bool kitColorEntry = ordinal == 5 || ordinal == 6 || ordinal == 7 ||
        ordinal == 9 || ordinal == 10 || ordinal == 11;
    if (textEntry)
        SWOS_UnitTest::queueKeys({ SDL_SCANCODE_ESCAPE });

    selectEntry(entry, kitColorEntry ? kRightMask : kShortFireMask);

    assertEqual(getCurrentMenu()->numEntries, 61);
    exitCurrentMenu();
    SetExitMenuFlag();
    swos.g_exitMenu = 0;
    showMainMenu();
    assertTrue(mainMenuActive());
}

void MenuWalkerTest::walkEditCustomTeamsImport()
{
    LogSilencer silenceUnsupportedLegacySprites;
    bool openedTeamSelection = false;
    SWOS_UnitTest::setMenuCallback([&] {
        openedTeamSelection = true;
        assertNotEqual(getCurrentMenu()->numEntries, 61);
        exitCurrentMenu();
        SetExitMenuFlag();
        return false;
    });

    selectItem(20);
    SWOS_UnitTest::setMenuCallback();

    assertTrue(openedTeamSelection);
    assertEqual(getCurrentMenu()->numEntries, 61);
    exitCurrentMenu();
    SetExitMenuFlag();
    swos.g_exitMenu = 0;
    showMainMenu();
    assertTrue(mainMenuActive());
}

void MenuWalkerTest::walkEditCustomTeamsOk()
{
    selectItem(21);
    assertTrue(swos.g_exitMenu);
    swos.g_exitMenu = 0;
    showMainMenu();
    assertTrue(mainMenuActive());
}
