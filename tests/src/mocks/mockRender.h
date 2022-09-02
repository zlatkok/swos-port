#pragma once

using UpdateHook = std::function<void ()>;
int addUpdateHook(UpdateHook updateHook);
void removeUpdateHook(int hookId);

SDL_Surface *getScreenSurface();
void updateRenderTarget(int width, int height);
