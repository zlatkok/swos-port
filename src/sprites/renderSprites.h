#pragma once

void drawMenuSprite(int spriteIndex, int x, int y);
void drawGameSprite(int spriteIndex, int x, int y);
void drawCharSprite(int spriteIndex, int x, int y, int alpha = 255);
bool drawSprite(int pictureIndex, float x, float y, bool applyZoom, float xOffset, float yOffset,
    bool ignoreCenter = false, int alpha = 255);
