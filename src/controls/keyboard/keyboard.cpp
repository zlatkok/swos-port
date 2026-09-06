#include "keyboard.h"

const DefaultKeySet kDefaultKeys1 = {
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_RCTRL, SDL_SCANCODE_RSHIFT
};

const DefaultKeySet kDefaultKeys2 = {
    SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_GRAVE
};

static KeyConfig createDefaultKeyboard1Config()
{
    KeyConfig config(kDefaultKeys1);
    config.addBinding(SDL_SCANCODE_LCTRL, kGameEventKick);
    return config;
}

static KeyConfig m_config1(createDefaultKeyboard1Config());
static KeyConfig m_config2(kDefaultKeys2);

static const char kKeyboard1Section[] = "keyboard1";
static const char kKeyboard2Section[] = "keyboard2";

bool keyboardPresent()
{
#ifdef __ANDROID__
    bool result = false;
    if (auto env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv())) {
        if (auto swosClass = env->FindClass("com/sensible_portware/SWOS")) {
            if (auto keyboardPresentMethodId = env->GetStaticMethodID(swosClass, "keyboardPresent", "()Z"))
                result = env->CallStaticBooleanMethod(swosClass, keyboardPresentMethodId) != 0;
            env->DeleteLocalRef(swosClass);
        }
    }
    return result;
#else
    return true;
#endif
}

GameControlEvents eventsFromAllKeysets()
{
    return m_config1.events() | m_config2.events();
}

GameControlEvents keyboard1Events()
{
    return m_config1.events();
}

GameControlEvents keyboard2Events()
{
    return m_config2.events();
}

GameControlEvents keyboardEvents(Keyboard keyboard)
{
    assert(keyboard == Keyboard::kSet1 || keyboard == Keyboard::kSet2);

    return keyboard == Keyboard::kSet1 ? keyboard1Events() : keyboard2Events();
}

bool keyboard1HasScancode(SDL_Scancode scancode)
{
    return m_config1.has(scancode);
}

bool keyboard2HasScancode(SDL_Scancode scancode)
{
    return m_config2.has(scancode);
}

static KeyConfig& getKeyConfig(Keyboard keyboard)
{
    return keyboard == Keyboard::kSet1 ? m_config1 : m_config2;
}

bool keyboardHasScancode(Keyboard keyboard, SDL_Scancode key)
{
    return getKeyConfig(keyboard).has(key);
}

bool keyboard1HasBasicBindings()
{
    return m_config1.hasMinimumEvents();
}

bool keyboard2HasBasicBindings()
{
    return m_config2.hasMinimumEvents();
}

bool keyboardHasBasicBindings(Keyboard keyboard)
{
    return keyboard == Keyboard::kSet1 ? keyboard1HasBasicBindings() : keyboard2HasBasicBindings();
}

const KeyConfig::KeyBindings& getKeyboardKeyBindings(Keyboard keyboard)
{
    return getKeyConfig(keyboard).getBindings();
}

const KeyConfig::KeyBinding& getKeyboardKeyBindingAt(Keyboard keyboard, size_t index)
{
    return getKeyConfig(keyboard).getBindingAt(index);
}

void addKeyboardKeyBinding(Keyboard keyboard, SDL_Scancode key, GameControlEvents events)
{
    getKeyConfig(keyboard).addBinding(key, events);
}

void updateKeyboardKeyBindingEventsAt(Keyboard keyboard, size_t index, GameControlEvents events)
{
    getKeyConfig(keyboard).updateBindingEventsAt(index, events);
}

void setDefaultKeyPackForKeyboard(Keyboard keyboard, const DefaultScancodesPack& scancodes)
{
    getKeyConfig(keyboard).setDefaultKeyPack(scancodes);
}

void deleteKeyBindingAt(Keyboard keyboard, size_t index)
{
    auto& config = getKeyConfig(keyboard);
    config.deleteBindingAt(index);
}

void loadKeyboardConfig(const CSimpleIni& ini)
{
    m_config1.load(ini, kKeyboard1Section, kDefaultKeys1);
    m_config2.load(ini, kKeyboard2Section, kDefaultKeys2);
}

void saveKeyboardConfig(CSimpleIni& ini)
{
    m_config1.save(ini, kKeyboard1Section, kDefaultKeys1);
    m_config2.save(ini, kKeyboard2Section, kDefaultKeys2);
}
