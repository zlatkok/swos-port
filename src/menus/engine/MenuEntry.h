#pragma once

enum MenuEntryContent : word
{
    kEntryNoForeground = 0,
    kEntryContentFunction = 1,
    kEntryString = 2,
    kEntrySprite2 = 3,
    kEntryStringTable = 4,
    kEntryMultilineText = 5,
    kEntryNumber = 6,
    kEntryColorConvertedSprite = 7,

    // extensions
    kEntryMenuSpecificSprite,
    kEntryBoolOption,
};

enum MenuEntryBackground : word
{
    kEntryNoBackground = 0,
    kEntryBackgroundFunction = 1,
    kEntryFrameAndBackColor = 2,
    kEntrySprite1 = 3,
};

enum MenuEntryBackgrounds
{
    kNoBackground = 0,
    kNoFrame = 0,
    kGray = 7,
    kDarkBlue = 3,
    kLightBrownWithOrangeFrame = 4,
    kLightBrownWithYellowFrame = 6,
    kRed = 10,
    kPurple = 11,
    kLightBlue = 13,
    kGreen = 14,
    kYellow = 15,
};

enum MenuEntryFrameColors
{
    kGrayFrame = 0x10,
    kWhiteFrame = 0x20,
    kBlackFrame = 0x30,
    kBrownFrame = 0x40,
    kLightBrownFrame = 0x50,
    kOrangeFrame = 0x60,
    kDarkGrayFrame = 0x70,
    kNearBlackFrame = 0x80,
    kVeryDarkGreenFrame = 0x90,
    kRedFrame = 0xa0,
    kBlueFrame = 0xb0,
    kPurpleFrame = 0xc0,
    kSoftBlueFrame = 0xd0,
    kGreenFrame = 0xe0,
    kYellowFrame = 0xf0
};

#pragma pack(push, 1)
struct MenuEntry
{
    word drawn;
    word ordinal;
    word invisible;
    word disabled;
    byte leftEntry;
    byte rightEntry;
    byte upEntry;
    byte downEntry;
    byte leftDirection;
    byte rightDirection;
    byte upDirection;
    byte downDirection;
    byte leftEntryDis;
    byte rightEntryDis;
    byte upEntryDis;
    byte downEntryDis;
    int16_t x;
    int16_t y;
    word width;
    word height;
    MenuEntryBackground background;
    union BackgroundData {
        BackgroundData() {}
        SwosProcPointer entryFunc;
        word entryColor;
        word spriteIndex;
    } bg;
    MenuEntryContent type;
    word stringFlags;
    union ContentData {
        ContentData() {}
        SwosProcPointer contentFunction;
        SwosDataPointer<char> string;
        SwosDataPointer<const char> constString;
        word spriteIndex;
        SwosDataPointer<StringTable> stringTable;
        SwosDataPointer<MultilineText> multilineText;
        word number;
        SwosDataPointer<void> spriteCopy;
    } fg;
    SwosProcPointer onSelect;
    int16_t controlMask;
    SwosProcPointer beforeDraw;
    SwosProcPointer afterDraw;

    enum Direction {
        kInitialDirection,
        kUp = 0,
        kRight,
        kDown,
        kLeft,
        kNumDirections,
    };

    MenuEntry() = default;

    int centerX() const;
    int endX() const;
    int endY() const;

    const char *typeToString() const;
    bool visible() const;
    void hide();
    void show();
    void setVisible(bool visible);
    void disable();
    void enable();
    void setEnabled(bool enabled);
    bool selectable() const;
    bool isString() const;
    char *string();
    const char *string() const;
    void setString(char *str);
    void setString(const char *str);
    void copyString(const char *str);
    bool bigFont() const;
    void setNumber(int number);
    void setSprite(int index);
    void setBackgroundColor(int color);
    void setFrameColor(int color);
    word backgroundColor() const;
    word innerFrameColor() const;
    int solidTextColor() const;
};
#pragma pack(pop)

static_assert(sizeof(MenuEntry) == 56, "Destination Unknown");
