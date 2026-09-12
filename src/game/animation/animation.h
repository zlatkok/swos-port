#pragma once

#include "direction.h"

struct Sprite;

enum AnimationTableFlags : uint8_t
{
    kHasTeam1Frames = 1 << 0,
    kHasTeam2Frames = 1 << 1,
    kHasGoalkeeper1Frames = 1 << 2,
    kHasGoalkeeper2Frames = 1 << 3,
    kIsRefereeAnimation = 1 << 4,
};

constexpr int16_t kInvalidFrameTableOffset = -1;
constexpr int16_t kInvalidAnimationTableOffset = -1;

#ifndef NDEBUG
bool isValidFrameTableOffset(int16_t offset);
#endif

#pragma pack(push, 1)
struct AnimationTable
{
    uint8_t frameDelay;
    uint8_t flags;

    int16_t getFrameTableOffset(Direction direction, int group = 0) const;

    const int16_t *getFrameTableOffsets() const
    {
        return reinterpret_cast<const int16_t *>(this + 1);
    }

    bool hasTeam1Frames() const
    {
        return flags & kHasTeam1Frames;
    }

    bool hasTeam2Frames() const
    {
        return flags & kHasTeam2Frames;
    }

    bool hasGoalkeeper1Frames() const
    {
        return flags & kHasGoalkeeper1Frames;
    }

    bool hasGoalkeeper2Frames() const
    {
        return flags & kHasGoalkeeper2Frames;
    }

    bool isRefereeAnimation() const
    {
        return flags & kIsRefereeAnimation;
    }
};
#pragma pack(pop)
static_assert(sizeof(AnimationTable) == sizeof(int16_t),
    "AnimationTable must occupy one table-data element");

const int16_t *getFrameTable(int16_t offset);
int16_t getFrameTableElement(int16_t offset, int index);
const AnimationTable *getAnimationTable(int16_t offset);

#ifdef SWOS_TEST
int16_t getOriginalSwosFrameTable(int32_t offset);
int16_t getOriginalSwosAnimationTable(int32_t offset);
#endif

// the actual animation table getters declarations
#include "animationTables.h"

void setPlayerAnimationTable(Sprite& player, int16_t animationTableOffset);
void setPlayerAnimationTableAndPictureIndex(Sprite& player, int16_t animationTableOffset);
