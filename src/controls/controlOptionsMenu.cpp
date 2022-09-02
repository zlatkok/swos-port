#include "controlOptionsMenu.h"
#include "keyboard.h"
#include "joypads.h"
#include "menuMouse.h"
#include "text.h"
#include "setupKeyboardMenu.h"
#include "joypadConfigMenu.h"

#include "controlOptions.mnu.h"

using namespace ControlOptionsMenu;

static int m_pl1ControlsOffset;
static int m_pl2ControlsOffset;

static std::pair<word, word> m_pl1EntryControls[kNumControlEntries];
static std::pair<word, word> m_pl2EntryControls[kNumControlEntries];

static bool m_keyboardPresent;

void showControlOptionsMenu()
{
    showMenu(controlOptionsMenu);
}

static void setupMouseWheelScrolling();
static void scrollJoypadIntoView(PlayerNumber player);
static int getNumControls();

static void controlOptionsMenuInit()
{
    m_keyboardPresent = keyboardPresent();

    setupMouseWheelScrolling();
    scrollJoypadIntoView(kPlayer1);
    scrollJoypadIntoView(kPlayer2);
}

static void setupMouseWheelScrolling()
{
    setMouseWheelEntries({
        { pl1Control0, pl1Control0 + kNumControlEntries - 1, pl1ScrollUp, pl1ScrollDown },
        { pl2Control0, pl2Control0 + kNumControlEntries - 1, pl2ScrollUp, pl2ScrollDown },
    });
}

static void scrollJoypadIntoView(PlayerNumber player)
{
    assert(player == kPlayer1 || player == kPlayer2);

    if (getControls(player) != kJoypad)
        return;

    auto joypadIndex = player == kPlayer1 ? getPl1JoypadIndex() : getPl2JoypadIndex();
    assert(joypadIndex >= 0 && joypadIndex < getNumJoypads());

    int numBasicControls = getNumControls() - getNumJoypads();
    auto minOffset = std::max(0, joypadIndex + numBasicControls - kNumControlEntries + 1);
    auto maxOffset = joypadIndex + numBasicControls;

    auto& scrollOffset = player == kPlayer1 ? m_pl1ControlsOffset : m_pl2ControlsOffset;
    scrollOffset = std::max(scrollOffset, minOffset);
    scrollOffset = std::min(scrollOffset, maxOffset);
}

static void setupPlayerJoypadControlEntries(decltype(m_pl1EntryControls)& plEntryControls,
    MenuEntry *baseEntry, int& entriesFilled, int scrollOffset, int numControls)
{
    int numJoypads = getNumJoypads();
    int numBasicControls = numControls - numJoypads;
    int startJoypad = std::max(0, scrollOffset - numBasicControls);

    assert(!numJoypads || numJoypads == 1 || startJoypad + kNumControlEntries - entriesFilled <= numJoypads);

    int numJoypadEntries = std::min(kNumControlEntries - entriesFilled, numJoypads);
    assert(numJoypadEntries <= kNumControlEntries);

    for (int i = startJoypad; i < startJoypad + numJoypadEntries; i++) {
        tryReopeningJoypad(i);
        auto& entry = baseEntry[entriesFilled];

        auto name = joypadName(i);
        entry.copyString(name);
        entry.setBackgroundColor(kLightBlue);
        entry.show();

        plEntryControls[entriesFilled].first = kJoypad;
        plEntryControls[entriesFilled++].second = i;
    }
}

static int getNumControls()
{
    // none & mouse always present
    return 2 + (2 * m_keyboardPresent) + getNumJoypads();
}

static void setupPlayerControlEntries(PlayerNumber player, decltype(m_pl1EntryControls)& plEntryControls,
    Entries baseControlsEntryIndex, Entries scrollUp, Entries scrollDown, int& scrollOffset)
{
    using namespace SwosVM;

    assert(player == kPlayer1 || player == kPlayer2);

    int numControls = getNumControls();

    bool showScrollArrows = numControls > kNumControlEntries;
    setEntriesVisibility({ scrollUp, scrollDown }, showScrollArrows);

    if (!showScrollArrows)
        scrollOffset = 0;
    else
        scrollOffset = std::min(scrollOffset, numControls - kNumControlEntries);

    auto baseEntry = getMenuEntry(baseControlsEntryIndex);

    static const std::array<std::pair<Controls, const char *>, 4> kBasicControls = {{
        { kNone, "NONE" }, { kKeyboard1, "KEYBOARD1" }, { kKeyboard2, "KEYBOARD2" },
#ifdef __ANDROID__
        { kMouse, "TOUCH", }
#else
        { kMouse, "MOUSE", }
#endif
    }};

    int entriesFilled = 0;

    for (int i = scrollOffset; i < static_cast<int>(kBasicControls.size()); i++) {
        if ((kBasicControls[i].first != kKeyboard1 && kBasicControls[i].first != kKeyboard2) || m_keyboardPresent) {
            auto& entry = baseEntry[entriesFilled];
            entry.copyString(kBasicControls[i].second);
            entry.bg.entryColor = kLightBlue;
            entry.show();
            plEntryControls[entriesFilled].first = kBasicControls[i].first;
            plEntryControls[entriesFilled++].second = kNoJoypad;
        }
    }

    setupPlayerJoypadControlEntries(plEntryControls, baseEntry, entriesFilled, scrollOffset, numControls);

    assert(entriesFilled <= kNumControlEntries);

    while (entriesFilled < kNumControlEntries)
        baseEntry[entriesFilled++].hide();

    assert(entriesFilled == kNumControlEntries);
}

static void highlightSelectedControlEntries(decltype(m_pl1EntryControls) entry, Entries baseEntry, Controls plControls, int joypadIndex)
{
    assert(plControls != kJoypad || joypadIndex != kNoJoypad);

    for (int i = 0; i < kNumControlEntries; i++) {
        auto entryControls = entry[i].first;
        auto entryJoypadIndex = entry[i].second;

        if (plControls == entryControls && (entryControls != kJoypad || entryJoypadIndex == joypadIndex))
            getMenuEntry(baseEntry + i)->bg.entryColor = kPurple;
    }
}

static void enableControlConfigButtons(MenuEntry &redefineEntry, Controls controls)
{
    if (controls == kJoypad || controls == kKeyboard1 || controls == kKeyboard2) {
        redefineEntry.setBackgroundColor(kGreen);
        redefineEntry.enable();
    } else {
        redefineEntry.setBackgroundColor(kGray);
        redefineEntry.disable();
    }
}

static void fixSelection()
{
    auto& selectedEntry = getCurrentMenu()->selectedEntry;

    if (selectedEntry && !selectedEntry->selectable()) {
        if (selectedEntry == &pl1ScrollUpEntry || selectedEntry == &pl1ScrollDownEntry)
            selectedEntry = &pl1LastControlEntry;
        else if (selectedEntry == &pl2ScrollUpEntry || selectedEntry == &pl2ScrollDownEntry)
            selectedEntry = &pl2LastControlEntry;

        while (selectedEntry >= &pl1FirstControlEntry && selectedEntry <= &pl1LastControlEntry && !selectedEntry->selectable())
            selectedEntry--;

        while (selectedEntry >= &pl2FirstControlEntry && selectedEntry <= &pl2LastControlEntry && !selectedEntry->selectable())
            selectedEntry--;

        if (!selectedEntry->selectable())
            selectedEntry = &exitEntry;
    }
}

static void controlOptionsMenuOnDraw()
{
    m_keyboardPresent = keyboardPresent();

    if (!m_keyboardPresent)
        unsetKeyboardControls();

    setupPlayerControlEntries(kPlayer1, m_pl1EntryControls, pl1Control0, pl1ScrollUp, pl1ScrollDown, m_pl1ControlsOffset);
    setupPlayerControlEntries(kPlayer2, m_pl2EntryControls, pl2Control0, pl2ScrollUp, pl2ScrollDown, m_pl2ControlsOffset);

    highlightSelectedControlEntries(m_pl1EntryControls, pl1Control0, getPl1Controls(), getPl1JoypadIndex());
    highlightSelectedControlEntries(m_pl2EntryControls, pl2Control0, getPl2Controls(), getPl2JoypadIndex());

    enableControlConfigButtons(pl1RedefineControlsEntry, getPl1Controls());
    enableControlConfigButtons(pl2RedefineControlsEntry, getPl2Controls());

    fixSelection();
}

static void redefineControls(PlayerNumber player)
{
    switch (getControls(player)) {
    case kKeyboard1:
        showSetupKeyboardMenu(Keyboard::kSet1);
        break;
    case kKeyboard2:
        showSetupKeyboardMenu(Keyboard::kSet2);
        break;
    case kJoypad:
        showJoypadConfigMenu(player, getJoypadIndex(player));
        break;
    default:
        assert(false);
    }
}

static void selectControlsForPlayer(PlayerNumber player, Controls controls, int joypadIndex)
{
    assert(player == kPlayer1 || player == kPlayer2);

    logInfo("Player %d %s selected", player + 1, controlsToString(controls));

    if (!setControls(player, controls, joypadIndex)) {
        logInfo("Couldn't set controls");
        return;
    }

    if (controls == kKeyboard1 || controls == kKeyboard2) {
        auto keyboard = controls == kKeyboard1 ? Keyboard::kSet1 : Keyboard::kSet2;
        if (!keyboardHasBasicBindings(keyboard)) {
            int keyboardNum = keyboard == Keyboard::kSet1 ? 1 : 2;
            logInfo("Keyboard %d controls don't include basic controls, redefining...", keyboardNum);
            redefineControls(player);
        }
    }
}

// A5 -> selected controls entry
static std::pair<Controls, int> getControlsInfo(PlayerNumber player)
{
    assert(player == kPlayer1 || player == kPlayer2);

    const auto& plControls = player == kPlayer1 ? m_pl1EntryControls : m_pl2EntryControls;
    int baseEntryIndex = player == kPlayer1 ? pl1Control0 : pl2Control0;

    SDL_UNUSED auto entry = A5.as<MenuEntry *>();
    assert(entry->ordinal >= baseEntryIndex && entry->ordinal < baseEntryIndex + kNumUniqueControls);

    int index = entry->ordinal - baseEntryIndex;
    auto newControls = static_cast<Controls>(plControls[index].first);
    int joypadNo = newControls == kJoypad ? plControls[index].second : kNoJoypad;

    return { newControls, joypadNo };
}

static void selectControls(PlayerNumber player)
{
    Controls controls;
    int joypadNo;
    std::tie(controls, joypadNo) = getControlsInfo(player);

    assert(controls != kJoypad || joypadNo != kNoJoypad);
    selectControlsForPlayer(player, controls, joypadNo);
}

static void pl1SelectControls()
{
    selectControls(kPlayer1);
}

static void pl2SelectControls()
{
    selectControls(kPlayer2);
}

static void exitControlsMenu()
{
    m_pl1ControlsOffset = 0;
    m_pl2ControlsOffset = 0;
    SetExitMenuFlag();
}

static int maxScrollOffset()
{
    return std::max(0, getNumControls() - kNumControlEntries);
}

static void pl1ScrollUpSelected()
{
    m_pl1ControlsOffset = std::max(0, m_pl1ControlsOffset - 1);
}

static void pl1ScrollDownSelected()
{
    m_pl1ControlsOffset = std::min(m_pl1ControlsOffset + 1, maxScrollOffset());
}

static void pl2ScrollUpSelected()
{
    m_pl2ControlsOffset = std::max(0, m_pl2ControlsOffset - 1);
}

static void pl2ScrollDownSelected()
{
    m_pl2ControlsOffset = std::min(m_pl2ControlsOffset + 1, maxScrollOffset());
}

static void redefinePlayer1Controls()
{
    redefineControls(kPlayer1);
}

static void redefinePlayer2Controls()
{
    redefineControls(kPlayer2);
}
