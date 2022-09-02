#include "WindowModeMenuTest.h"
#include "unitTest.h"
#include "windowModeMenu.h"
#include "windowManager.h"
#include "render.h"
#include "mockWindowManager.h"
#include "mockRender.h"
#include "sdlProcs.h"
#include "controls.h"
#include "mockLog.h"
#include "menuMouse.h"
#include "menuProc.h"
#include "drawMenu.h"
#include "color.h"

#define SWOS_STUB_MENU_DATA
static int16_t m_windowResizable = 1;
static int16_t m_windowMaximized;
#include "windowMode.mnu.h"

using namespace WindowModeMenu;

constexpr size_t kSlowResolutionsListSize = 20;

static WindowModeMenuTest t;

const std::vector<std::tuple<int, int, int>> kTestWindowSizeData = {
    { 640, 480, false },
    { 320, 200, true },
    { 1920, 1080, false },
    { 743, 369, true },
};

const std::vector<SDL_DisplayMode> kDisplayModes = {
    { SDL_PIXELFORMAT_ARGB32, 3840, 2160 }, { SDL_PIXELFORMAT_ARGB32, 1920, 1080 },
    { SDL_PIXELFORMAT_ARGB32, 1400, 1050 }, { SDL_PIXELFORMAT_ARGB32, 1360, 768 },
    { SDL_PIXELFORMAT_ARGB32, 1280, 800 }, { SDL_PIXELFORMAT_ARGB32, 1280, 768 },
    { SDL_PIXELFORMAT_ARGB32, 1280, 720 }, { SDL_PIXELFORMAT_ARGB32, 1024, 768 },
    { SDL_PIXELFORMAT_ARGB32, 960, 960 }, { SDL_PIXELFORMAT_ARGB32, 800, 1280 },
    { SDL_PIXELFORMAT_ARGB32, 800, 600 }, { SDL_PIXELFORMAT_ARGB32, 720, 576 },
    { SDL_PIXELFORMAT_ARGB32, 720, 480 }, { SDL_PIXELFORMAT_ARGB32, 720, 240 },
    { SDL_PIXELFORMAT_ARGB32, 640, 480 }, { SDL_PIXELFORMAT_ARGB32, 640, 400 },
    { SDL_PIXELFORMAT_ARGB32, 640, 240 }, { SDL_PIXELFORMAT_ARGB32, 360, 480 },
    { SDL_PIXELFORMAT_ARGB32, 360, 240 }, { SDL_PIXELFORMAT_ARGB32, 320, 480 },
    { SDL_PIXELFORMAT_ARGB32, 320, 240 }, { SDL_PIXELFORMAT_ARGB32, 320, 200 },
};

const std::tuple<int, int, std::vector<SDL_Scancode>, std::vector<SDL_Scancode>, int, int> kTestCustomSizeData[] = {
    { 640, 400, { SDL_SCANCODE_1, SDL_SCANCODE_0, SDL_SCANCODE_2, SDL_SCANCODE_4, SDL_SCANCODE_RETURN, },
        { SDL_SCANCODE_7, SDL_SCANCODE_6, SDL_SCANCODE_8, SDL_SCANCODE_RETURN }, 1024, 768 },
    { 720, 960, { SDL_SCANCODE_1, SDL_SCANCODE_9, SDL_SCANCODE_2, SDL_SCANCODE_0, SDL_SCANCODE_KP_ENTER, },
        { SDL_SCANCODE_1, SDL_SCANCODE_0, SDL_SCANCODE_8, SDL_SCANCODE_0, SDL_SCANCODE_KP_ENTER, }, 1920, 1080 },
    { 800, 600, { SDL_SCANCODE_9, SDL_SCANCODE_0, SDL_SCANCODE_0, SDL_SCANCODE_ESCAPE, },
        { SDL_SCANCODE_4, SDL_SCANCODE_8, SDL_SCANCODE_0, SDL_SCANCODE_RETURN2, }, 800, 480 },
    { 800, 600, { SDL_SCANCODE_9, SDL_SCANCODE_0, SDL_SCANCODE_0, SDL_SCANCODE_ESCAPE, },
        { SDL_SCANCODE_4, SDL_SCANCODE_8, SDL_SCANCODE_0, SDL_SCANCODE_ESCAPE, }, 800, 600 },
};

void WindowModeMenuTest::init()
{
    takeOverInput();
    freezeSdlTime();

    auto hook = std::bind(&WindowModeMenuTest::verifyYellowPleaseWaitTextPresence, this);
    m_hookId = addUpdateHook(hook);

    for (int i = 0; i < kNumDisplayListsForScrollTesting; i++) {
        auto modes(kDisplayModes);
        modes.resize(5 * i);
        for (int j = 0; j < kNumScrollMethods; j++)
            m_testScrollingData.emplace_back(modes, static_cast<ScrollMethod>(j));
    }
}

void WindowModeMenuTest::finish()
{
    restoreOriginalSdlFunctionTable();
    removeUpdateHook(m_hookId);
}

void WindowModeMenuTest::defaultCaseInit()
{
    setWindowSize(kDefaultWindowWidth, kDefaultWindowHeight);
    setWindowResizable(true);

    showWindowModeMenu();
}

const char *WindowModeMenuTest::name() const
{
    return "window-mode-menu";
}

const char *WindowModeMenuTest::displayName() const
{
    return "window mode menu";
}

auto WindowModeMenuTest::getCases() -> CaseList
{
    // don't make it static since scrolling data is generated in init()
    return {
        { "test window dimensions", "win-dimensions", bind(&WindowModeMenuTest::setupWindowSizeTest),
            bind(&WindowModeMenuTest::testWindowSize), kTestWindowSizeData.size(), },
        { "test various resolution lists", "res-lists", bind(&WindowModeMenuTest::setupVariousResolutionLists),
            bind(&WindowModeMenuTest::testVariousResolutionLists), kDisplayModes.size() + 1, true,
            bind(&WindowModeMenuTest::finalizeVariousResolutionsList) },
        { "test window mode switching", "mode-switching", bind(&WindowModeMenuTest::setupModeSwitchingTest),
            bind(&WindowModeMenuTest::testModeSwitching), kNumWindowModes },
        { "test resolution scrolling", "res-list-scrolling", bind(&WindowModeMenuTest::setupResolutionListScrollingTest),
            bind(&WindowModeMenuTest::testResolutionListScrolling), m_testScrollingData.size() },
        { "test custom window size", "custom-win-size", bind(&WindowModeMenuTest::setupCustomWindowSizeTest),
            bind(&WindowModeMenuTest::testCustomWindowSize), std::size(kTestCustomSizeData) },
        { "test exit button", "window-mode-exit", nullptr, bind(&WindowModeMenuTest::testExitButton) },
        { "test resolution switch fail", "res-fail", bind(&WindowModeMenuTest::setupResolutionSwitchFailureTest),
            bind(&WindowModeMenuTest::testResolutionSwitchFailure) },
        { "test trying to set the same resolution", "dont-switch-current-res", nullptr,
            bind(&WindowModeMenuTest::testTryingToSetCurrentResolution) },
        { "test resolution list caching per screen", "res-list-caching", nullptr,
            bind(&WindowModeMenuTest::testResolutionListRebuildWhenChangingScreen) },
    };
}

void WindowModeMenuTest::setupWindowSizeTest()
{
    auto [width, height, windowResizable] = kTestWindowSizeData[m_currentDataIndex];

    setWindowSize(width, height);
    setWindowResizable(windowResizable);

    LogSilencer logSilencer;
    showWindowModeMenu();
}

void WindowModeMenuTest::testWindowSize()
{
    auto [width, height, windowResizable] = kTestWindowSizeData[m_currentDataIndex];

    assertItemIsNumber(customWidth, width);
    assertItemIsNumber(customHeight, height);
    assertItemIsStringTable(resizableOnOff, windowResizable ? "ON" : "OFF");
}

void WindowModeMenuTest::setupVariousResolutionLists()
{
    setWindowSize(kDefaultWindowWidth, kDefaultWindowHeight);

    m_displayModeList.resize(m_currentDataIndex);
    std::copy_n(kDisplayModes.begin(), m_currentDataIndex, m_displayModeList.begin());

    setFakeDisplayModesForced(m_displayModeList);

    if (m_displayModeList.empty())
        setUpWindowedMode();
    else
        setIsInFullScreenMode(true);

    bool causeDelay = m_displayModeList.size() > kSlowResolutionsListSize;
    setSetTicksDelta(causeDelay ? 500 : 1);
    m_verifyYellowText = causeDelay;
    clearTextureColorMods();

    LogSilencer logSilencer;
    showWindowModeMenu();
}

void WindowModeMenuTest::testVariousResolutionLists()
{
    for (size_t j = 0; j < kNumResolutionFieldsShort; j++) {
        int entryIndex = j + resolutionField0;
        if (!m_displayModeList.empty() && j < m_displayModeList.size()) {
            assertItemIsVisible(entryIndex);
            auto resolutionString = getResolutionString(m_displayModeList[j].w, m_displayModeList[j].h);
            assertItemIsString(entryIndex, resolutionString);
        } else {
            assertItemIsInvisible(entryIndex);
        }
    }

    bool arrowsVisible = m_displayModeList.size() > kNumResolutionFieldsShort;
    assertItemVisibility(scrollUpArrow, arrowsVisible);
    assertItemVisibility(scrollDownArrow, arrowsVisible);
    assertItemVisibility(fullScreenArrow, !m_displayModeList.empty());
    if (!m_displayModeList.empty()) {
        assertItemHasTextColor(fullScreen, kSoftBlueText);
        assertItemIsString(fullScreen, "FULL SCREEN:");
    } else {
        assertItemHasTextColor(fullScreen, kDarkGrayText);
        assertItemIsString(fullScreen, "FULL SCREEN UNAVAILABLE");
    }
    assertItemEnabled(fullScreen, !m_displayModeList.empty());
}

void WindowModeMenuTest::finalizeVariousResolutionsList()
{
    freezeSdlTime();
}

void WindowModeMenuTest::setupModeSwitchingTest()
{
    m_displayModeList.resize(5);
    std::copy_n(kDisplayModes.begin(), 5, m_displayModeList.begin());
    setFakeDisplayModesForced(m_displayModeList);

    setWindowSize(kVgaWidth * 2, kVgaHeight * 2);
    setUpMode(m_currentDataIndex);
}

void WindowModeMenuTest::testModeSwitching()
{
    auto mode = m_currentDataIndex % kNumWindowModes;
    auto nextMode = (mode + 1) % kNumWindowModes;
    auto nextNextMode = (mode + 2) % kNumWindowModes;

    size_t modes[] = { mode, nextMode, mode, nextNextMode, mode };

    for (size_t i = 0; i < std::size(modes) - 1; i++) {
        verifyMode(modes[i]);
        switchToMode(modes[i + 1]);
    }

    verifyMode(modes[std::size(modes) - 1]);
}

void WindowModeMenuTest::setupResolutionListScrollingTest()
{
    auto [displayModes, scrollMethod] = m_testScrollingData[m_currentDataIndex];

    setWindowSize(kDefaultWindowWidth, kDefaultWindowHeight);
    setUpWindowedMode();

    setFakeDisplayModesForced(displayModes);
    resetSdlInput();

    LogSilencer logSilencer;
    showWindowModeMenu();
}

void WindowModeMenuTest::testResolutionListScrolling()
{
    auto [displayModes, scrollMethod] = m_testScrollingData[m_currentDataIndex];

    int scrollExtent = (displayModes.size() + 1) * 2;
    int scrollOffset = 0;

    auto normalizeScrollOffset = [&displayModes](int scrollOffset) {
        scrollOffset = std::max(scrollOffset, 0);
        int maxOffset = std::max(0, static_cast<int>(displayModes.size()) - kNumResolutionFieldsShort);
        scrollOffset = std::min(scrollOffset, maxOffset);
        return scrollOffset;
    };

    for (int linesToScroll = 1; linesToScroll < scrollExtent; linesToScroll++) {
        for (int i = 0; i < linesToScroll; i++) {
            scrollOffset = normalizeScrollOffset(scrollOffset + 1);
            auto oldScrollOffset = std::max(scrollOffset - 1, 0);
            scrollAndVerifyTwoStepsForwardOneStepBack(linesToScroll, scrollMethod, scrollOffset, oldScrollOffset, displayModes);
        }
    }

    for (int linesToScroll = 0; linesToScroll < scrollExtent; linesToScroll++)
        scrollResolutionListOneLine(-1, scrollMethod, displayModes);

    verifyResolutionListStrings(displayModes, 0);
}

void WindowModeMenuTest::scrollAndVerifyTwoStepsForwardOneStepBack(int linesToScroll, ScrollMethod scrollMethod,
    int scrollOffset, int oldScrollOffset, const DisplayModeList& displayModes)
{
    scrollResolutionListOneLine(linesToScroll, scrollMethod, displayModes);
    verifyResolutionListStrings(displayModes, scrollOffset);
    scrollResolutionListOneLine(-linesToScroll, scrollMethod, displayModes);
    verifyResolutionListStrings(displayModes, oldScrollOffset);
    scrollResolutionListOneLine(linesToScroll, scrollMethod, displayModes);
    verifyResolutionListStrings(displayModes, scrollOffset);
}

void WindowModeMenuTest::scrollResolutionListOneLine(int direction, ScrollMethod method, const DisplayModeList& displayModes)
{
    switch (method) {
    case kArrowClick:
    case kArrowMouseWheel:
        if (displayModes.size() > kNumResolutionFieldsShort) {
            int arrowItem = direction >= 0 ? scrollDownArrow : scrollUpArrow;
            if (method == kArrowClick)
                clickItem(arrowItem);
            else
                sendMouseWheelEvent(arrowItem, -direction);
        }
        break;
    case kResolutionListMouseWheel:
        if (!displayModes.empty()) {
            static int lastResolutionEntry;
            auto resolutionItem = resolutionField0 + (lastResolutionEntry + 1) % std::min<int>(displayModes.size(), kNumResolutionFieldsShort);
            sendMouseWheelEvent(resolutionItem, -direction);
            lastResolutionEntry++;
        }
        break;
    default:
        assert(false);
        break;
    }

    processControlEvents();
    menuProc();
    drawMenu();
    resetSdlInput();
    updateMouse();      // to register mouse button up and go out of scrolling mode
}

void WindowModeMenuTest::verifyResolutionListStrings(const DisplayModeList& displayModes, int scrollOffset /* = 0 */)
{
    assert(scrollOffset >= 0);
    assert(scrollOffset <= std::max(0, static_cast<int>(displayModes.size()) - kNumResolutionFieldsShort));

    for (int i = 0; i < std::min(kNumResolutionFieldsShort, static_cast<int>(displayModes.size()) - scrollOffset); i++) {
        int entryIndex = resolutionField0 + i;
        assertItemIsVisible(entryIndex);
        auto expectedDisplayModeString = getResolutionString(displayModes[scrollOffset + i].w, displayModes[scrollOffset + i].h);
        assertItemIsString(entryIndex, expectedDisplayModeString);
    }
}

void WindowModeMenuTest::setupCustomWindowSizeTest()
{
    LogSilencer logSilencer;
    showWindowModeMenu();

    auto [startingWidth, startingHeight, widthKeys, heightKeys, endingWidth, endingHeight] = kTestCustomSizeData[m_currentDataIndex];

    setNumericItemValue(customWidth, startingWidth);
    setNumericItemValue(customHeight, startingHeight);

    setUpWindowedMode();
    setWindowSize(startingWidth, startingHeight);
}

void WindowModeMenuTest::testCustomWindowSize()
{
    auto [startingWidth, startingHeight, widthKeys, heightKeys, endingWidth, endingHeight] = kTestCustomSizeData[m_currentDataIndex];

    assertItemIsNumber(customWidth, startingWidth);
    assertItemIsNumber(customHeight, startingHeight);

    static const std::vector<SDL_Scancode> kBackspaces(4, SDL_SCANCODE_BACKSPACE);

    SWOS_UnitTest::queueKeys(kBackspaces);
    SWOS_UnitTest::queueKeys(widthKeys);
    selectItem(customWidth);

    SWOS_UnitTest::queueKeys(kBackspaces);
    SWOS_UnitTest::queueKeys(heightKeys);
    selectItem(customHeight);

    drawMenu();

    assertItemIsNumber(customWidth, endingWidth);
    assertItemIsNumber(customHeight, endingHeight);

    auto [width, height] = getWindowSize();
    assertEqual(endingWidth, width);
    assertEqual(endingHeight, height);
}

void WindowModeMenuTest::testExitButton()
{
    assertEqual(swos.g_exitMenu, 0);
    selectItem(WindowModeMenu::exit);
    assertEqual(swos.g_exitMenu, 1);
}

void WindowModeMenuTest::setupResolutionSwitchFailureTest()
{
    setFullScreenResolution(kDefaultWindowWidth * 2, kDefaultWindowHeight * 2);
    setFakeDisplayModesForced(kDisplayModes);
    failNextDisplayModeSwitch();
    showWindowModeMenu();
}

void WindowModeMenuTest::testResolutionSwitchFailure()
{
    LogSilencer logSilencer;

    selectItem(resolutionField3);
    drawMenu();

    assertEqual(getCurrentMenu()->numEntries, 2);
    assertItemIsString(1, "CONTINUE");
}

void WindowModeMenuTest::testTryingToSetCurrentResolution()
{
    setFullScreenResolution(kDisplayModes[0].w, kDisplayModes[0].h);
    setFakeDisplayModesForced(kDisplayModes);

    showWindowModeMenu();
    selectItem(resolutionField0);

    // make sure we can get back to full screen by selecting both previous and different resolution
    for (auto i : { 0, 1 }) {
        switchToWindow();
        showWindowModeMenu();

        auto currentEntry = resolutionField0 + i;

        // make sure selecting it twice doesn't change anything
        for (int j = 0; j < 2; j++) {
            selectItem(currentEntry);
            assertEqual(getWindowMode(), kModeFullScreen);
            assertEqual(getFullScreenDimensions(), std::make_pair(kDisplayModes[i].w, kDisplayModes[i].h));

            showWindowModeMenu();
            assertItemHasColor(currentEntry, kPurple);
        }
    }
}

void WindowModeMenuTest::testResolutionListRebuildWhenChangingScreen()
{
    setFakeDisplayModesForced(kDisplayModes);
    showWindowModeMenu();
    verifyResolutionListStrings(kDisplayModes);

    DisplayModeList secondHalf(kDisplayModes.begin() + kDisplayModes.size() / 2, kDisplayModes.end());
    setFakeDisplayModes(secondHalf);
    drawMenu();
    verifyResolutionListStrings(kDisplayModes);

    setFakeDisplayModesForced(secondHalf);
    drawMenu();
    verifyResolutionListStrings(secondHalf);
}

void WindowModeMenuTest::setFakeDisplayModesForced(const DisplayModeList& displayModes)
{
    setFakeDisplayModes(displayModes);
    setWindowDisplayIndex(getWindowDisplayIndex() + 1);
}

// We must jump on the update hook since the text will be erased by an intermediate call to DrawMenu().
// That's why we also have to ignore subsequent calls.
void WindowModeMenuTest::verifyYellowPleaseWaitTextPresence()
{
    if (m_verifyYellowText) {
        const auto& c = kMenuPalette[kYellowText];
        assertTrue(hadTextureColorMod(c.r, c.g, c.b));
    }

    m_verifyYellowText = false;
}

void WindowModeMenuTest::setUpMode(size_t mode)
{
    switch (mode) {
    case kModeWindow:
        setUpWindowedMode();
        break;
    case kModeBorderlessMaximized:
        setUpBorderlessMaximizedMode();
        break;
    case kModeFullScreen:
        setUpFullScreenMode();
        break;
    default:
        assert(false);
        break;
    }

    showWindowModeMenu();
}

void WindowModeMenuTest::setUpWindowedMode()
{
    setIsInFullScreenMode(false);
    setWindowResizable(false);
    switchToWindow();
}

void WindowModeMenuTest::setUpBorderlessMaximizedMode()
{
    switchToBorderlessMaximized();
}

void WindowModeMenuTest::setUpFullScreenMode()
{
    setIsInFullScreenMode(true);
}

void WindowModeMenuTest::verifyMode(size_t mode)
{
    switch (mode) {
    case kModeWindow:
        verifyWindowedMode();
        break;
    case kModeBorderlessMaximized:
        verifyBorderlessMaximizedMode();
        break;
    case kModeFullScreen:
        verifyFullScreenMode();
        break;
    default:
        assert(false);
        break;
    }
}

void WindowModeMenuTest::verifyWindowedMode()
{
    assertEqual(getWindowMode(), kModeWindow);

    assertItemIsVisible(customSizeArrow);
    assertItemIsSprite(customSizeArrow, -1);
    assertItemIsInvisible(borderlessMaximizedArrow);
    assertItemIsInvisible(fullScreenArrow);

    assertWindowedModeItemsSelected(true);

    assertItemHasTextColor(borderlessMaximized, kWhiteText);
    assertItemHasTextColor(fullScreen, kGrayText);

    verifyResolutionListColors();
}

void WindowModeMenuTest::assertWindowedModeItemsSelected(bool selected)
{
    int displayItems[] = { customWidth, customHeight, resizableOnOff };

    assertItemHasTextColor(windowed, selected ? kSoftBlueText : kWhiteText);
    assertItemHasTextColor(resizable, kWhiteText);

    for (auto index : displayItems) {
        assertItemHasColor(index, kLightBlue);
        assertItemHasTextColor(index, kWhiteText);
    }
}

void WindowModeMenuTest::verifyBorderlessMaximizedMode()
{
    assertEqual(getWindowMode(), kModeBorderlessMaximized);

    assertItemIsInvisible(customSizeArrow);
    assertItemIsVisible(borderlessMaximizedArrow);
    assertItemIsSprite(borderlessMaximizedArrow, -1);
    assertItemIsInvisible(fullScreenArrow);

    assertItemHasColor(borderlessMaximized, kNoBackground);
    assertItemHasTextColor(borderlessMaximized, kSoftBlueText);

    assertWindowedModeItemsSelected(false);
    assertItemHasTextColor(fullScreen, kGrayText);

    verifyResolutionListColors();
}

void WindowModeMenuTest::verifyFullScreenMode()
{
    assertEqual(getWindowMode(), kModeFullScreen);

    assertItemIsInvisible(customSizeArrow);
    assertItemIsInvisible(borderlessMaximizedArrow);
    assertItemIsVisible(fullScreenArrow);
    assertItemIsSprite(fullScreenArrow, -1);

    assertItemHasColor(fullScreen, kNoBackground);
    assertItemHasTextColor(fullScreen, kSoftBlueText);

    assertWindowedModeItemsSelected(false);
    assertItemHasTextColor(borderlessMaximized, kWhiteText);

    auto [width, height] = getFullScreenDimensions();
    auto currentResolution = getResolutionString(width, height);
    verifyResolutionListColors(currentResolution);
}

void WindowModeMenuTest::verifyResolutionListColors(const char *current /* = nullptr */)
{
    auto entry = getMenuEntry(resolutionField0);
    auto visible = !entry->invisible;

    for (int i = 0; i < kNumResolutionFieldsShort; i++, entry++) {
        // make sure there is never a visible field once they start going invisible
        if (entry->invisible)
            visible = 0;
        assertEqual(visible, !entry->invisible);

        if (current && !strcmp(current, entry->fg.string))
            assertItemHasColor(resolutionField0 + i, kPurple);
        else
            assertItemHasColor(resolutionField0 + i, kLightBlue);
    }
}

void WindowModeMenuTest::switchToMode(size_t mode)
{
    switch (mode) {
    case kModeWindow:
        selectItem(windowed);
        break;
    case kModeBorderlessMaximized:
        selectItem(borderlessMaximized);
        break;
    case kModeFullScreen:
        {
            assertItemIsVisible(resolutionField0);

            auto [width, height] = getFullScreenDimensions();
            auto resolutionString = getResolutionString(width, height);

            int item = resolutionField0;
            if (!strcmp(getMenuEntry(resolutionField0)->fg.string, resolutionString))
                item++;

            assertItemIsVisible(item);
            selectItem(item);
        }
        break;
    }

    drawMenu();
}

const char *WindowModeMenuTest::getResolutionString(int width, int height)
{
    static char resolutionString[32];
    snprintf(resolutionString, sizeof(resolutionString),  "%d X %d", width, height);
    return resolutionString;
}
