#pragma once

struct PackedSprite
{
    // float coordinates are in VGA coordinate space
    SDL_Rect frame;
    int16_t xOffset;
    int16_t yOffset;
    float xOffsetF;
    float yOffsetF;
    float widthF;
    float heightF;
    float centerXF;
    float centerYF;
    int16_t centerX;
    int16_t centerY;
    int16_t originalWidth;
    int16_t originalHeight;
    int8_t texture;
    bool rotated;

    float width() const {
        return rotated ? heightF : widthF;
    }
    float height() const {
        return rotated ? widthF : heightF;
    }
};
