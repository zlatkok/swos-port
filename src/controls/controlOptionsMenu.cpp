#include "controls.h"
#include "joypads.h"
#include "menuMouse.h"
#include "joypadConfigMenu.h"
#include "controlOptions.mnu.h"

constexpr int kRedefineKeysHeaderY = 30;
constexpr int kRedefineKeysStartY = 70;
constexpr int kRedefineKeysPromptX = 160;
constexpr int kRedefineKeysColumn1X = 108;

enum class ScanCodeStatus {
    kAvailable,
    kAlreadyUsed,
    kTakenByOtherPlayer,
};

constexpr int kWarningY = 170;
static char m_warningBuffer[64];
static Uint32 m_showWarningTicks;

static PlayerNumber m_playerForSelectingJoypad;

static int m_pl1ControlsOffset;
static int m_pl2ControlsOffset;
static int m_lastNumberOfJoypads;

static std::pair<word, word> m_pl1EntryControls[ControlOptionsMenu::kNumControlEntries];
static std::pair<word, word> m_pl2EntryControls[ControlOptionsMenu::kNumControlEntries];

static int16_t m_autoConnectJoypads;

void showControlOptionsMenu()
{
    showMenu(controlOptionsMenu);
}

static void controlOptionsMenuInit()
{
    using namespace ControlOptionsMenu;

    MouseWheelEntryList entryList;
    static const auto kEntryInfo = {
        std::make_tuple(pl1Control0, pl1ScrollUp, pl1ScrollDown),
        std::make_tuple(pl2Control0, pl2ScrollUp, pl2ScrollDown),
    };

    for (auto entryInfo : kEntryInfo) {
        int baseEntry = std::get<0>(entryInfo);
        int scrollUpEntry = std::get<1>(entryInfo);
        int scrollDownEntry = std::get<2>(entryInfo);

        for (int i = 0; i < kNumControlEntries; i++)
            entryList.emplace_back(baseEntry + i, scrollUpEntry, scrollDownEntry);

        entryList.emplace_back(scrollUpEntry, scrollUpEntry, scrollDownEntry);
        entryList.emplace_back(scrollDownEntry, scrollUpEntry, scrollDownEntry);
    }

    setMouseWheelEntries(entryList);

    m_autoConnectJoypads = getAutoConnectJoypads();
}

static void setupPlayerControlEntries(PlayerNumber player)
{
    using namespace ControlOptionsMenu;

    assert(player == kPlayer1 || player == kPlayer2);
    assert((getPl1Controls() != kJoypad || pl1UsingJoypad()) && (getPl2Controls() != kJoypad || pl2UsingJoypad()));

    static const char aKeyboard[] = "KEYBOARD";
    static const char aMouse[] = "MOUSE";
    static const char aNone[] = "NONE";

    auto plEntryControls = m_pl1EntryControls;
    auto baseControlsEntryIndex = pl1Control0;
    auto scrollUp = pl1ScrollUp;
    auto scrollDown = pl1ScrollDown;
    auto scrollOffset = &m_pl1ControlsOffset;
    int initiallyVisisbleJoypadEntries = 2;

    if (player == kPlayer2) {
        plEntryControls = m_pl2EntryControls;
        baseControlsEntryIndex = pl2Control0;
        scrollUp = pl2ScrollUp;
        scrollDown = pl2ScrollDown;
        scrollOffset = &m_pl2ControlsOffset;
        initiallyVisisbleJoypadEntries = 1;
    }

    int numJoypads = SDL_NumJoysticks();

    bool showScrollArrows = numJoypads > (player == kPlayer1 ? 2 : 1);
    getMenuEntryAddress(scrollUp)->setVisible(showScrollArrows);
    getMenuEntryAddress(scrollDown)->setVisible(showScrollArrows);

    if (!showScrollArrows || *scrollOffset > numJoypads - initiallyVisisbleJoypadEntries)
        *scrollOffset = 0;

    auto baseEntry = getMenuEntryAddress(baseControlsEntryIndex);

    static const char *kFixedEntries[] = { aNone, aKeyboard, aMouse };
    static const Controls kFixedEntryControls[] = { kNone, kKeyboard1, kMouse };
    int startIndex = player == kPlayer1 ? 1 : 0;

    int entriesFilled = 0;

    for (int i = startIndex + *scrollOffset; i < static_cast<int>(std::size(kFixedEntries)); i++) {
        strcpy(baseEntry[entriesFilled].string(), kFixedEntries[i]);
        baseEntry[entriesFilled].u1.entryColor = kLightBlue;
        plEntryControls[entriesFilled].first = kFixedEntryControls[i];

        if (kFixedEntryControls[i] == kKeyboard1 && player == kPlayer2)
            plEntryControls[entriesFilled].first = kKeyboard2;

        entriesFilled++;
    }

    int numAvailableJoypadEntries = kNumControlEntries - entriesFilled;
    assert(entriesFilled <= kNumControlEntries);

    int startJoypad = std::max(0, *scrollOffset - kNumControlEntries + initiallyVisisbleJoypadEntries);
    assert(!numJoypads || startJoypad + numAvailableJoypadEntries <= numJoypads);

    int numJoypadEntries = std::min(kNumControlEntries - entriesFilled, numJoypads);
    assert(numJoypadEntries <= kNumControlEntries);

    for (int i = startJoypad; i < startJoypad + numJoypadEntries; i++) {
        auto destStr = baseEntry[entriesFilled].string();
        auto& joypad = getJoypad(i);

        joypad.tryReopening(i);

        if (joypad.handle) {
            auto name = SDL_JoystickName(joypad.handle);
            strncpy_s(destStr, kStdMenuTextSize, name, _TRUNCATE);
            elideString(destStr, strlen(destStr), kControlButtonWidth - 2);
            toUpper(destStr);
        } else {
            strncpy_s(destStr, kStdMenuTextSize, "<UNKNOWN>", _TRUNCATE);
        }

        baseEntry[entriesFilled].u1.entryColor = kLightBlue;
        plEntryControls[entriesFilled].first = kJoypad;
        plEntryControls[entriesFilled].second = i;
        entriesFilled++;
    }

    while (entriesFilled < kNumControlEntries)
        baseEntry[entriesFilled++].hide();

    assert(entriesFilled == kNumControlEntries);
}

static void highlightSelectedControlEntries()
{
    using namespace ControlOptionsMenu;

    const auto kControlEntries = {
        std::make_tuple(m_pl1EntryControls, pl1Control0, getPl1Controls(), getPl1JoypadIndex()),
        std::make_tuple(m_pl2EntryControls, pl2Control0, getPl2Controls(), getPl2JoypadIndex()),
    };

    for (const auto& entry : kControlEntries) {
        for (int i = 0; i < kNumControlEntries; i++) {
            auto entryControls = std::get<0>(entry)[i].first;
            auto entryJoypadIndex = std::get<0>(entry)[i].second;
            auto baseEntry = std::get<1>(entry);
            auto plControls = std::get<2>(entry);
            auto joypadIndex = std::get<3>(entry);

            if (plControls == entryControls && (entryControls != kJoypad || entryJoypadIndex == joypadIndex))
                getMenuEntryAddress(baseEntry + i)->u1.entryColor = kPurple;
        }
    }
}

static void enableControlConfigButtons()
{
    using namespace ControlOptionsMenu;

    const auto kRedefineControlEntries = {
        std::make_tuple(pl1RedefineControls, pl1SelectJoypad, getPl1Controls()),
        std::make_tuple(pl2RedefineControls, pl2SelectJoypad, getPl2Controls()),
    };

    auto numJoypads = SDL_NumJoysticks();

    for (const auto& entry : kRedefineControlEntries) {
        auto redefineEntryIndex = std::get<0>(entry);
        auto selectJoypadEntryIndex = std::get<1>(entry);
        auto controls = std::get<2>(entry);

        auto redefineEntry = getMenuEntryAddress(redefineEntryIndex);
        auto selectJoypadEntry = getMenuEntryAddress(selectJoypadEntryIndex);

        if (controls == kJoypad || controls == kKeyboard1 || controls == kKeyboard2) {
            redefineEntry->u1.entryColor = kGreen;
            redefineEntry->disabled = 0;
        } else {
            redefineEntry->u1.entryColor = kGray;
            redefineEntry->disabled = 1;
        }

        if (numJoypads) {
            selectJoypadEntry->u1.entryColor = kGreen;
            selectJoypadEntry->disabled = 0;
        } else {
            selectJoypadEntry->u1.entryColor = kGray;
            selectJoypadEntry->disabled = 1;
        }
    }
}

static void controlOptionsMenuOnDraw()
{
    using namespace ControlOptionsMenu;

    updateControls();

    setupPlayerControlEntries(kPlayer1);
    setupPlayerControlEntries(kPlayer2);

    highlightSelectedControlEntries();

    enableControlConfigButtons();

//TODO:
//make determine reachable entries public
//maybe make it cache last entry mnu/num.entries+entry selectable? (then don't recalculate if not needed)
//recalculate reachable entries, with some form of dirty flag?
}

static void drawRedefineKeysMenu(PlayerNumber player, bool allowQuit)
{
    assert(player == kPlayer1 || player == kPlayer2);

    static char selectKeysStr[] = "SELECT KEYS FOR PLAYER 1";
    auto playerNo = selectKeysStr + sizeof(selectKeysStr) - 2;
    assert(*playerNo == '1' || *playerNo == '2');

    *playerNo = player + '1';

    redrawMenuBackground();
    drawMenuTextCentered(kRedefineKeysPromptX, kRedefineKeysHeaderY, selectKeysStr);

    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY, "UP:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 10, "DOWN:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 20, "LEFT:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 30, "RIGHT:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 40, "FIRE:");
    drawMenuText(kRedefineKeysColumn1X, kRedefineKeysStartY + 50, "BENCH:");

    constexpr int kAbortY = kRedefineKeysStartY + 50 + kRedefineKeysStartY - kRedefineKeysHeaderY;

    static_assert(kWarningY >= kAbortY + 10, "Warning and abort labels overlap");

    if (allowQuit)
        drawMenuTextCentered(160, kAbortY, "(MOUSE CLICK ABORTS)");

    if (m_showWarningTicks >= SDL_GetTicks())
        drawMenuTextCentered(kVgaWidth / 2, kWarningY, m_warningBuffer, kYellowText);
}

// Block until next scan code is ready and return it; return -1 if any mouse button was clicked.
static int getScanCode()
{
    while (true) {
        if (m_showWarningTicks && m_showWarningTicks < SDL_GetTicks()) {
            redrawMenuBackground(kWarningY, kWarningY + 13);
            m_showWarningTicks = 0;
            SWOS::FlipInMenu();
        }

        updateControls();
        SWOS::GetKey();

        if (SDL_GetMouseState(nullptr, nullptr))
            return -1;

        if (lastKey)
            return lastKey;

        SDL_Delay(50);
    }
}

static ScanCodeStatus getScanCodeStatus(PlayerNumber player, byte scanCode, const byte *scanCodes, int numKeys)
{
    assert(player == kPlayer1 || player == kPlayer2);

    if (std::find(scanCodes, scanCodes + numKeys, scanCode) != scanCodes + numKeys)
        return ScanCodeStatus::kAlreadyUsed;

    ScanCodeStatus status = ScanCodeStatus::kAvailable;

    if (player == kPlayer1 && getPl2Controls() == kKeyboard2) {
        if (getPl2ScanCodes().has(scanCode))
            status = ScanCodeStatus::kTakenByOtherPlayer;
    } else if (player == kPlayer2 && getPl1Controls() == kKeyboard1) {
        if (getPl1ScanCodes().has(scanCode))
            status = ScanCodeStatus::kTakenByOtherPlayer;
    }

    return status;
}

static void applyNewScanCodes(PlayerNumber player, byte *scanCodes)
{
    if (player == kPlayer1)
        getPl1ScanCodes().fromArray(scanCodes);
    else
        getPl2ScanCodes().fromArray(scanCodes);
}

static void redefineKeys(PlayerNumber player, bool allowQuit = true)
{
    assert(player == kPlayer1 || player == kPlayer2);

    logInfo("Redefining keys for player %d, quit allowed: %s", player == kPlayer1 ? 1 : 2, allowQuit ? "yes" : "no");

    constexpr int kBlockSpriteIndex = 44;
    constexpr int kScanCodeX = kRedefineKeysColumn1X + 40;
    constexpr int kBlockSpriteX = kScanCodeX - 2;
    constexpr int kRowGap = 10;
    constexpr Uint32 kWarningInterval = 750;

    byte scanCodes[6];
    constexpr int numKeys = sizeofarray(scanCodes);

    clearKeyInput();
    m_showWarningTicks = 0;

    while (true) {
        for (int setKeys = 0; setKeys <= numKeys; setKeys++) {
            drawRedefineKeysMenu(player, allowQuit);

            int y = kRedefineKeysStartY;

            for (int i = 0; i < setKeys; i++) {
                auto str = pcScanCodeToString(scanCodes[i]);
                drawMenuText(kScanCodeX, y, str);
                y += kRowGap;
            }

            if (setKeys < numKeys)
                drawMenuSprite(kBlockSpriteX, y, kBlockSpriteIndex);

            SWOS::FlipInMenu();

            if (setKeys < numKeys) {
                auto scanCode = getScanCode();
                if (scanCode == -1)
                    return;

                auto status = getScanCodeStatus(player, scanCode, scanCodes, setKeys);
                if (status == ScanCodeStatus::kAvailable) {
                    scanCodes[setKeys] = scanCode;
                } else {
                    auto scanCodeString = pcScanCodeToString(scanCode);

                    if (status == ScanCodeStatus::kAlreadyUsed)
                        snprintf(m_warningBuffer, sizeof(m_warningBuffer), "%s IS ALREADY USED", scanCodeString);
                    else
                        snprintf(m_warningBuffer, sizeof(m_warningBuffer), "%s IS TAKEN BY PLAYER %d",
                            scanCodeString, player == kPlayer1 ? 2 : 1);

                    setKeys--;
                    m_showWarningTicks = SDL_GetTicks() + kWarningInterval;
                }
            }
        }

        char buf[128];
        sprintf_s(buf, "SATISFIED WITH THE CHOICE? (Y/N%s)", allowQuit ? "/Q" : "");
        drawMenuTextCentered(kRedefineKeysPromptX, 140, buf);

        SWOS::FlipInMenu();

        do {
            auto scanCode = getScanCode();

            if (scanCode == kKeyY) {
                applyNewScanCodes(player, scanCodes);
                clearKeyInput();
                return;
            } else if (scanCode == kKeyN) {
                break;
            } else if (allowQuit && (scanCode == kKeyEscape || scanCode == kKeyQ)) {
                clearKeyInput();
                return;
            }
        } while (true);
    }
}

static void pl1SelectKeyboard()
{
    assert(getPl1Controls() != kKeyboard1 && getPl1Controls() != kKeyboard2);

    logInfo("Player 1 keyboard controls selected");

    setPl1Controls(kKeyboard1);

    if (pl1KeyboardNullScancode()) {
        logInfo("Player 1 keyboard controls containing invalid key(s), redefining...");
        redefineKeys(kPlayer1, false);
    }
}

static void pl1SelectMouse()
{
    assert(getPl1Controls() != kMouse);

    logInfo("Player 1 mouse controls selected");

    setPl1Controls(kMouse);

    clearKeyInput();
    clearPlayer1State();

    if (getPl2Controls() == kMouse)
        setPl2Controls(kNone);
}

static std::tuple<MenuEntry *, int, int, int> getControlsInfo(int baseEntryIndex, decltype(m_pl1EntryControls) plControls)
{
    auto entry = A5.as<MenuEntry *>();
    assert(entry->ordinal >= baseEntryIndex && entry->ordinal < baseEntryIndex + kNumUniqueControls);

    int index = entry->ordinal - baseEntryIndex;
    int newControls = plControls[index].first;
    int joypadNo = plControls[index].second;

    return std::make_tuple(entry, index, newControls, joypadNo);
}

static void pl1SelectControls()
{
    using namespace ControlOptionsMenu;

    MenuEntry *entry;
    int index, newControls, joypadNo;

    std::tie(entry, index, newControls, joypadNo) = getControlsInfo(pl1Control0, m_pl1EntryControls);

    assert(newControls != kJoypad || joypadNo >= 0);

    if (getPl1Controls() != newControls || getPl1Controls() == kJoypad && getPl1JoypadIndex() != joypadNo) {
        switch (newControls) {
        case kKeyboard1:
            pl1SelectKeyboard();
            break;
        case kMouse:
            pl1SelectMouse();
            break;
        case kJoypad:
            selectJoypadControls(kPlayer1, joypadNo);
            break;
        default:
            assert(false);
        }
    }
}

static void pl2SelectNoControls()
{
    assert(getPl2Controls() != kNone);

    logInfo("Player 2 controls disabled");

    setPl2Controls(kNone);
}

static void pl2SelectKeyboard()
{
    assert(getPl2Controls() != kKeyboard1 && getPl2Controls() != kKeyboard2);

    logInfo("Player 2 keyboard controls selected");

    setPl2Controls(kKeyboard2);

    if (pl2KeyboardNullScancode()) {
        logInfo("Player 2 keyboard controls containing invalid key(s), redefining...");
        redefineKeys(kPlayer2, false);
    }
}

static void pl2SelectMouse()
{
    assert(getPl2Controls() != kMouse);

    logInfo("Player 2 mouse controls selected");

    setPl2Controls(kMouse);

    if (getPl1Controls() == kMouse)
        pl1SelectKeyboard();
}

static void pl2SelectControls()
{
    using namespace ControlOptionsMenu;

    MenuEntry *entry;
    int index, newControls, joypadNo;

    std::tie(entry, index, newControls, joypadNo) = getControlsInfo(pl2Control0, m_pl2EntryControls);

    if (getPl2Controls() != newControls || getPl2Controls() == kJoypad && getPl2Controls() != joypadNo) {
        switch (newControls) {
        case kNone:
            pl2SelectNoControls();
            break;
        case kKeyboard2:
            pl2SelectKeyboard();
            break;
        case kMouse:
            pl2SelectMouse();
            break;
        case kJoypad:
            selectJoypadControls(kPlayer2, joypadNo);
            break;
        default:
            assert(false);
        }
    }
}

static void toggleAutoConnectJoypads()
{
    m_autoConnectJoypads = !m_autoConnectJoypads;
    setAutoConnectJoypads(m_autoConnectJoypads != 0);
    logInfo("Auto-connect joypads is changed to %s", m_autoConnectJoypads ? "on" : "off");
}

static void exitControlsMenu()
{
    m_pl1ControlsOffset = 0;
    m_pl2ControlsOffset = 0;
    SetExitMenuFlag();
}

static void pl1ScrollUpSelected()
{
    m_pl1ControlsOffset = std::max(0, m_pl1ControlsOffset - 1);
}

static void pl1ScrollDownSelected()
{
    int maxOffset = std::max(2 + getNumJoypads() - kNumUniqueControls, 0);
    m_pl1ControlsOffset = std::min(m_pl1ControlsOffset + 1, maxOffset);
}

static void pl2ScrollUpSelected()
{
    m_pl2ControlsOffset = std::max(0, m_pl2ControlsOffset - 1);
}

static void pl2ScrollDownSelected()
{
    int maxOffset = std::max(3 + getNumJoypads() - kNumUniqueControls, 0);
    m_pl2ControlsOffset = std::min(m_pl2ControlsOffset + 1, maxOffset);
}

static bool selectJoypadWithButtonPress(PlayerNumber player)
{
    assert(player == kPlayer1 || player == kPlayer2);

    if (SDL_NumJoysticks() < 2)
        return false;

    redrawMenuBackground();

    constexpr int kPromptX = 160;
    constexpr int kPromptY = 40;
    constexpr int kAbortY = 150;

    drawMenuTextCentered(kPromptX, kPromptY, "PRESS ANY BUTTON TO SELECT");

    char text[] = "GAME CONTROLLER FOR PLAYER 1";
    text[sizeof(text) - 2] = player == kPlayer1 ? '1' : '2';

    drawMenuTextCentered(kPromptX, kPromptY + 10, text);
    drawMenuTextCentered(kPromptX, kAbortY, "ESCAPE/MOUSE CLICK ABORTS");

    SWOS::FlipInMenu();

    bool changed = false;

    clearJoypadInput();

    while (true) {
        updateControls();
        SWOS::GetKey();

        if (SDL_GetMouseState(nullptr, nullptr) || lastKey == kKeyEscape)
            break;

        auto joypadIndex = getJoypadWithButtonDown();

        if (joypadIndex >= 0) {
            auto currentControls = player == kPlayer1 ? getPl1Controls() : getPl2Controls();
            auto currentJoypadIndex = player == kPlayer1 ? getPl1JoypadIndex() : getPl2JoypadIndex();

            if (currentControls == kJoypad && currentJoypadIndex == joypadIndex) {
                changed = true;
            } else {
                logInfo("Selecting joypad %d for player %d via button press", joypadIndex, player == kPlayer1 ? 1 : 2);
                changed = selectJoypadControls(player, joypadIndex);
            }

            break;
        }

        SDL_Delay(25);
    }

    if (changed) {
        auto currentJoypadIndex = player == kPlayer1 ? getPl1JoypadIndex() : getPl2JoypadIndex();
        auto name = SDL_JoystickName(getJoypad(currentJoypadIndex).handle);

        char text[128];
        auto len = sprintf_s(text, "%s SELECTED", name ? name : "<UNKNOWN CONTROLLER>");
        elideString(text, len, kMenuScreenWidth - 20);
        toUpper(text);

        drawMenuTextCentered(kPromptX, (kPromptY + kAbortY) / 2, text, kYellowText);
        SWOS::FlipInMenu();
        SDL_Delay(600);
    }

    clearKeyInput();
    clearJoypadInput();

    return changed;
}

static void scrollJoypadIntoView(PlayerNumber player)
{
    assert(player == kPlayer1 || player == kPlayer2);

    auto joypadIndex = player == kPlayer1 ? getPl1JoypadIndex() : getPl2JoypadIndex();
    assert(joypadIndex >= 0 && joypadIndex < SDL_NumJoysticks());

    auto minOffset = std::max(0, joypadIndex - (player == kPlayer1));
    auto maxOffset = joypadIndex + 2 + (player == kPlayer2);

    auto& scrollOffset = player == kPlayer1 ? m_pl1ControlsOffset : m_pl2ControlsOffset;
    if (scrollOffset < minOffset)
        scrollOffset = minOffset;
    if (scrollOffset > maxOffset)
        scrollOffset = maxOffset;
}

static void pl1SelectWhichJoypad()
{
    if (selectJoypadWithButtonPress(kPlayer1))
        scrollJoypadIntoView(kPlayer1);
}

static void pl2SelectWhichJoypad()
{
    if (selectJoypadWithButtonPress(kPlayer2))
        scrollJoypadIntoView(kPlayer2);
}

static void redefinePlayer1Controls()
{
    if (getPl1Controls() == kKeyboard1)
        redefineKeys(kPlayer1);
    else if (getPl1Controls() == kJoypad)
        showJoypadConfigMenu(kPlayer1);
}

static void redefinePlayer2Controls()
{
    if (getPl2Controls() == kKeyboard2)
        redefineKeys(kPlayer2);
    else if (getPl2Controls() == kJoypad)
        showJoypadConfigMenu(kPlayer2);
}
