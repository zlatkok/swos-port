#include "videoOptions.mnu.h"

static bool m_menuShown;
static int16_t m_windowResizable = 1;

void showVideoOptionsMenu()
{
    m_menuShown = false;
    showMenu(videoOptionsMenu);
}

static void videoOptionsMenuOnInit()
{
    using namespace VideoOptionsMenu;

    MouseWheelEntryList entryList;

    for (int i = 0; i < kNumResolutionFields; i++)
        entryList.emplace_back(resolutionField0 + i, scrollUpArrow, scrollDownArrow);

    entryList.emplace_back(scrollUpArrow, scrollUpArrow, scrollDownArrow);
    entryList.emplace_back(scrollDownArrow, scrollUpArrow, scrollDownArrow);

    setMouseWheelEntries(entryList);
}

static void updateWindowSize()
{
    using namespace VideoOptionsMenu;

    int width, height;
    std::tie(width, height) = getWindowSize();

    auto widthEntry = getMenuEntryAddress(Entries::customWidth);
    widthEntry->u2.number = width;

    auto heightEntry = getMenuEntryAddress(Entries::customHeight);
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

    if (numModes > 20) {
        drawMenuText(55, 80, "PLEASE WAIT, ENUMERATING GRAPHICS MODES...", kYellowText);
        updateScreen();
    }

    DisplayModeList result;

    for (int i = 0; i < numModes; i++) {
        SDL_DisplayMode mode;
        if (!SDL_GetDisplayMode(displayIndex, i, &mode)) {
            logInfo("  %2d %d x %d, format: %x, refresh rate: %d", i, mode.w, mode.h, mode.format, mode.refresh_rate);

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
                bool acceptable = mode.w >= 640 && 10 * mode.w == 16 * mode.h || mode.w % 1920 == 0 && mode.h % 1080 == 0;

                if (different && acceptable)
                    result.emplace_back(mode.w, mode.h);
            }
        } else {
            logWarn("Failed to retrieve mode %d info, SDL reported: %s", i, SDL_GetError());
        }
    }

    logInfo("Accepted display modes:");
    for (size_t i = 0; i < result.size(); i++)
        logInfo("  %2d %d x %d", i, result[i].first, result[i].second);

    std::reverse(result.begin(), result.end());
    return result;
}

static DisplayModeList m_resolutions;
static int m_resListOffset;

static void fillResolutionListUi()
{
    using namespace VideoOptionsMenu;

    int currentEntry = getCurrentEntry();

    for (int i = resolutionField0; i < resolutionField0 + kNumResolutionFields; i++)
        getMenuEntryAddress(i)->invisible = 1;

    int visibleFields = m_resolutions.size() - m_resListOffset;
    for (int i = m_resListOffset; i < m_resListOffset + std::min(visibleFields, kNumResolutionFields); i++) {
        auto entry = getMenuEntryAddress(resolutionField0 + i - m_resListOffset);
        entry->invisible = 0;

        auto width = m_resolutions[i].first;
        auto height = m_resolutions[i].second;
        snprintf(entry->u2.string, 30, "%d X %d", width, height);

        if (isInFullScreenMode(width, height))
            entry->u1.entryColor = kPurple;
    }

    for (int i = resolutionField0; i < resolutionField0 + kNumResolutionFields; i++)
        if (i == currentEntry && getMenuEntryAddress(i)->invisible)
            setCurrentEntry(VideoOptionsMenu::exit);
}

static void scrollResolutionsDownSelected()
{
    using namespace VideoOptionsMenu;

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
    using namespace VideoOptionsMenu;

    int hide = m_resolutions.size() <= kNumResolutionFields;

    int selectedEntry = getCurrentEntry();

    for (auto i : { scrollUpArrow, scrollDownArrow }) {
        auto entry = getMenuEntryAddress(i);
        entry->invisible = hide;

        if (hide && i == selectedEntry)
            setCurrentEntry(resolutionField0);
    }
}

static void changeResolutionSelected()
{
    auto entry = A5.as<MenuEntry *>();

    int i = entry->ordinal - VideoOptionsMenu::resolutionField0 + m_resListOffset;
    assert(i >= 0 && i < static_cast<int>(m_resolutions.size()));

    if (!setFullScreenResolution(m_resolutions[i].first, m_resolutions[i].second)) {
        logWarn("Failed to switch to %s", entry->u2.string);
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "FAILED TO SWITCH TO %s", entry->u2.string);
        showError(buffer);
    } else {
        logInfo("Successfully switched to %s", entry->u2.string);
    }
}

static void inputWindowWidth()
{
    if (getWindowMode() == kModeWindow) {
        auto entry = A5.as<MenuEntry *>();
        auto numberEntered = inputNumber(entry, 5, 0, 99999);

        if (numberEntered) {
            int width = entry->u2.number;

            int windowWidth, windowHeight;
            std::tie(windowWidth, windowHeight) = getWindowSize();

            if (width != windowWidth && width >= kVgaWidth) {
                setWindowSize(width, windowHeight);
                centerWindow();
            }
        }
    }
}

static void inputWindowHeight()
{
    if (getWindowMode() == kModeWindow) {
        auto entry = A5.as<MenuEntry *>();
        auto numberEntered = inputNumber(entry, 5, 0, 99999);

        if (numberEntered) {
            int height = entry->u2.number;

            int windowWidth, windowHeight;
            std::tie(windowWidth, windowHeight) = getWindowSize();

            if (height != windowHeight && height >= kVgaHeight) {
                setWindowSize(windowWidth, height);
                centerWindow();
            }
        }
    }
}

static void enableCustomSizeFields(bool enabled)
{
    using namespace VideoOptionsMenu;

    for (int i : { windowed, resizable, })
        getMenuEntryAddress(i)->textColor = enabled ? kWhiteText : kGrayText;

    for (int i : { customWidth, customHeight, resizableOnOff, })
        getMenuEntryAddress(i)->u1.entryColor = enabled ? kLightBlue : kGray;
}

static void enableBorderlessFields(bool enabled)
{
    auto color = enabled ? kWhiteText : kGrayText;
    getMenuEntryAddress(VideoOptionsMenu::borderlessMaximized)->textColor = color;
}

static void enableFullScreenFields(bool enabled)
{
    using namespace VideoOptionsMenu;

    int textColor = enabled ? kWhiteText : kGrayText;
    int backColor = enabled ? kLightBlue : kGray;

    getMenuEntryAddress(fullScreen)->textColor = textColor;

    for (int i = resolutionField0; i < resolutionField0 + kNumResolutionFields; i++)
        getMenuEntryAddress(i)->u1.entryColor = backColor;
}

static void highlightCurrentMode()
{
    using namespace VideoOptionsMenu;

    static const std::array<int, 3> kHighlightArrows = {
        customSizeArrow, borderlessMaximizedArrow, fullScreenArrow,
    };

    for (int index : kHighlightArrows)
        getMenuEntryAddress(index)->invisible = 1;

    int showIndex = customSizeArrow;
    bool enableCustomSize = false, enableBorderless = false, enableFullScreen = false;

    switch (getWindowMode()) {
    case kModeWindow:
        showIndex = customSizeArrow;
        enableCustomSize = true;
        break;

    case kModeBorderlessMaximized:
        showIndex = borderlessMaximizedArrow;
        enableBorderless = true;
        break;

    case kModeFullScreen:
        showIndex = fullScreenArrow;
        enableFullScreen = true;
        break;

    default:
        assert(false);
    }

    getMenuEntryAddress(showIndex)->invisible = 0;

    enableCustomSizeFields(enableCustomSize);
    enableBorderlessFields(enableBorderless);
    enableFullScreenFields(enableFullScreen);
}

static void videoOptionsMenuOnDraw()
{
    if (inputingText)
        return;

    static int lastDisplayIndex = -1;

    updateWindowSize();
    highlightCurrentMode();

    int displayIndex = getWindowDisplayIndex();
    if (displayIndex >= 0) {
        if (displayIndex != lastDisplayIndex) {
            m_resolutions = getDisplayModes(displayIndex);
            lastDisplayIndex = displayIndex;
            m_resListOffset = 0;
        }

        fillResolutionListUi();
        updateScrollArrows();
    } else {
        if (!m_menuShown)
            logWarn("Failed to get window display index, SDL reported: %s", SDL_GetError());
    }

    m_menuShown = true;
}
