#include "controls.h"
#include "joypads.h"
#include "render.h"
#include "swossym.h"
#include "options.h"
#include "game.h"
#include "util.h"

enum InputEventBits {
    kUp = 1,
    kDown = 2,
    kLeft = 4,
    kRight = 8,
    kSpecialFire = 16,      // mouse long kicks
    kFire = 32,
    kSecondaryFire = 64,
    kReleased = 128,
};

static Controls m_pl1Controls = kKeyboard1;
static Controls m_pl2Controls = kNone;
static Controls m_pl1GameControls = kNone;
static Controls m_pl2GameControls = kNone;

// arrow up, arrow down, arrow left, arrow right, control, right shift
constexpr ScanCodes kPl1DefaultScanCodes{ 0x48, 0x50, 0x4b, 0x4d, 0x1d, 0x36 };

// W, A, S, D, left shift, `
constexpr ScanCodes kPl2DefaultScanCodes{ 0x11, 0x1f, 0x1e, 0x20, 0x2a, 0x29 };

static ScanCodes m_pl1ScanCodes = kPl1DefaultScanCodes;
static ScanCodes m_pl2ScanCodes = kPl2DefaultScanCodes;

static word m_pl2ControlWord;

static word m_mouseControlWord;
static int m_mouseWheelAmount;

static word m_joy1Status;
static word m_joy2Status;

struct JoypadState {
    SDL_JoystickID id = kInvalidJoypadId;
    std::vector<bool> buttons;
};

std::vector<JoypadState> m_joypadState;

// options
static constexpr char kControlsSection[] = "controls";

const char kPlayer1ControlsKey[] = "player1Controls";
const char kPlayer2ControlsKey[] = "player2Controls";

static const char *kPl1ScanCodeKeys[] = {
    "pl1UpScanCode",
    "pl1DownScanCode",
    "pl1LeftScanCode",
    "pl1RightScanCode",
    "pl1FireScanCode",
    "pl1BenchScanCode",
};

static const char *kPl2ScanCodeKeys[] = {
    "pl2UpScanCode",
    "pl2DownScanCode",
    "pl2LeftScanCode",
    "pl2RightScanCode",
    "pl2FireScanCode",
    "pl2BenchScanCode",
};

#pragma pack(push, 1)
struct ControlState {
    byte lastFired;
    byte left;
    byte right;
    byte up;
    byte down;
    byte fire;
    byte secondaryFire;
    byte upRight;
    byte upLeft;
    byte downRight;
    byte downLeft;
    dword unused;
    byte shortFire;
    int16_t shortFireCounter;
    int16_t finalStatus;

    void reset() {
        left = 0;
        right = 0;
        up = 0;
        down = 0;
        fire = 0;
        secondaryFire = 0;
        finalStatus = -1;
    }

    void clear() {
        memset(this, 0, sizeof(*this));
        finalStatus = -1;
    }

    ControlState& operator+=(const ControlState& other) {
        lastFired |= other.lastFired;
        left |= other.left;
        right |= other.right;
        up |= other.up;
        down |= other.down;
        fire |= other.fire;
        secondaryFire |= other.secondaryFire;
        shortFire |= other.shortFire;
        upRight |= other.upRight;
        upLeft |= other.upLeft;
        downRight |= other.downRight;
        downLeft |= other.downLeft;
        if (shortFireCounter >= 0 && other.shortFireCounter >= 0)
            shortFireCounter = std::max(shortFireCounter, other.shortFireCounter);
        else
            shortFireCounter = std::min(shortFireCounter, other.shortFireCounter);

        // let's prioritize other
        if (other.finalStatus != -1)
            finalStatus = other.finalStatus;
        return *this;
    }
};
#pragma pack(pop)

static ControlState& getControlState(PlayerNumber player)
{
    return reinterpret_cast<ControlState&>(player == kPlayer1 ? swos.pl1LastFired : swos.pl2LastFired);
}

static void resetFireVariables()
{
    swos.controlWord = m_pl2ControlWord = 0;
    swos.shortFire = 0;
    swos.pl1ShortFire = 0;
    swos.pl2ShortFire = 0;
}

static const char *controlsToString(Controls controls)
{
    switch (controls) {
    case kNone: return "<disabled>";
    case kKeyboard1: return "keyboard 1";
    case kKeyboard2: return "keyboard 2";
    case kMouse: return "mouse";
    case kJoypad: return "game controller";
    default:
        assert(false);
        return "<unknown>";
    }
}

Controls getPl1Controls()
{
    return m_pl1Controls;
}

Controls getPl2Controls()
{
    return m_pl2Controls;
}

Controls getGameControls(int playerNo)
{
    assert(playerNo == 1 || playerNo == 2);

    return playerNo == 1 ? m_pl1GameControls : m_pl2GameControls;
}

void setPl1Controls(Controls controls, int joypadIndex /* = -1 */)
{
    assert(controls != kJoypad || joypadIndex >= 0 && joypadIndex < getNumJoypads());
    assert(controls != kKeyboard2);

    if (controls != m_pl1Controls || controls == kJoypad && joypadIndex != getPl1JoypadIndex()) {
        logInfo("Setting player 1 controls to %s (joypad index = %d)", controlsToString(controls), joypadIndex);

        if (m_pl1Controls == kKeyboard1 || m_pl2Controls != kKeyboard2)
            clearKeyInput();

        m_pl1Controls = controls;
        setPl1JoypadIndex(joypadIndex);
        if (controls == kJoypad && joypadIndex >= 0)
            logInfo("Joypad name: %s", SDL_JoystickName(getPl1Joypad().handle));

        resetFireVariables();

        getControlState(kPlayer1).clear();
    }
}

void setPl2Controls(Controls controls, int joypadIndex /* = -1 */)
{
    assert(controls != kJoypad || joypadIndex >= 0 && joypadIndex < getNumJoypads());
    assert(controls != kKeyboard1);

    if (controls != m_pl2Controls || controls == kJoypad && joypadIndex != getPl2JoypadIndex()) {
        logInfo("Setting player 2 controls to %s (joypad index = %d)", controlsToString(controls), joypadIndex);
        if (controls == kJoypad)

        if (m_pl2Controls == kKeyboard2)
            clearKeyInput();

        m_pl2Controls = controls;
        setPl2JoypadIndex(joypadIndex);
        if (controls == kJoypad && joypadIndex >= 0)
            logInfo("Joypad name: %s", SDL_JoystickName(getPl2Joypad().handle));

        resetFireVariables();

        auto& state = getControlState(kPlayer2);
        state.clear();
        std::swap(state.finalStatus, state.shortFireCounter);
    }
}

void setPl1GameControls(Controls controls)
{
    m_pl1GameControls = controls;
}

void setPl2GameControls(Controls controls)
{
    m_pl2GameControls = controls;
}

void disableGameControls(int playerNo)
{
    assert(playerNo == 1 || playerNo == 2);

    if (playerNo == 1)
        m_pl1GameControls = kNone;
    else
        m_pl2GameControls = kNone;
}

ScanCodes& getPl1ScanCodes()
{
    return m_pl1ScanCodes;
}

ScanCodes& getPl2ScanCodes()
{
    return m_pl2ScanCodes;
}

bool pl1KeyboardNullScancode()
{
    return m_pl1ScanCodes.hasNullScancode();
}

bool pl2KeyboardNullScancode()
{
    return m_pl2Controls == kKeyboard2 && m_pl2ScanCodes.hasNullScancode();
}

void normalizeInput()
{
    normalizeJoypadConfig();

    auto numJoypads = SDL_NumJoysticks();

    if (!numJoypads && (m_pl1Controls == kJoypad || m_pl2Controls == kJoypad)) {
        if (m_pl1Controls == kJoypad) {
            logInfo("No joypads detected, restoring player 1 controls to keyboard");
            setPl1Controls(kKeyboard1);
        } else {
            logInfo("No joypads detected, disabling player 2 controls");
            setPl2Controls(kNone);
        }
    } else if (numJoypads == 1 && m_pl1Controls == kJoypad && m_pl2Controls == kJoypad) {
        logInfo("Second joypad not detected, setting player 2 controls to keyboard");
        m_pl2Controls = kKeyboard2;
    }

    if (m_pl1Controls == kMouse && m_pl2Controls == kMouse || m_pl1Controls == kNone)
        setPl1Controls(kKeyboard1);

    if (m_pl1Controls == kKeyboard2)
        m_pl1Controls = kKeyboard1;

    if (m_pl2Controls == kKeyboard1)
        m_pl2Controls = kKeyboard2;

    if (pl1KeyboardNullScancode()) {
        logInfo("Null scan code detected in player 1 keys, resetting to default");
        m_pl1ScanCodes = kPl1DefaultScanCodes;
    }

    if (pl2KeyboardNullScancode()) {
        logInfo("Null scan code detected in player 2 keys, resetting to default");
        m_pl2ScanCodes = kPl2DefaultScanCodes;
    }

    // check for possible key conflicts
    if (m_pl2Controls == kKeyboard2 && m_pl1Controls == kKeyboard1 && m_pl1ScanCodes.conflicts(m_pl2ScanCodes)) {
        logInfo("Keyboard controls conflict detected, resetting to default");
        m_pl1ScanCodes = kPl1DefaultScanCodes;
        m_pl2ScanCodes = kPl2DefaultScanCodes;
    }

    m_pl1ScanCodes.log("Player 1 keys: ");
    if (m_pl2Controls == kKeyboard2)
        m_pl2ScanCodes.log("Player 2 keys: ");
}

static void checkQuitEvent()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT)
            std::exit(EXIT_SUCCESS);
}

bool anyInputActive()
{
    SDL_PumpEvents();

    int numKeys;
    auto keyState = SDL_GetKeyboardState(&numKeys);

    if (std::find(keyState, keyState + numKeys, 1) != keyState + numKeys)
        return true;

    if (pl1UsingJoypad() && getPl1Joypad().handle &&
        (SDL_JoystickGetButton(getPl1Joypad().handle, getPl1Joypad().config.fireButtonIndex) ||
        SDL_JoystickGetButton(getPl1Joypad().handle, getPl1Joypad().config.benchButtonIndex)))
        return true;

    if (pl2UsingJoypad() && getPl2Joypad().handle &&
        (SDL_JoystickGetButton(getPl2Joypad().handle, getPl2Joypad().config.fireButtonIndex) ||
        SDL_JoystickGetButton(getPl2Joypad().handle, getPl2Joypad().config.benchButtonIndex)))
        return true;

    if (SDL_GetMouseState(nullptr, nullptr))
        return true;

    checkQuitEvent();

    return false;
}

static void setControlWord(word& controlWord, const ScanCodes& scanCodes)
{
    auto sc = swos.g_scanCode;
    auto cw = controlWord;

    if (!(sc & kReleased)) {
        // key pressed
        if (sc == scanCodes.fire)
            cw |= kFire;

        if (sc == scanCodes.bench)
            cw |= kSecondaryFire;

        if (sc == scanCodes.right) {
            cw |= kRight;
            cw &= ~kLeft;
        }
        if (sc == scanCodes.left) {
            cw |= kLeft;
            cw &= ~kRight;
        }
        if (sc == scanCodes.down) {
            cw |= kDown;
            cw &= ~kUp;
        }
        if (sc == scanCodes.up) {
            cw |= kUp;
            cw &= ~kDown;
        }
    } else {
        // key released
        sc &= ~kReleased;

        if (sc == scanCodes.fire)
            cw &= ~kFire;

        if (sc == scanCodes.bench)
            cw &= ~kSecondaryFire;

        if (sc == scanCodes.right)
            cw &= ~kRight;

        if (sc == scanCodes.left)
            cw &= ~kLeft;

        if (sc == scanCodes.down)
            cw &= ~kDown;

        if (sc == scanCodes.up)
            cw &= ~kUp;
    }

    controlWord = cw;
}

static void setControlWords()
{
    setControlWord(swos.controlWord, m_pl1ScanCodes);
    setControlWord(m_pl2ControlWord, m_pl2ScanCodes);
}

static void registerKey(uint8_t pcScanCode, bool pressed)
{
    if (!pressed)
        pcScanCode |= kReleased;

    swos.g_scanCode = pcScanCode;

    if (pressed && swos.keyCount < 9)
        swos.keyBuffer[swos.keyCount++] = pcScanCode;

    setControlWords();
}

// Update the same variables that interrupt 9 handler would.
static void registerKey(SDL_Scancode sdlScanCode, bool pressed)
{
    auto scanCode = sdlScanCodeToPc(sdlScanCode);
    if (scanCode == 255)
        return;

    registerKey(scanCode, pressed);
}

static byte getFireScanCode()
{
    return m_pl1Controls == kKeyboard1 || m_pl2Controls != kKeyboard2 ? m_pl1ScanCodes.fire : m_pl2ScanCodes.fire;
}

void pressFire()
{
    auto fireScanCode = getFireScanCode();
    if (swos.keyCount < sizeof(swos.keyBuffer) - 1 && (!swos.keyCount || (swos.keyBuffer[0] & ~kReleased) != fireScanCode))
        registerKey(fireScanCode, true);
}

void releaseFire()
{
    auto fireScanCode = getFireScanCode();
    registerKey(fireScanCode, false);
    resetFireVariables();
}

int mouseWheelAmount()
{
    return m_mouseWheelAmount;
}

void clearKeyInput()
{
    do {
        updateControls();
    } while (SDL_GetMouseState(nullptr, nullptr));

    do {
        updateControls();
        SWOS::GetKey();
    } while (swos.lastKey);
}

void clearPlayer1State()
{
    auto& state = getControlState(kPlayer1);
    state.clear();
}

static void setMouseControlWordButton(Uint8 button, Uint8 state)
{
    if (state == SDL_PRESSED) {
        switch (button) {
        case SDL_BUTTON_LEFT:
            if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK)
                m_mouseControlWord |= kSecondaryFire;
            else
                m_mouseControlWord |= kFire;
            break;

        case SDL_BUTTON_RIGHT:
            if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK)
                m_mouseControlWord |= kSecondaryFire;
            else
                m_mouseControlWord |= kSpecialFire;
            break;

        case SDL_BUTTON_MIDDLE:
            m_mouseControlWord = 0;
            break;
        }
    } else {
        assert(state == SDL_RELEASED);

        switch (button) {
        case SDL_BUTTON_LEFT:
            m_mouseControlWord &= ~kFire;
            break;

        case SDL_BUTTON_RIGHT:
            m_mouseControlWord &= ~kSpecialFire;
            break;
        }
    }
}

static void setMouseControlWord(Sint32 xrel, Sint32 yrel, Uint32 state)
{
    m_mouseControlWord = 0;

    if (state & SDL_BUTTON_MMASK)
        return;

    if (xrel < 0) {
        m_mouseControlWord |= kLeft;
        m_mouseControlWord &= ~kRight;
    }

    if (xrel > 0) {
        m_mouseControlWord |= kRight;
        m_mouseControlWord &= ~kLeft;
    }

    if (yrel < 0) {
        m_mouseControlWord |= kUp;
        m_mouseControlWord &= ~kDown;
    }

    if (yrel > 0) {
        m_mouseControlWord |= kDown;
        m_mouseControlWord &= ~kUp;
    }

    if (state & SDL_BUTTON_LMASK)
        m_mouseControlWord |= kFire;

    if (state & SDL_BUTTON_RMASK)
        m_mouseControlWord |= kSpecialFire;

    if ((state & SDL_BUTTON_LMASK) && (state & SDL_BUTTON_RMASK)) {
        m_mouseControlWord |= kSecondaryFire;
        m_mouseControlWord &= ~kFire;
        m_mouseControlWord &= ~kSpecialFire;
    }
}

static void processEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_QUIT:
        std::exit(EXIT_SUCCESS);

    case SDL_KEYDOWN:
    case SDL_KEYUP:
        checkKeyboardShortcuts(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
        registerKey(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
        break;

    case SDL_JOYAXISMOTION:
        updateJoypadMotion(event.jaxis.which, event.jaxis.axis, event.jaxis.value);
        break;

    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        updateJoypadButtonState(event.jbutton.which, event.jbutton.button, event.type == SDL_JOYBUTTONDOWN);
        break;

    case SDL_JOYDEVICEADDED:
        logInfo("Joypad %d added", event.jdevice.which);
        addJoypad(event.jdevice.which);
        break;

    case SDL_JOYDEVICEREMOVED:
        logInfo("Joypad %d removed", event.jdevice.which);
        removeJoypad(event.jdevice.which);
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        setMouseControlWordButton(event.button.button, event.button.state);
        break;

    case SDL_MOUSEMOTION:
        setMouseControlWord(event.motion.xrel, event.motion.yrel, event.motion.state);
        break;

    case SDL_MOUSEWHEEL:
        if (event.wheel.y)
            m_mouseWheelAmount = event.wheel.y;
        break;
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

void updateControls()
{
    m_mouseWheelAmount = 0;

    SDL_Event event;

    while (SDL_PollEvent(&event))
        processEvent(event);

    if (swos.paused)
        SDL_Delay(15);
}

void loadControlOptions(const CSimpleIni& ini)
{
    loadJoypadOptions(kControlsSection, ini);

    m_pl1Controls = kKeyboard1;
    auto pl1Controls = ini.GetLongValue(kControlsSection, kPlayer1ControlsKey);
    if (pl1Controls >= 0 && pl1Controls < kNumControls)
        m_pl1Controls = static_cast<Controls>(pl1Controls);

    m_pl2Controls = kNone;
    auto pl2Controls = ini.GetLongValue(kControlsSection, kPlayer2ControlsKey);
    if (pl2Controls >= 0 && pl2Controls < kNumControls)
        m_pl2Controls = static_cast<Controls>(pl2Controls);

    logInfo("Controls set to: %s (player 1), %s (player 2)", controlsToString(m_pl1Controls), controlsToString(m_pl2Controls));

    m_pl1ScanCodes.load(ini, kControlsSection, kPl1ScanCodeKeys, kPl1DefaultScanCodes);
    m_pl2ScanCodes.load(ini, kControlsSection, kPl2ScanCodeKeys, kPl2DefaultScanCodes);
}

void saveControlOptions(CSimpleIni& ini)
{
    saveJoypadOptions(kControlsSection, ini);

    const char kInputControlsComment[] = "; 0 = none, 1 = keyboard, 2 = mouse, 3 = joypad/game controller";

    ini.SetLongValue(kControlsSection, kPlayer1ControlsKey, m_pl1Controls, kInputControlsComment);
    ini.SetLongValue(kControlsSection, kPlayer2ControlsKey, m_pl2Controls);

    m_pl1ScanCodes.save(ini, kControlsSection, kPl1ScanCodeKeys);
    m_pl2ScanCodes.save(ini, kControlsSection, kPl2ScanCodeKeys);
}

void initGameControls()
{
    m_joy1Status = 0;
    m_joy2Status = 0;
    m_mouseControlWord = 0;

    memset(&swos.pl1LastFired, 0, reinterpret_cast<byte *>(&swos.pl2ShortFireCounter) - &swos.pl1LastFired + sizeof(swos.pl2ShortFireCounter));
}

void finishGameControls()
{
    resetMatchControls();
}

static std::vector<int> m_matchControlsFiring;

void updateMatchControls()
{
    updateControls();

    m_matchControlsFiring.clear();

    if (swos.controlWord & kFire)
        m_matchControlsFiring.push_back(kKeyboard1);

    if (m_pl2ControlWord & kFire)
        m_matchControlsFiring.push_back(kKeyboard2);

    if (SDL_GetMouseState(nullptr, nullptr))
        m_matchControlsFiring.push_back(kMouse);

    for (int i = 0; i < getNumJoypads(); i++)
        if (getJoypad(i).anyButtonDown())
            m_matchControlsFiring.push_back(kNumControls + i);
}

static int getControlsIndex(Controls controls, int joypadIndex = -1)
{
    return controls == kJoypad ? joypadIndex + kNumControls : controls;
}

static void setControlsSelected(Controls controls, int joypadIndex = -1)
{
    int index = getControlsIndex(controls, joypadIndex);
    m_matchControlsFiring.push_back(index);
}

static bool wereControlsFiring(Controls controls, int joypadIndex = -1)
{
    int index = getControlsIndex(controls, joypadIndex);
    return std::find(m_matchControlsFiring.begin(), m_matchControlsFiring.end(), index) != m_matchControlsFiring.end();
}

// assign player controls based on the controllers that are currently firing (disregard presses from previous frame)
int matchControlsSelected()
{
    assert(m_pl1GameControls == kNone || m_pl2GameControls == kNone);

    auto otherControls = kNone;
    auto otherJoypadIndex = -1;

    static const auto kControlInfo = {
        std::make_pair(&m_pl1GameControls, 1),
        std::make_pair(&m_pl2GameControls, 2),
    };

    for (const auto& controlInfo : kControlInfo) {
        auto controls = controlInfo.first;
        auto playerNo = controlInfo.second;

        if (*controls == kNone) {
            if (otherControls != kKeyboard1 && (swos.controlWord & kFire) && !wereControlsFiring(kKeyboard1)) {
                *controls = kKeyboard1;
            } else if (otherControls != kKeyboard2 && (m_pl2ControlWord & kFire) && !wereControlsFiring(kKeyboard2)) {
                *controls = kKeyboard2;
            } else if (otherControls != kMouse && SDL_GetMouseState(nullptr, nullptr) && !wereControlsFiring(kMouse)) {
                *controls = kMouse;
            } else {
                for (int i = 0; i < getNumJoypads(); i++) {
                    if (otherJoypadIndex != i && getJoypad(i).anyButtonDown() && !wereControlsFiring(kJoypad, i)) {
                        *controls = kJoypad;
                        playerNo == 1 ? setPl1GameJoypadIndex(i) : setPl2GameJoypadIndex(i);
                        logInfo("Player %d playing with joypad %d", playerNo, i);
                        return playerNo;
                    }
                }

                return -1;
            }

            logInfo("Player %d using controls %s", playerNo, controlsToString(*controls));
            return playerNo;
        }

        otherControls = *controls;
        otherJoypadIndex = playerNo == 1 ? getPl1GameJoypadIndex() : getPl2GameJoypadIndex();
    }

    return -1;
}

void resetMatchControls()
{
    m_pl1GameControls = kNone;
    m_pl2GameControls = kNone;
    resetMatchJoypads();
}

bool gotMousePlayer()
{
    return m_pl1GameControls == kMouse || m_pl2GameControls == kMouse;
}

// Returns true if last pressed key belongs to selected keys of any currently active player.
bool testForPlayerKeys()
{
    byte key = static_cast<byte>(swos.lastKey);

    return m_pl1GameControls == kKeyboard1 && m_pl1ScanCodes.has(key) ||
        m_pl2GameControls == kKeyboard2 && m_pl2ScanCodes.has(key);
}

// prevent locking out of first player controls inside play match menu, if it wasn't the player that fired
void SWOS::PreventPlayer1FireLockout()
{
    if (m_pl1Controls != m_pl1GameControls || m_pl1Controls == kJoypad && getPl1JoypadIndex() != getPl1GameJoypadIndex())
        D0 = 3;
}

// outputs:
//   ASCII code in convertedKey
//   scan code in lastKey
//   0 if nothing pressed
//
void SWOS::GetKey()
{
    updateControls();

    swos.convertedKey = 0;
    swos.lastKey = 0;

    if (swos.keyCount) {
        swos.lastKey = swos.keyBuffer[0];
        memmove(swos.keyBuffer, swos.keyBuffer + 1, sizeof(swos.keyBuffer) - 1);
        swos.keyCount--;
        swos.convertedKey = swos.convertKeysTable[swos.lastKey];
    }
}

__declspec(naked) void SWOS::JoyKeyOrCtrlPressed()
{
#ifdef SWOS_VM
    auto result = anyInputActive();
    SwosVM::flags.zero = result;
#else
    __asm {
        push ecx
        call anyInputActive
        test eax, eax
        pop  ecx
        retn
    }
#endif
}

static void joySetStatus(word& status, const JoypadValues& values)
{
    status = 0;

    if (values.left())
        status |= 1 << 2;
    else if (values.right())
        status |= 1 << 3;

    if (values.up())
        status |= 1 << 0;
    else if (values.down())
        status |= 1 << 1;

    if (values.fire)
        status |= 1 << 5;

    if (values.bench)
        status |= 1 << 6;
}

static void setControlState(ControlState& state, word controlWord)
{
    state.left = controlWord & kLeft ? -1 : 0;
    state.right = controlWord & kRight ? -1 : 0;
    state.up = controlWord & kUp ? -1 : 0;
    state.down = controlWord & kDown ? -1 : 0;
    state.fire = controlWord & kFire ? -1: 0;
    state.secondaryFire = controlWord & kSecondaryFire ? -1 : 0;

    state.upRight = state.up & state.right;
    state.downRight = state.down & state.right;
    state.downLeft = state.down & state.left;
    state.upLeft = state.up & state.left;

    if (state.lastFired) {
        state.shortFire = 0;
        if (state.fire) {
            if (state.shortFireCounter)
                state.shortFireCounter--;
        } else {
            state.lastFired = 0;
            state.shortFireCounter = -state.shortFireCounter;
        }
    } else {
        state.shortFire = 0;
        if (state.fire) {
            state.shortFire = 1;
            state.lastFired = 1;
            state.shortFireCounter = -1;
        }
    }

    if (controlWord & kSpecialFire) {
        state.lastFired = 1;
        state.shortFire = 0;
        state.shortFireCounter = 30;
    }

    if (state.upRight)
        state.finalStatus = 1;
    else if (state.downRight)
        state.finalStatus = 3;
    else if (state.downLeft)
        state.finalStatus = 5;
    else if (state.upLeft)
        state.finalStatus = 7;
    else if (state.up)
        state.finalStatus = 0;
    else if (state.right)
        state.finalStatus = 2;
    else if (state.down)
        state.finalStatus = 4;
    else if (state.left)
        state.finalStatus = 6;
    else
        state.finalStatus = -1;
}

static void gatherPlayerInput(PlayerNumber player)
{
    assert(m_pl1Controls != kKeyboard2 && m_pl2Controls != kKeyboard1);

    auto& state = getControlState(player);
    state.reset();

    auto inputSource = player == kPlayer1 ? m_pl1Controls : m_pl2Controls;
    if (isMatchRunning())
        inputSource = player == kPlayer1 ? m_pl1GameControls : m_pl2GameControls;

    switch (inputSource) {
    case kKeyboard1:
        setControlState(state, swos.controlWord);
        break;

    case kKeyboard2:
        setControlState(state, m_pl2ControlWord);
        break;

    case kJoypad:
        {
            auto& status = player == kPlayer1 ? m_joy1Status : m_joy2Status;
            auto& values = player == kPlayer1 ? getJoypad1Values(): getJoypad2Values();

            joySetStatus(status, values);
            setControlState(state, status);
        }
        break;

    case kMouse:
        if (isMatchRunning())
            setControlState(state, m_mouseControlWord);
        break;

    default:
        assert(false);  // assume fall-through

    case kNone:
        state.clear();
        break;
    }
}

void SWOS::Player1StatusProc()
{
    gatherPlayerInput(kPlayer1);
}

void SWOS::Player2StatusProc()
{
    auto& pl2State = getControlState(kPlayer2);

    // argh, SWOS listed these 2 variables for player 2 in reverse order
    std::swap(pl2State.shortFireCounter, pl2State.finalStatus);

    gatherPlayerInput(kPlayer2);

    // make sure menus are always controllable by the keyboard
    if (!isMatchRunning() && m_pl1Controls != kKeyboard1 && m_pl2Controls != kKeyboard2) {
        // combine keyboard and whatever controls are selected
        static ControlState keyboardState;
        setControlState(keyboardState, swos.controlWord);
        pl2State += keyboardState;
    }

    std::swap(pl2State.shortFireCounter, pl2State.finalStatus);
}
