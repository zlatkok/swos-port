#pragma once

enum class PlayerDirection : uint8_t
{
    kUp,
    kUpRight,
    kRight,
    kDownRight,
    kDown,
    kDownLeft,
    kLeft,
    kUpLeft,
};

template<typename... Directions>
constexpr uint8_t allowPlayerDirections(Directions... directions)
{
    return static_cast<uint8_t>(((uint8_t{1} << static_cast<unsigned>(directions)) | ...));
}
