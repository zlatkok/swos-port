#include "quickConfigMenu.h"
#include "menuBackground.h"
#include "renderSprites.h"
#include "text.h"
#include "drawPrimitives.h"
#include "controls.h"
#include "scancodes.h"
#include "keyBuffer.h"
#include "joypads.h"
#include "color.h"

constexpr int kRedefineKeysHeaderY = 30;
constexpr int kRedefineKeysStartY = 70;
constexpr int kRedefineKeysPromptX = 160;
constexpr int kRedefineKeysColumn1X = 108;
constexpr int kFinalPromptY = 140;
constexpr int kWarningY = 172;

// confirmation prompt boxes
constexpr int kMargin = 20;
constexpr int kSegmentLength = 90;
constexpr int kHorizontalGap = 5;
constexpr int kHorizontalSegmentLength = 8;
constexpr int kVerticalSegmentLength = 4;

constexpr int kTopLineY = kFinalPromptY - 4;
constexpr int kBottomLineY = kFinalPromptY + 10;
constexpr int kSegment1X = kMargin;
constexpr int kSegment2X = kSegment1X + kSegmentLength + kHorizontalGap;
constexpr int kSegment3X = kSegment2X + kSegmentLength + kHorizontalGap;

enum class FinalPromptResult {
    kSuccess,
    kAbort,
    kRestart,
};

static void drawQuickConfigMenu(const QuickConfigContext& context);
static void redrawMenu(const QuickConfigContext& context, bool flip = true);
static FinalPromptResult waitForConfirmation();

bool promptForDefaultGameEvents(QuickConfigContext& context)
{
    assert(context.player == kPlayer1 || context.player == kPlayer2);
    assert(context.controls == QuickConfigControls::kKeyboard1 || context.controls == QuickConfigControls::kKeyboard2 ||
        context.controls == QuickConfigControls::kJoypad);

    context.reset();
    if (context.warningY < 0)
        context.warningY = kWarningY;
    context.redrawMenuFn = std::bind(drawQuickConfigMenu, std::cref(context));

    logInfo("Configuring %s for player %d", context.controls == QuickConfigControls::kJoypad ? "joypad" : "keyboard",
        context.player == kPlayer1 ? 1 : 2);
    if (context.controls == QuickConfigControls::kJoypad)
        logInfo("Joypad: #%d", context.joypadIndex);

    redrawMenu(context);

    waitForKeyboardAndMouseIdle();

    while (true) {
        auto result = getNextControl(context);

        switch (result) {
        case QuickConfigStatus::kContinue:
            continue;
        case QuickConfigStatus::kAbort:
            return false;
        case QuickConfigStatus::kDone:
            break;
        default:
            assert(false);
        }

        redrawMenu(context);

        switch (waitForConfirmation()) {
        case FinalPromptResult::kSuccess:
            logInfo("Quick config successful");
            return true;
        case FinalPromptResult::kAbort:
            logInfo("Quick config aborted");
            return false;
        case FinalPromptResult::kRestart:
            logInfo("Restarting quick config...");
            context.reset();
            break;
        }
    }
}

static void clearWarningIfNeeded(Uint32& showWarningTicks, const QuickConfigContext& context);
static Uint32 printWarning(QuickConfigStatus status, const char *control, const QuickConfigContext& context, char *warningBuffer, size_t warningBufferSize);

QuickConfigStatus getNextControl(QuickConfigContext& context)
{
    constexpr int kWarningBufferSize = 64;
    char warningBuffer[kWarningBufferSize];

    Uint32 showWarningTicks = 0;

    redrawMenu(context);

    while (true) {
        processControlEvents();

        clearWarningIfNeeded(showWarningTicks, context);

        auto result = context.getNextControl();

        switch (result.first) {
        case QuickConfigStatus::kAbort:
        case QuickConfigStatus::kDone:
        case QuickConfigStatus::kContinue:
            return result.first;
        }

        if (result.first != QuickConfigStatus::kNoInput) {
            assert(result.second);
            showWarningTicks = printWarning(result.first, result.second, context, warningBuffer, kWarningBufferSize);
        }

        SDL_Delay(50);
    }
}

static void drawControls(const QuickConfigContext& context)
{
    constexpr int kRowGap = 10;
    constexpr int kControlX = kRedefineKeysColumn1X + 40;
    constexpr int kBlockSpriteX = kControlX - 2;

    int y = kRedefineKeysStartY;

    for (int i = 0; i < kNumDefaultGameControlEvents; i++) {
        if (context.elementNames[i][0]) {
            drawText(kControlX, y, context.elementNames[i]);
            y += kRowGap;
        }
    }

    if (context.currentSlot < kNumDefaultGameControlEvents)
        drawMenuSprite(kBlockSprite, kBlockSpriteX, y);
}

static void drawQuickConfigMenu(const QuickConfigContext& context)
{
    using namespace SwosVM;

    assert(context.player == kPlayer1 || context.player == kPlayer2);

    char buf[128];
    snprintf(buf, sizeof(buf), "SELECT %sS FOR PLAYER %d", context.selectWhat, context.player == kPlayer1 ? 1 : 2);

    drawMenuBackground();
    drawTextCentered(kRedefineKeysPromptX, kRedefineKeysHeaderY, buf);

    drawText(kRedefineKeysColumn1X, kRedefineKeysStartY, "UP:");
    drawText(kRedefineKeysColumn1X, kRedefineKeysStartY + 10, "DOWN:");
    drawText(kRedefineKeysColumn1X, kRedefineKeysStartY + 20, "LEFT:");
    drawText(kRedefineKeysColumn1X, kRedefineKeysStartY + 30, "RIGHT:");
    drawText(kRedefineKeysColumn1X, kRedefineKeysStartY + 40, "FIRE:");
    drawText(kRedefineKeysColumn1X, kRedefineKeysStartY + 50, "BENCH:");

    assert(kDefaultGameControlEvents[kDefaultControlsBenchIndex] == kGameEventBench);
    if (!context.benchRequired && context.currentSlot == kDefaultControlsBenchIndex)
        drawText(kRedefineKeysColumn1X, kRedefineKeysStartY + 60, "(HOLD FIRE TO SKIP)", -1, kYellowText);

    constexpr int kAbortY = kRedefineKeysStartY + 50 + kRedefineKeysStartY - kRedefineKeysHeaderY;

    static_assert(kWarningY >= kAbortY + 10, "Warning and abort labels overlap");

    drawTextCentered(160, kAbortY, context.abortText);

    drawControls(context);
}

static void redrawMenu(const QuickConfigContext& context, bool flip /* = true */)
{
    if (context.redrawMenuFn) {
        context.redrawMenuFn();
        if (flip)
            updateScreen();
    }
}

static void drawConfirmationMenu()
{
    drawMenuBackground();

    static const std::array<std::pair<int, const char *>, 3> kSegmentData = {{
        { kSegment1X, "ENTER/S - SAVE" }, { kSegment2X, "ESC/D - DISCARD" }, { kSegment3X, "R - RESTART" },
    }};

    const auto& color = kMenuPalette[kWhiteText2];

    for (const auto& segmentData : kSegmentData) {
        int x = segmentData.first;
        auto text = segmentData.second;

        drawHorizontalLine(x, kTopLineY, kHorizontalSegmentLength, color);
        drawVerticalLine(x, kTopLineY + 1, kVerticalSegmentLength - 1, color);
        drawHorizontalLine(x, kBottomLineY, kHorizontalSegmentLength, color);
        drawVerticalLine(x, kBottomLineY - kVerticalSegmentLength + 1, kVerticalSegmentLength - 1, color);

        drawTextCentered(x + kSegmentLength / 2 + 1, kFinalPromptY, text);

        drawHorizontalLine(x + kSegmentLength - kHorizontalSegmentLength, kTopLineY, kHorizontalSegmentLength, color);
        drawVerticalLine(x + kSegmentLength, kTopLineY + 1, kVerticalSegmentLength - 1, color);
        drawHorizontalLine(x + kSegmentLength - kHorizontalSegmentLength, kBottomLineY, kHorizontalSegmentLength, color);
        drawVerticalLine(x + kSegmentLength, kBottomLineY - kVerticalSegmentLength + 1, kVerticalSegmentLength - 1, color);
    }

    updateScreen();
}

static FinalPromptResult handleConfirmationClick(int x, int y)
{
    if (y >= kTopLineY && y <= kBottomLineY) {
        if (x >= kSegment1X && x < kSegment1X + kSegmentLength)
            return FinalPromptResult::kSuccess;
        if (x >= kSegment3X && x < kSegment3X + kSegmentLength)
            return FinalPromptResult::kRestart;
    }

    return FinalPromptResult::kAbort;
}

static FinalPromptResult waitForConfirmation()
{
    drawConfirmationMenu();

    do {
        processControlEvents();

        bool clicked;
        SDL_Scancode key;
        int x, y;

        std::tie(clicked, key, x, y) = getKeyInterruptible();
        waitForKeyboardAndMouseIdle();

        switch (key) {
        case SDL_SCANCODE_S:
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_RETURN2:
        case SDL_SCANCODE_KP_ENTER:
            return FinalPromptResult::kSuccess;
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_AC_BACK:
            return FinalPromptResult::kAbort;
        case SDL_SCANCODE_R:
            return FinalPromptResult::kRestart;
        }

        if (clicked)
            return handleConfirmationClick(x, y);

        SDL_Delay(50);
    } while (true);
}

static void clearWarningIfNeeded(Uint32& showWarningTicks, const QuickConfigContext& context)
{
    if (showWarningTicks && showWarningTicks < SDL_GetTicks()) {
        showWarningTicks = 0;
        redrawMenu(context);
    }
}

static Uint32 printWarning(QuickConfigStatus status, const char *control, const QuickConfigContext& context, char *warningBuffer, size_t warningBufferSize)
{
    constexpr Uint32 kWarningInterval = 750;

    if (status == QuickConfigStatus::kAlreadyUsed)
        snprintf(warningBuffer, warningBufferSize, "%s '%s' IS ALREADY USED", context.selectWhat, control);
    else
        snprintf(warningBuffer, warningBufferSize, "%s '%s' IS TAKEN BY PLAYER %d", context.selectWhat, control, context.player == kPlayer1 ? 2 : 1);

    auto showWarningTicks = SDL_GetTicks() + kWarningInterval;

    redrawMenu(context, false);
    drawTextCentered(kVgaWidth / 2, context.warningY, warningBuffer, -1, kYellowText);
    updateScreen();

    return showWarningTicks;
}
