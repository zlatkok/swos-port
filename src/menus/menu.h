#pragma once

#include "swossym.h"
#include "swos.h"

namespace SWOS_Menu {

enum EntryElementCode : word {
    kEndOfEntry,
    kInvisibilityCloak,
    kPositions,
    kCustomBackgroundFunc,
    kColor,
    kBackgroundSprite,
    kInvalid,
    kCustomForegroundFunc,
    kString,
    kForegroundSprite,
    kStringTable,
    kMultiLineText,
    kInteger,
    kOnSelect,
    kOnSelectWithMask,
    kBeforeDraw,
    kAfterDraw,
    kLeftSkip,
    kRightSkip,
    kUpSkip,
    kDownSkip,
    kColorConvertedSprite,
};

static_assert(kColorConvertedSprite == 21, "Element code enum is broken");

#pragma pack(push, 1)
struct BaseMenu {};

struct StringTable {
    int16_t *index;
    int16_t initialValue;
    // followed by string pointers
    StringTable(int16_t *index, int16_t initialValue) : index(index), initialValue(initialValue) {}
};

struct SpriteConversionTable {
    word sourceIndex;
    word destinationIndex;
    byte colorConversionTable[32];
};

struct MenuHeader
{
    void (*onInit)();
    void (*afterDraw)();
    void (*onDraw)();
    int32_t selectedEntry;
};

struct MenuEnd {
    word code = -999;
};

struct TemplateEntry {
    word code = -997;
};

struct ResetTemplateEntry {
    word code = -996;
};

struct Entry
{
    word x;
    word y;
    word width;
    word height;
    Entry(word x, word y, word width, word height) : x(x), y(y), width(width), height(height) {
        assert(width && height);
    }
};

class EntryElement {
    EntryElementCode code;
protected:
    using Func = void (*)();
    EntryElement(EntryElementCode code) : code(code) {}
};

struct EntryEnd : EntryElement {
    EntryEnd() : EntryElement(kEndOfEntry) {}
};

struct EntryInvisible : EntryElement {
    EntryInvisible() : EntryElement(kInvisibilityCloak) {}
};

class EntryNextPositions : EntryElement {
    byte left;
    byte right;
    byte up;
    byte down;
public:
    EntryNextPositions(byte left = -1, byte right = -1, byte up = -1, byte down = -1)
        : EntryElement(kPositions), left(left), right(right), up(up), down(down) {}
};

class EntryCustomBackgroundFunction : EntryElement {
    Func func;
public:
    EntryCustomBackgroundFunction(Func func) : EntryElement(kCustomBackgroundFunc), func(func) {}
};

class EntryColor : EntryElement {
    word color;
public:
    EntryColor(unsigned color) : EntryElement(kColor), color(color) {}
};

class EntryBackgroundSprite : EntryElement {
    word index;
public:
    EntryBackgroundSprite(unsigned index) : EntryElement(kBackgroundSprite), index(index) {}
};

class EntryCustomForegroundFunction : EntryElement {
    using Func = void (*)(word, word);
    Func func;
public:
    EntryCustomForegroundFunction(Func func) : EntryElement(kCustomForegroundFunc), func(func) {}
};

class EntryText : EntryElement {
    word flags;
    const char *str;
public:
    EntryText(word flags, const char *str) : EntryElement(kString), flags(flags), str(str) {}
};

class EntryForegroundSprite : EntryElement {
    word index;
public:
    EntryForegroundSprite(unsigned index) : EntryElement(kForegroundSprite), index(index) {}
};

class EntryStringTable : EntryElement {
    word flags;
    StringTable *stringTable;
public:
    EntryStringTable(word flags, StringTable *stringTable) : EntryElement(kStringTable), flags(flags), stringTable(stringTable) {}
};

class EntryMultiLineText : EntryElement {
    word flags;
    void *stringList;
public:
    EntryMultiLineText(word flags, void *stringList) : EntryElement(kMultiLineText), flags(flags), stringList(stringList) {}
};

class EntryNumber : EntryElement {
    word flags;
    word number;
public:
    EntryNumber(word flags, word number) : EntryElement(kInteger), flags(flags), number(number) {}
};

class EntryOnSelectFunction : EntryElement {
    Func func;
public:
    EntryOnSelectFunction(Func func) : EntryElement(kOnSelect), func(func) {}
};

class EntryOnSelectFunctionWithMask : EntryElement {
    word mask;
    Func func;
public:
    EntryOnSelectFunctionWithMask(Func func, word mask) : EntryElement(kOnSelectWithMask), func(func), mask(mask) {}
};

class EntryBeforeDrawFunction : EntryElement {
    Func func;
public:
    EntryBeforeDrawFunction(Func func) : EntryElement(kBeforeDraw), func(func) {}
};

class EntryAfterDrawFunction : EntryElement {
    Func func;
public:
    EntryAfterDrawFunction(Func func) : EntryElement(kAfterDraw), func(func) {}
};

class EntryLeftSkip : EntryElement {
    byte directionLeft;
    byte skipLeftEntry;
public:
    EntryLeftSkip(byte directionLeft, byte skipLeftEntry)
        : EntryElement(kLeftSkip), directionLeft(directionLeft), skipLeftEntry(skipLeftEntry) {}
};

class EntryRightSkip : EntryElement {
    byte directionRight;
    byte skipRightEntry;
public:
    EntryRightSkip(byte directionRight, byte skipRightEntry)
        : EntryElement(kRightSkip), directionRight(directionRight), skipRightEntry(skipRightEntry) {}
};

class EntryUpSkip : EntryElement {
    byte directionUp;
    byte skipUpEntry;
public:
    EntryUpSkip(byte directionUp, byte skipUpEntry)
        : EntryElement(kUpSkip), directionUp(directionUp), skipUpEntry(skipUpEntry) {}
};

class EntryDownSkip : EntryElement {
    byte directionDown;
    byte skipDownEntry;
public:
    EntryDownSkip(byte directionDown, byte skipDownEntry)
        : EntryElement(kDownSkip), directionDown(directionDown), skipDownEntry(skipDownEntry) {}
};

class EntryColorConvertedSprite : EntryElement {
    SpriteConversionTable *table;
public:
    EntryColorConvertedSprite(SpriteConversionTable *table) : EntryElement(kColorConvertedSprite), table(table) {}
};

#pragma pack(pop)

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

static inline MenuEntry *getMenuEntryAddress(int ordinal)
{
    assert(ordinal >= 0 && ordinal < 256);

    D0 = ordinal;
    CalcMenuEntryAddress();

    assert(A0.asInt() != 0);
    return A0.as<MenuEntry *>();
}

static inline void drawMenuItem(MenuEntry *entry)
{
    A5 = entry;
    DrawMenuItem();
}

static inline int getCurrentEntry()
{
    auto currentMenu = reinterpret_cast<Menu *>(g_currentMenu);
    return currentMenu->selectedEntry ? currentMenu->selectedEntry->ordinal : -1;
}

static inline void setCurrentEntry(int ordinal)
{
    D0 = ordinal;
    SetCurrentEntry();
}
