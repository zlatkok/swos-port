#pragma once

#include "swos.h"
#include "swossym.h"
#include "menuCodes.h"
#include "util.h"
#include "render.h"

constexpr int kMenuStringLength = 70;
constexpr int kMenuOffset = 8;

static inline void prepareMenu(const SWOS_Menu::BaseMenu& menu)
{
    A6 = &menu;
    SWOS::PrepareMenu();
}

static inline void showMenu(const SWOS_Menu::BaseMenu& menu)
{
    A6 = &menu;
    SWOS::ShowMenu();
}

static inline void showError(const char *error)
{
    A0 = error;
    ShowErrorMenu();
}

static inline Menu *getCurrentMenu()
{
    return reinterpret_cast<Menu *>(g_currentMenu);
}

static inline MenuEntry *getMenuEntry(int ordinal)
{
    assert(ordinal >= 0 && ordinal < 255);

    auto ptr = g_currentMenu + sizeof(Menu) + ordinal * sizeof(MenuEntry);
    return reinterpret_cast<MenuEntry *>(ptr);
}

static inline void drawMenuItem(MenuEntry *entry)
{
    A5 = entry;
    DrawMenuItem();
}

static inline int getCurrentEntryOrdinal()
{
    auto currentMenu = getCurrentMenu();
    return currentMenu->selectedEntry ? currentMenu->selectedEntry->ordinal : -1;
}

static inline void setCurrentEntry(int ordinal)
{
    D0 = ordinal;
    SetCurrentEntry();
}

static inline void drawMenuText(int x, int y, const char *text, int color = kWhiteText2)
{
    D1 = x;
    D2 = y;
    D3 = color;
    A1 = &smallCharsTable;
    A0 = text;
    DrawMenuText();
}

// coordinates mark the center of the string
static inline void drawMenuTextCentered(int x, int y, const char *text, int color = kWhiteText2)
{
    D1 = x;
    D2 = y;
    D3 = color;
    A1 = &smallCharsTable;
    A0 = text;
    SAFE_INVOKE(DrawMenuTextCentered);
}

static inline void drawMenuSprite(int x, int y, int index)
{
    D0 = index;
    D1 = x;
    D2 = y;
    DrawSprite();
}

static inline void redrawMenuBackground()
{
    memcpy(linAdr384k, linAdr384k + 128 * 1024, 320 * 200);
}

static inline void redrawMenuBackground(int lineFrom, int lineTo)
{
    int offset = lineFrom * kMenuScreenWidth;
    int length = (lineTo - lineFrom) * kMenuScreenWidth;
    memcpy(linAdr384k + offset, linAdr384k + 128 * 1024 + offset, length);
}

static inline void selectEntry(MenuEntry *entry)
{
    assert(entry && entry->onSelect);
    if (entry && entry->onSelect) {
        save68kRegisters();

        A5 = entry;
        auto func = entry->onSelect;
        SAFE_INVOKE(func);

        restore68kRegisters();
    }
}

static inline void selectEntry(int ordinal)
{
    assert(ordinal >= 0 && ordinal < 255 && ordinal < getCurrentMenu()->numEntries);

    if (ordinal >= 0 && ordinal < 255) {
        auto entry = getMenuEntry(ordinal);
        selectEntry(entry);
    }
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

static inline bool inputText(char *destBuffer, int maxLength, bool allowExtraChars = false)
{
    A0 = destBuffer;
    D0 = maxLength;
    g_allowExtraCharsFlag = allowExtraChars;

    SAFE_INVOKE(InputText);

    return D0.asWord() == 0;
}

int getStringPixelLength(const char *str, bool bigText = false);
void elideString(char *str, int maxStrLen, int maxPixels, bool bigText = false);
bool inputNumber(MenuEntry *entry, int maxDigits, int minNum, int maxNum);
