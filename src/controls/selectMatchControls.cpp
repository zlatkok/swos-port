#include "selectMatchControls.h"
#include "menuMouse.h"
#include "drawMenu.h"
#include "render.h"
#include "controls.h"
#include "joypads.h"
#include "keyboard.h"
#include "setupKeyboardMenu.h"
#include "joypadConfigMenu.h"
#include "selectMatchControls.mnu.h"

static bool m_success;
static int m_controlsScrollOffset;
static bool m_twoPlayers;
static bool m_keyboardPresent;

static bool m_blockControlAssignment;
static bool m_fadedOut;

using namespace SelectMatchControlsMenu;

static std::array<std::pair<Controls, int>, kNumControlEntries> m_controls;

static int numPlayers()
{
    return (swos.playMatchTeam1Ptr->teamControls != kComputerTeam) + (swos.playMatchTeam2Ptr->teamControls != kComputerTeam);
}

static bool isFinalTeamSetup()
{
    if (numPlayers() == 0)
        return false;

    return !swos.isFirstPlayerChoosingTeam || numPlayers() < 2;
}

void SWOS::PlayMatchSelected()
{
    m_fadedOut = false;

    if (reinterpret_cast<TeamFile *>(swos.careerTeam)->teamControls == kComputerTeam)
        swos.careerTeamSetupDone = 1;

    swos.g_allowShorterMenuItemsWithFrames = 0;

    if (!isFinalTeamSetup()) {
        swos.playerNumThatStarted = 1;
    } else {
        swos.playerNumThatStarted = std::max(1, numPlayers());

        if (swos.squadChangesAllowed && getShowSelectMatchControlsMenu() && !showSelectMatchControlsMenu())
            return;

        if (!m_fadedOut)
            menuFadeOut();
    }

    SetExitMenuFlag();
}

// Makes sure players can't be selected via mouse, and also returns mouse selection to normal
// once we're done with viewing opponent's team.
void SWOS::RecalculateReachableEntries()
{
    determineReachableEntries(true);
}

bool showSelectMatchControlsMenu()
{
    assert(swos.playMatchTeam1Ptr && swos.playMatchTeam2Ptr);

    m_success = false;
    m_twoPlayers = numPlayers() > 1;

    showMenu(selectMatchControlsMenu);

    return m_success;
}

static void initMenu();
static void rearrangeEntriesForSinglePlayer();
static void setupMouseWheelScrolling();
static void setPlayOrWatchLabel();
static void updateMenu();
static void updateTeamNames();
static void updateInstructions();
static void updateCurrentControls();
static void updateControlList();
static void updateScrollButtons();
static void updateConfigureButtons();
static void updateWarnings();
static void updatePlayButton();
static void fixSelection();

static void selectMatchControlsMenuOnInit()
{
    initMenu();
    updateMenu();
}

static void selectMatchControlsMenuOnRestore()
{
    initMenu();
    updateMenu();
}

static void updateDragBlock()
{
    bool mouseDown = (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;

    if (!m_twoPlayers) {
        bool fire = (swos.controlMask & (kFireMask | kShortFireMask)) != 0;
        if (!mouseDown && !fire)
            m_blockControlAssignment = false;
    } else {
        bool keyboardLeftRightDown = (swos.controlMask & (kLeftMask | kRightMask)) != 0;
        if (!mouseDown && !keyboardLeftRightDown)
            m_blockControlAssignment = false;
    }
}

static void selectMatchControlsMenuOnDraw()
{
    updateDragBlock();
    updateMenu();
}

static void selectControl()
{
    if (m_blockControlAssignment)
        return;

    auto entry = A5.asMenuEntry();

    assert(entry->ordinal >= firstControl && entry->ordinal <= lastControl);

    if (swos.controlMask & (kFireMask | kShortFireMask)) {
        int offset = entry->ordinal - firstControl;

        if (!m_twoPlayers || (swos.controlMask & (kLeftMask | kRightMask))) {
            bool moveLeft = (swos.controlMask & kLeftMask) != 0;
            bool setPlayer1 = !m_twoPlayers || moveLeft;
            auto player = setPlayer1 ? kPlayer1 : kPlayer2;

            switch (m_controls[offset].first) {
            case kKeyboard1:
                setControls(player, kKeyboard1);
                break;
            case kKeyboard2:
                setControls(player, kKeyboard2);
                break;
            case kMouse:
                setControls(player, kMouse);
                break;
            case kJoypad:
                setControls(player, kJoypad, m_controls[offset].second);
                break;
            default:
                assert(false);
            }

            m_blockControlAssignment = true;
        }
    }
}

static void player1ControlsSelected()
{
    if (!m_blockControlAssignment && (swos.controlMask & (kFireMask | kShortFireMask)))
        setPl1Controls(kNone);

    m_blockControlAssignment = true;
}

static void player2ControlsSelected()
{
    assert(m_twoPlayers);

    if (!m_blockControlAssignment && (swos.controlMask & (kFireMask | kShortFireMask)))
        setPl2Controls(kNone);

    m_blockControlAssignment = true;
}

static void cancelSelection()
{
    m_success = false;
    SetExitMenuFlag();
}

static void confirmSelection()
{
    m_success = true;
    SetExitMenuFlag();

    menuFadeOut();
    m_fadedOut = true;
}

static void configureControls(PlayerNumber player)
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

static void configurePl1Controls()
{
    configureControls(kPlayer1);
}

static void configurePl2Controls()
{
    assert(m_twoPlayers);

    configureControls(kPlayer2);
}

static void scrollUpSelected()
{
    if (m_controlsScrollOffset > 0) {
        m_controlsScrollOffset--;
        updateControlList();
    }
}

static int numControls()
{
    int numJoypadPlayers = (getPl1Controls() == kJoypad) + (m_twoPlayers && getPl2Controls() == kJoypad);

    int numBaseControls = 1;
    int numBaseControlsPlayers = getPl1Controls() == kMouse || getPl2Controls() == kMouse;
    if (m_keyboardPresent) {
        numBaseControlsPlayers += getPl1Controls() == kKeyboard1 || getPl1Controls() == kKeyboard2;
        numBaseControlsPlayers += m_twoPlayers && (getPl2Controls() == kKeyboard1 || getPl2Controls() == kKeyboard2);
        numBaseControls += 2;
    }

    return getNumJoypads() - numJoypadPlayers + numBaseControls - numBaseControlsPlayers;
}

static void scrollDownSelected()
{
    int maxScrollOffset = numControls() - kNumControlEntries;

    if (m_controlsScrollOffset < maxScrollOffset) {
        m_controlsScrollOffset++;
        updateControlList();
    }
}

static void initMenu()
{
    // must initialize it here since rearranging entries for single player depends on it
    m_keyboardPresent = keyboardPresent();

    updateTeamNames();
    updateInstructions();
    rearrangeEntriesForSinglePlayer();
    setupMouseWheelScrolling();
    setPlayOrWatchLabel();
}

static void rearrangeEntriesForSinglePlayer()
{
    if (!m_twoPlayers) {
        constexpr int kColumn1Width = 153;
        constexpr int kColumn2Width = 152;
        static_assert(3 * kGapWidth + kColumn1Width + kColumn2Width == kMenuScreenWidth, "Yarrr");

        for (auto entry : { &team1Entry, &player1ControlsEntry, &player1WarningEntry, &configure1Entry })
            entry->width = kColumn1Width;

        bool gotScroller = numControls() > kNumControlEntries;

        int column2Width = kColumn2Width - (gotScroller ? scrollUpEntry.width : 0);

        for (auto entry = &controlsLabelEntry; entry <= &lastControlEntry; entry++) {
            entry->x = 2 * kGapWidth + kColumn1Width;
            entry->width = column2Width;

            if (gotScroller && entry < &secondLastControlEntry)
                entry->rightEntry = scrollUp;
        }

        scrollUpEntry.x = controlsLabelEntry.endX() + 1;
        scrollDownEntry.x = scrollUpEntry.x;

        playEntry.x = controlsLabelEntry.x;
        playEntry.width = (column2Width - kBottomButtonsColumnGap) / 2;
        backEntry.x = playEntry.endX() + kBottomButtonsColumnGap;
        backEntry.width = playEntry.width;
    }
}

static void setupMouseWheelScrolling()
{
    setMouseWheelEntries({{ firstControl, firstControl + kNumControlEntries - 1, scrollUp, scrollDown }});
}

void setPlayOrWatchLabel()
{
    int numCoaches = (swos.playMatchTeam1Ptr->teamControls == kCoach) + (swos.playMatchTeam2Ptr->teamControls == kCoach);
    playEntry.copyString(numCoaches == numPlayers() ? "COACH" : "PLAY");
}

static void updateMenu()
{
    m_keyboardPresent = keyboardPresent();

    if (!m_keyboardPresent)
        unsetKeyboardControls();

    updateScrollButtons();
    updateControlList();
    updateCurrentControls();
    updateConfigureButtons();
    updateWarnings();
    updatePlayButton();
    fixSelection();
}

static void updateTeamNames()
{
    team1Entry.copyString(swos.playMatchTeam1Ptr->teamName);
    if (m_twoPlayers)
        team2Entry.copyString(swos.playMatchTeam2Ptr->teamName);
    else
        team2Entry.hide();
}

void updateInstructions()
{
    legendEntry.copyString(m_twoPlayers ? "SELECT CONTROLLER, HOLD FIRE AND PRESS LEFT/RIGHT" :
        "SELECT YOUR CONTROLLER ABOVE");
}

static const char *controlsToString(Controls controls, int joypadIndex)
{
    switch (controls) {
    case kNone: return "NONE";
    case kKeyboard1: return "KEYBOARD1";
    case kKeyboard2: return "KEYBOARD2";
#ifdef __ANDROID__
    case kMouse: return "TOUCH";
#else
    case kMouse: return "MOUSE";
#endif
    case kJoypad: return joypadName(joypadIndex);
    default: return "";
    }
}

static void updateCurrentControls()
{
    auto pl1Controls = getPl1Controls();
    if (pl1Controls != kNone) {
        player1ControlsEntry.show();
        auto controlString = controlsToString(pl1Controls, getPl1JoypadIndex());
        player1ControlsEntry.copyString(controlString);
    } else {
        player1ControlsEntry.hide();
    }

    auto pl2Controls = getPl2Controls();
    if (m_twoPlayers && pl2Controls != kNone) {
        player2ControlsEntry.show();
        auto controlString = controlsToString(pl2Controls, getPl2JoypadIndex());
        player2ControlsEntry.copyString(controlString);
    } else {
        player2ControlsEntry.hide();
    }
}

static void updateControlList()
{
    assert(kNumControlEntries > 3 && (numControls() > kNumControlEntries || !m_controlsScrollOffset));

    int keyboard1Available = m_keyboardPresent && getPl1Controls() != kKeyboard1 && (!m_twoPlayers || getPl2Controls() != kKeyboard1);
    int keyboard2Available = m_keyboardPresent && getPl1Controls() != kKeyboard2 && (!m_twoPlayers || getPl2Controls() != kKeyboard2);

    auto entry = &firstControlEntry;
    auto sentinel = entry + kNumControlEntries;
    int controlsIndex = 0;

    if (m_controlsScrollOffset < 1 && keyboard1Available) {
        entry->copyString("KEYBOARD1");
        entry++->show();
        m_controls[controlsIndex++].first = kKeyboard1;
    }
    if (m_controlsScrollOffset < (keyboard1Available + 1) && keyboard2Available) {
        entry->copyString("KEYBOARD2");
        entry++->show();
        m_controls[controlsIndex++].first = kKeyboard2;
    }
    if (m_controlsScrollOffset < (keyboard1Available + keyboard2Available + 1)) {
#ifdef __ANDROID__
        entry->copyString("TOUCH");
#else
        entry->copyString("MOUSE");
#endif
        entry++->show();
        m_controls[controlsIndex++].first = kMouse;
    }

    int numBaseControls = 1 + keyboard1Available + keyboard2Available;
    int initialJoypadIndex = std::max(0, m_controlsScrollOffset - numBaseControls);

    for (int joypadIndex = initialJoypadIndex; joypadIndex < getNumJoypads() && entry < sentinel; joypadIndex++) {
        if (joypadIndex != getPl1JoypadIndex() && (!m_twoPlayers || joypadIndex != getPl2JoypadIndex())) {
            entry->copyString(joypadName(joypadIndex));
            entry++->show();
            m_controls[controlsIndex].first = kJoypad;
            m_controls[controlsIndex++].second = joypadIndex;
        }
    }

    while (entry < sentinel)
        entry++->hide();
}

static void updateScrollButtons()
{
    static const std::vector<int> kScrollButtons = { scrollDown, scrollUp };

    if (numControls() <= kNumControlEntries) {
        m_controlsScrollOffset = 0;
        setEntriesVisibility(kScrollButtons, false);
    } else {
        m_controlsScrollOffset = std::min(m_controlsScrollOffset, numControls() - kNumControlEntries);
        setEntriesVisibility(kScrollButtons, true);
    }
}

static void setEntryDisabled(MenuEntry& entry, bool disabled)
{
    if (disabled) {
        entry.disable();
        entry.setBackgroundColor(kDisabledColor);
    } else {
        entry.enable();
        entry.setBackgroundColor(kEnabledColor);
    }
}

void updateConfigureButtons()
{
    bool configure1Disabled = getPl1Controls() == kNone || getPl1Controls() == kMouse;
    setEntryDisabled(configure1Entry, configure1Disabled);

    if (m_twoPlayers) {
        bool configure2Disabled = getPl2Controls() == kNone || getPl2Controls() == kMouse;
        setEntryDisabled(configure2Entry, configure2Disabled);
    } else {
        configure2Entry.hide();
    }
}

static bool hasBasicEvents(Controls controls, int joypadIndex)
{
    switch (controls) {
    case kKeyboard1:
        return keyboard1HasBasicBindings();
    case kKeyboard2:
        return keyboard2HasBasicBindings();
    case kMouse:
    case kNone:
        return true;
    case kJoypad:
        return joypadHasBasicBindings(joypadIndex);
    default:
        assert(false);
        return false;
    }
}

static void updateWarnings()
{
    if (!hasBasicEvents(getPl1Controls(), getPl1JoypadIndex())) {
        player1WarningEntry.show();
        player1ControlsEntry.setFrameColor(kYellow);
    } else {
        player1ControlsEntry.setFrameColor(kNoFrame);
        player1WarningEntry.hide();
    }

    if (m_twoPlayers && !hasBasicEvents(getPl2Controls(), getPl2JoypadIndex())) {
        player2WarningEntry.show();
        player2ControlsEntry.setFrameColor(kYellow);
    } else {
        player2ControlsEntry.setFrameColor(kNoFrame);
        player2WarningEntry.hide();
    }
}

void updatePlayButton()
{
    if (getPl1Controls() == kNone || m_twoPlayers && getPl2Controls() == kNone) {
        playEntry.disable();
        playEntry.setBackgroundColor(kDisabledColor);
    } else {
        playEntry.enable();
        playEntry.setBackgroundColor(kRed);
    }
}

void fixSelection()
{
    auto& selectedEntry = getCurrentMenu()->selectedEntry;

    if (!selectedEntry->selectable()) {
        if (selectedEntry->ordinal == play)
            selectedEntry = &backEntry;
        else if (selectedEntry->ordinal == player1Controls || selectedEntry->ordinal == player2Controls)
            selectedEntry = &firstControlEntry;

        while (!selectedEntry->selectable() && selectedEntry->ordinal >= firstControl)
            selectedEntry--;

        if (selectedEntry->ordinal < firstControl)
            selectedEntry = &backEntry;
    }

    assert(selectedEntry->selectable());
}
