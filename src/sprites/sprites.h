#pragma once

#include "windowManager.h"
#include "PackedSprite.h"

struct Color;

void initSprites();
void initMatchSprites(const TeamGame *topTeam, const TeamGame *bottomTeam);
void initMenuSprites();
const PackedSprite& getSprite(int index);
SDL_Texture *getTexture(const PackedSprite& sprite);
void setMenuSpritesColor(const Color& color);
void fillPlayerSprites(SDL_Texture **topTeamTextures, SDL_Texture **bottomTeamTextures, int numTextures,
    const PackedSprite *sprites, int numSprites);
void fillGoalkeeperSprites(SDL_Texture **topTeamTextures, SDL_Texture **bottomTeamTextures, int numTextures,
    const PackedSprite *sprites, int numSprites);
void fillBenchSprites(SDL_Texture *topTeamTexture, SDL_Texture *bottomTeamTexture, const PackedSprite *sprites, int numSprites);

enum AdditionalSpriteIndices
{
    kExitIcon = 1334,
};
