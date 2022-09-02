#pragma once

#include "gameControlEvents.h"

using DefaultKeySet = std::array<SDL_Scancode, kNumDefaultGameControlEvents>;

class KeyConfig
{
public:
    KeyConfig(const DefaultKeySet& scancodes);
    GameControlEvents events() const;
    bool conflicts(const KeyConfig& other) const;
    bool has(SDL_Scancode scancode) const;
    bool hasMinimumEvents() const;
    void setFromKeySet(const DefaultKeySet& scancodes);
    void load(const CSimpleIni& ini, const char *section, const DefaultKeySet& defaultKeys);
    void save(CSimpleIni& ini, const char *section, const DefaultKeySet& defaultKeys);

    struct KeyBinding {
        KeyBinding(SDL_Scancode key, GameControlEvents events) : key(key), events(events) {}
        SDL_Scancode key;
        GameControlEvents events;
    };
    using KeyBindings = std::vector<KeyBinding>;

    const KeyBindings& getBindings() const;
    const KeyBinding& getBindingAt(size_t index) const;
    void addBinding(SDL_Scancode key, GameControlEvents events);
    void updateBindingEventsAt(size_t index, GameControlEvents events);
    void setDefaultKeyPack(const DefaultScancodesPack& scancodes);
    void deleteBindingAt(size_t index);

private:
    void log(const char *section) const;

    KeyBindings m_bindings;
};
