#include "controls.h"
#include "gameControlEvents.h"
#include "keyBuffer.h"
#include "keyboard.h"
#include "joypads.h"
#include "mouse.h"
#include "menuMouse.h"
#include "game.h"
#include "util.h"
#include "VirtualJoypad.h"

static Controls m_pl1Controls = kNone;
static Controls m_pl2Controls = kNone;

static int m_mouseWheelAmount;

static bool m_shortcutsEnabled = true;

// options
static bool m_showSelectMatchControlsMenu = true;

static constexpr char kControlsSection[] = "controls";

const char kPlayer1ControlsKey[] = "player1Controls";
const char kPlayer2ControlsKey[] = "player2Controls";

static constexpr char kShowSelectMatchControlsMenu[] = "showSelectMatchControlsMenu";

const char *controlsToString(Controls controls)
{
    switch (controls) {
    case kNone: return "<none>";
    case kKeyboard1: return "keyboard 1";
    case kKeyboard2: return "keyboard 2";
    case kMouse: return "mouse";
    case kJoypad: return "game controller";
    default:
        assert(false);
        return "<unknown>";
    }
    static_assert(kNumControls == 5, "A control type is missing a string description");
}

Controls getPl1Controls()
{
    return m_pl1Controls;
}

Controls getPl2Controls()
{
    return m_pl2Controls;
}

Controls getControls(PlayerNumber player)
{
    return player == kPlayer1 ? m_pl1Controls : m_pl2Controls;
}

bool setControls(PlayerNumber player, Controls controls, int joypadIndex)
{
    assert(player == kPlayer1 || player == kPlayer2);

    auto& plControls = player == kPlayer1 ? m_pl1Controls : m_pl2Controls;
    auto otherControls = player == kPlayer1 ? m_pl2Controls : m_pl1Controls;
    auto otherPlayer = static_cast<PlayerNumber>(player ^ 1);
    auto currentJoypadIndex = player == kPlayer1 ? getPl1JoypadIndex() : getPl2JoypadIndex();

    if (controls != plControls || controls == kJoypad && joypadIndex != currentJoypadIndex) {
        logInfo("Setting player %d controls to %s", player == kPlayer1 ? 1 : 2, controlsToString(controls));

        plControls = controls;
        bool success = setJoypad(player, joypadIndex);

        if (joypadIndex == kNoJoypad && controls == otherControls) {
            assert(controls != kJoypad);

            switch (controls) {
            case kKeyboard1:
                setControls(otherPlayer, kKeyboard2);
                break;
            case kMouse:
            case kKeyboard2:
                setControls(otherPlayer, kKeyboard1);
                break;
            }
        }

        return success;
    }

    return true;
}

void setPl1Controls(Controls controls, int joypadIndex /* = kNoJoypad */)
{
    setControls(kPlayer1, controls, joypadIndex);
}

void setPl2Controls(Controls controls, int joypadIndex /* = kNoJoypad */)
{
    setControls(kPlayer2, controls, joypadIndex);
}

void unsetKeyboardControls()
{
    if (m_pl1Controls == kKeyboard1 || m_pl1Controls == kKeyboard2)
        setPl1Controls(kNone);
    if (m_pl2Controls == kKeyboard1 || m_pl2Controls == kKeyboard2)
        setPl2Controls(kNone);
}

static void checkQuitEvent()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT)
            std::exit(EXIT_SUCCESS);
}

static bool keyboardOrMouseActive()
{
    processControlEvents();

    int numKeys;
    auto keyState = SDL_GetKeyboardState(&numKeys);

    if (std::find(keyState, keyState + numKeys, 1) != keyState + numKeys)
        return true;

    if (std::get<0>(getClickCoordinates()))
        return true;

    return false;
}

bool anyInputActive()
{
    if (keyboardOrMouseActive())
        return true;

    if (getJoypadWithButtonDown() >= 0)
        return true;

    checkQuitEvent();

    return false;
}

std::tuple<bool, int, int> mouseClickAndRelease()
{
    auto click = getClickCoordinates();
    int x = std::get<1>(click);
    int y = std::get<2>(click);

    if (std::get<0>(click)) {
        do {
            processControlEvents();
#ifdef VIRTUAL_JOYPAD
            if (getVirtualJoypad().events() != kNoGameEvents)
                return {};
#endif
            SDL_Delay(50);
            click = getClickCoordinates();
        } while (std::get<0>(click));

        return { true, x, y };
    }

    return click;
}

void waitForKeyboardAndMouseIdle()
{
    while (keyboardOrMouseActive())
        SDL_Delay(20);
}

int mouseWheelAmount()
{
    // for scrolling menu lists
    return m_mouseWheelAmount;
}

static void processEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_QUIT:
        std::exit(EXIT_SUCCESS);

    case SDL_KEYDOWN:
    case SDL_KEYUP:
        {
            auto key = event.key.keysym.scancode;
            auto sym = event.key.keysym.sym;    // needed for Unicode chars
            bool pressed = event.type == SDL_KEYDOWN;

            if (m_shortcutsEnabled)
                checkGlobalKeyboardShortcuts(key, pressed);

            if (pressed)
                registerKey(key, sym);
        }
        break;

    case SDL_JOYDEVICEADDED:
        logInfo("Adding joypad %d", event.jdevice.which);
        addNewJoypad(event.jdevice.which);
        break;

    case SDL_JOYDEVICEREMOVED:
        logInfo("Removing joypad %d", event.jdevice.which);
        removeJoypad(event.jdevice.which);
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        updateMouseButtons(event.button.button, event.button.state);
        break;

    case SDL_MOUSEMOTION:
        updateMouseMovement(event.motion.xrel, event.motion.yrel, event.motion.state);
        break;

    case SDL_MOUSEWHEEL:
        if (event.wheel.y)
            m_mouseWheelAmount = event.wheel.y;
        break;
#ifdef __ANDROID__
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
        if (!getVirtualJoypad().updateTouch(event.tfinger.x, event.tfinger.y, event.tfinger.fingerId, event.tfinger.timestamp))
            updateTouch(event.tfinger.x, event.tfinger.y, event.tfinger.fingerId);
        break;
    case SDL_FINGERUP:
        getVirtualJoypad().removeTouch(event.tfinger.fingerId);
        fingerUp(event.tfinger.fingerId);
        break;
#endif
    }
}

void waitForKeyPress()
{
    SDL_Event event;

    while (true) {
        if (SDL_WaitEvent(&event)) {
            processEvent(event);

            if (event.type == SDL_KEYDOWN)
                return;
        }
    }
}

std::tuple<bool, SDL_Scancode, int, int> getKeyInterruptible()
{
    auto click = mouseClickAndRelease();

    if (std::get<0>(click))
        return { true, SDL_SCANCODE_UNKNOWN, std::get<1>(click), std::get<2>(click) };

    processControlEvents();
    auto key = getKey();

    if (numKeysInBuffer() > 3)
        flushKeyBuffer();

    return { false, key, 0, 0 };
}

void processControlEvents()
{
    m_mouseWheelAmount = 0;

    SDL_Event event;

    while (SDL_PollEvent(&event))
        processEvent(event);
}

void setGlobalShortcutsEnabled(bool enabled)
{
    m_shortcutsEnabled = enabled;
}

bool getShowSelectMatchControlsMenu()
{
    return m_showSelectMatchControlsMenu;
}

void setShowSelectMatchControlsMenu(bool value)
{
    m_showSelectMatchControlsMenu = value;
}

void loadControlOptions(const CSimpleIni& ini)
{
    loadJoypadOptions(kControlsSection, ini);
    loadKeyboardConfig(ini);

    m_pl1Controls = kKeyboard1;
    auto pl1Controls = ini.GetLongValue(kControlsSection, kPlayer1ControlsKey);
    if (pl1Controls >= 0 && pl1Controls < kNumControls)
        m_pl1Controls = static_cast<Controls>(pl1Controls);

    m_pl2Controls = kNone;
    auto pl2Controls = ini.GetLongValue(kControlsSection, kPlayer2ControlsKey);
    if (pl2Controls >= 0 && pl2Controls < kNumControls)
        m_pl2Controls = static_cast<Controls>(pl2Controls);

    m_showSelectMatchControlsMenu = ini.GetBoolValue(kControlsSection, kShowSelectMatchControlsMenu, true);

    logInfo("Controls set to: %s (player 1), %s (player 2)", controlsToString(m_pl1Controls), controlsToString(m_pl2Controls));
}

void printEventInfoComment(CSimpleIni& ini)
{
    char comment[512] = "; ";
    auto sentinel = comment + sizeof(comment);
    auto p = comment + 2;

    traverseEvents(kGameEventAll, [&](auto event) {
        p += gameControlEventToString(event, p, sentinel - p);
        if (p < sentinel)
            p += snprintf(p, sentinel - p, " = 0x%x\n; ", event);
    });

    assert(p > comment + 4 && p < sentinel && p[-2] == ';');

    p[-2] = '\0';

    for (auto q = comment; *q; q++)
        *q = tolower(*q);

    ini.SetValue(kControlsSection, nullptr, nullptr, comment);
}

void saveControlOptions(CSimpleIni& ini)
{
    printEventInfoComment(ini);

    const char kInputControlsComment[] = "; 0 = none, 1 = keyboard, 2 = mouse, 3 = game controller";

    ini.SetLongValue(kControlsSection, kPlayer1ControlsKey, m_pl1Controls, kInputControlsComment);
    ini.SetLongValue(kControlsSection, kPlayer2ControlsKey, m_pl2Controls);

    ini.SetBoolValue(kControlsSection, kShowSelectMatchControlsMenu, m_showSelectMatchControlsMenu);

    saveJoypadOptions(kControlsSection, ini);
    saveKeyboardConfig(ini);
}

void initGameControls()
{
    resetMouse();

    // TODO: kill with fire when possible
    swos.pl1Fire = 0;
    swos.pl2Fire = 0;
}

bool gotMousePlayer()
{
    return m_pl1Controls == kMouse || m_pl2Controls == kMouse;
}

// Returns true if the key belongs to selected keys of any currently active player.
bool testForPlayerKeys(SDL_Scancode key)
{
    return swos.playMatchTeam1Ptr && swos.playMatchTeam1Ptr->teamControls != kComputerTeam && m_pl1Controls == kKeyboard1 && keyboard1HasScancode(key) ||
        swos.playMatchTeam2Ptr && swos.playMatchTeam2Ptr->teamControls != kComputerTeam && m_pl2Controls == kKeyboard2 && keyboard2HasScancode(key);
}
