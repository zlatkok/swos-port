#include "quickConfigMenu.h"
#include "menuBackground.h"
#include "menus.h"
#include "text.h"
#include "controls.h"
#include "scancodes.h"
#include "keyBuffer.h"
#include "joypads.h"

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
static FinalPromptResult waitForConfirmation();

bool promptForDefaultGameEvents(QuickConfigContext& context)
{
    assert(context.player == kPlayer1 || context.player == kPlayer2);
    assert(context.controls == QuickConfigControls::kKeyboard1 || context.controls == QuickConfigControls::kKeyboard2 ||
        context.controls == QuickConfigControls::kJoypad);

    context.reset();
    if (context.warningY < 0)
        context.warningY = kWarningY;

    logInfo("Configuring %s for player %d", context.controls == QuickConfigControls::kJoypad ? "joypad" : "keyboard",
        context.player == kPlayer1 ? 1 : 2);
    if (context.controls == QuickConfigControls::kJoypad)
        logInfo("Joypad: #%d", context.joypadIndex);

    waitForKeyboardAndMouseIdle();

    while (true) {
        drawQuickConfigMenu(context);

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

        drawQuickConfigMenu(context);

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

static void clearWarningIfNeeded(Uint32& showWarningTicks, int warningY);
static Uint32 printWarning(QuickConfigStatus status, const char *control, const QuickConfigContext& context, char *warningBuffer, size_t warningBufferSize);

QuickConfigStatus getNextControl(QuickConfigContext& context)
{
    constexpr int kWarningBufferSize = 64;
    char warningBuffer[kWarningBufferSize];

    Uint32 showWarningTicks = 0;

    while (true) {
        processControlEvents();

        clearWarningIfNeeded(showWarningTicks, context.warningY);

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
            drawMenuText(kControlX, y, context.elementNames[i]);
            y += kRowGap;
        }
    }

    if (context.currentSlot < kNumDefaultGameControlEvents)
        drawMenuSprite(kBlockSpriteX, y, kBlockSprite);
}

static void drawQuickConfigMenu(const QuickConfigContext& context)
{
    using namespace SwosVM;

    assert(context.player == kPlayer1 || context.player == kPlayer2);

    char buf[128];
    snprintf(buf, sizeof(buf), "SELECT %sS FOR PLAYER %d", context.selectWhat, context.player == kPlayer1 ? 1 : 2);

    drawMenuBackground();
    drawMenuTextCentered(kRedefineKeysPromptX, kRedefineKeysHeaderY, buf);

    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY, "UP:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 10, "DOWN:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 20, "LEFT:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 30, "RIGHT:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 40, "FIRE:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 50, "BENCH:");

    assert(kDefaultGameControlEvents[kDefaultControlsBenchIndex] == kGameEventBench);
    if (!context.benchRequired && context.currentSlot == kDefaultControlsBenchIndex)
        drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 60, "(HOLD FIRE TO SKIP)", -1, kYellowText);

    constexpr int kAbortY = kRedefineKeysStartY + 50 + kRedefineKeysStartY - kRedefineKeysHeaderY;

    static_assert(kWarningY >= kAbortY + 10, "Warning and abort labels overlap");

    drawMenuTextCentered(160, kAbortY, context.abortText);

    drawControls(context);

    updateScreen();
}

enum LineDirection { kVertical, kHorizontal };
static void drawLine(int x, int y, int length, LineDirection direction)
{
    auto dest = swos.linAdr384k + y * kVgaWidth + x;

    if (direction == kHorizontal) {
        memset(dest, kWhiteText2, length);
    } else {
        while (length--) {
            *dest = kWhiteText2;
            dest += kVgaWidth;
        }
    }
}

static void drawConfirmationMenu()
{
    static const std::array<std::pair<int, const char *>, 3> kSegmentData = {{
        { kSegment1X, "ENTER/S - SAVE" }, { kSegment2X, "ESC/D - DISCARD" }, { kSegment3X, "R - RESTART" },
    }};

    for (const auto& segmentData : kSegmentData) {
        int x = segmentData.first;
        auto text = segmentData.second;
        drawLine(x, kTopLineY, kHorizontalSegmentLength, kHorizontal);
        drawLine(x, kTopLineY + 1, kVerticalSegmentLength - 1, kVertical);
        drawLine(x, kBottomLineY, kHorizontalSegmentLength, kHorizontal);
        drawLine(x, kBottomLineY - kVerticalSegmentLength + 1, kVerticalSegmentLength - 1, kVertical);

        drawMenuTextCentered(x + kSegmentLength / 2 + 1, kFinalPromptY, text);

        drawLine(x + kSegmentLength - kHorizontalSegmentLength + 1, kTopLineY, kHorizontalSegmentLength, kHorizontal);
        drawLine(x + kSegmentLength, kTopLineY + 1, kVerticalSegmentLength - 1, kVertical);
        drawLine(x + kSegmentLength - kHorizontalSegmentLength + 1, kBottomLineY, kHorizontalSegmentLength, kHorizontal);
        drawLine(x + kSegmentLength, kBottomLineY - kVerticalSegmentLength + 1, kVerticalSegmentLength - 1, kVertical);
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

static void clearWarningIfNeeded(Uint32& showWarningTicks, int warningY)
{
    if (showWarningTicks && showWarningTicks < SDL_GetTicks()) {
        drawMenuBackground(warningY, warningY + 13);
        showWarningTicks = 0;
        updateScreen();
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

    drawMenuBackground(context.warningY, context.warningY + 13);
    drawMenuTextCentered(kVgaWidth / 2, context.warningY, warningBuffer, -1, kYellowText);
    updateScreen();

    return showWarningTicks;
}
