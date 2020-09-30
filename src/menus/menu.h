#pragma once

#include "menuCodes.h"
#include "util.h"
#include "render.h"

constexpr int kMenuStringLength = 70;
constexpr int kMenuOffset = 8;

static inline void showError(const char *error)
{
    A0 = error;
    ShowErrorMenu();
}

static inline bool showContinueAbortPrompt(const char *header, const char *continueText,
    const char *abortText, const std::vector<const char *>& message)
{
    assert(message.size() < 7);

    A0 = header;
    A2 = continueText;
    A3 = abortText;

    char buf[512];

    char numLines = 0;
    auto p = buf + 1;
    auto sentinel = buf + sizeof(buf) - 1;

    for (auto line : message) {
        numLines++;
        do {
            if (p >= sentinel)
                break;
            *p++ = *line;
        } while (*line++);
    }

    *p = '\0';
    buf[0] = numLines;

    A1 = buf;

    DoContinueAbortMenu();

    return D0 == 0;
}

static inline Menu *getCurrentMenu()
{
    return reinterpret_cast<Menu *>(swos.g_currentMenu);
}

static inline MenuEntry *getMenuEntry(int ordinal)
{
    assert(ordinal >= 0 && ordinal < 255);

    auto ptr = reinterpret_cast<char *>(swos.g_currentMenu) + sizeof(Menu) + ordinal * sizeof(MenuEntry);
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

static inline void drawMenuText(int x, int y, const char *text, int color = kWhiteText2)
{
    D1 = x;
    D2 = y;
    D3 = color;
    A1 = &swos.smallCharsTable;
    A0 = text;
    DrawMenuText();
}

// coordinates mark the center of the string
static inline void drawMenuTextCentered(int x, int y, const char *text, int color = kWhiteText2)
{
    D1 = x;
    D2 = y;
    D3 = color;
    A1 = &swos.smallCharsTable;
    A0 = text;
    SAFE_INVOKE(DrawMenuTextCentered);
}

#include "sprites.h"
static inline void drawMenuSprite(int x, int y, int index)
{
    D0 = index;
    D1 = x;
    D2 = y;
    drawSprite(index, x, y);
}

static inline void redrawMenuBackground()
{
    memcpy(swos.linAdr384k, swos.linAdr384k + 128 * 1024, 320 * 200);
}

static inline void redrawMenuBackground(int lineFrom, int lineTo)
{
    int offset = lineFrom * kMenuScreenWidth;
    int length = (lineTo - lineFrom) * kMenuScreenWidth;
    memcpy(swos.linAdr384k + offset, swos.linAdr384k + 128 * 1024 + offset, length);
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

static inline void setCurrentEntry(int ordinal)
{
    highlightEntry(ordinal);
}

static inline bool inputText(char *destBuffer, int maxLength, bool allowExtraChars = false)
{
    A0 = destBuffer;
    D0 = maxLength;
    swos.g_allowExtraCharsFlag = allowExtraChars;

    SAFE_INVOKE(InputText);

    return D0.asWord() == 0;
}

void showMenu(const BaseMenu& menu);
void upackMenu(const void *src, char *dst = reinterpret_cast<char *>(swos.g_currentMenu));
void restoreMenu(const void *menu, int selectedEntry);
void activateMenu(const void *menu);
const void *getCurrentPackedMenu();

bool inputNumber(MenuEntry *entry, int maxDigits, int minNum, int maxNum);
