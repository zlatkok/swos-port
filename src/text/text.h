#pragma once

constexpr int kBigFontHeight = 8;
constexpr int kSmallFontHeight = 6;

// this places characters in our SWOS-specific ASCII table
enum CharMapping : byte {
    kEmDashChar = 128,
    kAUmlautChar = 196,
    kOUmlautChar = 214,
    kUUmlautChar = 220,
    kPoundChar = 156,
    kThickVerticalBarChar = 159,
    kEuroChar = 254,
    kCursorBlockChar = 255,
    kSCaronChar = 138,
    kDCrossedChar = 208,
    kCCaronChar = 200,
    kCAcuteAccentChar = 198,
    kZCaronChar = 142,
    kEszettChar = 223,
};

void drawText(int x, int y, const char *str, int maxWidth = INT_MAX, int color = kWhiteText2, bool bigFont = false, int alpha = 255);
void drawTextRightAligned(int x, int y, const char *str, int maxWidth = INT_MAX, int color = kWhiteText2, bool bigFont = false, int alpha = 255);
// x coordinate marks the center of the string
void drawTextCentered(int x, int y, const char *str, int maxWidth = INT_MAX, int color = kWhiteText2, bool bigFont = false, int alpha = 255);

int entryTextHeight(const MenuEntry& entry);

int getStringPixelLength(const char *str, bool bigFont = false);
void elideString(char *str, int maxStrLen, int maxPixels, bool bigFont = false);
void toUpper(char *str);

inline bool isLower(char c)
{
    return c >= 'a' && c <= 'z';
}

inline bool isUpper(char c)
{
    return c >= 'A' && c <= 'Z';
}
