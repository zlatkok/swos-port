#include "keyBuffer.h"

struct KeyPress {
    KeyPress(SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod) :
        scancode(scancode), keycode(keycode), mod(mod) {}
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    SDL_Keycode keycode = SDLK_UNKNOWN;
    SDL_Keymod mod = KMOD_NONE;
};

static KeyPress getKeyPress();

static std::deque<KeyPress> m_keyBuffer;

void registerKey(SDL_Scancode scancode, SDL_Keycode keycode)
{
    m_keyBuffer.emplace_back(scancode, keycode, SDL_GetModState());
}

SDL_Scancode getKey()
{
    return getKeyAndModifier().first;
}

std::pair<SDL_Scancode, SDL_Keycode> getExtendedKey()
{
    auto key = getKeyPress();
    return { key.scancode, key.keycode };
}

std::pair<SDL_Scancode, SDL_Keymod> getKeyAndModifier()
{
    auto key = getKeyPress();
    return { key.scancode, key.mod };
}

size_t numKeysInBuffer()
{
    return m_keyBuffer.size();
}

void flushKeyBuffer()
{
    m_keyBuffer.clear();
}

bool isLastKeyPressed(SDL_Scancode scancode)
{
    if (m_keyBuffer.empty())
        return false;

    return m_keyBuffer.back().scancode == scancode;
}

static KeyPress getKeyPress()
{
    KeyPress key{ SDL_SCANCODE_UNKNOWN, SDLK_UNKNOWN, KMOD_NONE };

    if (!m_keyBuffer.empty()) {
        key = m_keyBuffer.front();
        m_keyBuffer.pop_front();
    }

    return key;
}
