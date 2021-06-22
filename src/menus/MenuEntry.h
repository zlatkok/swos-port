#pragma once

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
        SwosDataPointer<void> multiLineText;
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

    MenuEntry() {}

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
    void setNumber(int number);
    void setBackgroundColor(int color);
    void setFrameColor(int color);
    word backgroundColor() const;
    word innerFrameColor() const;
    int solidTextColor() const;
};
