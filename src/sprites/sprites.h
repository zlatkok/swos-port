#pragma once

#include "windowManager.h"
#include "PackedSprite.h"

struct Color;
class SharedTexture;

void initSprites();
void finishSprites();
void initMatchSprites(const TeamGame *topTeam, const TeamGame *bottomTeam);
void initMenuSprites();
const PackedSprite& getSprite(int index);
SDL_Texture *getTexture(const PackedSprite& sprite);
void setMenuSpritesColor(const Color& color);
void fillPlayerSprites(SharedTexture *topTeamTextures, SharedTexture *bottomTeamTextures, int numTextures,
    const PackedSprite *sprites, int numSprites);
void fillGoalkeeperSprites(SharedTexture *topTeamTextures, SharedTexture *bottomTeamTextures, int numTextures,
    const PackedSprite *sprites, int numSprites);
void fillBenchSprites(SharedTexture *topTeamTexture, SharedTexture *bottomTeamTexture, const PackedSprite *sprites, int numSprites);

enum AdditionalSpriteIndices
{
    kExitIcon = 1334,
};
