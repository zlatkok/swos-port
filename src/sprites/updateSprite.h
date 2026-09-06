#pragma once

struct DeltasAndAngle
{
    FixedPoint deltaX = 0;
    FixedPoint deltaY = 0;
    int direction = -1;
};

DeltasAndAngle calculateDeltaXAndY(int speed, int x, int y, int destX, int destY);
void updateSpriteDirectionAndDeltas(Sprite& sprite);
void updateSpriteAnimation(Sprite& sprite);
void movePlayers();
void moveSprite(Sprite& sprite);
