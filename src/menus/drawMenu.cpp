#include "drawMenu.h"
#include "menus.h"
#include "menuBackground.h"
#include "menuItemRenderer.h"
#include "sprites.h"
#include "text.h"
#include "windowManager.h"

static void clearAllItemsDrawnFlag();
static void executeMenuOnDrawFunction();
static void drawMenuItems();
static void drawSelectedFrame();
static void drawMenuItemBackground(MenuEntry *entry);
static void drawMenuItemContent(MenuEntry *entry);
static void drawStringMenuItem(MenuEntry *entry, const char *string);
static void drawStringTableMenuItem(MenuEntry *entry);
static void drawMultilineTextMenuItem(MenuEntry *entry);
static void drawNumberMenuItem(MenuEntry *entry);
static void drawSpriteMenuItem(MenuEntry *entry, int spriteIndex);
static void drawColorConvertedSpriteMenuItem(MenuEntry *entry);
static void executeEntryContentFunction(MenuEntry *entry);
static const CharTable *getCharTable(const MenuEntry *entry);
static void getTextBox(int& x, int& y, int& width, const MenuEntry *entry, int charHeight);

constexpr int kTooSmallHeightForFrame = 8;

void drawMenu()
{
    // must save A5 for InputText(), old DrawMenu accidentally sets it to the selected entry (which happened to be the original value)
    auto savedA5 = A5;

    drawMenuBackground();
    clearAllItemsDrawnFlag();
    executeMenuOnDrawFunction();
    drawMenuItems();
    drawSelectedFrame();
    flipInMenu();

    A5 = savedA5;
}

void SWOS::DrawMenu()
{
    drawMenu();
}

// in:
//     A5 -> menu item to draw
//
void SWOS::DrawMenuItem()
{
    drawMenuItem(A5.asMenuEntry());
}

void drawMenuItem(MenuEntry *entry)
{
    assert(entry);

    if (entry->invisible)
        return;

    A5 = entry;

    entry->drawn = 1;
    entry->beforeDraw();

    // check the invisible flag again, beforeDraw() routine might decide not to draw the item
    if (entry->invisible)
        return;

    drawMenuItemBackground(entry);
    drawMenuItemContent(entry);

    entry->afterDraw();
}

void flipInMenu()
{
    frameDelay(2);
    updateScreen();
}

static void clearAllItemsDrawnFlag()
{
    auto entry = getCurrentMenu()->entries();
    auto sentinelEntry = entry + getCurrentMenu()->numEntries;

    for (; entry < sentinelEntry; entry++)
        entry->drawn = 0;
}

static void executeMenuOnDrawFunction()
{
    D6 = getCurrentMenu();
    auto func = D6.as<Menu *>()->onDraw;
    func();
}

static bool shouldBlink()
{
    return SDL_GetTicks() / 256 & 1;
}

static void drawMenuItems()
{
    auto entry = getCurrentMenu()->entries();
    auto sentinelEntry = entry + getCurrentMenu()->numEntries;

    for (; entry < sentinelEntry; entry++) {
        bool textBlinking = entry->type != kEntryNoForeground && entry->stringFlags & kBlinkText;
        bool skipText = textBlinking && shouldBlink();

        if (skipText) {
            assert(entry->type == kEntryString);

            if (entry->bg.entryColor) {
                auto savedText = entry->fg.string;
                entry->fg.string = nullptr;
                drawMenuItem(entry);
                entry->fg.string = savedText;
            }
        } else {
            drawMenuItem(entry);
        }
    }
}

static void drawSelectedFrame()
{
    auto entry = getCurrentMenu()->selectedEntry;
    if (entry && entry->width && entry->height) {
        constexpr int kFlashInterval = 32;
        static Uint32 s_lastFlashTime;

        auto color = swos.kColorTableShine[swos.menuCursorFrame & 7];

        if (cursorFlashingEnabled()) {
            auto now = SDL_GetTicks();
            if (now - s_lastFlashTime > kFlashInterval) {
                swos.menuCursorFrame++;
                s_lastFlashTime = now;
            }
        }

        drawMenuItemInnerFrame(entry->x, entry->y, entry->width, entry->height, color);
    }
}

static void drawMenuItemBackground(MenuEntry *entry)
{
    assert(entry);

    switch (entry->background) {
    case kEntryFrameAndBackColor:
        if (entry->backgroundColor()) {
            drawMenuItemSolidBackground(entry);
            if (entry->height > kTooSmallHeightForFrame)
                drawMenuItemOuterFrame(entry);
            if (auto innerFrameColor = entry->innerFrameColor())
                drawMenuItemInnerFrame(entry->x + 1, entry->y + 1, entry->width - 2, entry->height - 2, innerFrameColor);
        }
        break;
    case kEntrySprite1:
        drawMenuSprite(entry->x, entry->y, entry->bg.spriteIndex);
        break;
    case kEntryBackgroundFunction:
        D1 = entry->x;
        D2 = entry->y;
        A5 = entry;
        entry->bg.entryFunc();
        break;
    case kEntryNoBackground:
        break;
    default:
        assert(false);
    }
}

static void drawMenuItemContent(MenuEntry *entry)
{
    assert(entry);

    switch (entry->type) {
    case kEntryString:
        drawStringMenuItem(entry, entry->string());
        break;
    case kEntryStringTable:
        drawStringTableMenuItem(entry);
        break;
    case kEntryMultilineText:
        drawMultilineTextMenuItem(entry);
        break;
    case kEntryNumber:
        drawNumberMenuItem(entry);
        break;
    case kEntrySprite2:
        drawSpriteMenuItem(entry, entry->fg.spriteIndex);
        break;
    case kEntryColorConvertedSprite:
        drawColorConvertedSpriteMenuItem(entry);
        break;
    case kEntryContentFunction:
        executeEntryContentFunction(entry);
        break;
    case kEntryNoForeground:
        break;
    default:
        assert(false);
    }
}

static void drawStringMenuItem(MenuEntry *entry, const char *string)
{
    assert(entry && (entry->type == kEntryString || entry->type == kEntryStringTable || entry->type == kEntryNumber));

    if (string) {
        auto charTable = getCharTable(entry);
        bool bigFont = charTable == &swos.bigCharsTable;

        int x = entry->x;
        int y = entry->y;
        int width = entry->width;
        int color = entry->solidTextColor();

        if (entry->stringFlags & kTextLeftAligned) {
            getTextBox(x, y, width, entry, charTable->charHeight);
            drawMenuText(x, y, string, width, color, bigFont);
        } else if (entry->stringFlags & kTextRightAligned) {
            getTextBox(x, y, width, entry, charTable->charHeight);
            x += width;
            drawMenuTextRightAligned(x, y, string, width, color, bigFont);
        } else {
            // for some reason, centered text doesn't reset delta color
            getTextBox(x, y, width, entry, charTable->charHeight);
            x += (width + 1) / 2;
            drawMenuTextCentered(x, y, string, width, color, bigFont);
        }
    }
}

static void drawStringTableMenuItem(MenuEntry *entry)
{
    assert(entry);

    auto initialValue = fetch(&entry->fg.stringTable->initialValue);
    auto stringIndex = entry->fg.stringTable->index.fetch() - initialValue;
    auto string = (*entry->fg.stringTable)[stringIndex];
    drawStringMenuItem(entry, string);
}

static void drawMultilineTextMenuItem(MenuEntry *entry)
{
    assert(entry);

    if (entry->fg.multiLineText) {
        int color = entry->solidTextColor();

        auto charTable = getCharTable(entry);
        bool bigFont = charTable == &swos.bigCharsTable;

        auto text = entry->fg.multiLineText.asConstCharPtr() + 1;
        auto numLines = text[-1];

        assert(numLines > 0 && entry->height >= numLines * charTable->charHeight);

        int x = entry->x;
        int y = entry->y;
        int width = entry->width;

        getTextBox(x, y, width, entry, charTable->charHeight);
        int frameThickness = x - entry->x;

        int slackSpace = entry->height - numLines * charTable->charHeight - 2 * frameThickness;
        auto spaceBetweenLines = std::div(slackSpace, numLines + 1);    // +1 since we start (and end) with a free space

        x += width / 2;
        y = entry->y + frameThickness + spaceBetweenLines.quot + (spaceBetweenLines.rem-- > 0);

        while (numLines--) {
            drawMenuTextCentered(x, y, text, width, color, bigFont);
            y += spaceBetweenLines.quot + charTable->charHeight + (spaceBetweenLines.rem-- > 0);
            while (*text++)
                ;
        }
    }
}

static void drawNumberMenuItem(MenuEntry *entry)
{
    assert(entry);

    auto buf = getCurrentMenu()->endOfMenuPtr.asAlignedCharPtr();
    SDL_itoa(entry->fg.number, buf, 10);
    drawStringMenuItem(entry, buf);
}

static void drawSpriteMenuItem(MenuEntry *entry, int spriteIndex)
{
    assert(entry && (entry->type == kEntrySprite2 || entry->type == kEntryColorConvertedSprite));

    if (spriteIndex) {
        int spriteWidth, spriteHeight;
        std::tie(spriteWidth, spriteHeight) = getSpriteDimensions(spriteIndex);
        int x = entry->x + entry->width / 2 - spriteWidth / 2;
        int y = entry->y + entry->height / 2 - spriteHeight / 2;
        drawSprite(spriteIndex, x, y);
    }
}

static void drawColorConvertedSpriteMenuItem(MenuEntry *entry)
{
    assert(entry && entry->fg.spriteCopy);

    auto data = entry->fg.spriteCopy.as<word *>();
    auto srcIndex = fetch(data);
    auto dstIndex = fetch(data + 1);

    D0 = srcIndex;
    D1 = dstIndex;
    CopyWholeSprite();

//TODO

    drawSpriteMenuItem(entry, dstIndex);
}

static void executeEntryContentFunction(MenuEntry *entry)
{
    assert(entry);

    D1 = entry->x;
    D2 = entry->y;
    entry->fg.contentFunction();
}

static const CharTable *getCharTable(const MenuEntry *entry)
{
    assert(entry);

    auto useBigCharTable = (entry->stringFlags >> 4) & 1;
    return useBigCharTable ? &swos.bigCharsTable : &swos.smallCharsTable;
}

static void getTextBox(int& x, int& y, int& width, const MenuEntry *entry, int charHeight)
{
    assert(entry && (entry->type == kEntryString || entry->type == kEntryStringTable ||
        entry->type == kEntryNumber || entry->type == kEntryMultilineText));

    int height = entry->height;

    if (entry->background == kEntryFrameAndBackColor && entry->backgroundColor()) {
        if (entry->height > kTooSmallHeightForFrame) {
            x++;
            y++;
            width -= 2;
            height -= 2;
        }
        if (entry->innerFrameColor()) {
            x++;
            y++;
            width -= 2;
            height -= 2;
        }
    }

    int verticalSlack = height - charHeight;
    if (verticalSlack > 1)
        verticalSlack = verticalSlack == 3 ? 2 : verticalSlack / 2;

    y += verticalSlack;
}
