#pragma once

void initGameSprites(const TeamGame *topTeam, const TeamGame *bottomTeam);
void initDisplaySprites();
void initializePlayerSpriteFrameIndices();
void drawSprites(float xOffset, float yOffset);
int getGoalkeeperSpriteOffset(bool topTeam, int face);
int getPlayerSpriteOffsetFromFace(int face);
void updateCornerFlags();

#ifdef SWOS_TEST
Sprite *indexToSprite(unsigned index);
#endif
