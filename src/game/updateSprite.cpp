#include "updateSprite.h"
#include "amigaMode.h"

// Values are: round(32 * y / x), which is effectively 32 * tangent of an acute angle in a right triangle
// whose adjacent side length is x and opposite side is y
// It divides 90 degrees into positions 0..64
// x - horizontal axis (column)
// y - vertical axis (row)
// for x >= y: kAngleTangent[y][x] = round(32 * x / y)
// for x < y:  kAngleTangent[y][x] = 64 - kAngleTangent[x][y]
static const int8_t kAngleTangent[32][32] = {
    -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    64, 32, 16, 11,  8,  6,  5,  5,  4,  4,  3,  3,  3,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    64, 48, 32, 21, 16, 13, 11,  9,  8,  7,  6,  6,  5,  5,  5,  4,  4,  4,  4,  3,  3,  3,  3,  3,  3,  3,  2,  2,  2,  2,  2,  2,
    64, 53, 43, 32, 24, 19, 16, 14, 12, 11, 10,  9,  8,  7,  7,  6,  6,  6,  5,  5,  5,  5,  4,  4,  4,  4,  4,  4,  3,  3,  3,  3,
    64, 56, 48, 40, 32, 26, 21, 18, 16, 14, 13, 12, 11, 10,  9,  9,  8,  8,  7,  7,  6,  6,  6,  6,  5,  5,  5,  5,  5,  4,  4,  4,
    64, 58, 51, 45, 38, 32, 27, 23, 20, 18, 16, 15, 13, 12, 11, 11, 10,  9,  9,  8,  8,  8,  7,  7,  7,  6,  6,  6,  6,  6,  5,  5,
    64, 59, 53, 48, 43, 37, 32, 27, 24, 21, 19, 17, 16, 15, 14, 13, 12, 11, 11, 10, 10,  9,  9,  8,  8,  8,  7,  7,  7,  7,  6,  6,
    64, 59, 55, 50, 46, 41, 37, 32, 28, 25, 22, 20, 19, 17, 16, 15, 14, 13, 12, 12, 11, 11, 10, 10,  9,  9,  9,  8,  8,  8,  7,  7,
    64, 60, 56, 52, 48, 44, 40, 36, 32, 28, 26, 23, 21, 20, 18, 17, 16, 15, 14, 13, 13, 12, 12, 11, 11, 10, 10,  9,  9,  9,  9,  8,
    64, 60, 57, 53, 50, 46, 43, 39, 36, 32, 29, 26, 24, 22, 21, 19, 18, 17, 16, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 10,  9,
    64, 61, 58, 54, 51, 48, 45, 42, 38, 35, 32, 29, 27, 25, 23, 21, 20, 19, 18, 17, 16, 15, 15, 14, 13, 13, 12, 12, 11, 11, 11, 10,
    64, 61, 58, 55, 52, 49, 47, 44, 41, 38, 35, 32, 29, 27, 25, 23, 22, 21, 20, 19, 18, 17, 16, 15, 15, 14, 14, 13, 13, 12, 12, 11,
    64, 61, 59, 56, 53, 51, 48, 45, 43, 40, 37, 35, 32, 30, 27, 26, 24, 23, 21, 20, 19, 18, 17, 17, 16, 15, 15, 14, 14, 13, 13, 12,
    64, 62, 59, 57, 54, 52, 49, 47, 44, 42, 39, 37, 34, 32, 30, 28, 26, 24, 23, 22, 21, 20, 19, 18, 17, 17, 16, 15, 15, 14, 14, 13,
    64, 62, 59, 57, 55, 53, 50, 48, 46, 43, 41, 39, 37, 34, 32, 30, 28, 26, 25, 24, 22, 21, 20, 19, 19, 18, 17, 17, 16, 15, 15, 14,
    64, 62, 60, 58, 55, 53, 51, 49, 47, 45, 43, 41, 38, 36, 34, 32, 30, 28, 27, 25, 24, 23, 22, 21, 20, 19, 18, 18, 17, 17, 16, 15,
    64, 62, 60, 58, 56, 54, 52, 50, 48, 46, 44, 42, 40, 38, 36, 34, 32, 30, 28, 27, 26, 24, 23, 22, 21, 20, 20, 19, 18, 18, 17, 17,
    64, 62, 60, 58, 56, 55, 53, 51, 49, 47, 45, 43, 41, 40, 38, 36, 34, 32, 30, 29, 27, 26, 25, 24, 23, 22, 21, 20, 19, 19, 18, 18,
    64, 62, 60, 59, 57, 55, 53, 52, 50, 48, 46, 44, 43, 41, 39, 37, 36, 34, 32, 30, 29, 27, 26, 25, 24, 23, 22, 21, 21, 20, 19, 19,
    64, 62, 61, 59, 57, 56, 54, 52, 51, 49, 47, 45, 44, 42, 40, 39, 37, 35, 34, 32, 30, 29, 28, 26, 25, 24, 23, 23, 22, 21, 20, 20,
    64, 62, 61, 59, 58, 56, 54, 53, 51, 50, 48, 46, 45, 43, 42, 40, 38, 37, 35, 34, 32, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 21,
    64, 62, 61, 59, 58, 56, 55, 53, 52, 50, 49, 47, 46, 44, 43, 41, 40, 38, 37, 35, 34, 32, 31, 29, 28, 27, 26, 25, 24, 23, 22, 22,
    64, 63, 61, 60, 58, 57, 55, 54, 52, 51, 49, 48, 47, 45, 44, 42, 41, 39, 38, 36, 35, 33, 32, 31, 29, 28, 27, 26, 25, 24, 23, 23,
    64, 63, 61, 60, 58, 57, 56, 54, 53, 51, 50, 49, 47, 46, 45, 43, 42, 40, 39, 38, 36, 35, 33, 32, 31, 29, 28, 27, 26, 25, 25, 24,
    64, 63, 61, 60, 59, 57, 56, 55, 53, 52, 51, 49, 48, 47, 45, 44, 43, 41, 40, 39, 37, 36, 35, 33, 32, 31, 30, 28, 27, 26, 26, 25,
    64, 63, 61, 60, 59, 58, 56, 55, 54, 52, 51, 50, 49, 47, 46, 45, 44, 42, 41, 40, 38, 37, 36, 35, 33, 32, 31, 30, 29, 28, 27, 26,
    64, 63, 62, 60, 59, 58, 57, 55, 54, 53, 52, 50, 49, 48, 47, 46, 44, 43, 42, 41, 39, 38, 37, 36, 34, 33, 32, 31, 30, 29, 28, 27,
    64, 63, 62, 60, 59, 58, 57, 56, 55, 53, 52, 51, 50, 49, 47, 46, 45, 44, 43, 41, 40, 39, 38, 37, 36, 34, 33, 32, 31, 30, 29, 28,
    64, 63, 62, 61, 59, 58, 57, 56, 55, 54, 53, 51, 50, 49, 48, 47, 46, 45, 43, 42, 41, 40, 39, 38, 37, 35, 34, 33, 32, 31, 30, 29,
    64, 63, 62, 61, 60, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 36, 35, 34, 33, 32, 31, 30,
    64, 63, 62, 61, 60, 59, 58, 57, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 39, 38, 37, 36, 35, 34, 33, 32, 31,
    64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32,
};

// Input is the direction on the circle 0..255 gotten from the table above, output is sine of this angle,
// in SWOS fixed point format [0..1).
// approximately 32,767 * sin(x / 256 * 360), in degrees
// or 32,767 * sin(x / 256 * 2*pi) in radians
// it has -128..127 range after discarding high byte
static const std::array<int16_t, 256> kSineCosineTable = {
         0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
      6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
     12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
     18204,  18867,  19519,  20159,  20787,  21402,  22005,  22594,
     23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
     27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
     30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
     32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
     32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
     32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
     30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
     27245,  26790,  26319,  25832,  25329,  24812,  24279,  23731,
     23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
     18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
     12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
      6393,   5602,   4808,   4011,   3212,   2411,   1608,    804,
         0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
     -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
    -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24812, -25329, -25832, -26319, -26790,
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
    -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
    -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21402, -20787, -20159, -19519, -18867,
    -18204, -17530, -16845, -16151, -15446, -14732, -14009, -13278,
    -12539, -11792, -11039, -10278,  -9512,  -8739,  -7961,  -7179,
     -6392,  -5602,  -4808,  -4011,  -3211,  -2410,  -1608,   -804,
};

struct DeltasAndAngle {
    FixedPoint deltaX = 0;
    FixedPoint deltaY = 0;
    int direction = -1;
};

static DeltasAndAngle calculateDeltaXAndY(int speed, int destX, int destY, int x, int y);

void updateSpriteDirectionAndDeltas(Sprite& sprite)
{
    const auto& result = calculateDeltaXAndY(sprite.speed, sprite.destX, sprite.destY, sprite.x.rounded(), sprite.y.rounded());
    sprite.deltaX = result.deltaX;
    sprite.deltaY = result.deltaY;
    sprite.fullDirection = result.direction;
    sprite.direction = ((result.direction + 16) & 0xff) >> 5;
}

static DeltasAndAngle calculateDeltaXAndY(int speed, int destX, int destY, int x, int y)
{
    bool xNegative = false;
    int deltaX = destX - x;
    if (deltaX < 0) {
        xNegative = true;
        deltaX = -deltaX;
    }

    bool yNegative = false;
    int deltaY = destY - y;
    if (deltaY < 0) {
        yNegative = true;
        deltaY = -deltaY;
    }

    while (deltaX >= 32 || deltaY >= 32) {
        deltaX /= 2;
        deltaY /= 2;
    }

    int angle = kAngleTangent[deltaY][deltaX];
    assert(angle >= -1 && angle <= 64);

    // What we get is a value representing the angle in the 1st quadrant. Abscissa (x coordinate) goes to
    // the right, and ordinate (y coordinate) to bottom. Values start from 0 and increase clockwise.
    //     +----------> x 0
    //     |\
    //     |   \
    //     |      \
    //     |        32
    //     v
    //     y 64

    DeltasAndAngle result;

    if (angle >= 0) {
        // Transform the value to represent full circle,
        // range [0..255], 0 is down, increases counter-clockwise.
        // This is to match the way sine/cosine table was built.
        //               128
        //         160    |     96
        //            \   |   /
        //              \ | /
        //      192 ------+------ 64
        //              / | \
        //            /   |   \
        //          224   |     32
        //                0
        if (xNegative) {
            if (yNegative)
                angle = 192 - angle;
            else
                angle += 192;
        } else {
            if (yNegative)
                angle += 64;
            else
                angle = 64 - angle;
        }
        angle &= 0xff;

        // Finally bring the angle to the range used in the game, [0..255], 0 = top, increases clockwise:
        //                0
        //         224    |     32
        //            \   |   /
        //              \ | /
        //      192 ------+------ 64
        //              / | \
        //            /   |   \
        //          160   |     96
        //               128
        //
        result.direction = (256 - angle + 128) & 0xff;

        // angle is set up to match the cosine index, add 64 (quarter of the circle) for sine
        int cos = kSineCosineTable[angle];
        int sin = kSineCosineTable[(angle + 64) & 0xff];

        assert(sin >= -32'768 && sin <= 32'767 && cos >= -32'768 && cos <= 32'767);
        assert(speed < 65'538);

        // by doing multiplication first, and shift later we could get more precision
        // but left as is, to comply with the original code
        sin = (sin >> 8) * speed;
        cos = (cos >> 8) * speed;

        if (amigaModeActive()) {
            result.deltaX.setRaw(cos);
            result.deltaY.setRaw(sin);
        } else {
            constexpr int kDampenFactorNumerator = 41;
            constexpr int kDampenFactorDenominator = 64;

            if (cos >= 0)
                result.deltaX.setRaw((cos * kDampenFactorNumerator + kDampenFactorDenominator / 2) / kDampenFactorDenominator);
            else
                result.deltaX.setRaw((cos * kDampenFactorNumerator - kDampenFactorDenominator / 2) / kDampenFactorDenominator);
            if (sin >= 0)
                result.deltaY.setRaw((sin * kDampenFactorNumerator + kDampenFactorDenominator / 2) / kDampenFactorDenominator);
            else
                result.deltaY.setRaw((sin * kDampenFactorNumerator - kDampenFactorDenominator / 2) / kDampenFactorDenominator);
        }
    }

    return result;
}

// in:
//      D0 - speed
//      D1 - destination x
//      D2 - destination y
//      D3 - current x
//      D4 - current y
//      (all inputs are 16-bit integers)
// out:
//      D0 - angle (0..255) (0 is 270 degrees, start of trigonometric circle is 64)
//           -1 if no movement
//      D1 - delta x (32-bit fixed point)
//      D2 - delta y (32-bit fixed point)
//      sign flag: set = no movement, clear = there is movement
//
// Delta x and y are numbers of pixels that sprite needs to be moved in one cycle to reach
// destination coordinates.
//
void SWOS::CalculateDeltaXAndY()
{
    int speed = D0.asInt16();
    int destX = D1.asInt16();
    int destY = D2.asInt16();
    int x = D3.asInt16();
    int y = D4.asInt16();
    const auto& result = calculateDeltaXAndY(speed, destX, destY, x, y);
    D0.lo16 = result.direction;
    D1 = result.deltaX.raw();
    D2 = result.deltaY.raw();
    SwosVM::flags.sign = result.direction < 0;
}
