#pragma once

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    bool operator!=(const Color& other) const {
        return r != other.r || g != other.g || b != other.b;
    }
};

struct ColorF {
    float r;
    float g;
    float b;
};

extern const std::array<Color, 16> kMenuPalette;
extern const std::array<Color, 16> kTeamPalette;
extern const std::array<Color, 16> kGamePalette;

enum GamePalette
{
    kGameColorGray = 1,
    kGameColorWhite = 2,
    kGameColorBlack = 3,
    kGameColorGreenishBlack = 8,
    kGameColorRed = 10,
    kGameColorBlue = 11,
    kGameColorGreen = 14,
    kGameColorYellow = 15,
};

using ColorPerFace = std::array<Color, kNumFaces>;

static constexpr ColorPerFace kSkinColor = {{
    { 252, 108, 0 }, { 252, 108, 0 }, { 54, 18, 0 },
}};
static constexpr ColorPerFace kHairColor = {{
    { 60, 60, 60 }, { 180, 72, 0 }, { 60, 60, 60 },
}};
