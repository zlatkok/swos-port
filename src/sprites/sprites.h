#pragma once

#include "windowManager.h"
#include "FixedPoint.h"

struct PackedSprite;

void initSprites();
void initMenuSprites();
void initGameSprites(const TeamGame *topTeam, const TeamGame *bottomTeam);
void drawSprite(int spriteIndex, int x, int y);
void drawCharSprite(int spriteIndex, int x, int y, int color);
void copySprite(int sourceSpriteIndex, int destSpriteIndex, int xOffset, int yOffset);
std::pair<int, int> getSpriteDimensions(int spriteIndex);
void fillPlayerSprites(SDL_Texture **topTeamTextures, SDL_Texture **bottomTeamTextures, int numTextures,
    const PackedSprite *sprites, int numSprites);
void fillGoalkeeperSprites(SDL_Texture **topTeamTextures, SDL_Texture **bottomTeamTextures, int numTextures,
    const PackedSprite *sprites, int numSprites);
void fillBenchSprites(SDL_Texture *topTeamTexture, SDL_Texture *bottomTeamTexture, const PackedSprite *sprites, int numSprites);

enum AdditionalSpriteIndices
{
    kBigPitchSpriteBottom = 1334,
    kExitIcon = 1335,
};
