#include "textInput.h"
#include "text.h"
#include "controls.h"
#include "keyBuffer.h"
#include "MenuEntry.h"
#include "drawMenu.h"
#include "menuAlloc.h"
#include "util.h"

enum ExtendedCodes {
    kAUmlautCode = 228,
    kOUmlautCode = 246,
    kUUmlautCode = 252,
    kEszettCode = 223,
    kSCaronCode = 353,
    kDCrossedCode = 273,
    kCCaronCode = 269,
    kCAcuteAccentCode = 263,
    kZCaronCode = 382,
};

// Converts pressed key to our internal character representation that can be rendered on screen.
static char keyToChar(SDL_Scancode key, SDL_Keycode extendedCode, bool allowShift)
{
    switch (extendedCode) {
    case kAUmlautCode: return kAUmlautChar;
    case kOUmlautCode: return kOUmlautChar;
    case kUUmlautCode: return kUUmlautChar;
    case kEszettCode: return kEszettChar;
    case kSCaronCode: return kSCaronChar;
    case kDCrossedCode: return kDCrossedChar;
    case kCCaronCode: return kCCaronChar;
    case kCAcuteAccentCode: return kCAcuteAccentChar;
    case kZCaronCode: return kZCaronChar;
    }

    bool shiftDown = false;
    if (allowShift)
        shiftDown = (SDL_GetModState() & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0;

    if (key >= SDL_SCANCODE_A && key <= SDL_SCANCODE_Z)
        return key - SDL_SCANCODE_A + (shiftDown ? 'A' : 'a');
    if (key >= SDL_SCANCODE_KP_A && key <= SDL_SCANCODE_KP_F)
        return key - SDL_SCANCODE_KP_A + (shiftDown ? 'A' : 'a');

    switch (key) {
    case SDL_SCANCODE_GRAVE:
        return shiftDown ? '~' : '`';
    case SDL_SCANCODE_0:
    case SDL_SCANCODE_KP_0:
        return shiftDown ? ')' : '0';
    case SDL_SCANCODE_1:
    case SDL_SCANCODE_KP_1:
        return shiftDown ? '!' : '1';
    case SDL_SCANCODE_2:
    case SDL_SCANCODE_KP_2:
        return shiftDown ? '@' : '2';
    case SDL_SCANCODE_3:
    case SDL_SCANCODE_KP_3:
        return shiftDown ? '#' : '3';
    case SDL_SCANCODE_4:
    case SDL_SCANCODE_KP_4:
        return shiftDown ? '$' : '4';
    case SDL_SCANCODE_5:
    case SDL_SCANCODE_KP_5:
        return shiftDown ? '%' : '5';
    case SDL_SCANCODE_6:
    case SDL_SCANCODE_KP_6:
        return shiftDown ? '^' : '6';
    case SDL_SCANCODE_7:
    case SDL_SCANCODE_KP_7:
        return shiftDown ? '&' : '7';
    case SDL_SCANCODE_8:
    case SDL_SCANCODE_KP_8:
        return shiftDown ? '*' : '8';
    case SDL_SCANCODE_9:
    case SDL_SCANCODE_KP_9:
        return shiftDown ? '(' : '9';
    case SDL_SCANCODE_KP_VERTICALBAR:
        return '|';
    case SDL_SCANCODE_LEFTBRACKET:
        return shiftDown ? '{' : '[';
    case SDL_SCANCODE_RIGHTBRACKET:
        return shiftDown ? '}' : ']';
    case SDL_SCANCODE_KP_LEFTBRACE:
        return '{';
    case SDL_SCANCODE_KP_RIGHTBRACE:
        return '}';
    case SDL_SCANCODE_BACKSLASH:
        return shiftDown ? '|' : '\\';
    case SDL_SCANCODE_KP_EXCLAM:
        return '!';
    case SDL_SCANCODE_APOSTROPHE:
        return shiftDown ? '"' : '\'';
    case SDL_SCANCODE_COMMA:
    case SDL_SCANCODE_KP_COMMA:
        return shiftDown ? '<' : ',';
    case SDL_SCANCODE_MINUS:
        return shiftDown ? '_' : '-';
    case SDL_SCANCODE_KP_MINUS:
        return '-';
    case SDL_SCANCODE_EQUALS:
        return shiftDown ? '+' : '=';
    case SDL_SCANCODE_KP_PLUS:
        return '+';
    case SDL_SCANCODE_PERIOD:
    case SDL_SCANCODE_KP_PERIOD:
        return shiftDown ? '>' : '.';
    case SDL_SCANCODE_SLASH:
    case SDL_SCANCODE_KP_DIVIDE:
        return shiftDown ? '?' : '/';
    case SDL_SCANCODE_KP_MULTIPLY:
        return '*';
    case SDL_SCANCODE_SEMICOLON:
        return shiftDown ? ':' : ';';
    case SDL_SCANCODE_SPACE:
    case SDL_SCANCODE_KP_SPACE:
        return ' ';
    case SDL_SCANCODE_KP_LEFTPAREN:
        return '(';
    case SDL_SCANCODE_KP_RIGHTPAREN:
        return ')';
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

static bool inputText(MenuEntry& entry, size_t maxLength, bool allowShift, std::function<bool(SDL_Scancode, SDL_Keycode, char *, size_t, char *)> processKey)
{
    assert(entry.isString());
    assert(maxLength <= kStdMenuTextSize);

    if (maxLength > kStdMenuTextSize)
        return false;

    char *buf = menuAlloc(maxLength);
    strncpy_s(buf, entry.string(), maxLength + 1);

    auto originalString = entry.string();
    entry.setString(buf);

    auto sentinel = buf + maxLength + 2;    // + 1 for the cursor
    auto end = buf + strlen(buf);

    *end++ = kCursorChar;
    *end = '\0';

    auto cursorPtr = end - 1;

    TextInputScope textInput;

    while (true) {
        processControlEvents();
        drawMenu();
        SWOS::WaitRetrace();

        auto key = getExtendedKey();

        while (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK) {
            key = { SDL_SCANCODE_ESCAPE, SDLK_UNKNOWN };
            processControlEvents();
            SDL_Delay(25);
        }

        switch (key.first) {
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_RETURN2:
        case SDL_SCANCODE_KP_ENTER:
            memmove(cursorPtr, cursorPtr + 1, end - cursorPtr);
            entry.setString(originalString);
            strcpy(entry.string(), buf);
            menuFree(maxLength);
            return true;

        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_AC_BACK:
            entry.setString(originalString);
            menuFree(maxLength);
            return false;

        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_LEFT:
            if (cursorPtr != buf) {
                std::swap(cursorPtr[-1], cursorPtr[0]);
                cursorPtr--;
            }
            break;

        case SDL_SCANCODE_RSHIFT:
        case SDL_SCANCODE_RIGHT:
            if (cursorPtr[1]) {
                std::swap(cursorPtr[0], cursorPtr[1]);
                cursorPtr++;
            }
            break;

        case SDL_SCANCODE_HOME:
        case SDL_SCANCODE_DOWN:
            memmove(buf + 1, buf, cursorPtr - buf);
            *(cursorPtr = buf) = kCursorChar;
            break;

        case SDL_SCANCODE_END:
        case SDL_SCANCODE_UP:
            if (cursorPtr[1]) {
                memmove(cursorPtr, cursorPtr + 1, end - cursorPtr);
                *(cursorPtr = end - 1) = kCursorChar;
            }
            break;

        case SDL_SCANCODE_BACKSPACE:
            if (cursorPtr != buf) {
                memmove(cursorPtr, cursorPtr + 1, end - cursorPtr);
                *--cursorPtr = kCursorChar;
                end--;
            }
            break;

        case SDL_SCANCODE_DELETE:
            if (cursorPtr[1]) {
                memmove(cursorPtr, cursorPtr + 1, end - cursorPtr);
                *cursorPtr = kCursorChar;
                end--;
            }
            break;

        default:
            if (end + 1 < sentinel - 1 && processKey(key.first, key.second, buf, end - buf - 1, cursorPtr)) {
                if (auto c = keyToChar(key.first, key.second, allowShift)) {
                    *cursorPtr++ = c;
                    memmove(cursorPtr + 1, cursorPtr, end++ - cursorPtr + 1);
                    *cursorPtr = kCursorChar;
                }
            }
            break;
        }
    }
}

bool inputText(MenuEntry& entry, size_t maxLength, InputTextAllowedChars allowExtraChars /* = kLimitedCharset */)
{
    assert(entry.isString());

    maxLength = std::min<size_t>(maxLength, kStdMenuTextSize);

    return inputText(entry, maxLength, allowExtraChars == kFullCharset, [allowExtraChars](auto key, auto keycode, auto, auto, auto) {
        static_assert(SDL_SCANCODE_Z + 1 == SDL_SCANCODE_1, "Cock-a-doodle-doo!");

        if (key >= SDL_SCANCODE_A && key <= SDL_SCANCODE_0)
            return true;

        switch (keycode) {
        case kAUmlautCode:
        case kOUmlautCode:
        case kUUmlautCode:
        case kSCaronCode:
        case kDCrossedCode:
        case kCCaronCode:
        case kCAcuteAccentCode:
        case kZCaronCode:
        case kEszettCode:
            return true;
        }

        if (allowExtraChars > kLimitedCharset && key == '.')
            return true;

        return allowExtraChars == kFullCharset;
    });
}

static int calculateCurrentValue(const char *start, int newDigit)
{
    int value = 0;

    for (; *start; start++) {
        if (*start != kCursorChar) {
            assert(*start >= '0' && *start <= '9');
            value = value * 10 + *start - '0';
        }
    }

    return value * 10 + newDigit;
}

bool inputNumber(MenuEntry& entry, int maxDigits, int minNum, int maxNum, bool allowNegative /* = false */)
{
    assert(entry.type == kEntryNumber && maxDigits <= 6);

    int num = static_cast<int16_t>(entry.fg.number);
    assert(num >= minNum && num <= maxNum);

    constexpr int kBufferSize = 32;
    auto buf = menuAlloc(kBufferSize);
    SDL_itoa(num, buf, 10);
    assert(static_cast<int>(strlen(buf)) <= maxDigits);

    entry.type = kEntryString;
    entry.setString(buf);

    // +1 for the cursor
    auto result = inputText(entry, maxDigits + 1, false, [&](auto key, auto, auto start, auto size, auto cursorPtr) {
        switch (key) {
        case SDL_SCANCODE_MINUS:
        case SDL_SCANCODE_KP_MINUS:
            return allowNegative ? cursorPtr != start || minNum >= 0 : false;

        case SDL_SCANCODE_0: case SDL_SCANCODE_1:
        case SDL_SCANCODE_2: case SDL_SCANCODE_3:
        case SDL_SCANCODE_4: case SDL_SCANCODE_5:
        case SDL_SCANCODE_6: case SDL_SCANCODE_7:
        case SDL_SCANCODE_8: case SDL_SCANCODE_9:
            if (static_cast<int>(size) < maxDigits) {
                auto newDigit = key == SDL_SCANCODE_0 ? 0 : key - SDL_SCANCODE_1 + 1;
                auto newValue = calculateCurrentValue(start, newDigit);
                bool inRange = newValue >= minNum && newValue <= maxNum;
                bool multipleZeros = newValue != 0 || size > 1 || key != SDL_SCANCODE_0;
                return inRange && multipleZeros;
            }
            return false;

        default:
            return false;
        }
    });

    entry.type = kEntryNumber;
    entry.setNumber(result ? atoi(buf) : num);

    menuFree(kBufferSize);
    return result;
}

// in:
//      D0 -  input length (max 99) + 1 for zero terminator
//      A5 -> menu entry to input text into (must be string type)
// out:
//      D0 - 0, zf set = OK, ended with carriage
//           1, zf clear = aborted, escape pressed
//
void SWOS::InputText()
{
    constexpr int kInputLimit = 99;

    int limit = D0.asWord();
    if (limit > kInputLimit) {
        logWarn("Too much text to input, aborting (%d characters vs %d limit)", D0.asWord(), kInputLimit);
        return;
    }

    auto entry = A5.asMenuEntry();
    assert(entry->isString());

    auto result = inputText(*entry, limit);

    SwosVM::flags.zero = result;
    D0 = !result;
}
