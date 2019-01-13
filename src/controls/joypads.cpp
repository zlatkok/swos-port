#include "joypads.h"
#include "controls.h"
#include "util.h"

static std::vector<JoypadConfig> m_joypadConfig;
static std::vector<JoypadInfo> m_joypads;
static std::vector<SDL_JoystickID> m_orderedJoypadIds;

static int m_pl1Joypad = -1;
static int m_pl2Joypad = -1;
static int m_pl1GameJoypad = -1;
static int m_pl2GameJoypad = -1;

static JoypadValues m_joypad1Values;
static JoypadValues m_joypad2Values;

static std::string m_joy1GuidStr;
static std::string m_joy2GuidStr;

static bool m_joypadsInitialized;

static bool m_autoConnectJoypads = true;

// options
const char kJoypad1Key[] = "player1Joypad";
const char kJoypad2Key[] = "player2Joypad";

const char kAutoConnectJoypadsKey[] = "autoConnectJoypads";

const char kJoyConfigPrefix[] = "joystickConfig-";

const char kDeadZonePositiveX[] = "joypadDeadZoneXPositive";
const char kDeadZoneNegativeX[] = "joypadDeadZoneXNegative";
const char kDeadZoneNegativeY[] = "joypadDeadZoneYPositive";
const char kDeadZonePositiveY[] = "joypadDeadZoneYNegative";

const char kJoypadFireButtonIndexKey[] = "fireButton";
const char kJoypadBenchButtonIndexKey[] = "benchButton";

bool JoypadDeadZone::left(int x) const
{
    return x < xNeg;
}

bool JoypadDeadZone::right(int x) const
{
    return x > xPos;
}

bool JoypadDeadZone::up(int y) const
{
    return y < yNeg;
}

bool JoypadDeadZone::down(int y) const
{
    return y > yPos;
}

static inline bool guidEqual(const SDL_JoystickGUID& guid1, const SDL_JoystickGUID& guid2)
{
    return !memcmp(&guid1, &guid2, sizeof(guid1));
}

bool JoypadDeadZone::operator==(const JoypadDeadZone& other) const
{
    return xPos == other.xPos && xNeg == other.xNeg && yPos == other.yPos && yNeg == other.yNeg;
}

bool JoypadConfig::operator==(const JoypadConfig& other) const
{
    return guidEqual(guid, other.guid);
}

bool JoypadConfig::isDefault() const
{
    JoypadConfig defaultConfig;
    return fireButtonIndex == defaultConfig.fireButtonIndex && benchButtonIndex == defaultConfig.benchButtonIndex &&
        deadZone == defaultConfig.deadZone;
}

JoypadInfo::JoypadInfo(SDL_Joystick *handle, SDL_JoystickID id, JoypadConfig config)
    : handle(handle), id(id), config(config)
{}

bool JoypadInfo::tryReopening(int index)
{
    if (!handle && (handle = SDL_JoystickOpen(index)))
        id = SDL_JoystickInstanceID(handle);

    return handle != nullptr;
}

void JoypadInfo::buttonStateChanged(int index, bool pressed)
{
    // special handling of crazy USB adapter that sends first movement as button
    SDL_JoystickGUID kUsbAdapterGuid {
        0x03, 0x00, 0x00, 0x00, 0x9d, 0x07, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    if (index > 1 && guidEqual(config.guid, kUsbAdapterGuid))
        return;

    auto it = std::find_if(buttons.begin(), buttons.end(), [index](const auto& button) { return button.first == index; });
    if (it != buttons.end())
        it->second = pressed;
    else
        buttons.emplace_back(index, pressed);
}

bool JoypadInfo::anyButtonDown() const
{
    for (const auto& buttonState : buttons)
        if (buttonState.second)
            return true;

    return false;
}

bool JoypadValues::left() const
{
    return deadZone.left(x);
}

bool JoypadValues::right() const
{
    return deadZone.right(x);
}

bool JoypadValues::up() const
{
    return deadZone.up(y);
}

bool JoypadValues::down() const
{
    return deadZone.down(y);
}

static int findFreeJoypad(int thisIndex, int otherIndex)
{
    for (int i = static_cast<int>(m_joypads.size()) - 1; i >= 0; i--)
        if (i != thisIndex && i != otherIndex)
            return i;

    return -1;
}

void initJoypads()
{
    updateControls();

    if (getPl1Controls() == kJoypad || getPl2Controls() == kJoypad) {
        SDL_JoystickGUID joy1Guid{}, joy2Guid{};

        joy1Guid = SDL_JoystickGetGUIDFromString(m_joy1GuidStr.c_str());
        joy2Guid = SDL_JoystickGetGUIDFromString(m_joy2GuidStr.c_str());

        int numJoypadsToFind = (getPl1Controls() == kJoypad) + (getPl2Controls() == kJoypad);
        int joypadsFound = 0;

        assert(numJoypadsToFind <= SDL_NumJoysticks());

        for (size_t i = 0; i < m_joypads.size() && joypadsFound < numJoypadsToFind; i++) {
            if (m_joypads[i].handle) {
                auto guid = SDL_JoystickGetGUID(m_joypads[i].handle);
                if (m_pl1Joypad < 0 && getPl1Controls() == kJoypad && guidEqual(guid, joy1Guid)) {
                    logInfo("Found joypad for player 1 with GUID %s", m_joy1GuidStr.c_str());
                    m_pl1Joypad = i;
                    joypadsFound++;
                } else if (m_pl2Joypad < 0 && getPl2Controls() == kJoypad && guidEqual(guid, joy2Guid)) {
                    logInfo("Found joypad for player 2 with GUID %s", m_joy2GuidStr.c_str());
                    m_pl2Joypad = i;
                    joypadsFound++;
                }
            }
        }

        // in case we found nothing by GUID's, just assign any
        if (getPl1Controls() == kJoypad && m_pl1Joypad < 0)
            m_pl1Joypad = findFreeJoypad(m_pl1Joypad, m_pl2Joypad);

        if (getPl2Controls() == kJoypad && m_pl2Joypad < 0)
            m_pl2Joypad = findFreeJoypad(m_pl1Joypad, m_pl2Joypad);

        // revert controls if no joypads after all
        if (getPl1Controls() == kJoypad && m_pl1Joypad < 0)
            setPl1Controls(!pl1KeyboardNullScancode() ? kKeyboard1 : kMouse);

        if (getPl2Controls() == kJoypad && m_pl2Joypad < 0)
            setPl2Controls(kNone);

        assert(getPl1Controls() != kJoypad || m_pl1Joypad >= 0 && m_joypads[m_pl1Joypad].id != kInvalidJoypadId);
        assert(getPl2Controls() != kJoypad || m_pl2Joypad >= 0 && m_joypads[m_pl2Joypad].id != kInvalidJoypadId);

        if (getPl1Controls() == kJoypad)
            logInfo("Player 1 selected joystick %d", m_pl1Joypad);

        if (getPl2Controls() == kJoypad)
            logInfo("Player 2 selected joystick %d", m_pl2Joypad);
    }

    m_joypadsInitialized = true;

    m_joy1GuidStr.clear();
    m_joy2GuidStr.clear();
}

int getPl1JoypadIndex()
{
    return m_pl1Joypad;
}

int getPl2JoypadIndex()
{
    return m_pl2Joypad;
}

int getPl1GameJoypadIndex()
{
    return m_pl1GameJoypad;
}

int getPl2GameJoypadIndex()
{
    return m_pl2GameJoypad;
}

JoypadInfo& getPl1Joypad()
{
    return getJoypad(m_pl1Joypad);
}

JoypadInfo& getPl2Joypad()
{
    return getJoypad(m_pl2Joypad);
}

JoypadInfo& getJoypad(int index)
{
    assert(index >= 0 && index < getNumJoypads());
    return m_joypads[index];
}

int getJoypadIndexFromId(SDL_JoystickID id)
{
    auto it = std::find_if(m_joypads.begin(), m_joypads.end(), [id](const auto& joypad) { return joypad.id == id; });
    return it != m_joypads.end() ? it - m_joypads.begin() : -1;
}

bool pl1UsingJoypad()
{
    return m_pl1Joypad >= 0;
}

bool pl2UsingJoypad()
{
    return m_pl2Joypad >= 0;
}

void setPl1JoypadIndex(int index)
{
    assert(index >= -1 && index < getNumJoypads());
    m_pl1Joypad = index;
}

void setPl2JoypadIndex(int index)
{
    assert(index >= -1 && index < getNumJoypads());
    m_pl2Joypad = index;
}

void setPl1GameJoypadIndex(int index)
{
    assert(index >= -1 && index < getNumJoypads());
    m_pl1GameJoypad = index;
}

void setPl2GameJoypadIndex(int index)
{
    assert(index >= -1 && index < getNumJoypads());
    m_pl2GameJoypad = index;
}

int getNumJoypads()
{
    return m_joypads.size();
}

JoypadValues& getJoypad1Values()
{
    return m_joypad1Values;
}

JoypadValues& getJoypad2Values()
{
    return m_joypad2Values;
}

void resetMatchJoypads()
{
    m_pl1GameJoypad = -1;
    m_pl2GameJoypad = -1;
}

static void selectJoypad(int index)
{
    assert(static_cast<int>(m_joypads.size()) > index);

    if (!m_autoConnectJoypads || !m_joypadsInitialized)
        return;

    const auto& joypad = m_joypads[index];
    if (!joypad.handle)
        return;

    if (getPl2Controls() != kJoypad) {
        setPl2Controls(kJoypad, index);
    } else if (getPl1Controls() != kJoypad) {
        setPl1Controls(kJoypad, index);
    } else {
        assert(m_pl1Joypad >= 0 && m_pl1Joypad < static_cast<int>(m_joypads.size()) &&
            m_pl2Joypad >= 0 && m_pl2Joypad < static_cast<int>(m_joypads.size()));

        // kick out the one that was first to connect
        auto joy1It = std::find(m_orderedJoypadIds.begin(), m_orderedJoypadIds.end(), m_joypads[m_pl1Joypad].id);
        auto joy2It = std::find(m_orderedJoypadIds.begin(), m_orderedJoypadIds.end(), m_joypads[m_pl2Joypad].id);

        if (joy1It < joy2It)
            setPl1Controls(kJoypad, index);
        else
            setPl2Controls(kJoypad, index);
    }
}

void addJoypad(int index)
{
    auto handle = SDL_JoystickOpen(index);
    auto id = kInvalidJoypadId;
    JoypadConfig config;
    SDL_JoystickGUID guid{};

    if (handle) {
        id = SDL_JoystickInstanceID(handle);
        guid = SDL_JoystickGetGUID(handle);

        auto it = std::find_if(m_joypadConfig.begin(), m_joypadConfig.end(), [&guid](const auto& config) {
            return guidEqual(config.guid, guid);
        });

        if (it == m_joypadConfig.end()) {
            m_joypadConfig.emplace_back(guid);
            it = m_joypadConfig.end() - 1;
        }

        auto numButtons = SDL_JoystickNumButtons(handle);
        if (it->fireButtonIndex > numButtons)
            it->fireButtonIndex = 0;
        if (it->benchButtonIndex > numButtons)
            it->benchButtonIndex = it->fireButtonIndex != 1;

        config = *it;
    } else {
        logWarn("Failed to open joypad %d, SDL error: %s", index, SDL_GetError());
        beep();
    }

    JoypadInfo joypadInfo(handle, id, config);
    m_joypads.insert(m_joypads.begin() + index, joypadInfo);

    m_orderedJoypadIds.push_back(id);

    // index shouldn't ever be less than number of joypads, but just in case...
    m_pl1Joypad += m_pl1Joypad >= index;
    m_pl2Joypad += m_pl2Joypad >= index;
    m_pl1GameJoypad += m_pl1GameJoypad >= index;
    m_pl2GameJoypad += m_pl2GameJoypad >= index;

    selectJoypad(index);
}

void removeJoypad(SDL_JoystickID id)
{
    auto it = std::find_if(m_joypads.begin(), m_joypads.end(), [id](const auto& joypad) { return joypad.id == id; });

    if (it != m_joypads.end()) {
        int index = it - m_joypads.begin();

        if (index == m_pl1Joypad) {
            assert(getPl1Controls() == kJoypad);
            m_pl1Joypad = findFreeJoypad(m_pl1Joypad, m_pl2Joypad);
            if (m_pl1Joypad < 0)
                setPl1Controls(kKeyboard1);
        } else if (index == m_pl2Joypad) {
            assert(getPl2Controls() == kJoypad);
            m_pl2Joypad = findFreeJoypad(m_pl1Joypad, m_pl2Joypad);
            if (m_pl2Joypad < 0)
                setPl2Controls(kNone);
        }

        if (index == m_pl1GameJoypad) {
            logInfo("Player %d joypad disconnected during the game", 1);
            m_pl1GameJoypad = findFreeJoypad(m_pl1GameJoypad, m_pl2GameJoypad);
            if (m_pl1GameJoypad < 0) {
                logInfo("Can't find another joypad for player %d, switching to keyboard", 1);
                setPl1GameControls(kKeyboard1);
            } else {
                logInfo("Player %d switching to joypad %d (%s)", 1, m_pl1GameJoypad, SDL_JoystickName(m_joypads[m_pl1GameJoypad].handle));
            }
        } else if (index == m_pl2GameJoypad) {
            logInfo("Player %d joypad disconnected during the game", 2);
            m_pl2GameJoypad = findFreeJoypad(m_pl1GameJoypad, m_pl2GameJoypad);
            if (m_pl2GameJoypad < 0) {
                logInfo("Can't find another joypad for player %d, switching to keyboard", 2);
                setPl2GameControls(kKeyboard2);
            } else {
                logInfo("Player %d switching to joypad %d (%s)", 2, m_pl2GameJoypad, SDL_JoystickName(m_joypads[m_pl2GameJoypad].handle));
            }
        }

        m_joypads.erase(it);

        auto idIterator = std::find(m_orderedJoypadIds.begin(), m_orderedJoypadIds.end(), id);
        if (idIterator != m_orderedJoypadIds.end())
            m_orderedJoypadIds.erase(idIterator);

        // update all the indices
        m_pl1Joypad -= m_pl1Joypad > index;
        m_pl2Joypad -= m_pl2Joypad > index;
        m_pl1GameJoypad -= m_pl1GameJoypad > index;
        m_pl2GameJoypad -= m_pl2GameJoypad > index;
    } else {
        logWarn("Joypad id %d not found!");
        assert(false);
    }
}

static const auto kJoypadIndicesValues = {
    std::make_tuple(false, &m_pl1Joypad, &m_joypad1Values),
    std::make_tuple(false, &m_pl2Joypad, &m_joypad2Values),
    std::make_tuple(true, &m_pl1GameJoypad, &m_joypad1Values),
    std::make_tuple(true, &m_pl2GameJoypad, &m_joypad2Values),
};

void updateJoypadButtonState(SDL_JoystickID id, int buttonIndex, bool pressed)
{
    auto it = std::find_if(m_joypads.begin(), m_joypads.end(), [id](const auto& joypad) { return joypad.id == id; });
    if (it != m_joypads.end()) {
        it->buttonStateChanged(buttonIndex, pressed);
        int index = it - m_joypads.begin();

        for (const auto& indexValue : kJoypadIndicesValues) {
            bool matchValues = std::get<0>(indexValue);
            auto currentIndex = *std::get<1>(indexValue);
            auto values = std::get<2>(indexValue);

            if (matchValues == isMatchRunning() && currentIndex >= 0 && m_joypads[currentIndex].id == id) {
                const auto& config = m_joypads[index].config;

                if (buttonIndex == config.fireButtonIndex)
                    values->fire = pressed;
                else if (buttonIndex == config.benchButtonIndex)
                    values->bench = pressed;

                break;
            }
        }
    } else {
        logWarn("Joystick with id %d not found!", id);
        assert(false);
    }
}

void updateJoypadMotion(SDL_JoystickID id, Uint8 axis, Sint16 value)
{
    for (const auto& indexValue : kJoypadIndicesValues) {
        bool matchValues = std::get<0>(indexValue);
        auto index = *std::get<1>(indexValue);

        if (matchValues == isMatchRunning() && index >= 0 && id == m_joypads[index].id) {
            auto values = std::get<2>(indexValue);

            auto bu = SDL_JoystickGetAxis(m_joypads[index].handle, 0);
            auto bg = SDL_JoystickGetAxis(m_joypads[index].handle, 1);
            Sint16 state;
            auto ii = SDL_JoystickGetAxisInitialState(m_joypads[index].handle, axis, &state);

            if (axis == 0)
                values->x = value;
            else if (axis > 0)
                values->y = value;

            break;
        }
    }
}

void propagateJoypadConfig(int index)
{
    const auto& joypad = getJoypad(index);

    for (auto& config : m_joypadConfig) {
        if (guidEqual(config.guid, joypad.config.guid))
            config = joypad.config;
    }
}

int getJoypadWithButtonDown()
{
    auto it = std::find_if(m_joypads.begin(), m_joypads.end(), [](const auto& joypad) {
        return joypad.anyButtonDown();
    });

    if (it == m_joypads.end())
        return -1;

    return it - m_joypads.begin();
}

std::vector<int> getJoypadButtonsDown(int index)
{
    auto& joypad = getJoypad(index);

    std::vector<int> result;

    for (const auto& buttonInfo : joypad.buttons)
        if (buttonInfo.second)
            result.push_back(buttonInfo.first);

    return result;
}

void clearJoypadInput()
{
    do {
        updateControls();
    } while (getJoypadWithButtonDown() >= 0);

    fire = 0;
}

// Transfer control of the joypad from this player to the other one.
static void transferJoypad(PlayerNumber player, int& joypadIndex, int& otherJoypadIndex)
{
    assert(joypadIndex >= 0 && joypadIndex < getNumJoypads() && m_joypads[joypadIndex].handle);

    logInfo("Trying to reassign joypad %d from player %d to player %d", joypadIndex, player + 1, 2 - player);

    otherJoypadIndex = joypadIndex;
    joypadIndex = -1;

    joypadIndex = findFreeJoypad(joypadIndex, otherJoypadIndex);

    if (joypadIndex < 0) {
        if (player == kPlayer1)
            setPl1Controls(pl1KeyboardNullScancode() ? kMouse : kKeyboard1);
        else
            setPl2Controls(kNone);
    }
}

bool selectJoypadControls(PlayerNumber player, int joypadNo)
{
    assert(player == kPlayer1 || player == kPlayer2);
    assert(joypadNo >= 0 && joypadNo < static_cast<int>(m_joypads.size()));

    logInfo("Trying to select joypad %d for player %d", joypadNo, player == kPlayer1 ? 1 : 2);

    auto controls = getPl1Controls();
    auto otherControls = getPl2Controls();
    auto joypadIndex = &m_pl1Joypad;
    auto otherJoypadIndex = &m_pl2Joypad;
    auto otherPlayer = kPlayer2;

    if (player == kPlayer2) {
        std::swap(controls, otherControls);
        std::swap(joypadIndex, otherJoypadIndex);
        otherPlayer = kPlayer1;
    }

    if (otherControls == kJoypad && joypadNo == *otherJoypadIndex)
        transferJoypad(otherPlayer, *otherJoypadIndex, *joypadIndex);

    auto& joypad = m_joypads[joypadNo];

    joypad.tryReopening(joypadNo);

    if (joypad.handle) {
        if (player == kPlayer1)
            setPl1Controls(kJoypad, joypadNo);
        else
            setPl2Controls(kJoypad, joypadNo);
        return true;
    } else {
        logWarn("Failed to open joypad %d", joypadNo);
        beep();
        return false;
    }
}

bool getAutoConnectJoypads()
{
    return m_autoConnectJoypads;
}

void setAutoConnectJoypads(bool value)
{
    m_autoConnectJoypads = value;
}

//
// joypad options
//

static void loadJoypadConfig(const CSimpleIni& ini)
{
    CSimpleIni::TNamesDepend sectionList;
    ini.GetAllSections(sectionList);

    for (const auto& sectionName : sectionList) {
        if (!strncmp(sectionName.pItem, kJoyConfigPrefix, sizeof(kJoyConfigPrefix) - 1)) {
            auto guidStr = sectionName.pItem + sizeof(kJoyConfigPrefix) - 1;
            assert(strlen(guidStr) > 10);

            JoypadConfig joypadConfig;
            joypadConfig.guid = SDL_JoystickGetGUIDFromString(guidStr);

            joypadConfig.fireButtonIndex = ini.GetLongValue(sectionName.pItem, kJoypadFireButtonIndexKey, 0);
            joypadConfig.benchButtonIndex = ini.GetLongValue(sectionName.pItem, kJoypadBenchButtonIndexKey, 1);

            joypadConfig.deadZone.xPos = ini.GetLongValue(sectionName.pItem, kDeadZonePositiveX, kDefaultDeadZoneValue);
            joypadConfig.deadZone.xNeg = ini.GetLongValue(sectionName.pItem, kDeadZoneNegativeX, -kDefaultDeadZoneValue);
            joypadConfig.deadZone.yPos = ini.GetLongValue(sectionName.pItem, kDeadZonePositiveY, kDefaultDeadZoneValue);
            joypadConfig.deadZone.yNeg = ini.GetLongValue(sectionName.pItem, kDeadZoneNegativeY, -kDefaultDeadZoneValue);

            assert(std::find(m_joypadConfig.begin(), m_joypadConfig.end(), joypadConfig) == m_joypadConfig.end());

            m_joypadConfig.push_back(joypadConfig);
        }
    }
}

static void saveJoypadConfig(CSimpleIni& ini)
{
    for (const auto& joypadConfig : m_joypadConfig) {
        if (joypadConfig.isDefault())
            continue;

        char sectionName[64];
        strcpy(sectionName, kJoyConfigPrefix);

        auto guidPtr = sectionName + sizeof(kJoyConfigPrefix) - 1;
        SDL_JoystickGetGUIDString(joypadConfig.guid, guidPtr, sizeof(sectionName) - sizeof(kJoyConfigPrefix) + 1);

        ini.SetLongValue(sectionName, kJoypadFireButtonIndexKey, joypadConfig.fireButtonIndex);
        ini.SetLongValue(sectionName, kJoypadBenchButtonIndexKey, joypadConfig.benchButtonIndex);

        ini.SetLongValue(sectionName, kDeadZonePositiveX, joypadConfig.deadZone.xPos);
        ini.SetLongValue(sectionName, kDeadZoneNegativeX, joypadConfig.deadZone.xNeg);
        ini.SetLongValue(sectionName, kDeadZonePositiveY, joypadConfig.deadZone.yPos);
        ini.SetLongValue(sectionName, kDeadZoneNegativeY, joypadConfig.deadZone.yNeg);
    }
}

void loadJoypadOptions(const char *controlsSection, const CSimpleIni& ini)
{
    loadJoypadConfig(ini);

    auto joypad1 = ini.GetValue(controlsSection, kJoypad1Key);
    m_joy1GuidStr = joypad1 ? joypad1 : "";

    auto joypad2 = ini.GetValue(controlsSection, kJoypad2Key);
    m_joy2GuidStr = joypad2 ? joypad2 : "";

    m_autoConnectJoypads = ini.GetBoolValue(controlsSection, kAutoConnectJoypadsKey, true);
}

void saveJoypadOptions(const char *controlsSection, CSimpleIni& ini)
{
    saveJoypadConfig(ini);

    const auto kJoypadSaveData = {
        std::make_tuple(getPl1Controls(), m_pl1Joypad, kJoypad1Key),
        std::make_tuple(getPl2Controls(), m_pl2Joypad, kJoypad2Key),
    };

    for (const auto& joypadData : kJoypadSaveData) {
        auto controls = std::get<0>(joypadData);
        auto joypadIndex = std::get<1>(joypadData);
        auto key = std::get<2>(joypadData);

        if (controls == kJoypad) {
            char guidString[64];
            SDL_JoystickGetGUIDString(m_joypads[joypadIndex].config.guid, guidString, sizeof(guidString));
            ini.SetValue(controlsSection, key, guidString);
        }
    }

    ini.SetBoolValue(controlsSection, kAutoConnectJoypadsKey, m_autoConnectJoypads);
}

void normalizeJoypadConfig()
{
    for (auto& config : m_joypadConfig) {
        auto& deadZone = config.deadZone;

        if (deadZone.xPos < 0)
            deadZone.xPos = kDefaultDeadZoneValue;
        if (deadZone.xNeg > 0)
            deadZone.xNeg = -kDefaultDeadZoneValue;
        if (deadZone.yPos < 0)
            deadZone.yPos = kDefaultDeadZoneValue;
        if (deadZone.yNeg > 0)
            deadZone.yNeg = -kDefaultDeadZoneValue;
    }
}
