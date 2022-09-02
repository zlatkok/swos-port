#pragma once

struct PackedSprite;

using FacesArray = std::array<int, kNumFaces>;

void initSpriteColorizer(int res);
void clearMatchSpriteCache();
void colorizeGameSprites(int res, const TeamGame *topTeam, const TeamGame *bottomTeam);
int getGoalkeeperIndexFromFace(bool topTeam, int face);
SDL_Surface *loadSurface(const char *filename);
FacesArray faceTypesInTeam(const TeamGame *team, bool forGame);
void copyShirtPixels(int baseColor, int stripesColor, const PackedSprite& back, const PackedSprite& shirt,
    const SDL_Surface *backSurface, const SDL_Surface *shirtSurface);
