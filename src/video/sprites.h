#pragma once

struct SpriteClipper {
    int x;
    int y;
    int width;
    int height;
    const char *from;
    int pitch;
    bool odd;

    SpriteClipper(int spriteIndex, int x, int y);
    SpriteClipper(const SpriteGraphics *sprite, int x, int y);
    bool clip();

private:
    void init(const SpriteGraphics *sprite, int x, int y);
};

void drawTeamNameSprites(int spriteIndex, int x, int y);
void drawSprite(int spriteIndex, int x, int y, bool saveSpritePixelsFlag = true);
void drawSpriteUnclipped(SpriteClipper& c, bool saveSpritePixelsFlag = true);
void copySprite(int sourceSpriteIndex, int destSpriteIndex, int xOffset, int yOffset);
int getStringPixelLength(const char *str, bool bigText = false);
void elideString(char *str, int maxStrLen, int maxPixels, bool bigText = false);
