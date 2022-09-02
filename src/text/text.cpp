#include "text.h"
#include "sprites.h"
#include "renderSprites.h"
#include "MenuEntry.h"
#include "color.h"

constexpr int kSmallFontSpace = 3;
constexpr int kBigFontSpace = 4;
constexpr int kTabSpace = 5;

constexpr int kBigFontOffset = 57;

constexpr int kSmallDotSprite = 6;
constexpr int kBigDotSprite = 63;
constexpr int kSmallDotWidth = 2;
constexpr int kBigDotWidth = 3;

static int charToSprite(unsigned char c, bool bigFont)
{
    int spriteIndex = 0;

    if (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z') {
        spriteIndex = ((c | 0x20) - 'a' + 18);
    } else if (c >= '0' && c <= '9') {
        spriteIndex = c - '0' + 8;
    } else switch (c) {
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
    case kEmDashChar: spriteIndex = 47; break;
    case kAUmlautChar: spriteIndex = 53; break;
    case kOUmlautChar: spriteIndex = 55; break;
    case kUUmlautChar: spriteIndex = 54; break;
    case kPoundChar: spriteIndex = 46; break;
    case kThickVerticalBarChar: spriteIndex = 56; break;
    case kCursorBlockChar: spriteIndex = 44; break;
    // latest modern SWOS additions
    case '~': return bigFont ? 1352 : 1335;
    case '`': return bigFont ? 1380 : 1379;
    case '$': return bigFont ? 1359 : 1370;
    case '&': return bigFont ? 1358 : 1341;
    case '!': return bigFont ? 1353 : 1336;
    case '|': return bigFont ? 1354 : 1337;
    case '@': return bigFont ? 1355 : 1338;
    case '#': return bigFont ? 1356 : 1339;
    case '^': return bigFont ? 1357 : 1340;
    case '<': return bigFont ? 1361 : 1343;
    case '>': return bigFont ? 1362 : 1344;
    case '"': return bigFont ? 1363 : 1345;
    case '_': return bigFont ? 1364 : 1346;
    case '=': return bigFont ? 1374 : 1373;
    case '[': return bigFont ? 1377 : 1375;
    case ']': return bigFont ? 1378 : 1376;
    case '{': return bigFont ? 1385 : 1383;
    case '}': return bigFont ? 1386 : 1384;
    case '\\': return bigFont ? 1382 : 1381;
    case kEuroChar: return bigFont ? 1360 : 1342;
    case kSCaronChar: return bigFont ? 1365 : 1347;
    case kDCrossedChar: return bigFont ? 1366 : 1348;
    case kCCaronChar: return bigFont ? 1367 : 1349;
    case kCAcuteAccentChar: return bigFont ? 1368 : 1350;
    case kZCaronChar: return bigFont ? 1369 : 1351;
    case kEszettChar: return bigFont ? 1372 : 1371;
    }

    if (spriteIndex && bigFont)
        spriteIndex += kBigFontOffset;

    return spriteIndex;
}

static bool isBlank(char c)
{
    return c == ' ';
}

static int charSpriteWidth(char c, bool bigFont)
{
    assert(static_cast<unsigned char>(c) >= ' ' || c == '\t');

    if (isBlank(c))
        return bigFont ? kBigFontSpace : kSmallFontSpace;
    else if (c == '\t')
        return kTabSpace;

    auto spriteIndex = charToSprite(c, bigFont);
    return spriteIndex ? getSprite(spriteIndex).width : 0;
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
    assert(charSpriteWidth('.', false) == kSmallDotWidth &&
        charSpriteWidth('.', true) == kBigDotWidth);

    int len = 0;
    int ellipsisWidth = 3 * (bigFont ? kBigDotWidth : kSmallDotWidth);

    constexpr int kPrevWidthSize = 8;
    int previousWidths[kPrevWidthSize];

    maxWidth = std::min(maxWidth, kMenuScreenWidth - x);

    int i = 0;
    for (; str[i]; i++) {
        int charWidth = charSpriteWidth(str[i], bigFont);
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

static void setTextColor(int color)
{
    assert(color >= 0 && color < 16);

    const auto& colorRgb = color == 0 || color == 2 ? Color{ 255, 255, 255 } : kMenuPalette[color];
    setMenuSpritesColor(colorRgb);
}

static void drawText(int x, int y, const char *str, const char *limit, int color, bool bigFont, bool addEllipsis, int alpha = 255)
{
    setTextColor(color);

    int fontHeight = bigFont ? kBigFontHeight : kSmallFontHeight;

    while (str < limit) {
        auto c = *str++;
        if (isBlank(c)) {
            x += bigFont ? kBigFontSpace : kSmallFontSpace;
        } else if (c == '\t') {
            x += kTabSpace;
        } else if (int spriteIndex = charToSprite(c, bigFont)) {
            const auto& sprite = getSprite(spriteIndex);
            // account for characters with diacritics, they will have 1 extra pixel at the top
            int cy = sprite.height - fontHeight;
            drawCharSprite(spriteIndex, x, y - cy, alpha);
            x += sprite.width;
        }
    }

    if (addEllipsis) {
        int dotSprite = bigFont ? kBigDotSprite : kSmallDotSprite;
        auto dotSpriteWidth = getSprite(dotSprite).width;
        drawCharSprite(dotSprite, x, y, alpha);
        x += dotSpriteWidth;
        drawCharSprite(dotSprite, x, y, alpha);
        x += dotSpriteWidth;
        drawCharSprite(dotSprite, x, y, alpha);
    }
}

void drawText(int x, int y, const char *str, int maxWidth /* = INT_MAX */, int color /* = kWhiteText2 */,
    bool bigFont /* = false */, int alpha /* = 255 */)
{
    maxWidth = maxWidth <= 0 ? INT_MAX : maxWidth;

    auto elisionInfo = getElisionInfo(x, maxWidth, str, bigFont);
    auto end = elisionInfo.start + elisionInfo.stringLength;
    drawText(x, y, elisionInfo.start, end, color, bigFont, elisionInfo.needsEllipsis, alpha);
}

void drawTextRightAligned(int x, int y, const char *str, int maxWidth /* = INT_MAX */, int color /* = kWhiteText2 */,
    bool bigFont /* = false */, int alpha /* = 255 */)
{
    maxWidth = maxWidth <= 0 ? INT_MAX : maxWidth;
    int textLength = getStringPixelLength(str, bigFont);
    int tempX = x - std::min(textLength, maxWidth);
    auto elisionInfo = getElisionInfo(tempX, maxWidth, str, bigFont);
    auto end = elisionInfo.start + elisionInfo.stringLength;
    drawText(x - elisionInfo.pixelWidth, y, elisionInfo.start, end, color, bigFont, elisionInfo.needsEllipsis, alpha);
}

void drawTextCentered(int x, int y, const char *str, int maxWidth /* = INT_MAX */, int color /* = kWhiteText2 */,
    bool bigFont /* = false */, int alpha /* = 255 */)
{
    maxWidth = maxWidth <= 0 ? INT_MAX : maxWidth;
    int textLength = getStringPixelLength(str, bigFont);
    int tempX = x - (std::min(textLength, maxWidth) + 1) / 2;
    auto elisionInfo = getElisionInfo(tempX, maxWidth, str, bigFont);
    x -= (elisionInfo.pixelWidth + 1) / 2;
    auto end = elisionInfo.start + elisionInfo.stringLength;
    drawText(x, y, elisionInfo.start, end, color, bigFont, elisionInfo.needsEllipsis, alpha);
}

int entryTextHeight(const MenuEntry& entry)
{
    return entry.bigFont() ? kBigFontHeight : kSmallFontHeight;
}

int getStringPixelLength(const char *str, bool bigFont /* = false */)
{
    int len = 0;

    for (char c; c = *str; str++)
        len += charSpriteWidth(c, bigFont);

    return len;
}

// Ensures that a string fits inside a given pixel limitation.
// If not, replaces last characters with "..." in a way that the string will fit.
void elideString(char *str, int maxStrLen, int maxPixels, bool bigFont /* = false */)
{
    assert(str && maxPixels);

    int dotWidth = charSpriteWidth('.', bigFont);

    constexpr int kNumDotsInEllipsis = 3;
    int ellipsisWidth = kNumDotsInEllipsis * dotWidth;

    int len = 0;
    std::array<int, kNumDotsInEllipsis> prevWidths = {};

    for (int i = 0; str[i]; i++) {
        auto c = str[i];

        int charWidth = charSpriteWidth(c, bigFont);

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
