#pragma once

namespace SWOS_Menu {

enum EntryElementCode : int16_t
{
    kEndOfMenu = -999,
    kMenuXY = -998,
    kFillTemplate = -997,
    kResetTemplate = -996,
    kEndOfEntry = 0,
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
    kMultilineText,
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
    kCustomBackgroundFuncNative,
    kCustomForegroundFuncNative,
    kStringNative,
    kStringTableNative,
    kMultilineTextNative,
    kOnSelectNative,
    kOnSelectWithMaskNative,
    kBeforeDrawNative,
    kAfterDrawNative,
    kColorConvertedSpriteNative,
};

static_assert(kColorConvertedSprite == 21, "Element code enum is broken");

#pragma pack(push, 1)

struct SpriteConversionTable
{
    word sourceIndex;
    word destinationIndex;
    byte colorConversionTable[32];
};

struct MenuHeader
{
    SwosProcPointer onInit;
    SwosProcPointer onReturn;
    SwosProcPointer onDraw;
    int32_t selectedEntry;
};

constexpr int32_t kMenuHeaderV2Mark = -100'000;

struct MenuHeaderV2
{
    int32_t mark;
    void (*onInit)();
    void (*onReturn)();
    void (*onDraw)();
    int32_t initialEntry;
    bool nativePtr[4];

    const int16_t *data() const {
        return (int16_t *)(this + 1);
    }
};

struct MenuEnd {
    word code = static_cast<word>(kEndOfMenu);
};

struct MenuXY
{
    word code = static_cast<word>(kMenuXY);
    int16_t x;
    int16_t y;
    MenuXY(int16_t x, int16_t y) : x(x), y(y) {}
};

struct TemplateEntry {
    word code = static_cast<word>(kFillTemplate);
};

struct ResetTemplateEntry {
    word code = static_cast<word>(kResetTemplate);
};

struct Entry
{
    int16_t x;
    int16_t y;
    word width;
    word height;
#ifndef DEBUG
    constexpr
#endif
    Entry(int16_t x, int16_t y, word width, word height) : x(x), y(y), width(width), height(height) {
#ifdef DEBUG
        assert(width && height);
#endif
    }
};

template<typename FuncType>
class EntryElementBase {
protected:
    EntryElementCode code;
    using Func = FuncType;
    constexpr EntryElementBase(EntryElementCode code) : code(code) {}
};

using EntryElement = EntryElementBase<SwosProcPointer>;
using EntryElementNative = EntryElementBase<void (*)()>;

struct EntryEnd : EntryElement {
    constexpr EntryEnd() : EntryElement(kEndOfEntry) {}
};

struct EntryInvisible : EntryElement {
    constexpr EntryInvisible() : EntryElement(kInvisibilityCloak) {}
};

class EntryNextPositions : EntryElement {
    byte left;
    byte right;
    byte up;
    byte down;
public:
    constexpr EntryNextPositions(byte left = -1, byte right = -1, byte up = -1, byte down = -1)
        : EntryElement(kPositions), left(left), right(right), up(up), down(down) {}
};

template<typename Base>
class EntryFunction : Base {
protected:
    typename Base::Func func;
    using typename Base::Func;
public:
    constexpr EntryFunction(typename Base::Func func, EntryElementCode code) : Base(code), func(func) {}
};

using EntryFunctionSwos = EntryFunction<EntryElement>;
using EntryFunctionNative = EntryFunction<EntryElementNative>;

class EntryCustomBackgroundFunction : EntryFunctionSwos {
    constexpr EntryCustomBackgroundFunction(Func func) : EntryFunctionSwos(func, kCustomBackgroundFunc) {}
};
class EntryCustomBackgroundFunctionNative : EntryFunctionNative {
    constexpr EntryCustomBackgroundFunctionNative(Func func)
        : EntryFunction<EntryElementNative>(func, kCustomBackgroundFunc) {}
};

class EntryColor : EntryElement {
    word color;
public:
    constexpr EntryColor(unsigned color) : EntryElement(kColor), color(color) {}
};

class EntryBackgroundSprite : EntryElement {
    word index;
public:
    constexpr EntryBackgroundSprite(unsigned index) : EntryElement(kBackgroundSprite), index(index) {}
};

struct EntryCustomForegroundFunction : EntryFunctionSwos {
    constexpr EntryCustomForegroundFunction(Func func) : EntryFunctionSwos(func, kCustomForegroundFunc) {}
};
struct EntryCustomForegroundFunctionNative : EntryFunctionNative {
    constexpr EntryCustomForegroundFunctionNative(Func func) : EntryFunctionNative(func, kCustomForegroundFuncNative) {}
};

class EntryText : EntryElement {
    word flags;
    SwosDataPointer<const char> str;
public:
    constexpr EntryText(word flags, SwosDataPointer<const char> str) : EntryElement(kString), flags(flags), str(str) {}
};

class EntryTextNative : EntryElementNative {
    word flags;
    const char *str;
public:
    constexpr EntryTextNative(word flags, const char *str) : EntryElementNative(kStringNative), flags(flags), str(str) {}
};

class EntryForegroundSprite : EntryElement {
    word index;
public:
    constexpr EntryForegroundSprite(unsigned index) : EntryElement(kForegroundSprite), index(index) {}
};

class EntryStringTable : EntryElement {
    word flags;
    SwosDataPointer<StringTable> stringTable;
public:
    constexpr EntryStringTable(word flags, SwosDataPointer<StringTable> stringTable) : EntryElement(kStringTable), flags(flags), stringTable(stringTable) {}
};

class EntryStringTableNative : EntryElementNative {
    word flags;
    const StringTableNative *stringTable;
public:
    constexpr EntryStringTableNative(word flags, const StringTableNative *stringTable) : EntryElementNative(kStringTableNative), flags(flags), stringTable(stringTable) {}
};

class EntryMultilineTextNative : EntryElementNative {
    word flags;
    void *stringList;
public:
    constexpr EntryMultilineTextNative(word flags, void *stringList) : EntryElementNative(kMultilineText), flags(flags), stringList(stringList) {}
};

class EntryMultilineText : EntryElement {
    word flags;
    void *stringList;
public:
    constexpr EntryMultilineText(word flags, void *stringList) : EntryElement(kMultilineText), flags(flags), stringList(stringList) {}
};

class EntryNumber : EntryElement {
    word flags;
    word number;
public:
    constexpr EntryNumber(word flags, word number) : EntryElement(kInteger), flags(flags), number(number) {}
};

struct EntryOnSelectFunction : EntryFunctionSwos {
    constexpr EntryOnSelectFunction(Func func) : EntryFunctionSwos(func, kOnSelect) {}
};
struct EntryOnSelectFunctionNative : EntryFunctionNative {
    constexpr EntryOnSelectFunctionNative(Func func) : EntryFunctionNative(func, kOnSelectNative) {}
};

class EntryOnSelectFunctionWithMask : EntryElement {
    word mask;
    Func func;
public:
    constexpr EntryOnSelectFunctionWithMask(Func func, word mask) : EntryElement(kOnSelectWithMask), mask(mask), func(func) {}
};

class EntryOnSelectFunctionWithMaskNative : EntryElementNative {
    word mask;
    Func func;
public:
    constexpr EntryOnSelectFunctionWithMaskNative(Func func, word mask) : EntryElementNative(kOnSelectWithMaskNative), mask(mask), func(func) {}
};

struct EntryBeforeDrawFunction : EntryFunctionSwos {
    constexpr EntryBeforeDrawFunction(Func func) : EntryFunctionSwos(func, kBeforeDraw) {}
};
struct EntryBeforeDrawFunctionNative : EntryFunctionNative {
    constexpr EntryBeforeDrawFunctionNative(Func func) : EntryFunctionNative(func, kBeforeDrawNative) {}
};

struct EntryAfterDrawFunction : EntryFunctionSwos {
    constexpr EntryAfterDrawFunction(Func func) : EntryFunctionSwos(func, kAfterDraw) {}
};
struct EntryAfterDrawFunctionNative : EntryFunctionNative {
    constexpr EntryAfterDrawFunctionNative(Func func) : EntryFunctionNative(func, kAfterDrawNative) {}
};

class EntryLeftSkip : EntryElement {
    byte skipLeftEntry;
    byte directionLeft;
public:
    constexpr EntryLeftSkip(byte skipLeftEntry, byte directionLeft)
        : EntryElement(kLeftSkip), skipLeftEntry(skipLeftEntry), directionLeft(directionLeft) {}
};

class EntryRightSkip : EntryElement {
    byte skipRightEntry;
    byte directionRight;
public:
    constexpr EntryRightSkip(byte skipRightEntry, byte directionRight)
        : EntryElement(kRightSkip), skipRightEntry(skipRightEntry), directionRight(directionRight) {}
};

class EntryUpSkip : EntryElement {
    byte skipUpEntry;
    byte directionUp;
public:
    constexpr EntryUpSkip(byte skipUpEntry, byte directionUp)
        : EntryElement(kUpSkip), skipUpEntry(skipUpEntry), directionUp(directionUp) {}
};

class EntryDownSkip : EntryElement {
    byte skipDownEntry;
    byte directionDown;
public:
    constexpr EntryDownSkip(byte skipDownEntry, byte directionDown)
        : EntryElement(kDownSkip), skipDownEntry(skipDownEntry), directionDown(directionDown) {}
};

class EntryColorConvertedSprite : EntryElement {
    SpriteConversionTable *table;
public:
    constexpr EntryColorConvertedSprite(SpriteConversionTable *table)
        : EntryElement(kColorConvertedSprite), table(table) {}
};

#pragma pack(pop)

}
