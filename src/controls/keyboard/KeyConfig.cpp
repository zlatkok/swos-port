#include "KeyConfig.h"
#include "text.h"
#include "scancodes.h"

KeyConfig::KeyConfig(const DefaultKeySet& scancodes)
{
    setFromKeySet(scancodes);
}

GameControlEvents KeyConfig::events() const
{
    auto events = kNoGameEvents;
    auto keyMap = SDL_GetKeyboardState(nullptr);

    for (const auto& binding : m_bindings)
        if (keyMap[binding.key])
            events |= binding.events;

    return events;
}

bool KeyConfig::conflicts(const KeyConfig& other) const
{
    bool keyMap[SDL_NUM_SCANCODES] = { false };

    for (const auto& binding : m_bindings)
        keyMap[binding.key] = true;

    for (const auto& binding : other.m_bindings)
        if (keyMap[binding.key])
            return true;

    return false;
}

bool KeyConfig::has(SDL_Scancode scancode) const
{
    auto it = std::find_if(m_bindings.begin(), m_bindings.end(), [scancode](const auto& binding) {
        return binding.key == scancode;
    });

    return it != m_bindings.end();
}

bool KeyConfig::hasMinimumEvents() const
{
    auto events = kNoGameEvents;
    for (const auto& binding : m_bindings)
        events |= binding.events;

    return (events & kMinimumGameEventsMask) == kMinimumGameEventsMask;
}

void KeyConfig::setFromKeySet(const DefaultKeySet& scancodes)
{
    m_bindings.clear();

    for (size_t i = 0; i < scancodes.size(); i++) {
        assert(scancodes[i] != SDL_SCANCODE_UNKNOWN);
        m_bindings.emplace_back(scancodes[i], kDefaultGameControlEvents[i]);
    }
}

void KeyConfig::load(const CSimpleIni& ini, const char *section, const DefaultKeySet& defaultKeys)
{
    CSimpleIni::TNamesDepend keys;
    ini.GetAllKeys(section, keys);

    if (!keys.empty()) {
        bool keysTaken[SDL_NUM_SCANCODES] = { false };

        m_bindings.clear();

        for (const auto& iniKey : keys) {
            auto intEvents = ini.GetLongValue(section, iniKey.pItem);
            auto keyValue = SDL_atoi(iniKey.pItem);

            GameControlEvents events;

            if (keyValue <= 0 || keyValue >= SDL_NUM_SCANCODES) {
                logWarn("Out of range key value (%d), events: %#x", keyValue, intEvents);
            } else if (!convertEvents(events, intEvents)) {
                logWarn("Out of range events value (%#x) for key %d", intEvents, keyValue);
            } else if (events == kNoGameEvents) {
                logWarn("Empty event encountered for key %d", keyValue);
            } else {
                auto key = static_cast<SDL_Scancode>(keyValue);

                if (keysTaken[keyValue])
                    logWarn("Duplicate key value found (%d), events: %#x", keyValue, intEvents);
                else
                    m_bindings.emplace_back(key, events);

                keysTaken[key] = true;
            }
        }

        log(section);
    }

    if (m_bindings.empty()) {
        for (size_t i = 0; i < defaultKeys.size(); i++)
            m_bindings.emplace_back(defaultKeys[i], kDefaultGameControlEvents[i]);

        log("default set");
    }
}

void KeyConfig::save(CSimpleIni& ini, const char *section, const DefaultKeySet& defaultKeys)
{
    if (m_bindings.size() == defaultKeys.size() && std::equal(m_bindings.begin(), m_bindings.end(), defaultKeys.begin(),
        [](const auto& binding, auto key) { return binding.key == key; }))
        return;

    ini.SetValue(section, nullptr, nullptr, "; SDL key codes are on the left side");

    for (const auto& binding : m_bindings) {
        char keyBuf[32];
        SDL_itoa(binding.key, keyBuf, 10);

        char commentBuf[64] = "; ";
        scancodeToString(binding.key, commentBuf + 2, sizeof(commentBuf) - 2);

        ini.SetLongValue(section, keyBuf, binding.events, commentBuf, true);
    }
}

void KeyConfig::log(const char *section) const
{
    if (m_bindings.empty())
        return;

    std::string keys;
    keys.reserve(512);

    keys = "Loaded keys from [";
    keys += section;
    keys += "]: ";

    char buf[128];

    for (const auto& binding : m_bindings) {
        snprintf(buf, sizeof(buf), "%d (%s) -> %s, ", binding.key, scancodeToString(binding.key),
            gameControlEventToString(binding.events).first);
        keys += buf;
    }

    if (keys[keys.size() - 2] == ',')
        keys.erase(keys.end() - 2);

    logInfo("%s", keys.c_str());
}

auto KeyConfig::getBindings() const -> const std::vector<KeyBinding>&
{
    return m_bindings;
}

auto KeyConfig::getBindingAt(size_t index) const -> const KeyBinding&
{
    return m_bindings[index];
}

void KeyConfig::addBinding(SDL_Scancode key, GameControlEvents events)
{
    auto it = std::find_if(m_bindings.begin(), m_bindings.end(), [key](const auto& binding) {
        return binding.key == key;
    });
    assert(it == m_bindings.end());

    if (it == m_bindings.end())
        m_bindings.emplace_back(key, events);
}

void KeyConfig::updateBindingEventsAt(size_t index, GameControlEvents events)
{
    m_bindings[index].events = events;
}

void KeyConfig::setDefaultKeyPack(const DefaultScancodesPack& scancodes)
{
    m_bindings.clear();

    for (int i = 0; i < kNumDefaultGameControlEvents; i++)
        m_bindings.emplace_back(scancodes[i], kDefaultGameControlEvents[i]);
}

void KeyConfig::deleteBindingAt(size_t index)
{
    assert(index < m_bindings.size());

    m_bindings.erase(m_bindings.begin() + index);
}
