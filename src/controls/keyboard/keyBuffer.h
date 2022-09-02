#pragma once

void registerKey(SDL_Scancode scancode, SDL_Keycode keycode);
SDL_Scancode getKey();
std::pair<SDL_Scancode, SDL_Keycode> getExtendedKey();
std::pair<SDL_Scancode, SDL_Keymod> getKeyAndModifier();
size_t numKeysInBuffer();
void flushKeyBuffer();
bool isLastKeyPressed(SDL_Scancode scancode);
