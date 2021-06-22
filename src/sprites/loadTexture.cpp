#include "loadTexture.h"
#include "windowManager.h"
#include "util.h"
#include "file.h"

SDL_Texture *loadTexture(const char *path)
{
    SDL_Texture *texture{};

    auto resPath = joinPaths(getAssetDir(), path);

    if (auto f = openFile(resPath.c_str()))
        texture = IMG_LoadTexture_RW(getRenderer(), f, true);
    else
        errorExit("Failed to open texture file %s", path);

    if (!texture)
        errorExit("Failed to load texture %s: %s", path, IMG_GetError());

    return texture;
}
