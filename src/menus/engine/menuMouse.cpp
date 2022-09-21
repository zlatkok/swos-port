// Handles mouse movement and clicks in the menus.

#include "menuMouse.h"
#include "menus.h"
#include "unpackMenu.h"
#include "menuControls.h"
#include "mainMenu.h"
#include "controls.h"
#include "windowManager.h"
#include "gameFieldMapping.h"
#include "render.h"
#include "game.h"
#include "replayExitMenu.h"

constexpr int kScrollRateMs = 100;

static Uint32 m_buttons;
static SDL_UNUSED Uint32 m_previousButtons;

static int m_x;     // current mouse coordinates
static int m_y;
static int m_clickX;
static int m_clickY;

static bool m_mouseInitialBlock;
static int m_initialX;
static int m_initialY;

#ifdef  __ANDROID__
constexpr auto kInvalidFinger = std::numeric_limits<SDL_FingerID>::max();
static bool m_hasTouch;
static auto m_fingerId = kInvalidFinger;
static float m_touchX;
static float m_touchY;
static SDL_FingerID m_blockedFinger;
#endif

static Uint32 m_lastScrollTick;
static bool m_scrolling;

static MenuEntry *m_currentEntryUnderPointer;
static MenuEntry *m_scrollEntry;

struct MenuMouseWheelData {
    MouseWheelEntryGroupList entries;
    int upEntry = -1;
    int downEntry = -1;

    bool doesEntryRespondToMouseWheel(const MenuEntry& entry) const {
        auto it = std::find_if(entries.begin(), entries.end(), [&entry](const auto& wheelEntry) {
            return entry.ordinal >= wheelEntry.first && entry.ordinal <= wheelEntry.last;
        });

        return it != entries.end();
    }

    bool globalHandlerActive() const {
        return upEntry >= 0 || downEntry >= 0;
    }

    void resetGlobalHandler() {
        upEntry = downEntry = -1;
    }
};

// hold one initial entry for the main menu
static std::vector<MenuMouseWheelData> m_mouseWheelData = { {} };

using ReachabilityMap = std::array<bool, 256>;
static ReachabilityMap m_reachableEntries;

#ifdef __ANDROID__
static std::tuple<bool, bool, bool> updateMouseStatus()
{
    m_buttons = 0;

    auto keys = SDL_GetKeyboardState(nullptr);
    bool backButtonNow = keys[SDL_SCANCODE_AC_BACK] != 0;
    static bool s_previousBackButton;

    if (backButtonNow && !s_previousBackButton)
        m_buttons = SDL_BUTTON_RMASK;

    s_previousBackButton = backButtonNow;

    if (m_hasTouch || m_buttons) {
        if (m_hasTouch) {
            auto viewport = getViewport();
            m_x = static_cast<int>(m_touchX * viewport.w);
            m_y = static_cast<int>(m_touchY * viewport.h);

            static int s_lastX, s_lastY;

            if (m_x != s_lastX || m_y != s_lastY)
                m_buttons |= SDL_BUTTON_LMASK;

            s_lastX = m_x;
            s_lastY = m_y;
            m_hasTouch = false;
        }

        return { (m_buttons & SDL_BUTTON_LMASK) != 0, (m_buttons & SDL_BUTTON_LMASK) != 0, (m_buttons & SDL_BUTTON_RMASK) != 0 };
    } else {
        return { false, false, false };
    }
}
#else
static std::tuple<bool, bool, bool> updateMouseStatus()
{
    m_buttons = SDL_GetMouseState(&m_x, &m_y);

    bool leftButtonDownNow = (m_buttons & SDL_BUTTON_LMASK) != 0;
    bool leftButtonDownPreviously = (m_previousButtons & SDL_BUTTON_LMASK) != 0;

    bool rightButtonDownNow = (m_buttons & SDL_BUTTON_RMASK) != 0;
    bool rightButtonDownPreviously = (m_previousButtons & SDL_BUTTON_RMASK) != 0;

    bool leftButtonJustPressed = leftButtonDownNow && !leftButtonDownPreviously;
    bool leftButtonJustReleased = !leftButtonDownNow && leftButtonDownPreviously;
    bool rightButtonJustReleased = !rightButtonDownNow && rightButtonDownPreviously;

    m_previousButtons = m_buttons;

    return { leftButtonJustPressed, leftButtonJustReleased, rightButtonJustReleased };
}
#endif

// Tries to recognize if the entry is a scroll arrow by using some heuristics.
static bool isEntryScroller(const MenuEntry& entry)
{
    assert(!m_mouseWheelData.empty());
    const auto& wheelData = m_mouseWheelData.back();

    if (entry.ordinal == wheelData.upEntry || entry.ordinal == wheelData.downEntry)
        return true;

    static const std::array<int, 4> kArrowSprites = { kUpArrowSprite, kDownArrowSprite, kLeftArrowSprite, kRightArrowSprite };
    if (entry.type == kEntrySprite2 && std::find(kArrowSprites.begin(), kArrowSprites.end(), entry.fg.spriteIndex) != kArrowSprites.end())
        return true;

    // also quicken +/- fields
    if (entry.isString() && (entry.string()[0] == '+' || entry.string()[0] == '-') && entry.string()[1] == '\0')
        return true;

    return false;
}

static bool handleClickScrolling()
{
    if (m_scrolling) {
        if (m_buttons & SDL_BUTTON_LMASK) {
            auto now = SDL_GetTicks();

            if (now >= m_lastScrollTick + kScrollRateMs) {
                assert(m_scrollEntry && isEntryScroller(*m_scrollEntry));
                selectEntry(m_scrollEntry);
                m_lastScrollTick = now;
            }

            return true;
        } else {
            m_scrolling = false;
            return true;
        }
    }

    return false;
}

static bool handleRightClickExitMenu(bool rightButtonJustReleased)
{
    if (rightButtonJustReleased) {
        exitCurrentMenu();
        return true;
    }

    return false;
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

static void handleScrollerEntry(MenuEntry& entry)
{
    if (entry.onSelect && isEntryScroller(entry)) {
        m_scrolling = true;
        m_scrollEntry = &entry;
        m_lastScrollTick = SDL_GetTicks() + kScrollRateMs;
        selectEntry(m_scrollEntry);
    }
}

static bool performMouseWheelAction(const MenuEntry& entry, int scrollValue)
{
    assert(!m_mouseWheelData.empty());

    for (const auto& mouseWheelEntry : m_mouseWheelData.back().entries) {
        if (entry.ordinal >= mouseWheelEntry.first && entry.ordinal <= mouseWheelEntry.last) {
            int scrollUpEntryIndex = mouseWheelEntry.scrollUpEntry;
            int scrollDownEntryIndex = mouseWheelEntry.scrollDownEntry;

            assert(scrollUpEntryIndex >= 0 && scrollUpEntryIndex < 256);
            assert(scrollDownEntryIndex >= 0 && scrollDownEntryIndex < 256);

            auto scrollEntry = getMenuEntry(scrollValue > 0 ? scrollUpEntryIndex : scrollDownEntryIndex);
            if (scrollEntry->selectable()) {
                assert(!scrollEntry->onSelect.empty());
                scrollEntry->onSelect();
                return true;
            }

            break;
        }
    }

    return false;
}

static bool isPointInsideEntry(int x, int y, const MenuEntry& entry, int slack = 0)
{
    return x + slack >= entry.x && x - slack <= entry.x + entry.width &&
        y + slack >= entry.y && y - slack <= entry.y + entry.height;
}

static bool isPointerInsideEntry(const MenuEntry& entry, int slack = 0)
{
    return isPointInsideEntry(m_x, m_y, entry, slack);
}

template<typename F>
static MenuEntry *findPointedMenuEntry(F f, int slack = 0)
{
    auto currentMenu = getCurrentMenu();
    auto entries = currentMenu->entries();

    // make sure to check all the entries since there could be invisible/unreachable ones overlapping with selectable
    for (size_t i = 0; i < currentMenu->numEntries; i++) {
        auto& entry = entries[i];
        if (isPointerInsideEntry(entry, slack) && entry.selectable() && m_reachableEntries[entry.ordinal] && f(entry))
            return &entry;
    }

    return nullptr;
}

static MenuEntry *findPointedMenuEntry()
{
    return findPointedMenuEntry([](const auto&) { return true; });
}

// Allow certain area around the mouse pointer to trigger entries when mouse wheel scrolling
static void checkForExpandedWheelScrolling(int scrollAmount)
{
    constexpr int kMouseScrollSlack = 3;
    findPointedMenuEntry([scrollAmount](const auto& entry) { return performMouseWheelAction(entry, scrollAmount); }, kMouseScrollSlack);
}

static bool mouseMovedFromPreviousFrame()
{
    static int m_lastX, m_lastY;

    bool moved = m_lastX != m_x || m_lastY != m_y;

    m_lastX = m_x;
    m_lastY = m_y;

    return moved;
}

static void saveClickCoordinates(bool leftButtonJustPressed)
{
    if (leftButtonJustPressed) {
        m_clickX = m_x;
        m_clickY = m_y;
    }
}

static void checkForEntryClicksAndMouseWheelMovement(bool leftButtonTriggered)
{
    auto currentMenu = getCurrentMenu();

    auto wheelScrollAmount = mouseWheelAmount();
    auto wheelHandled = globalWheelHandler(wheelScrollAmount);

    auto entry = m_currentEntryUnderPointer;

    if (!entry || !entry->selectable() || !isPointerInsideEntry(*entry))
        entry = findPointedMenuEntry();

    if (entry) {
        if (!(m_buttons & SDL_BUTTON_LMASK) && mouseMovedFromPreviousFrame())
            currentMenu->selectedEntry = entry;

        if (leftButtonTriggered) {
            if (isPointInsideEntry(m_clickX, m_clickY, *entry)) {
                currentMenu->selectedEntry = entry;
                triggerMenuFire();
            }
        } else if (m_buttons & SDL_BUTTON_LMASK) {
            handleScrollerEntry(*entry);
        } else if (!wheelHandled && wheelScrollAmount) {
            performMouseWheelAction(*entry, wheelScrollAmount);
        }

        // check again, in case handler hid/disabled it
        if (!entry->selectable())
            entry = nullptr;
    } else if (!wheelHandled && wheelScrollAmount) {
        checkForExpandedWheelScrolling(wheelScrollAmount);
    }

    assert(!entry || entry->selectable());
    m_currentEntryUnderPointer = entry;
}

// Turn delta x and y into direction via tangent with 15 degrees slack.
static int getDirectionMask(int dx, int dy)
{
    assert(dx || dy);

    // 3 | 1
    // --+--
    // 2 | 0
    int quadrantIndex = 2 * (dx < 0) + (dy < 0);

    dx = std::abs(dx);
    dy = std::abs(dy);

    int angleComparison = dx * dx + dy * (dy - 4 * dx);

    // 0 = 0..15 degrees, 1 = 15..75 degrees, 2 = 75..90 degrees
    int angleIndex = dy < 2 * dx ? angleComparison <= 0 : 1 + (angleComparison >= 0);

    static const int kDirections[4][3] = {
        { kRightMask, kDownMask | kRightMask, kDownMask },
        { kRightMask, kUpMask | kRightMask, kUpMask },
        { kLeftMask, kLeftMask | kDownMask, kDownMask },
        { kLeftMask, kUpMask | kLeftMask, kUpMask },
    };

    assert(quadrantIndex >= 0 && quadrantIndex < 4 && angleIndex >= 0 && angleIndex < 3);

    return kDirections[quadrantIndex][angleIndex];
}

static void checkForMouseDragging(bool leftButtonJustPressed)
{
    if (leftButtonJustPressed || !(m_buttons & SDL_BUTTON_LMASK))
        return;

    constexpr int kMinDragDistance = 4;

    auto selectedEntry = getCurrentMenu()->selectedEntry;

    if (selectedEntry && selectedEntry->controlMask & ~(kShortFireMask | kFireMask)) {
        assert(selectedEntry->selectable());

        int dx = m_x - m_clickX;
        int dy = m_y - m_clickY;

        if (std::abs(dx) > kMinDragDistance || std::abs(dy) > kMinDragDistance) {
            auto controlMask = getDirectionMask(dx, dy) | kFireMask;
            selectEntry(selectedEntry, controlMask);
        }
    }
}

void initMenuMouse()
{
    SDL_GetMouseState(&m_initialX, &m_initialY);
    m_mouseInitialBlock = true;
}

void resetMenuMouseData()
{
    determineReachableEntries();
    m_currentEntryUnderPointer = nullptr;
    m_clickX = -1;
    m_clickY = -1;
#ifdef __ANDROID__
    m_blockedFinger = m_fingerId;
    m_hasTouch = false;
#endif
}

void menuMouseOnAboutToShowNewMenu()
{
    m_mouseWheelData.push_back({});
}

void menuMouseOnOldMenuRestored()
{
    m_mouseWheelData.pop_back();
}

void setMouseWheelEntries(const MouseWheelEntryGroupList& mouseWheelEntries)
{
    assert(!m_mouseWheelData.empty());
    auto& wheelData = m_mouseWheelData.back();

    assert(!wheelData.globalHandlerActive());

    wheelData.entries = mouseWheelEntries;
    wheelData.resetGlobalHandler();
}

void setGlobalWheelEntries(int upEntry /* = -1 */, int downEntry /* = -1 */)
{
    assert(!m_mouseWheelData.empty());
    auto& wheelData = m_mouseWheelData.back();

    assert(wheelData.entries.empty());

    wheelData.upEntry = upEntry;
    wheelData.downEntry = downEntry;
    wheelData.entries.clear();
}

static bool mouseMovedFromStartingPosition()
{
    if (m_mouseInitialBlock) {
        if (m_x == m_initialX && m_y == m_initialY)
            return false;
        m_mouseInitialBlock = false;
    }
    return true;
}

static bool shouldMouseReact()
{
    return hasMouseFocus() && mouseMovedFromStartingPosition() && (!isMatchRunning() || replayExitMenuShown());
}

static bool clickValid(int x, int y)
{
#ifdef __ANDROID__
    return shouldMouseReact() && m_x >= 0 && m_x < kVgaWidth && m_y >= 0 && m_y < kVgaHeight;
#else
    return shouldMouseReact() && mapCoordinatesToGameArea(m_x, m_y);
#endif
}

void updateMouse()
{
    bool leftButtonJustPressed, leftButtonJustReleased, rightButtonJustReleased;
    std::tie(leftButtonJustPressed, leftButtonJustReleased, rightButtonJustReleased) = updateMouseStatus();

    if (handleClickScrolling() || handleRightClickExitMenu(rightButtonJustReleased))
        return;

    if (clickValid(m_x, m_y)) {
        saveClickCoordinates(leftButtonJustPressed);
        checkForEntryClicksAndMouseWheelMovement(leftButtonJustReleased);
        checkForMouseDragging(leftButtonJustPressed);
    }
}

std::tuple<bool, int, int> getClickCoordinates()
{
    updateMouseStatus();
    bool valid = clickValid(m_x, m_y);
    return { valid && m_buttons != 0, m_x, m_y };
}

#ifdef __ANDROID__
void updateTouch(float x, float y, SDL_FingerID fingerId)
{
    if (fingerId != m_blockedFinger && fingerId <= m_fingerId) {
        m_touchX = x;
        m_touchY = y;
        m_fingerId = fingerId;
        m_hasTouch = true;
    }
}

void fingerUp(SDL_FingerID fingerId)
{
    if (m_blockedFinger == fingerId)
        m_blockedFinger = kInvalidFinger;
    if (m_fingerId == fingerId)
        m_fingerId = kInvalidFinger;
    m_hasTouch = false;
}
#endif

// Finds and marks every reachable item starting from the initial one. We must track reachability in order
// to avoid selecting items with the mouse that are never supposed to be selected, like say labels.
void determineReachableEntries(bool force /* = false */)
{
    static const void *s_lastMenu;

    if (!force && s_lastMenu == getCurrentPackedMenu())
        return;

    s_lastMenu = getCurrentPackedMenu();

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

bool isEntryReachable(int index)
{
    return m_reachableEntries[index];
}
