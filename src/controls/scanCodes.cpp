#include "scanCodes.h"
#include "log.h"

bool ScanCodes::conflicts(const ScanCodes& other) const
{
    return up == other.up || down == other.down || left == other.left || fire == other.fire || bench == other.bench;
}

bool ScanCodes::has(byte scanCode) const
{
    return up == scanCode || down == scanCode || left == scanCode || right == scanCode || fire == scanCode || bench == scanCode;
}

bool ScanCodes::hasNullScancode() const
{
    return has(0);
}

void ScanCodes::fromArray(const byte *scanCodes)
{
    up = scanCodes[0];
    down = scanCodes[1];
    left = scanCodes[2];
    right = scanCodes[3];
    fire = scanCodes[4];
    bench = scanCodes[5];
}

void ScanCodes::load(const CSimpleIni& ini, const char *section, const char **keys, const ScanCodes& default)
{
    auto load = [&ini, section](const char *key, long default) {
        auto keyValue = ini.GetLongValue(section, key, default);

        if (keyValue > 255)
            logWarn("Out of range key value (%d) for %s", keyValue, key);

        return static_cast<byte>(keyValue);
    };

    up = load(keys[0], default.up);
    down = load(keys[1], default.down);
    left = load(keys[2], default.left);
    right = load(keys[3], default.right);
    fire = load(keys[4], default.fire);
    bench = load(keys[5], default.bench);
}

void ScanCodes::save(CSimpleIni& ini, const char *section, const char **keys) {
    auto save = [&ini, section](const char *key, long value) {
        ini.SetLongValue(section, key, value);
    };

    save(keys[0], up);
    save(keys[1], down);
    save(keys[2], left);
    save(keys[3], right);
    save(keys[4], fire);
    save(keys[5], bench);
}

void ScanCodes::log(const char *leading /* = "" */) {
    logInfo("%sup: %#x (%s), down: %#x (%s), left: %#x (%s), right: %#x (%s), fire: %#x (%s), bench: %#x (%s)",
        leading, up, pcScanCodeToString(up), down, pcScanCodeToString(down), left, pcScanCodeToString(left),
        right, pcScanCodeToString(right), fire, pcScanCodeToString(fire), bench, pcScanCodeToString(bench));
}

const char *pcScanCodeToString(uint8_t scanCode)
{
    static const uint8_t kOneCharTable[] = "1234567890-=\0\0QWERTYUIOP[]\0\0ASDFGHJKL;'`\0\\ZXCVBNM,./";
    static char keyBuf[2] = {};

    scanCode &= 0x7f;
    auto scanCodeIndex = scanCode - 2;

    if (scanCodeIndex < sizeof(kOneCharTable) - 1 && kOneCharTable[scanCodeIndex]) {
        keyBuf[0] = kOneCharTable[scanCodeIndex];
        return keyBuf;
    }

    static const uint8_t kKeyTable[] = {
    //   code length string
        "\x01" "\x7" "ESCAPE\0"
        "\x0f" "\x4" "TAB\0"
        "\x1c" "\x6" "ENTER\0"
        "\x39" "\x6" "SPACE\0"
        "\x48" "\x9" "UP ARROW\0"
        "\x4b" "\xb" "LEFT ARROW\0"
        "\x4d" "\xc" "RIGHT ARROW\0"
        "\x50" "\xb" "DOWN ARROW\0"
        "\x0e" "\xa" "BACKSPACE\0"
        "\x3a" "\xa" "CAPS LOCK\0"
        "\x3b" "\x3" "F1\0"
        "\x3c" "\x3" "F2\0"
        "\x3d" "\x3" "F3\0"
        "\x3e" "\x3" "F4\0"
        "\x3f" "\x3" "F5\0"
        "\x40" "\x3" "F6\0"
        "\x41" "\x3" "F7\0"
        "\x42" "\x3" "F8\0"
        "\x43" "\x3" "F9\0"
        "\x44" "\x4" "F10\0"
        "\x57" "\x4" "F11\0"
        "\x58" "\x4" "F12\0"
        "\x4e" "\xc" "KEYPAD PLUS\0"
        "\x4a" "\xd" "KEYPAD MINUS\0"
        "\x53" "\x4" "DEL\0"
        "\x1d" "\x8" "CONTROL\0"
        "\x2a" "\xb" "LEFT SHIFT\0"
        "\x36" "\xc" "RIGHT SHIFT\0"
        "\x38" "\x4" "ALT\0"
        "\x45" "\x9" "NUM LOCK\0"
        "\x37" "\x9" "KEYPAD *\0"
        "\x46" "\xc" "SCROLL LOCK\0"
        "\x4c" "\x9" "KEYPAD 5\0"
        "\x52" "\x7" "INSERT\0"
        "\x47" "\x5" "HOME\0"
        "\x4f" "\x4" "END\0"
        "\x49" "\x8" "PAGE UP\0"
        "\x51" "\xa" "PAGE DOWN\0"
        "\xff"
    };

    for (auto p = kKeyTable; *p != 0xff; p += p[1] + 2)
        if (*p == scanCode)
            return reinterpret_cast<const char *>(p) + 2;

    assert(false);
    return "UNKNOWN";
}

uint8_t sdlScanCodeToPc(SDL_Scancode sdlScanCode)
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
