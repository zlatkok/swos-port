#include "drawMenu.h"
#include "menus.h"
#include "unpackMenu.h"
#include "menuBackground.h"
#include "menuItemRenderer.h"
#include "sprites.h"
#include "renderSprites.h"
#include "text.h"
#include "gameFieldMapping.h"
#include "timer.h"

constexpr int kTooSmallHeightForFrame = 8;

static bool m_flashMenuCursor = true;

static bool m_fadeIn;

struct LocalSprite {
    LocalSprite(int width, int height, SDL_Texture *texture, bool cleanUp) :
        width(width), height(height), texture(texture), cleanUp(cleanUp) {}
    int width;
    int height;
    SDL_Texture *texture;
    bool cleanUp;
};
static std::vector<LocalSprite> m_menuLocalSprites;

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
static void drawMenuLocalSprite(MenuEntry *entry, int spriteIndex);
static void drawBoolOption(MenuEntry *entry);
static void executeEntryContentFunction(MenuEntry *entry);
static void getTextBox(int& x, int& y, int& width, const MenuEntry *entry, int charHeight);

void drawMenu(bool updateScreen /* = true */)
{
    // must save A5 for InputText(), old DrawMenu accidentally sets it to the selected entry (which happened to be the original value)
    auto savedA5 = A5;

    drawMenuBackground();
    clearAllItemsDrawnFlag();
    executeMenuOnDrawFunction();
    drawMenuItems();
    drawSelectedFrame();
    if (updateScreen)
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

// in:
//      D0  - max. number of items to draw
//      A5 -> first entry to draw
void SWOS::DrawMultipleItems()
{
    auto entry = A5.asMenuEntry();
    for (int i = 0; i < D0.asInt16(); i++)
        drawMenuItem(entry++);
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
    menuFrameDelay();

    if (m_fadeIn) {
        menuFadeIn();
        m_fadeIn = 0;
    } else {
        updateScreen();
    }
}

void menuFadeIn()
{
    fadeIn([]() { drawMenu(false); });
}

void menuFadeOut()
{
    fadeOut([]() { drawMenu(false); });
}

void enqueueMenuFadeIn()
{
    m_fadeIn = true;
}

void registerMenuLocalSprite(int width, int height, SDL_Texture *texture, bool cleanUp /* = true */)
{
    assert(texture);
    m_menuLocalSprites.emplace_back(width, height, texture, cleanUp);
}

void clearMenuLocalSprites()
{
    for (const auto& sprite : m_menuLocalSprites) {
        assert(sprite.texture);
        if (sprite.cleanUp && sprite.texture)
            SDL_DestroyTexture(sprite.texture);
    }

    m_menuLocalSprites.clear();
}

bool cursorFlashingEnabled()
{
    return m_flashMenuCursor;
}

void setFlashMenuCursor(bool flashMenuCursor)
{
    m_flashMenuCursor = flashMenuCursor;
    swos.menuCursorFrame = 0;
}

static void clearAllItemsDrawnFlag()
{
    for (auto entry = getCurrentMenu()->entries(); entry < getCurrentMenu()->sentinelEntry(); entry++)
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
    for (auto entry = getCurrentMenu()->entries(); entry < getCurrentMenu()->sentinelEntry(); entry++) {
        bool textBlinking = entry->type != kEntryNoForeground && entry->stringFlags & kBlinkText;
        bool skipText = textBlinking && shouldBlink();

        if (skipText) {
            assert(entry->type == kEntryString || entry->type == kEntrySprite2);

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
    static const std::array<byte, 8> kColorTableShine = { 2, 2, 1, 1, 7, 7, 1, 1 };

    auto entry = getCurrentMenu()->selectedEntry;
    if (entry && entry->width && entry->height) {
        constexpr int kFlashInterval = 32;
        static Uint32 s_lastFlashTime;

        auto color = kColorTableShine[swos.menuCursorFrame & 7];

        if (cursorFlashingEnabled()) {
            auto now = SDL_GetTicks();
            if (now - s_lastFlashTime > kFlashInterval) {
                swos.menuCursorFrame++;
                s_lastFlashTime = now;
            }
        }

        if (entry->width > 1 && entry->height > 1)
            drawMenuItemInnerFrame(entry->x, entry->y, entry->width, entry->height, color);
    }
}

static bool canHaveOuterFrame(const MenuEntry *entry)
{
    assert(entry->background == kEntryFrameAndBackColor && entry->backgroundColor());

    int tooSmallHeight = kTooSmallHeightForFrame;

    if (swos.g_allowShorterMenuItemsWithFrames && entry->x >= kMenuScreenWidth / 2) // AARGGHHH... I'm still throwing up
        tooSmallHeight -= 2;

    return entry->height > tooSmallHeight;
}

static void drawMenuItemBackground(MenuEntry *entry)
{
    assert(entry);

    switch (entry->background) {
    case kEntryFrameAndBackColor:
        if (entry->backgroundColor()) {
            drawMenuItemSolidBackground(entry);
            if (canHaveOuterFrame(entry))
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
        assert(false);
        logWarn("Color converted sprites not supported");
        break;
    case kEntryContentFunction:
        executeEntryContentFunction(entry);
        break;
    case kEntryMenuSpecificSprite:
        drawMenuLocalSprite(entry, entry->fg.spriteIndex);
        break;
    case kEntryBoolOption:
        drawBoolOption(entry);
        break;
    case kEntryNoForeground:
        break;
    default:
        assert(false);
    }
}

static void drawStringMenuItem(MenuEntry *entry, const char *string)
{
    assert(entry && (entry->type == kEntryString || entry->type == kEntryStringTable ||
        entry->type == kEntryNumber || entry->type == kEntryBoolOption));

    if (string) {
        bool bigFont = entry->bigFont();
        int textHeight = entryTextHeight(*entry);

        int x = entry->x;
        int y = entry->y;
        int width = entry->width;
        int color = entry->solidTextColor();

        if (entry->stringFlags & kTextLeftAligned) {
            getTextBox(x, y, width, entry, textHeight);
            if (entry->background != kEntryNoBackground)
                x += 2;
            drawText(x, y, string, width, color, bigFont);
        } else if (entry->stringFlags & kTextRightAligned) {
            getTextBox(x, y, width, entry, textHeight);
            x += width;
            drawTextRightAligned(x, y, string, width, color, bigFont);
        } else {
            // for some reason, centered text doesn't reset delta color
            getTextBox(x, y, width, entry, textHeight);
            x += (width + 1) / 2;
            drawTextCentered(x, y, string, width, color, bigFont);
        }
    }
}

static void drawStringTableMenuItem(MenuEntry *entry)
{
    assert(entry);

    auto startIndex = fetch(&entry->fg.stringTable->startIndex);
    auto stringIndex = entry->fg.stringTable->index.fetch() - startIndex;
    auto string = (*entry->fg.stringTable)[stringIndex];
    drawStringMenuItem(entry, string);
}

static void drawMultilineTextMenuItem(MenuEntry *entry)
{
    assert(entry);

    if (entry->fg.multilineText) {
        int color = entry->solidTextColor();

        bool bigFont = entry->bigFont();
        int textHeight = entryTextHeight(*entry);

        auto text = entry->fg.multilineText.asConstCharPtr() + 1;
        auto numLines = text[-1];

        assert(numLines > 0 && entry->height >= numLines * textHeight);

        int x = entry->x;
        int y = entry->y;
        int width = entry->width;

        getTextBox(x, y, width, entry, textHeight);
        int frameThickness = x - entry->x;

        int slackSpace = entry->height - numLines * textHeight - 2 * frameThickness;
        auto spaceBetweenLines = std::div(slackSpace, numLines + 1);    // +1 since we start (and end) with a free space

        x += width / 2;
        y = entry->y + frameThickness + spaceBetweenLines.quot + (spaceBetweenLines.rem-- > 0);

        while (numLines--) {
            drawTextCentered(x, y, text, width, color, bigFont);
            y += spaceBetweenLines.quot + textHeight + (spaceBetweenLines.rem-- > 0);
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
        const auto& sprite = getSprite(spriteIndex);

        int x = entry->x + (entry->width - sprite.width) / 2;
        int y = entry->y + (entry->height - sprite.height) / 2;
        drawMenuSprite(spriteIndex, x, y);
    }
}

static void drawMenuLocalSprite(MenuEntry *entry, int spriteIndex)
{
    assert(entry && entry->type == kEntryMenuSpecificSprite);
    assert(static_cast<size_t>(spriteIndex) < m_menuLocalSprites.size());

    const auto& sprite = m_menuLocalSprites[spriteIndex];

    auto scale = getGameScale();
    auto widthF = static_cast<float>(sprite.width);
    auto heightF = static_cast<float>(sprite.height);

    auto x = getGameScreenOffsetX() + (entry->x + (entry->width - widthF) / 2) * scale;
    auto y = getGameScreenOffsetY() + (entry->y + (entry->height - heightF) / 2) * scale;

    SDL_FRect dst{ x, y, widthF * scale, heightF * scale };
    SDL_RenderCopyF(getRenderer(), sprite.texture, nullptr, &dst);
}

static void drawBoolOption(MenuEntry *entry)
{
    auto accessors = getBoolOptionAccessors(*entry);
    auto text = accessors.first() ? "ON" : "OFF";
    drawStringMenuItem(entry, text);
}

static void executeEntryContentFunction(MenuEntry *entry)
{
    assert(entry);

    D1 = entry->x;
    D2 = entry->y;
    entry->fg.contentFunction();
}

static void getTextBox(int& x, int& y, int& width, const MenuEntry *entry, int charHeight)
{
    assert(entry && (entry->type == kEntryString || entry->type == kEntryStringTable ||
        entry->type == kEntryNumber || entry->type == kEntryMultilineText || entry->type == kEntryBoolOption));

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
