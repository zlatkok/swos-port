#pragma once

enum class Direction : int16_t
{
    kNoDirection = -1,
    kTop = 0,   // put top first or intellisense shows 0 as lowest direction
    kLowestDirection = 0,
    kTopRight,
    kRight,
    kBottomRight,
    kBottom,
    kBottomLeft,
    kLeft,
    kTopLeft,
    kNumDirections,
};

static_assert(static_cast<int>(Direction::kNumDirections) == 8);

// Direction masks retain their original byte representation. This alias makes
// the distinction from a single Direction explicit without affecting the ABI.
using DirectionMask = uint8_t;

template<typename... Directions>
constexpr DirectionMask makeDirectionMask(Directions... directions)
{
    return static_cast<DirectionMask>(((DirectionMask{1} << static_cast<unsigned>(directions)) | ...));
}

int operator-(Direction lhs, Direction rhs)
{
    return static_cast<int>(lhs) - static_cast<int>(rhs);
}

constexpr bool isUpwardFacing(Direction direction)
{
    return direction == Direction::kTop || direction == Direction::kTopLeft || direction == Direction::kTopRight;
}

constexpr bool isDownwardFacing(Direction direction)
{
    return direction == Direction::kBottom || direction == Direction::kBottomLeft || direction == Direction::kBottomRight;
}
