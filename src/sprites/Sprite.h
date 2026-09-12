#pragma once

#include "animation.h"
#include "direction.h"

enum class PlayerState : uint8_t
{
    kNormal = 0,
    kTackling = 1,
    kTackled = 3,
    kGoalieCatchingBall = 4,
    kThrowIn = 5,
    kGoalieDivingHigh = 6,
    kGoalieDivingLow = 7,
    kStaticHeader = 8,
    kJumpHeader = 9,
    kDown = 10,
    kGoalieClaimed = 11,
    kBooked = 12,
    kInjured = 13,
    kSad = 14,
    kHappy = 15,
    kUnknown = 255,
};

enum class DestinationState : uint16_t
{
    kNotSet = 0,
    kStarting = 1,
    kTraveling = 2,
    kReached = 3,
};

enum class TackleState : uint16_t {
    kStarted = 0,
    kTackling = 1,
    kGoodTackle = 2,
};

#pragma pack(push, 1)
struct Sprite
{
    uint16_t teamNumber;    // 1 or 2 for player controls, 0 for CPU
    uint16_t playerOrdinal; // 1-11 for players, 0 for other sprites
    uint16_t frameOffset;
    int16_t animTable;
    int16_t tag01;
    Direction startingDirection;
    PlayerState state;
    int8_t playerDownTimer;
    uint16_t unk001;
    uint16_t unk002;
    int16_t frameIndicesTable;
    int16_t tag02;
    int16_t frameIndex;
    uint16_t frameDelay;
    uint16_t cycleFramesTimer;
    int16_t frameSwitchCounter;
    FixedPoint x;
    FixedPoint y;
    FixedPoint z;
    Direction direction;
    int16_t speed;          // signed Q7.9 magnitude; 512 represents 1 pixel/tick before PC scaling
    FixedPoint deltaX;
    FixedPoint deltaY;
    FixedPoint deltaZ;
    int16_t destX;
    int16_t destY;
    uint8_t unk003[6];
    uint16_t visible;       // skip it when rendering if false
    int16_t imageIndex;     // <0 if none
    uint16_t saveSprite;
    uint32_t ballDistance;
    uint16_t unk004;
    uint16_t unk005;
    uint16_t fullDirection;
    uint16_t onScreen;
    uint16_t unk006;
    uint16_t unk007;
    uint16_t unk008;
    int16_t playerDirection;
    uint16_t isMoving;
    TackleState tackleState;
    uint16_t isHeadingBall;
    DestinationState destReachedState;
    int16_t cards;
    uint16_t injuryLevel;
    uint16_t tacklingTimer;
    uint16_t sentAway;

    void init() {
        constexpr int offset = offsetof(Sprite, frameOffset);
        memset(reinterpret_cast<char *>(this) + offset, 0, sizeof(*this) - offset);
        animTable = kInvalidAnimationTableOffset;
        frameIndicesTable = kInvalidAnimationTableOffset;
        // this is for SWOS++/tests compatibility
        playerDirection = playerOrdinal ? 0 : -1;
        state = PlayerState::kUnknown;
        frameIndex = -1;
        frameDelay = 5;
        cycleFramesTimer = 1;
        // SWOS initial value for frameSwitchCoutner is 0, but we will initialize
        // with -1 to be compatible with SWOS++ and tests
        frameSwitchCounter = -1;
        unk003[4] = 1;
        clearImage();
        onScreen = 1;
        show();
    }

    void hide() {
        visible = 0;
    }
    void show() {
        visible = 1;
    }
    bool hasImage() const {
        return imageIndex >= 0;
    }
    bool hasNoImage() const {
        return imageIndex < 0;
    }
    bool stationary() const {
        return !deltaX && !deltaY;
    }
    void setImage(int index) {
        imageIndex = index;
    }
    void clearImage() {
        imageIndex = -1;
    }
    bool inNormalState() const {
        return state == PlayerState::kNormal;
    }
    void setToNormalState() {
        state = PlayerState::kNormal;
    }
    bool isGoalkeeper() const {
        return playerOrdinal == 1;
    }
};
#pragma pack(pop)

// Sprite planar speed is signed Q7.9, where 1.0 represents one pixel per tick.
constexpr int16_t operator""_speed(long double value)
{
    return static_cast<int16_t>(value * 512.0L);
}
