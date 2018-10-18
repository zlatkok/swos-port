#include "controls.h"
#include "render.h"
#include "swossym.h"

SDL_JoystickID m_joypad1Id = -1;
SDL_JoystickID m_joypad2Id = -1;

SDL_Joystick *m_joypad1;
SDL_Joystick *m_joypad2;

int m_joy1XValue;
int m_joy1YValue;
bool m_joy1Fire1;
bool m_joy1Fire2;

int m_joy2XValue;
int m_joy2YValue;
bool m_joy2Fire1;
bool m_joy2Fire2;

struct JoypadDeadZone {
    int xPos;
    int xNeg;
    int yPos;
    int yNeg;
};

static JoypadDeadZone m_joypad1DeadZone;
static JoypadDeadZone m_joypad2DeadZone;

static SDL_JoystickID openJoypad(int index, SDL_Joystick *& joystick)
{
    SDL_JoystickID id;
    joystick = SDL_JoystickOpen(index);

    if (joystick) {
        id = SDL_JoystickInstanceID(joystick);
        logInfo("Opened joystick %d successfully, name: %s, %d buttons, id = %x", index,
            SDL_JoystickNameForIndex(index), SDL_JoystickNumButtons(joystick), id);
    } else {
        logWarn("Failed to open joystick %d", index);
        id = -1;
    }

    return id;
}

static void closeJoypad(SDL_Joystick *& joystick, SDL_JoystickID& id)
{
    SDL_JoystickClose(joystick);
    joystick = nullptr;
    id = -1;
}

void initJoypads()
{
    // we should receive add joypad message, so let it be handled there
    updateControls();
}

void finishJoypads()
{
    SDL_JoystickClose(m_joypad1);
    SDL_JoystickClose(m_joypad2);
}

void normalizeInput()
{
    auto numJoypads = SDL_NumJoysticks();
    if (!numJoypads && g_inputControls > 1) {
        logInfo("No joypads detected, restoring controls to keyboard only");
        g_inputControls = 1;
    } else if (numJoypads == 1 && g_inputControls == 5) {
        logInfo("Second joypad not detected, restoring controls to keyboard+joypad");
        g_inputControls = 3;
    }
}

void setJoypadButton(SDL_JoystickID id, int button, bool pressed)
{
    if (id == m_joypad1Id) {
        if (button == 0)
            m_joy1Fire1 = pressed;
        else if (button == 1)
            m_joy1Fire2 = pressed;
    } else if (id == m_joypad2Id) {
        if (button == 0)
            m_joy2Fire1 = pressed;
        else if (button == 1)
            m_joy2Fire2 = pressed;
    }
}

static void checkQuitEvent()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT)
            std::exit(0);
}

static uint8_t sdlScanCodeToPc(SDL_Scancode sdlScanCode)
{
    switch (sdlScanCode) {
    case SDL_SCANCODE_A: return 30;
    case SDL_SCANCODE_B: return 48;
    case SDL_SCANCODE_C: return 46;
    case SDL_SCANCODE_D: return 32;
    case SDL_SCANCODE_E: return 18;
    case SDL_SCANCODE_F: return 33;
    case SDL_SCANCODE_G: return 34;
    case SDL_SCANCODE_H: return 35;
    case SDL_SCANCODE_I: return 23;
    case SDL_SCANCODE_J: return 36;
    case SDL_SCANCODE_K: return 37;
    case SDL_SCANCODE_L: return 38;
    case SDL_SCANCODE_M: return 50;
    case SDL_SCANCODE_N: return 49;
    case SDL_SCANCODE_O: return 24;
    case SDL_SCANCODE_P: return 25;
    case SDL_SCANCODE_Q: return 16;
    case SDL_SCANCODE_R: return 19;
    case SDL_SCANCODE_S: return 31;
    case SDL_SCANCODE_T: return 20;
    case SDL_SCANCODE_U: return 22;
    case SDL_SCANCODE_V: return 47;
    case SDL_SCANCODE_W: return 17;
    case SDL_SCANCODE_X: return 45;
    case SDL_SCANCODE_Y: return 21;
    case SDL_SCANCODE_Z: return 44;

    case SDL_SCANCODE_1: return 2;
    case SDL_SCANCODE_2: return 3;
    case SDL_SCANCODE_3: return 4;
    case SDL_SCANCODE_4: return 5;
    case SDL_SCANCODE_5: return 6;
    case SDL_SCANCODE_6: return 7;
    case SDL_SCANCODE_7: return 8;
    case SDL_SCANCODE_8: return 9;
    case SDL_SCANCODE_9: return 10;
    case SDL_SCANCODE_0: return 11;

    case SDL_SCANCODE_ESCAPE: return 1;
    case SDL_SCANCODE_MINUS: return 12;
    case SDL_SCANCODE_EQUALS: return 13;
    case SDL_SCANCODE_BACKSPACE: return 14;
    case SDL_SCANCODE_TAB: return 15;
    case SDL_SCANCODE_LEFTBRACKET: return 26;
    case SDL_SCANCODE_RIGHTBRACKET: return 27;
    case SDL_SCANCODE_KP_ENTER:
    case SDL_SCANCODE_RETURN: return 28;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL: return 29;
    case SDL_SCANCODE_SEMICOLON: return 39;
    case SDL_SCANCODE_APOSTROPHE: return 40;
    case SDL_SCANCODE_GRAVE: return 41;
    case SDL_SCANCODE_LSHIFT: return 42;
    case SDL_SCANCODE_BACKSLASH: return 43;
    case SDL_SCANCODE_COMMA: return 51;
    case SDL_SCANCODE_PERIOD: return 52;
    case SDL_SCANCODE_SLASH: return 53;
    case SDL_SCANCODE_RSHIFT: return 54;
    case SDL_SCANCODE_PRINTSCREEN: return 55;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT: return 56;
    case SDL_SCANCODE_SPACE: return 57;
    case SDL_SCANCODE_CAPSLOCK: return 58;

    case SDL_SCANCODE_F1: return 59;
    case SDL_SCANCODE_F2: return 60;
    case SDL_SCANCODE_F3: return 61;
    case SDL_SCANCODE_F4: return 62;
    case SDL_SCANCODE_F5: return 63;
    case SDL_SCANCODE_F6: return 64;
    case SDL_SCANCODE_F7: return 65;
    case SDL_SCANCODE_F8: return 66;
    case SDL_SCANCODE_F9: return 67;
    case SDL_SCANCODE_F10: return 68;

    case SDL_SCANCODE_NUMLOCKCLEAR: return 69;
    case SDL_SCANCODE_SCROLLLOCK: return 70;
    case SDL_SCANCODE_HOME: return 71;
    case SDL_SCANCODE_UP: return 72;
    case SDL_SCANCODE_PAGEUP: return 73;
    case SDL_SCANCODE_KP_MINUS: return 74;
    case SDL_SCANCODE_LEFT: return 75;
    case SDL_SCANCODE_KP_5: return 76;
    case SDL_SCANCODE_RIGHT: return 77;
    case SDL_SCANCODE_KP_PLUS: return 78;
    case SDL_SCANCODE_END: return 79;
    case SDL_SCANCODE_DOWN: return 80;
    case SDL_SCANCODE_PAGEDOWN: return 81;
    case SDL_SCANCODE_INSERT: return 82;
    case SDL_SCANCODE_DELETE: return 83;

    default: return 255;
    }
}

bool anyInputActive()
{
    SDL_PumpEvents();

    int numKeys;
    auto keyState = SDL_GetKeyboardState(&numKeys);

    if (std::find(keyState, keyState + numKeys, true) != keyState + numKeys)
        return true;

    if (m_joypad1 && (SDL_JoystickGetButton(m_joypad1, 0) || SDL_JoystickGetButton(m_joypad1, 1)))
        return true;

    if (m_joypad2 && (SDL_JoystickGetButton(m_joypad2, 0) || SDL_JoystickGetButton(m_joypad2, 1)))
        return true;

    if (SDL_GetMouseState(nullptr, nullptr))
        return true;

    checkQuitEvent();

    return false;
}

static void registerKey(uint8_t pcScanCode, bool pressed)
{
    if (!pressed)
        pcScanCode |= 0x80;

    g_prevScanCode = g_scanCode;
    g_scanCode = pcScanCode;

    if (pressed && keyCount < 9)
        keyBuffer[keyCount++] = pcScanCode;

    SetControlWord();
}

// Update the same variables that interrupt 9 handler would.
static void registerKey(SDL_Scancode sdlScanCode, bool pressed)
{
    auto scanCode = sdlScanCodeToPc(sdlScanCode);
    if (scanCode == 255)
        return;

    registerKey(scanCode, pressed);
}

void pressFire()
{
    if (keyCount < 9 && (!keyCount || (keyBuffer[0] & ~0x80) != g_fireScanCode))
        registerKey(g_fireScanCode, true);
}

void releaseFire()
{
    registerKey(g_fireScanCode, false);
}

void updateControls()
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            std::exit(0);

        // keys
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            registerKey(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
            break;

        // joypad
        case SDL_JOYAXISMOTION:
            if (event.jaxis.which == m_joypad1Id) {
                if (event.jaxis.axis == 0)
                    m_joy1XValue = event.jaxis.value;
                else if (event.jaxis.axis > 0)
                    m_joy1YValue = event.jaxis.value;
            } else if (event.jaxis.which == m_joypad2Id) {
                if (event.jaxis.axis == 0)
                    m_joy2XValue = event.jaxis.value;
                else if (event.jaxis.axis > 0)
                    m_joy2YValue = event.jaxis.value;
            }
            break;

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            setJoypadButton(event.jbutton.which, event.jbutton.button, event.type == SDL_JOYBUTTONDOWN);
            break;

        case SDL_JOYDEVICEADDED:
            logInfo("Joypad added");

            if (!m_joypad1) {
                m_joypad1Id = openJoypad(event.jdevice.which, m_joypad1);
                if (m_joypad1Id >= 0)
                    g_inputControls = 3;
            } else if (!m_joypad2) {
                m_joypad2Id = openJoypad(event.jdevice.which, m_joypad2);
                if (m_joypad2Id >= 0)
                    g_inputControls = 5;
            }
            break;

        case SDL_JOYDEVICEREMOVED:
            logInfo("Joypad removed");

            if (event.jdevice.which == m_joypad1Id)
                closeJoypad(m_joypad1, m_joypad1Id);
            else if (event.jdevice.which == m_joypad2Id)
                closeJoypad(m_joypad2, m_joypad2Id);

            if (g_inputControls == 5)
                g_inputControls = 3;
            else if (g_inputControls >= 2 && g_inputControls <= 4)
                g_inputControls = 1;
            break;
        }
    }

    if (paused)
        SDL_Delay(15);
}

std::tuple<int, int, int, int> joypadDeadZone(const JoypadDeadZone& deadZone)
{
    return { deadZone.xPos, deadZone.xNeg, deadZone.yPos, deadZone.yNeg };
}

std::tuple<int, int, int, int> joypad1DeadZone()
{
    return joypadDeadZone(m_joypad1DeadZone);
}

std::tuple<int, int, int, int> joypad2DeadZone()
{
    return joypadDeadZone(m_joypad2DeadZone);
}

void setJoypadDeadZone(int xPos, int xNeg, int yPos, int yNeg, JoypadDeadZone& deadZone)
{
    deadZone.xPos = xPos;
    deadZone.xNeg = xNeg;
    deadZone.yPos = yPos;
    deadZone.yNeg = yNeg;
}

void setJoypad1DeadZone(int xPos, int xNeg, int yPos, int yNeg)
{
    setJoypadDeadZone(xPos, xNeg, yPos, yNeg, m_joypad1DeadZone);
}

void setJoypad2DeadZone(int xPos, int xNeg, int yPos, int yNeg)
{
    setJoypadDeadZone(xPos, xNeg, yPos, yNeg, m_joypad2DeadZone);
}

// outputs:
//   ASCII code in convertedKey
//   scan code in lastKey
//   0 if nothing pressed
//
void SWOS::GetKey()
{
    updateControls();

    convertedKey = 0;
    lastKey = 0;

    if (keyCount) {
        lastKey = keyBuffer[0];
        memmove(keyBuffer, keyBuffer + 1, sizeof(keyBuffer) - 1);
        keyCount--;
        convertedKey = convertKeysTable[lastKey];

        // preserve alt-F1, ultra fast exit from SWOS (actually meant for invoking the debugger ;))
        if (g_prevScanCode == 0x38 && lastKey == 0x3b)
            std::exit(0);

        if (g_prevScanCode == 0x38 && lastKey == 0x1c)
            toggleBorderlessMaximizedMode();
    }
}


__declspec(naked) void SWOS::JoyKeyOrCtrlPressed()
{
    __asm {
        push ecx
        call anyInputActive
        test eax, eax
        pop  ecx
        retn
    }
}

static void joySetStatus(word& status, int xValue, int yValue, bool fire1, bool fire2, const JoypadDeadZone& deadZone)
{
    status = 0;

    if (xValue < deadZone.xNeg)
        status |= 1 << 2;
    else if (xValue > deadZone.xPos)
        status |= 1 << 3;

    if (yValue < deadZone.yNeg)
        status |= 1 << 0;
    else if (yValue > deadZone.yPos)
        status |= 1 << 1;

    if (fire1)
        status |= 1 << 5;

    if (fire2)
        status |= 1 << 6;
}

void SWOS::Joy1SetStatus()
{
    joySetStatus(g_joy1Status, m_joy1XValue, m_joy1YValue, m_joy1Fire1, m_joy1Fire2, m_joypad1DeadZone);
}

void SWOS::Joy2SetStatus()
{
    joySetStatus(g_joy2Status, m_joy2XValue, m_joy2YValue, m_joy2Fire1, m_joy2Fire2, m_joypad2DeadZone);
}
