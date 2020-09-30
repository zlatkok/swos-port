#include "joypadConfig.mnu.h"
#include "joypads.h"

static int m_joypadIndex;
static SDL_JoystickID m_joypadId;
static PlayerNumber m_playerNumber;

static int m_yPosPercentage;
static int m_yNegPercentage;
static int m_xPosPercentage;
static int m_xNegPercentage;

static JoypadInfo& getJoypad()
{
    return m_playerNumber == kPlayer1 ? getPl1Joypad() : getPl2Joypad();
}

static JoypadValues& getJoypadValues()
{
    return m_playerNumber == kPlayer1 ? getJoypad1Values() : getJoypad2Values();
}

void showJoypadConfigMenu(PlayerNumber player)
{
    assert(player == kPlayer1 || player == kPlayer2);

    m_playerNumber = player;
    m_joypadId = getJoypad().id;
    m_joypadIndex = player == kPlayer1 ? getPl1JoypadIndex() : getPl2JoypadIndex();

    showMenu(joypadConfigMenu);
}

static bool joypadDisconnected()
{
    m_joypadIndex = getJoypadIndexFromId(m_joypadId);
    return m_joypadIndex < 0;
}

static void updateDeadZonePercentageEntry(int entryIndex, int& percentageValue, int value)
{
    if (value >= 0)
        percentageValue = (value * 100 + SDL_JOYSTICK_AXIS_MAX - 1) / SDL_JOYSTICK_AXIS_MAX;
    else
        percentageValue = (-value * 100 + std::abs(SDL_JOYSTICK_AXIS_MIN + 1)) / std::abs(SDL_JOYSTICK_AXIS_MIN);

    auto entry = getMenuEntry(entryIndex);
    assert(entry->type2 == kEntryString);

    auto str = entry->string();
    _itoa(percentageValue, str, 10);
    strcat(str, "%");
}

static void updateYPosPercentageEntry()
{
    updateDeadZonePercentageEntry(JoypadConfigMenu::yPosPercentage, m_yPosPercentage, getJoypad().config.deadZone.yPos);
}

static void updateYNegPercentageEntry()
{
    updateDeadZonePercentageEntry(JoypadConfigMenu::yNegPercentage, m_yNegPercentage, getJoypad().config.deadZone.yNeg);
}

static void updateXPosPercentageEntry()
{
    updateDeadZonePercentageEntry(JoypadConfigMenu::xPosPercentage, m_xPosPercentage, getJoypad().config.deadZone.xPos);
}

static void updateXNegPercentageEntry()
{
    updateDeadZonePercentageEntry(JoypadConfigMenu::xNegPercentage, m_xNegPercentage, getJoypad().config.deadZone.xNeg);
}

static void updateModifiableValues()
{
    using namespace JoypadConfigMenu;

    const auto& joypad = getJoypad();

    const auto kJoypadModifiableValues = {
        std::make_pair(fireButton, joypad.config.fireButtonIndex), std::make_pair(benchButton, joypad.config.benchButtonIndex),
        std::make_pair(deadZoneXPos, joypad.config.deadZone.xPos), std::make_pair(deadZoneXNeg, joypad.config.deadZone.xNeg),
        std::make_pair(deadZoneYPos, joypad.config.deadZone.yPos), std::make_pair(deadZoneYNeg, joypad.config.deadZone.yNeg),
    };

    for (const auto& entryValue : kJoypadModifiableValues) {
        auto entryIndex = entryValue.first;
        auto value = entryValue.second;
        getMenuEntry(entryIndex)->u2.number = value;
    }
}

static void fillJoypadInfo()
{
    using namespace JoypadConfigMenu;

    const auto& joypad = getJoypad();

    auto nameStr = getMenuEntry(name)->string();
    auto joypadName = SDL_JoystickName(joypad.handle);

    strncpy_s(nameStr, kStdMenuTextSize, joypadName, _TRUNCATE);
    toUpper(nameStr);
    elideString(nameStr, strlen(nameStr), kInfoFieldWidth - 2);

    auto guidStr = getMenuEntry(guid)->string();
    SDL_JoystickGetGUIDString(joypad.config.guid, guidStr, kStdMenuTextSize);
    toUpper(guidStr);

    const auto kJoypadInfoEntries = {
        std::make_pair(id, joypad.id),
        std::make_pair(numButtons, SDL_JoystickNumButtons(joypad.handle)),
        std::make_pair(numAxes, SDL_JoystickNumAxes(joypad.handle)),
    };

    for (const auto& entryInfo : kJoypadInfoEntries) {
        auto index = entryInfo.first;
        auto value = entryInfo.second;

        auto entry = getMenuEntry(index);
        entry->u2.number = value;

        int numDigits = 0;
        while (value) {
            value /= 10;
            numDigits++;
        }

        char buf[32];
        _itoa(entry->u2.number, buf, 10);
        entry->width = std::max(8, getStringPixelLength(buf));
    }

    auto powerLevelDest = getMenuEntry(powerLevel)->string();
    auto powerLevel = SDL_JoystickCurrentPowerLevel(joypad.handle);
    const char *powerLevelDesc = "UNKNOWN";

    switch (powerLevel) {
    case SDL_JOYSTICK_POWER_EMPTY:
        powerLevelDesc = "EMPTY";
        break;
    case SDL_JOYSTICK_POWER_LOW:
        powerLevelDesc = "LOW";
        break;
    case SDL_JOYSTICK_POWER_MEDIUM:
        powerLevelDesc = "MEDIUM";
        break;
    case SDL_JOYSTICK_POWER_FULL:
        powerLevelDesc = "FULL";
        break;
    case SDL_JOYSTICK_POWER_WIRED:
        powerLevelDesc = "WIRED";
        break;
    case SDL_JOYSTICK_POWER_MAX:
        powerLevelDesc = "MAX";
        break;
    }

    strcpy(powerLevelDest, powerLevelDesc);

    updateModifiableValues();

    updateYPosPercentageEntry();
    updateYNegPercentageEntry();
    updateXPosPercentageEntry();
    updateXNegPercentageEntry();
}

static void joypadConfigMenuOnInit()
{
    if (joypadDisconnected()) {
        SetExitMenuFlag();
        return;
    }

    fillJoypadInfo();
}

static void updateJoypadValues()
{
    using namespace JoypadConfigMenu;

    const auto& joypadValues = getJoypadValues();

    getMenuEntry(rawX)->u2.number = joypadValues.x;
    getMenuEntry(rawY)->u2.number = joypadValues.y;
}

static void fillRectangle(int x1, int y1, int x2, int y2, int color)
{
    auto dest = swos.linAdr384k + y1 * kMenuScreenWidth + x1;
    auto width = x2 - x1 + 1;

    for (int y = y1; y <= y2; y++) {
        memset(dest, color, width);
        dest += kMenuScreenWidth;
    }
}

static void drawDeadZoneBoxBackground()
{
    using namespace JoypadConfigMenu;

    constexpr int kInactiveGreenColor = 226;
    constexpr int kActiveGreenColor = 239;
    constexpr int kInactiveRedColor = 160;
    constexpr int kActiveRedColor = 128;

    auto width = kDeadZoneBoxX2 - kDeadZoneBoxX1 + 1;
    auto insideHeight = kDeadZoneBoxY2 - kDeadZoneBoxY1 - 1;

    auto p = swos.linAdr384k + kMenuScreenWidth * (kDeadZoneBoxY1 + 1) + kDeadZoneBoxX1;

    for (int i = 0; i < insideHeight; i++) {
        memset(p + 1, kInactiveGreenColor, width - 2);
        p += kMenuScreenWidth;
    }

    const auto& values = getJoypadValues();
    auto& deadZone = getJoypad().config.deadZone;

    auto topLeftBackColor = values.y < 0 && values.up() || values.x < 0 && values.left() ? kActiveGreenColor : kInactiveGreenColor;
    auto topRightBackColor = values.y < 0 && values.up() || values.x > 0 && values.right() ? kActiveGreenColor : kInactiveGreenColor;
    auto bottomLeftBackColor = values.y > 0 && values.down() || values.x < 0 && values.left() ? kActiveGreenColor : kInactiveGreenColor;
    auto bottomRightBackColor = values.y > 0 && values.down() || values.x > 0 && values.right() ? kActiveGreenColor : kInactiveGreenColor;

    auto centerX = kDeadZoneBoxX1 + (kDeadZoneBoxX2 - kDeadZoneBoxX1 + 1) / 2;
    auto centerY = kDeadZoneBoxY1 + (kDeadZoneBoxY2 - kDeadZoneBoxY1 + 1) / 2;

    fillRectangle(kDeadZoneBoxX1, kDeadZoneBoxY1, centerX, centerY, topLeftBackColor);
    fillRectangle(kDeadZoneBoxX1, centerY, centerX, kDeadZoneBoxY2, bottomLeftBackColor);
    fillRectangle(centerX, kDeadZoneBoxY1, kDeadZoneBoxX2, centerY, topRightBackColor);
    fillRectangle(centerX, centerY, kDeadZoneBoxX2, kDeadZoneBoxY2, bottomRightBackColor);

    auto topLeftFrontColor = values.y < 0 && !values.up() || values.x < 0 && !values.left() ? kActiveRedColor : kInactiveRedColor;
    auto topRightFrontColor = values.y < 0 && !values.up() || values.x > 0 && !values.right() ? kActiveRedColor : kInactiveRedColor;
    auto bottomLeftFrontColor = values.y > 0 && !values.down() || values.x < 0 && !values.left() ? kActiveRedColor : kInactiveRedColor;
    auto bottomRightFrontColor = values.y > 0 && !values.down() || values.x > 0 && !values.right() ? kActiveRedColor : kInactiveRedColor;

    auto deadZoneX1 = centerX - (centerX - kDeadZoneBoxX1) * m_xNegPercentage / 100;
    auto deadZoneY1 = centerY - (centerY - kDeadZoneBoxY1) * m_yNegPercentage / 100;
    auto deadZoneX2 = centerX + (kDeadZoneBoxX2 - centerX) * m_xPosPercentage / 100;
    auto deadZoneY2 = centerY + (kDeadZoneBoxY2 - centerY) * m_yPosPercentage / 100;

    fillRectangle(deadZoneX1, deadZoneY1, centerX, centerY, topLeftFrontColor);
    fillRectangle(deadZoneX1, centerY, centerX, deadZoneY2, bottomLeftFrontColor);
    fillRectangle(centerX, deadZoneY1, deadZoneX2, centerY, topRightFrontColor);
    fillRectangle(centerX, centerY, deadZoneX2, deadZoneY2, bottomRightFrontColor);
}

static void drawDeadZoneBoxFrame()
{
    using namespace JoypadConfigMenu;

    constexpr int kFrameColor = 1;

    auto width = kDeadZoneBoxX2 - kDeadZoneBoxX1 + 1;
    auto insideHeight = kDeadZoneBoxY2 - kDeadZoneBoxY1 - 1;

    memset(swos.linAdr384k + kMenuScreenWidth * kDeadZoneBoxY1 + kDeadZoneBoxX1, kFrameColor, width);
    memset(swos.linAdr384k + kMenuScreenWidth * kDeadZoneBoxY2 + kDeadZoneBoxX1, kFrameColor, width);

    auto p = swos.linAdr384k + kMenuScreenWidth * (kDeadZoneBoxY1 + 1) + kDeadZoneBoxX1;

    for (int i = 0; i < insideHeight; i++) {
        *p = kFrameColor;
        p[width - 1] = kFrameColor;
        p[width / 2] = kFrameColor;
        p += kMenuScreenWidth;
    }

    auto middleLine = swos.linAdr384k + (kDeadZoneBoxY1 + (kDeadZoneBoxY2 - kDeadZoneBoxY1 + 1) / 2) * kMenuScreenWidth + kDeadZoneBoxX1 + 1;
    memset(middleLine, kFrameColor, width - 2);
}

static void drawDeadZoneBox()
{
    drawDeadZoneBoxBackground();
    drawDeadZoneBoxFrame();
}

static void joypadConfigMenuOnDraw()
{
    if (joypadDisconnected()) {
        SetExitMenuFlag();
        return;
    }

    updateJoypadValues();
    drawDeadZoneBox();
}

static bool selectButton(int& button, int& otherButton, const char *buttonName)
{
    assert(button != otherButton);

    redrawMenuBackground();

    constexpr int kCenterX = kMenuScreenWidth / 2;
    constexpr int kLine1Y = 40;
    constexpr int kLine2Y = 50;
    constexpr int kAbortY = 150;

    auto joypadName = SDL_JoystickName(getJoypad().handle);

    char text[128];
    strcpy_s(text, joypadName);
    toUpper(text);
    elideString(text, strlen(text), kMenuScreenWidth - 20);

    drawMenuTextCentered(kCenterX, kLine1Y, text);

    sprintf_s(text, "PRESS %s BUTTON", buttonName);
    drawMenuTextCentered(kCenterX, kLine2Y, text);

    drawMenuTextCentered(kCenterX, kAbortY, "ESCAPE/MOUSE CLICK ABORTS");

    SWOS::FlipInMenu();

    clearJoypadInput();

    bool result = false;

    while (true) {
        updateControls();
        SWOS::GetKey();

        if (SDL_GetMouseState(nullptr, nullptr) || swos.lastKey == kKeyEscape)
            break;

        if (joypadDisconnected())
            break;

        auto buttons = getJoypadButtonsDown(m_joypadIndex);

        if (!buttons.empty()) {
            if (buttons.size() == 1 && buttons[0] == otherButton) {
                logInfo("Selected already taken game controller button, swapping them");
                std::swap(button, otherButton);
            } else {
                for (auto pressedButon : buttons) {
                    if (pressedButon != otherButton) {
                        logInfo("Selected button %d for %s", button, buttonName);
                        button = pressedButon;
                        break;
                    }
                }
            }

            result = true;
            break;
        }

        SDL_Delay(25);
    }

    if (result) {
        char text[128];
        sprintf_s(text, "BUTTON %d SELECTED", button);

        drawMenuTextCentered(kCenterX, (kLine2Y + kAbortY) / 2, text, kYellowText);
        SWOS::FlipInMenu();
        SDL_Delay(600);
    }

    clearKeyInput();
    clearJoypadInput();

    return result;
}

static void selectFireButton()
{
    auto& config = getJoypad().config;
    if (selectButton(config.fireButtonIndex, config.benchButtonIndex, "FIRE")) {
        propagateJoypadConfig(m_joypadIndex);
        updateModifiableValues();
    }
}

static void selectBenchButton()
{
    auto& config = getJoypad().config;
    if (selectButton(config.benchButtonIndex, config.fireButtonIndex, "BENCH")) {
        propagateJoypadConfig(m_joypadIndex);
        updateModifiableValues();
    }
}

static void enterDeadZone(int& dest, int min, int max)
{
    auto entry = A5.as<MenuEntry *>();
    if (inputNumber(entry, 6, min, max)) {
        dest = static_cast<int16_t>(entry->u2.number);
        propagateJoypadConfig(m_joypadIndex);
    }
}

static void enterDeadZoneXPos()
{
    enterDeadZone(getJoypad().config.deadZone.xPos, 0, SDL_JOYSTICK_AXIS_MAX);
    updateXPosPercentageEntry();
}

static void enterDeadZoneXNeg()
{
    enterDeadZone(getJoypad().config.deadZone.xNeg, SDL_JOYSTICK_AXIS_MIN, 0);
    updateXNegPercentageEntry();
}

static void enterDeadZoneYPos()
{
    enterDeadZone(getJoypad().config.deadZone.yPos, 0, SDL_JOYSTICK_AXIS_MAX);
    updateYPosPercentageEntry();
}

static void enterDeadZoneYNeg()
{
    enterDeadZone(getJoypad().config.deadZone.yNeg, SDL_JOYSTICK_AXIS_MIN, 0);
    updateYNegPercentageEntry();
}

static void setDeadZoneValueAndPropagate(int entryIndex, int value)
{
    getMenuEntry(entryIndex)->u2.number = value;
    propagateJoypadConfig(m_joypadIndex);
}

template <typename Op, int fullRange>
static void changeDeadZonePercentage(int percentageEntryIndex, int absoluteValueEntryIndex, int& percentageValue, int& value)
{
    auto percentageEntry = getMenuEntry(percentageEntryIndex);
    assert(percentageEntry->type2 == kEntryString);

    auto str = percentageEntry->string();
    assert(strrchr(str, '%'));

    auto newPercentageValue = Op()(percentageValue, 1);

    if (newPercentageValue >= 0 && newPercentageValue <= 100) {
        percentageValue = newPercentageValue;
        value = percentageValue * fullRange / 100;

        auto absoluteValueEntry = getMenuEntry(absoluteValueEntryIndex);
        assert(absoluteValueEntry->type2 == kEntryNumber);
        absoluteValueEntry->u2.number = value;

        _itoa(percentageValue, str, 10);
        strcat(str, "%");

        propagateJoypadConfig(m_joypadIndex);
    }
}

static void decreaseYPos()
{
    using namespace JoypadConfigMenu;

    int& value = getJoypad().config.deadZone.yPos;
    changeDeadZonePercentage<std::minus<int>, SDL_JOYSTICK_AXIS_MAX>(yPosPercentage, deadZoneYPos, m_yPosPercentage, value);
}

static void increaseYPos()
{
    using namespace JoypadConfigMenu;

    int& value = getJoypad().config.deadZone.yPos;
    changeDeadZonePercentage<std::plus<int>, SDL_JOYSTICK_AXIS_MAX>(yPosPercentage, deadZoneYPos, m_yPosPercentage, value);
}

static void decreaseYNeg()
{
    using namespace JoypadConfigMenu;

    int& value = getJoypad().config.deadZone.yNeg;
    changeDeadZonePercentage<std::minus<int>, SDL_JOYSTICK_AXIS_MIN>(yNegPercentage, deadZoneYNeg, m_yNegPercentage, value);
}

static void increaseYNeg()
{
    using namespace JoypadConfigMenu;

    int& value = getJoypad().config.deadZone.yNeg;
    changeDeadZonePercentage<std::plus<int>, SDL_JOYSTICK_AXIS_MIN>(yNegPercentage, deadZoneYNeg, m_yNegPercentage, value);
}

static void decreaseXPos()
{
    using namespace JoypadConfigMenu;

    int& value = getJoypad().config.deadZone.xPos;
    changeDeadZonePercentage<std::minus<int>, SDL_JOYSTICK_AXIS_MAX>(xPosPercentage, deadZoneXPos, m_xPosPercentage, value);
}

static void increaseXPos()
{
    using namespace JoypadConfigMenu;

    int& value = getJoypad().config.deadZone.xPos;
    changeDeadZonePercentage<std::plus<int>, SDL_JOYSTICK_AXIS_MAX>(xPosPercentage, deadZoneXPos, m_xPosPercentage, value);
}

static void decreaseXNeg()
{
    using namespace JoypadConfigMenu;

    int& value = getJoypad().config.deadZone.xNeg;
    changeDeadZonePercentage<std::minus<int>, SDL_JOYSTICK_AXIS_MIN>(xNegPercentage, deadZoneXNeg, m_xNegPercentage, value);
}

static void increaseXNeg()
{
    using namespace JoypadConfigMenu;

    int& value = getJoypad().config.deadZone.xNeg;
    changeDeadZonePercentage<std::plus<int>, SDL_JOYSTICK_AXIS_MIN>(xNegPercentage, deadZoneXNeg, m_xNegPercentage, value);
}
