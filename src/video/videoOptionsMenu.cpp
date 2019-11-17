#include "videoOptions.mnu.h"
#include "menuMouse.h"

constexpr auto kSelectedColor = kSoftBlueText;
constexpr int kPleaseWaitLimitMs = 333;

static bool m_menuShown;
static int16_t m_windowResizable = 1;

using namespace VideoOptionsMenu;

void showVideoOptionsMenu()
{
    m_menuShown = false;
    showMenu(videoOptionsMenu);
}

static void videoOptionsMenuOnInit()
{
    setGlobalWheelEntries(scrollUpArrow, scrollDownArrow);
}

static void updateDisplayedWindowSize()
{
    int width, height;
    std::tie(width, height) = getWindowSize();

    auto widthEntry = getMenuEntry(customWidth);
    widthEntry->u2.number = width;

    auto heightEntry = getMenuEntry(customHeight);
    heightEntry->u2.number = height;

    m_windowResizable = getWindowResizable();
}

using DisplayModeList = std::vector<std::pair<int, int>>;

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
                drawMenuText(55, 80, "PLEASE WAIT, ENUMERATING GRAPHICS MODES...", kYellowText);
                updateScreen_MenuScreen();
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

static DisplayModeList m_resolutions;
static int m_resListOffset;

static void fillResolutionListUi()
{
    int currentEntry = getCurrentEntryOrdinal();

    for (int i = resolutionField0; i < resolutionField0 + kNumResolutionFields; i++)
        getMenuEntry(i)->hide();

    int visibleFields = m_resolutions.size() - m_resListOffset;
    for (int i = m_resListOffset; i < m_resListOffset + std::min(visibleFields, kNumResolutionFields); i++) {
        auto entry = getMenuEntry(resolutionField0 + i - m_resListOffset);
        entry->show();

        auto width = m_resolutions[i].first;
        auto height = m_resolutions[i].second;
        snprintf(entry->string(), 30, "%d X %d", width, height);

        entry->u1.entryColor = kLightBlue;
        if (isInFullScreenMode() && getFullScreenDimensions() == std::make_pair(width, height))
            entry->u1.entryColor = kPurple;
    }

    for (int i = resolutionField0; i < resolutionField0 + kNumResolutionFields; i++)
        if (i == currentEntry && getMenuEntry(i)->invisible)
            setCurrentEntry(VideoOptionsMenu::exit);
}

static void updateFullScreenAvailability()
{
    auto fullScreenEntry = getMenuEntry(fullScreen);
    bool fullScreenAvailable = !m_resolutions.empty();

    strcpy(fullScreenEntry->string(), "FULL SCREEN:");
    if (!fullScreenAvailable)
        strcpy(fullScreenEntry->string() + 11, " UNAVAILABLE");

    fullScreenEntry->disabled = !fullScreenAvailable;

    auto fullScreenArrowEntry = getMenuEntry(fullScreenArrow);
    fullScreenArrowEntry->setVisible(getWindowMode() == kModeFullScreen && fullScreenAvailable);
}

static void scrollResolutionsDownSelected()
{
    if (m_resolutions.size() > kNumResolutionFields)
        m_resListOffset = std::min(m_resListOffset + 1, static_cast<int>(m_resolutions.size()) - kNumResolutionFields);
}

static void scrollResolutionsUpSelected()
{
    if (m_resolutions.size() > VideoOptionsMenu::kNumResolutionFields)
        m_resListOffset = std::max(0, m_resListOffset - 1);
}

static void updateScrollArrows()
{
    bool show = m_resolutions.size() > kNumResolutionFields;

    int selectedEntry = getCurrentEntryOrdinal();

    for (auto i : { scrollUpArrow, scrollDownArrow }) {
        auto entry = getMenuEntry(i);
        entry->setVisible(show);

        if (!show && i == selectedEntry)
            setCurrentEntry(resolutionField0);
    }
}

static void changeResolutionSelected()
{
    auto entry = A5.as<MenuEntry *>();

    int i = entry->ordinal - VideoOptionsMenu::resolutionField0 + m_resListOffset;
    assert(i >= 0 && i < static_cast<int>(m_resolutions.size()));

    if (i < 0 || i >= static_cast<int>(m_resolutions.size()))
        return;

    if (isInFullScreenMode()) {
        auto currentResolution = getFullScreenDimensions();
        if (currentResolution == m_resolutions[i])
            return;
    }

    char buffer[64];

    if (setFullScreenResolution(m_resolutions[i].first, m_resolutions[i].second)) {
        strcpy_s(buffer, entry->string());
        if (auto space = strchr(buffer, ' ')) {
            *space = 'x';

            while (space++[3])
                *space = space[2];

            *space = '\0';
        }

        logInfo("Successfully switched to %s", buffer);
    } else {
        logWarn("Failed to switch to %s", entry->string());
        snprintf(buffer, sizeof(buffer), "FAILED TO SWITCH TO %s", entry->string());
        showError(buffer);
    }
}

enum Dimension { kWidth, kHeight };

static void inputWindowWidthOrHeight(Dimension dimension)
{
    if (getWindowMode() == kModeWindow) {
        auto entry = A5.as<MenuEntry *>();
        auto numberEntered = inputNumber(entry, 5, 0, 99999);

        if (numberEntered) {
            int widthOrHeight = entry->u2.number;

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

static void setWindowedFieldsColor(bool selected)
{
    getMenuEntry(windowed)->textColor = selected ? kSelectedColor : kWhiteText;
}

static void setBorderlessFieldsColor(bool selected)
{
    getMenuEntry(VideoOptionsMenu::borderlessMaximized)->textColor = selected ? kSelectedColor : kWhiteText;
}

static void setFullScreenFieldsColor(bool selected)
{
    int textColor = selected ? kSelectedColor : kWhiteText;
    if (m_resolutions.empty())
        textColor = kGrayText;

    getMenuEntry(fullScreen)->textColor = textColor;
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

static void videoOptionsMenuOnDraw()
{
    if (g_inputingText)
        return;

    static int lastDisplayIndex = -1;

    int displayIndex = getWindowDisplayIndex();
    if (displayIndex >= 0) {
        if (displayIndex != lastDisplayIndex) {
            m_resolutions = getDisplayModes(displayIndex);
            lastDisplayIndex = displayIndex;
            m_resListOffset = 0;
        }

        updateDisplayedWindowSize();
        highlightCurrentMode();
        fillResolutionListUi();
        updateFullScreenAvailability();
        updateScrollArrows();
    } else if (!m_menuShown) {
        logWarn("Failed to get window display index, SDL reported: %s", SDL_GetError());
    }

    m_menuShown = true;
}
