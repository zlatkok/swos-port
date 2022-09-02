#include "windowModeMenu.h"
#include "windowManager.h"
#include "render.h"
#include "menuMouse.h"
#include "continueMenu.h"
#include "text.h"
#include "textInput.h"

SDL_UNUSED constexpr auto kSelectedColor = kSoftBlueText;

constexpr int kPleaseWaitLimitMs = 333;

static bool m_menuShown;
static int16_t m_windowResizable = 1;
static int16_t m_windowMaximized;

#include "windowMode.mnu.h"

using DisplayModeList = std::vector<std::pair<int, int>>;

static const char *m_pleaseWaitText;
static DisplayModeList m_resolutions;
static int m_resListOffset;

#ifndef __ANDROID__
static void updateDisplayedWindowSize();
static void updateFullScreenAvailability();
static void highlightCurrentMode();
#endif
static void centerItemsVertically();
static DisplayModeList getDisplayModes(int displayIndex);
static void fillResolutionListUi();
static void fixSelectedEntry();
static void updateScrollArrows();

using namespace WindowModeMenu;

void showWindowModeMenu()
{
    m_menuShown = false;
    showMenu(windowModeMenu);
}

static void windowModeMenuOnInit()
{
    setGlobalWheelEntries(scrollUpArrow, scrollDownArrow);
    m_pleaseWaitText = nullptr;

#ifdef __ANDROID__
    for (auto entry = &headerEntry + 1; entry < &fullScreenEntry; entry++)
        entry->hide();

    fullScreenEntry.copyString("DISPLAY MODES:");
    resolutionField5Entry.downEntry = resolutionField6;
#endif
}

static void windowModeMenuOnDraw()
{
    if (SDL_IsTextInputActive())
        return;

    static int lastDisplayIndex = -1;

    int displayIndex = getWindowDisplayIndex();
    if (displayIndex >= 0) {
        if (displayIndex != lastDisplayIndex) {
            m_resolutions = getDisplayModes(displayIndex);
            lastDisplayIndex = displayIndex;
            m_resListOffset = 0;
        }

#ifdef __ANDROID__
        fillResolutionListUi();
        updateScrollArrows();
        fixSelectedEntry();
#else
        updateDisplayedWindowSize();
        highlightCurrentMode();
        fillResolutionListUi();
        updateFullScreenAvailability();
        updateScrollArrows();
        fixSelectedEntry();
#endif
    } else if (!m_menuShown) {
        logWarn("Failed to get window display index, SDL reported: %s", SDL_GetError());
    }

    centerItemsVertically();

    m_menuShown = true;
}

static void centerItemsVertically()
{
    int topY = exitEntry.y + 1;
    int bottomY = 0;

    for (auto entry = &headerEntry + 1; entry < &exitEntry; entry++) {
        if (entry->visible()) {
            topY = std::min<int>(entry->y, topY);
            bottomY = std::max(entry->endY(), bottomY);
        }
    }

    int verticalSpace = exitEntry.y - headerEntry.endY();
    int usedHeight = bottomY - topY;
    int targetY = headerEntry.endY() + (verticalSpace - usedHeight) / 3;
    int diff = targetY - topY;

    if (diff) {
        std::for_each(&headerEntry + 1, &exitEntry, [diff](auto& entry) {
            entry.y += diff;
        });
    }
}

constexpr static int numResolutionFields()
{
#ifdef __ANDROID__
    return kNumResolutionFieldsFull;
#else
    return kNumResolutionFieldsShort;
#endif
}

#ifndef __ANDROID__
static void updateDisplayedWindowSize()
{
    int width, height;
    std::tie(width, height) = getWindowSize();

    customWidthEntry.setNumber(width);
    customHeightEntry.setNumber(height);

    m_windowResizable = getWindowResizable();
    m_windowMaximized = getWindowMaximized();
}
#endif

static DisplayModeList getDisplayModes(int displayIndex)
{
    assert(displayIndex >= 0);

    int numModes = SDL_GetNumDisplayModes(displayIndex);
    if (numModes < 1)
        logWarn("No display modes, SDL reported: %s", SDL_GetError());
    else
        logInfo("Enumerating display modes, %d found", numModes);

    auto startTicks = SDL_GetTicks();
    bool shownWarning = false;

    DisplayModeList result;

    for (int i = 0; i < numModes; i++) {
        SDL_DisplayMode mode;
        if (!SDL_GetDisplayMode(displayIndex, i, &mode)) {
            logInfo("  %2d %d x %d, format: %x, refresh rate: %d", i, mode.w, mode.h, mode.format, mode.refresh_rate);

            if (!shownWarning && SDL_GetTicks() > startTicks + kPleaseWaitLimitMs) {
                if (!m_pleaseWaitText)
                    m_pleaseWaitText = SwosVM::allocateString("PLEASE WAIT, ENUMERATING GRAPHICS MODES...");
                drawText(55, 80, m_pleaseWaitText, -1, kYellowText);
                updateScreen();
                shownWarning = true;
            }

            // disallow any silly display mode
            switch (mode.format) {
            case SDL_PIXELFORMAT_INDEX1LSB:
            case SDL_PIXELFORMAT_INDEX1MSB:
            case SDL_PIXELFORMAT_INDEX4LSB:
            case SDL_PIXELFORMAT_INDEX4MSB:
            case SDL_PIXELFORMAT_INDEX8:
                continue;
            default:
                bool different = result.empty() ? true : result.back().first != mode.w || result.back().second != mode.h;
                if (different)
                    result.emplace_back(mode.w, mode.h);
            }
        } else {
            logWarn("Failed to retrieve mode %d info, SDL reported: %s", i, SDL_GetError());
        }
    }

    logInfo("Accepted display modes:");
    for (size_t i = 0; i < result.size(); i++)
        logInfo("  %2d %d x %d", i, result[i].first, result[i].second);

    std::sort(result.rbegin(), result.rend());
    return result;
}

static void fillResolutionListUi()
{
    for (int i = resolutionField0; i < resolutionField0 + numResolutionFields(); i++)
        getMenuEntry(i)->hide();

    int visibleFields = m_resolutions.size() - m_resListOffset;
    int maxField = m_resListOffset + std::min(visibleFields, numResolutionFields());

    for (int i = m_resListOffset; i < maxField; i++) {
        int entryIndex = resolutionField0 + i - m_resListOffset;
        auto entry = getMenuEntry(entryIndex);
        entry->show();

        auto width = m_resolutions[i].first;
        auto height = m_resolutions[i].second;
        snprintf(entry->string(), 30, "%d X %d", width, height);

        entry->bg.entryColor = kLightBlue;
        if (isInFullScreenMode() && getFullScreenDimensions() == std::make_pair(width, height))
            entry->bg.entryColor = kPurple;

        entry->rightEntry = entryIndex - resolutionField0 < (maxField - m_resListOffset) / 2 ? scrollUpArrow : scrollDownArrow;
    }
}

static void fixSelectedEntry()
{
    auto& selectedEntry = getCurrentMenu()->selectedEntry;
    if (selectedEntry && !selectedEntry->selectable()) {
        if (selectedEntry == &windowedEntry && resolutionField0Entry.selectable())
            selectedEntry = &resolutionField0Entry;
        if (!selectedEntry->selectable())
            setCurrentEntry(WindowModeMenu::exit);
    }
}

#ifndef __ANDROID__
static void updateFullScreenAvailability()
{
    bool fullScreenAvailable = !m_resolutions.empty();

    strcpy(fullScreenEntry.string(), "FULL SCREEN:");
    if (!fullScreenAvailable)
        strcpy(fullScreenEntry.string() + 11, " UNAVAILABLE");

    fullScreenEntry.disabled = !fullScreenAvailable;
    fullScreenArrowEntry.setVisible(getWindowMode() == kModeFullScreen && fullScreenAvailable);
}
#endif

static void scrollResolutionsDownSelected()
{
    if (m_resolutions.size() > static_cast<size_t>(numResolutionFields()))
        m_resListOffset = std::min(m_resListOffset + 1, static_cast<int>(m_resolutions.size()) - numResolutionFields());
}

static void scrollResolutionsUpSelected()
{
    if (m_resolutions.size() > static_cast<size_t>(numResolutionFields()))
        m_resListOffset = std::max(0, m_resListOffset - 1);
}

static void updateScrollArrows()
{
    bool show = m_resolutions.size() > static_cast<size_t>(numResolutionFields());

    int selectedEntry = getCurrentEntryOrdinal();

    for (auto i : { scrollUpArrow, scrollDownArrow }) {
        auto entry = getMenuEntry(i);
        entry->setVisible(show);

        if (!show && i == selectedEntry)
            setCurrentEntry(resolutionField0);
    }

#ifdef __ANDROID__
    if (show) {
        scrollUpArrowEntry.y = resolutionField0Entry.y;
        scrollDownArrowEntry.y = lastResolutionFieldEntry.y;

        for (auto entry = &lastResolutionFieldEntry; entry >= &firstResolutionFieldEntry; entry--) {
            if (entry->visible()) {
                scrollDownArrowEntry.leftEntry = entry->ordinal;
                break;
            }
        }
    }
#endif
}

static void changeResolutionSelected()
{
    auto entry = A5.as<MenuEntry *>();

    int i = entry->ordinal - resolutionField0 + m_resListOffset;
    assert(i >= 0 && i < static_cast<int>(m_resolutions.size()));

    if (i < 0 || i >= static_cast<int>(m_resolutions.size()))
        return;

    if (isInFullScreenMode()) {
        auto currentResolution = getFullScreenDimensions();
        if (currentResolution == m_resolutions[i])
            return;
    }

    constexpr int kBufferSize = 64;
    auto buffer = SwosVM::allocateMemory(kBufferSize).asCharPtr();

    if (setFullScreenResolution(m_resolutions[i].first, m_resolutions[i].second)) {
        strncpy_s(buffer, entry->string(), kBufferSize);
        if (auto space = strchr(buffer, ' ')) {
            *space = 'x';

            while (space++[3])
                *space = space[2];

            *space = '\0';
        }

        logInfo("Successfully switched to %s", buffer);
    } else {
        logWarn("Failed to switch to %s", entry->string());
        snprintf(buffer, kBufferSize, "FAILED TO SWITCH TO %s", entry->string());
        showErrorMenu(buffer);
    }
}

enum Dimension { kWidth, kHeight };

static void inputWindowWidthOrHeight(Dimension dimension)
{
    if (getWindowMode() == kModeWindow) {
        auto entry = A5.as<MenuEntry *>();
        auto numberEntered = inputNumber(*entry, 4, 0, 9999);

        if (numberEntered) {
            int widthOrHeight = entry->fg.number;

            int windowWidth, windowHeight;
            std::tie(windowWidth, windowHeight) = getWindowSize();

            bool isInputWidth = dimension == kWidth;
            int oldWidthOrHeight = isInputWidth ? windowWidth : windowHeight;
            int minAllowedSize = isInputWidth ? kVgaWidth : kVgaHeight;

            if (widthOrHeight != oldWidthOrHeight && widthOrHeight >= minAllowedSize) {
                isInputWidth ? setWindowSize(widthOrHeight, windowHeight) : setWindowSize(windowWidth, widthOrHeight);
                centerWindow();
            }
        }
    } else {
        switchToWindow();
    }
}

static void inputWindowWidth()
{
    inputWindowWidthOrHeight(kWidth);
}

static void inputWindowHeight()
{
    inputWindowWidthOrHeight(kHeight);
}

#ifndef __ANDROID__
static void setWindowedFieldsColor(bool selected)
{
    windowedEntry.stringFlags = selected ? kSelectedColor : kWhiteText;
}

static void setBorderlessFieldsColor(bool selected)
{
    borderlessMaximizedEntry.stringFlags = selected ? kSelectedColor : kWhiteText;
}

static void setFullScreenFieldsColor(bool selected)
{
    int color = selected ? kSelectedColor : kGrayText;
    if (m_resolutions.empty())
        color = kDarkGrayText;

    fullScreenEntry.stringFlags = color;
}

static void highlightCurrentMode()
{
    static const std::array<int, 3> kHighlightArrows = {
        customSizeArrow, borderlessMaximizedArrow, fullScreenArrow,
    };

    for (int index : kHighlightArrows)
        getMenuEntry(index)->hide();

    int showArrowIndex = customSizeArrow;
    bool windowedSelected = false, borderlessSelected = false, fullScreenSelected = false;

    switch (getWindowMode()) {
    case kModeWindow:
        showArrowIndex = customSizeArrow;
        windowedSelected = true;
        break;

    case kModeBorderlessMaximized:
        showArrowIndex = borderlessMaximizedArrow;
        borderlessSelected = true;
        break;

    case kModeFullScreen:
        showArrowIndex = fullScreenArrow;
        fullScreenSelected = true;
        break;

    default:
        assert(false);
    }

    getMenuEntry(showArrowIndex)->show();

    setWindowedFieldsColor(windowedSelected);
    setBorderlessFieldsColor(borderlessSelected);
    setFullScreenFieldsColor(fullScreenSelected);
}
#endif
