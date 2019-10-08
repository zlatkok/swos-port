#pragma once

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
    kOnReturn,
    kLeftSkip,
    kRightSkip,
    kUpSkip,
    kDownSkip,
    kColorConvertedSprite,
};

static_assert(kColorConvertedSprite == 21, "Element code enum is broken");

#pragma pack(push, 1)
struct BaseMenu {};

struct SpriteConversionTable {
    word sourceIndex;
    word destinationIndex;
    byte colorConversionTable[32];
};

struct MenuHeader
{
    void (*onInit)();
    void (*onReturn)();
    void (*onDraw)();
    int32_t selectedEntry;
};

struct MenuEnd {
    word code = static_cast<word>(-999);
};

struct MenuXY {
    word code = static_cast<word>(-998);
    word x;
    word y;
    MenuXY(word x, word y) : x(x), y(y) {}
};

struct TemplateEntry {
    word code = static_cast<word>(-997);
};

struct ResetTemplateEntry {
    word code = static_cast<word>(-996);
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

class EntryOnReturnFunction : EntryElement {
    Func func;
public:
    EntryOnReturnFunction(Func func) : EntryElement(kOnReturn), func(func) {}
};

class EntryLeftSkip : EntryElement {
    byte skipLeftEntry;
    byte directionLeft;
public:
    EntryLeftSkip(byte skipLeftEntry, byte directionLeft)
        : EntryElement(kLeftSkip), skipLeftEntry(skipLeftEntry), directionLeft(directionLeft) {}
};

class EntryRightSkip : EntryElement {
    byte skipRightEntry;
    byte directionRight;
public:
    EntryRightSkip(byte skipRightEntry, byte directionRight)
        : EntryElement(kRightSkip), skipRightEntry(skipRightEntry), directionRight(directionRight) {}
};

class EntryUpSkip : EntryElement {
    byte skipUpEntry;
    byte directionUp;
public:
    EntryUpSkip(byte skipUpEntry, byte directionUp)
        : EntryElement(kUpSkip), skipUpEntry(skipUpEntry), directionUp(directionUp) {}
};

class EntryDownSkip : EntryElement {
    byte skipDownEntry;
    byte directionDown;
public:
    EntryDownSkip(byte skipDownEntry, byte directionDown)
        : EntryElement(kDownSkip), skipDownEntry(skipDownEntry), directionDown(directionDown) {}
};

class EntryColorConvertedSprite : EntryElement {
    SpriteConversionTable *table;
public:
    EntryColorConvertedSprite(SpriteConversionTable *table) : EntryElement(kColorConvertedSprite), table(table) {}
};

#pragma pack(pop)

}
