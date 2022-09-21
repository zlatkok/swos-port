#pragma once

#include "Menu.h"
#include "MenuEntry.h"
#include "menuCodes.h"
#include "util.h"

constexpr int kMenuStringLength = 70;
constexpr int kMenuOffset = 8;

static inline Menu *getCurrentMenu()
{
    return reinterpret_cast<Menu *>(swos.g_currentMenu);
}

static inline MenuEntry *getMenuEntry(int ordinal)
{
    assert(ordinal >= 0 && ordinal < 255);

    auto ptr = swos.g_currentMenu + sizeof(Menu) + ordinal * sizeof(MenuEntry);
    return reinterpret_cast<MenuEntry *>(ptr);
}

static inline int getCurrentEntryOrdinal()
{
    auto currentMenu = getCurrentMenu();
    return currentMenu->selectedEntry ? currentMenu->selectedEntry->ordinal : -1;
}

static inline void highlightEntry(MenuEntry *entry)
{
    auto menu = getCurrentMenu();
    menu->selectedEntry = entry;
}

static inline void highlightEntry(int ordinal)
{
    auto entry = getMenuEntry(ordinal);
    highlightEntry(entry);
}

static inline void setCurrentEntry(int ordinal)
{
    highlightEntry(ordinal);
}

static inline void copyStringToEntry(int entryIndex, const char *str)
{
    getMenuEntry(entryIndex)->copyString(str);
}

void setEntriesVisibility(const std::vector<int>& entryIndices, bool visible);
void selectEntry(MenuEntry *entry, int controlMask = kShortFireMask);
void selectEntry(int ordinal);

void showMenu(const BaseMenu& menu);
void saveCurrentMenuAndStartGameLoop();
void exitCurrentMenu();

char *copyStringToMenuBuffer(const char *str);
char *getMenuTempBuffer();
