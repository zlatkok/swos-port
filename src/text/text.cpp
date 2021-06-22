#include "text.h"
#include "sprites.h"

constexpr int kBigFontOffset = 57;

constexpr int kSmallDotSprite = 6;
constexpr int kBigDotSprite = 63;
constexpr int kSmallDotWidth = 2;
constexpr int kBigDotWidth = 3;

constexpr int kBigExclamationMarkWidth = 3;
constexpr int kBigExclamationMarkHeight = 8;
constexpr int kSmallExclamationMarkWidth = 2;
constexpr int kSmallExclamationMarkHeight = 6;

static int charToSprite(unsigned char c, bool bigFont)
{
    int spriteIndex = 0;

    if (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z')
        spriteIndex = ((c | 0x20) - 'a' + 18);
    else if (c >= '0' && c <= '9')
        spriteIndex = c - '0' + 8;
    else switch (c) {
    case '%': spriteIndex = 51; break;
    case '\'': spriteIndex = 1; break;
    case '(': spriteIndex = 2; break;
    case ')': spriteIndex = 3; break;
    case '*': spriteIndex = 45; break;
    case '+': spriteIndex = 50; break;
    case ',': spriteIndex = 4; break;
    case '-': spriteIndex = 5; break;
    case '.': spriteIndex = 6; break;
    case '/': spriteIndex = 7; break;
    case ':': spriteIndex = 49; break;
    case ';': spriteIndex = 52; break;
    case '?': spriteIndex = 48; break;
    // SWOS specific extensions
    case 128: spriteIndex = 47; break;  // em dash
    case 142: spriteIndex = 53; break;  // A umlaut
    case 153: spriteIndex = 55; break;  // O umlaut
    case 154: spriteIndex = 54; break;  // U umlaut
    case 156: spriteIndex = 46; break;  // pound sign
    case 159: spriteIndex = 56; break;  // thick vertical bar
    case 255: spriteIndex = 44; break;  // cursor block
    }

    if (spriteIndex && bigFont)
        spriteIndex += kBigFontOffset;

    return spriteIndex;
}

static int drawExclamationMark(int x, int y, int color, bool bigFont)
{
    static const uint8_t kSmallData[kSmallExclamationMarkWidth * kSmallExclamationMarkHeight] = {
        2, 0,
        2, 8,
        2, 8,
        0, 8,
        2, 0,
        0, 8,
    };
    static const uint8_t kBigData[kBigExclamationMarkWidth * kBigExclamationMarkHeight] = {
        2, 2, 0,
        2, 2, 8,
        2, 2, 8,
        2, 2, 8,
        0, 0, 0,
        2, 2, 0,
        2, 2, 8,
        0, 8, 8,
    };

    int width = bigFont ? 3 : 2;
    int height = bigFont ? 8 : 6;
    auto data = bigFont ? kBigData : kSmallData;

    auto dest = swos.linAdr384k + kMenuScreenWidth * (y + (swos.g_cameraY & 0x0f)) + x;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            auto pixel = *data++;
            if (pixel)
                dest[j] = pixel == kNearBlackText ? pixel : color;
        }
        dest += kMenuScreenWidth;
    }

    return width;
}

static bool isSpace(char c)
{
    return c == ' ' || c == '_';
}

static int charSpriteWidth(char c, const CharTable *charTable)
{
    assert(static_cast<unsigned char>(c) >= ' ');

    bool bigFont = charTable == &swos.bigCharsTable;

    if (isSpace(c))
        return charTable->spaceWidth;
    else if (c == '!')
        return bigFont ? kBigExclamationMarkWidth : kSmallExclamationMarkWidth;

    auto spriteIndex = charToSprite(c, bigFont);
    return spriteIndex ? getSpriteDimensions(spriteIndex).first : 0;
}

struct ElisionInfo {
    const char *start;
    int stringLength;
    int pixelWidth;
    bool needsEllipsis;
};

// Measures string with the given constraints. Skips whitespace from both ends.
static ElisionInfo getElisionInfo(int x, int maxWidth, const char *str, bool bigFont)
{
    assert(str);
    assert(maxWidth > 0);
    assert(charSpriteWidth('.', &swos.smallCharsTable) == kSmallDotWidth &&
        charSpriteWidth('.', &swos.bigCharsTable) == kBigDotWidth);

    const auto charTable = bigFont ? &swos.bigCharsTable : &swos.smallCharsTable;
    int len = 0;
    int ellipsisWidth = 3 * (bigFont ? kBigDotWidth : kSmallDotWidth);

    constexpr int kPrevWidthSize = 8;
    int previousWidths[kPrevWidthSize];

    maxWidth = std::min(maxWidth, kMenuScreenWidth - x);

    int i = 0;
    for (; str[i]; i++) {
        int charWidth = charSpriteWidth(str[i], charTable);
        int newLen = len + charWidth;

        if (newLen > maxWidth) {
            if (maxWidth < ellipsisWidth)
                return { str, 0, 0, false };

            while (len + ellipsisWidth > maxWidth) {
                assert(i > 0);
                int prevWidth = previousWidths[--i % kPrevWidthSize];
                len -= prevWidth;
            }

            return { str, i, len + ellipsisWidth, true };
        }

        previousWidths[i % kPrevWidthSize] = charWidth;

        len = newLen;
        x += charWidth;
    }

    return { str, i, len, false };
}

static void drawMenuText(int x, int y, const char *str, const char *limit, int color, bool bigFont, bool addEllipsis)
{
    const auto& charTable = bigFont ? swos.bigCharsTable : swos.smallCharsTable;

    while (str < limit) {
        auto c = *str++;
        if (isSpace(c)) {
            x += charTable.spaceWidth;
        } else if (c == '!') {
            x += drawExclamationMark(x, y, color, bigFont) + charTable.charSpacing;
        } else if (int spriteIndex = charToSprite(c, bigFont)) {
            drawCharSprite(spriteIndex, x, y, color);
            x += getSpriteDimensions(spriteIndex).first + charTable.charSpacing;
        }
    }

    if (addEllipsis) {
        int dotSprite = bigFont ? kBigDotSprite : kSmallDotSprite;
        auto dotSpriteWidth = getSpriteDimensions(dotSprite).first;
        drawCharSprite(dotSprite, x, y, color);
        x += dotSpriteWidth;
        drawCharSprite(dotSprite, x, y, color);
        x += dotSpriteWidth;
        drawCharSprite(dotSprite, x, y, color);
    }
}

void drawMenuText(int x, int y, const char *str, int maxWidth /* = INT_MAX */, int color /* = kWhiteText2 */, bool bigFont /* = false */)
{
    maxWidth = maxWidth <= 0 ? INT_MAX : maxWidth;

    auto elisionInfo = getElisionInfo(x, maxWidth, str, bigFont);
    auto end = elisionInfo.start + elisionInfo.stringLength;
    drawMenuText(x, y, elisionInfo.start, end, color, bigFont, elisionInfo.needsEllipsis);
}

void drawMenuTextRightAligned(int x, int y, const char *str, int maxWidth /* = INT_MAX */, int color /* = kWhiteText2 */, bool bigFont /* = false */)
{
    maxWidth = maxWidth <= 0 ? INT_MAX : maxWidth;
    int textLength = getStringPixelLength(str, bigFont);
    int tempX = x - std::min(textLength, maxWidth);
    auto elisionInfo = getElisionInfo(tempX, maxWidth, str, bigFont);
    auto end = elisionInfo.start + elisionInfo.stringLength;
    drawMenuText(x - elisionInfo.pixelWidth, y, elisionInfo.start, end, color, bigFont, elisionInfo.needsEllipsis);
}

void drawMenuTextCentered(int x, int y, const char *str, int maxWidth /* = INT_MAX */, int color /* = kWhiteText2 */, bool bigFont /* = false */)
{
    maxWidth = maxWidth <= 0 ? INT_MAX : maxWidth;
    int textLength = getStringPixelLength(str, bigFont);
    int tempX = x - (std::min(textLength, maxWidth) + 1) / 2;
    auto elisionInfo = getElisionInfo(tempX, maxWidth, str, bigFont);
    x -= (elisionInfo.pixelWidth + 1) / 2;
    auto end = elisionInfo.start + elisionInfo.stringLength;
    drawMenuText(x, y, elisionInfo.start, end, color, bigFont, elisionInfo.needsEllipsis);
}

int getStringPixelLength(const char *str, bool bigFont /* = false */)
{
    const auto charTable = bigFont ? &swos.bigCharsTable : &swos.smallCharsTable;
    int len = 0;

    for (char c; c = *str; str++)
        len += charSpriteWidth(c, charTable);

    return len;
}

// Ensures that a string fits inside a given pixel limitation.
// If not, replaces last characters with "..." in a way that the string will fit.
void elideString(char *str, int maxStrLen, int maxPixels, bool bigFont /* = false */)
{
    assert(str && maxPixels);

    const auto charTable = bigFont ? &swos.bigCharsTable : &swos.smallCharsTable;
    int dotWidth = charSpriteWidth('.', charTable);

    constexpr int kNumDotsInEllipsis = 3;
    int ellipsisWidth = kNumDotsInEllipsis * dotWidth;

    int len = 0;
    std::array<int, kNumDotsInEllipsis> prevWidths = {};

    for (int i = 0; str[i]; i++) {
        auto c = str[i];

        int charWidth = charSpriteWidth(c, charTable);

        if (len + charWidth > maxPixels) {
            int pixelsRemaining = maxPixels - len;

            int j = prevWidths.size() - 1;

            while (true) {
                if (pixelsRemaining >= ellipsisWidth) {
                    if (maxStrLen - i >= kNumDotsInEllipsis + 1) {
                        auto dotsInsertPoint = str + i;
                        std::fill(dotsInsertPoint, dotsInsertPoint + kNumDotsInEllipsis, '.');
                        str[i + kNumDotsInEllipsis] = '\0';

                        assert(getStringPixelLength(str, bigFont) <= maxPixels);
                        return;
                    } else {
                        if (i > 0) {
                            i--;
                        } else {
                            *str = '\0';
                            return;
                        }
                    }
                } else {
                    i--;
                    if (i < 0) {
                        *str = '\0';
                        return;
                    }
                    pixelsRemaining += prevWidths[j];
                    j--;
                    assert(j >= 0 || pixelsRemaining >= ellipsisWidth);
                }
            }
        }

        len += charWidth;
        std::move(prevWidths.begin() + 1, prevWidths.end(), prevWidths.begin());
        prevWidths.back() = charWidth;
    }

    assert(getStringPixelLength(str, bigFont) <= maxPixels);
}

void toUpper(char *str)
{
    while (*str++)
        str[-1] = toupper(str[-1]);
}

// GetTextSize
//
// in:
//      A0 -> text
//      A1 -> chars table (big or small)
// out:
//      D7 - number of pixels needed to display string
//
void SWOS::GetTextSize()
{
    auto text = A0.asConstPtr();
    auto charTable = A1.as<const CharTable *>();
    auto bigFont = charTable == &swos.bigCharsTable;

    D7 = getStringPixelLength(text, bigFont);
}
