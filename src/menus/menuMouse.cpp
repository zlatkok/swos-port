// Handles mouse movement and clicks in the menus.

#include "menuMouse.h"
#include "menu.h"
#include "controls.h"

constexpr int kScrollRateMs = 100;

static Uint32 m_buttons;
static int m_x;
static int m_y;

static Uint32 m_lastScrollTick;

static bool m_selectedLastFrame = true;
static bool m_clickedLastFrame;
static bool m_scrolling;

static MenuEntry *m_scrollEntry;

struct MenuMouseWheelData {
    MouseWheelEntryList entries;
    int upEntry = -1;
    int downEntry = -1;

    bool doesEntryRespondToMouseWheel(const MenuEntry& entry) const {
        auto it = std::find_if(entries.begin(), entries.end(), [&entry](const auto& wheelEntry) {
            return wheelEntry.ordinal == entry.ordinal;
        });

        return it != entries.end();
    }

    bool globalHandlerActive() const {
        return upEntry >= 0 || downEntry >= 0;
    }
};

// hold one initial entry for main menu
static std::vector<MenuMouseWheelData> m_mouseWheelData = { {} };

using ReachabilityMap = std::array<bool, 256>;
static ReachabilityMap m_reachableEntries;

static bool checkForButtonRelease()
{
    if (m_clickedLastFrame) {
        if (!m_buttons) {
            m_clickedLastFrame = false;
            m_selectedLastFrame = false;
            m_scrolling = false;
        }

        releaseFire();
        return true;
    }

    return false;
}

// Tries to recognize if the entry is a scroll arrow by using some heuristics.
static bool isEntryScroller(const MenuEntry& entry)
{
    assert(!m_mouseWheelData.empty());
    const auto& wheelData = m_mouseWheelData.back();

    if (entry.ordinal == wheelData.upEntry || entry.ordinal == wheelData.downEntry)
        return true;

    if (wheelData.doesEntryRespondToMouseWheel(entry))
        return true;

    if (wheelData.globalHandlerActive() && entry.type2 == kEntrySprite2 &&
        (entry.u2.spriteIndex == kUpArrowSprite || entry.u2.spriteIndex == kDownArrowSprite))
        return true;

    // also quicken +/- fields
    if (entry.isString() && (entry.string()[0] == '+' || entry.string()[0] == '-') && entry.string()[1] == '\0')
        return true;

    return false;
}

static bool handleScrolling()
{
    if ((m_buttons & SDL_BUTTON_LMASK) && m_scrolling) {
        auto now = SDL_GetTicks();

        if (now >= m_lastScrollTick + kScrollRateMs) {
            assert(m_scrollEntry && isEntryScroller(*m_scrollEntry));
            selectEntry(m_scrollEntry);
            m_lastScrollTick = now;
        }

        return true;
    }

    return false;
}

static bool checkForRightClickExitMenu()
{
    if (m_buttons & SDL_BUTTON_RMASK) {
        m_clickedLastFrame = true;
        SetExitMenuFlag();
        return true;
    }

    return false;
}

static bool mapCoordinatesToGameArea()
{
    int windowWidth, windowHeight;

    if (isInFullScreenMode()) {
        std::tie(windowWidth, windowHeight) = getFullScreenDimensions();
        m_x = m_x * kVgaWidth / windowWidth;
        m_y = m_y * kVgaHeight / windowHeight;
        return true;
    }

    std::tie(windowWidth, windowHeight) = getWindowSize();

    SDL_Rect viewport = getViewport();

    int slackWidth = 0;
    int slackHeight = 0;

    if (viewport.x) {
        auto logicalWidth = kVgaWidth * windowHeight / kVgaHeight;
        slackWidth = (windowWidth - logicalWidth) / 2;
    } else if (viewport.y) {
        auto logicalHeight = kVgaHeight * windowWidth / kVgaWidth;
        slackHeight = (windowHeight - logicalHeight) / 2;
    }

    if (m_y < slackHeight || m_y >= windowHeight - slackHeight)
        return false;

    if (m_x < slackWidth || m_x >= windowWidth - slackWidth)
        return false;

    m_x = (m_x - slackWidth) * kVgaWidth / (windowWidth - 2 * slackWidth);
    m_y = (m_y - slackHeight) * kVgaHeight / (windowHeight - 2 * slackHeight);

    return true;
}

static bool globalWheelHandler(int scrollAmount)
{
    assert(!m_mouseWheelData.empty());

    const auto& wheelData = m_mouseWheelData.back();
    bool handled = false;

    if (scrollAmount > 0 && wheelData.upEntry >= 0) {
        selectEntry(wheelData.upEntry);
        handled = true;
    } else if (scrollAmount < 0 && wheelData.downEntry >= 0) {
        selectEntry(wheelData.downEntry);
        handled = true;
    }

    return handled;
}

static void getRidOfClickIfNeeded(MenuEntry& entry)
{
    if (isEntryScroller(entry)) {
        m_scrolling = true;
        m_scrollEntry = &entry;
        m_lastScrollTick = SDL_GetTicks();
    } else {
        // get rid of the damn click
        SDL_PumpEvents();

        while (SDL_GetMouseState(nullptr, nullptr)) {
            SDL_Delay(10);
            SDL_PumpEvents();
        }
    }
}

static void performMouseWheelAction(const MenuEntry& entry, int scrollValue)
{
    assert(!m_mouseWheelData.empty());

    for (const auto& mouseWheelEntry : m_mouseWheelData.back().entries) {
        if (mouseWheelEntry.ordinal == entry.ordinal) {
            int scrollUpEntry = mouseWheelEntry.scrollUpEntry;
            int scrollDownEntry = mouseWheelEntry.scrollDownEntry;

            assert(scrollUpEntry >= 0 && scrollUpEntry < 256);
            assert(scrollDownEntry >= 0 && scrollDownEntry < 256);

            if (scrollValue > 0) {
                assert(getMenuEntry(scrollUpEntry)->onSelect);
                getMenuEntry(scrollUpEntry)->onSelect();
            } else {
                assert(getMenuEntry(scrollDownEntry)->onSelect);
                getMenuEntry(scrollDownEntry)->onSelect();
            }

            break;
        }
    }
}

static void checkForEntryClicksAndMouseWheelMovement()
{
    auto currentMenu = getCurrentMenu();
    auto entries = currentMenu->entries();

    auto wheelScrollAmount = mouseWheelAmount();
    auto wheelHandled = globalWheelHandler(wheelScrollAmount);

    for (size_t i = 0; i < currentMenu->numEntries; i++) {
        auto& entry = entries[i];
        bool insideEntry = m_x >= entry.x && m_x < entry.x + entry.width && m_y >= entry.y && m_y < entry.y + entry.height;

        if (insideEntry && entry.selectable() && m_reachableEntries[entry.ordinal]) {
            currentMenu->selectedEntry = &entry;
            m_selectedLastFrame = true;

            bool isPlayMatch = entry.type2 == kEntryString && entry.string() && !strcmp(entry.string(), "PLAY MATCH");

            if ((m_buttons & SDL_BUTTON_LMASK) && !isPlayMatch) {
                pressFire();
                getRidOfClickIfNeeded(entry);
                m_clickedLastFrame = true;
                break;
            } else if (!wheelHandled && wheelScrollAmount) {
                performMouseWheelAction(entry, wheelScrollAmount);
            }
        }
    }
}

void menuMouseOnAboutToShowNewMenu()
{
    m_mouseWheelData.push_back({});
}

void menuMouseOnOldMenuRestored()
{
    m_mouseWheelData.pop_back();
}

void setMouseWheelEntries(const MouseWheelEntryList& mouseWheelEntries)
{
    assert(!m_mouseWheelData.empty());
    auto& wheelData = m_mouseWheelData.back();

    assert(!wheelData.globalHandlerActive());

    wheelData.entries = mouseWheelEntries;
}

void setGlobalWheelEntries(int upEntry /* = -1 */, int downEntry /* = -1 */)
{
    assert(!m_mouseWheelData.empty());
    auto& wheelData = m_mouseWheelData.back();

    assert(wheelData.entries.empty());

    wheelData.upEntry = upEntry;
    wheelData.downEntry = downEntry;
}

static std::pair<int, int> getLastMouseCoordinates()
{
    // make sure not to select menu item initially under mouse arrow
    static bool initialized;
    static int lastX;
    static int lastY;

    if (!initialized) {
        lastX = m_x;
        lastY = m_y;
        initialized = true;
    }

    auto result = std::make_pair(lastX, lastY);

    lastX = m_x;
    lastY = m_y;

    return result;
}

void updateMouse()
{
    m_buttons = SDL_GetMouseState(&m_x, &m_y);

    int lastX, lastY;
    std::tie(lastX, lastY) = getLastMouseCoordinates();

    if ((m_x != lastX || m_y != lastY))
        m_selectedLastFrame = false;

    if (handleScrolling())
        return;

    if (checkForButtonRelease())
        return;

    if (checkForRightClickExitMenu())
        return;

    bool noMouseActivity = m_selectedLastFrame && !(m_buttons & SDL_BUTTON_LMASK) && !mouseWheelAmount();
    if (noMouseActivity)
        return;

    if (!mapCoordinatesToGameArea())
        return;

    if (hasMouseFocus() && !isMatchRunning())
        checkForEntryClicksAndMouseWheelMovement();
}

// Finds and marks every reachable item starting from the initial one. We must track reachability in order
// to avoid selecting items with mouse that are never supposed to be selected, like say labels.
void determineReachableEntries()
{
    std::fill(m_reachableEntries.begin(), m_reachableEntries.end(), false);

    static_assert(offsetof(MenuEntry, downEntry) == offsetof(MenuEntry, upEntry) + 1 &&
        offsetof(MenuEntry, upEntry) == offsetof(MenuEntry, rightEntry) + 1 &&
        offsetof(MenuEntry, rightEntry) == offsetof(MenuEntry, leftEntry) + 1 &&
        sizeof(MenuEntry::leftEntry) == 1, "MenuEntry: assumptions about next entries failed");

    std::vector<std::pair<const MenuEntry *, int>> entryStack;
    entryStack.reserve(256);

    auto currentMenu = getCurrentMenu();
    auto currentEntry = currentMenu->selectedEntry;
    auto entries = currentMenu->entries();

    entryStack.emplace_back(currentEntry, 0);

    while (!entryStack.empty()) {
        const MenuEntry *entry;
        int direction;

        std::tie(entry, direction) = entryStack.back();

        assert(entry && direction >= MenuEntry::kInitialDirection && direction <= MenuEntry::kNumDirections);
        assert(entry->ordinal < 256);

        m_reachableEntries[entry->ordinal] = true;

        auto nextEntries = reinterpret_cast<const uint8_t *>(&entry->leftEntry);

        for (; direction < MenuEntry::kNumDirections; direction++) {
            auto ord = nextEntries[direction];

            if (ord != 255 && !m_reachableEntries[ord]) {
                entryStack.emplace_back(&entries[ord], 0);
                break;
            }
        }

        if (direction >= MenuEntry::kNumDirections) {
            entryStack.pop_back();
        } else {
            assert(entryStack.size() >= 2);
            entryStack[entryStack.size() - 2].second = direction + 1;
        }
    }
}
