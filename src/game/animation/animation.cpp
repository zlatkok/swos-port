#include "animation.h"
#include "animationTables.cpp"

int16_t AnimationTable::getFrameTableOffset(int direction, int group /* = 0 */) const
{
    assert(direction >= 0 && direction < 8);
    assert(group >= 0 && group < 4);

    if (direction < 0 || direction >= 8 || group < 0 || group >= 4)
        return kInvalidFrameTableOffset;

    int packedGroup = 0;
    if (flags & kIsRefereeAnimation) {
        assert(group == 0);
        if (group != 0)
            return kInvalidFrameTableOffset;
    } else {
        assert(flags & (kHasGoalkeeper1Frames | kHasGoalkeeper2Frames | kHasTeam1Frames | kHasTeam2Frames));
        const auto groupFlag = 1 << group;
        assert(flags & groupFlag);
        if (!(flags & groupFlag))
            return kInvalidFrameTableOffset;

        for (int precedingGroup = 0; precedingGroup < group; precedingGroup++)
            if (flags & (1 << precedingGroup))
                packedGroup++;
    }

    const auto offset = getFrameTableOffsets()[direction + packedGroup * 8];
    assert(isValidFrameTableOffset(offset));
    return offset;
}

static void setPlayerAnimationTable(Sprite& player, int16_t animationTableOffset, bool setPictureIndex)
{
    auto animation = getAnimationTable(animationTableOffset);
    assert(animation);
    if (!animation)
        return;

    // frame tables: goalkeeper 1, goalkeeper 2, player team 1, player team 2
    int group = 2 * (player.playerOrdinal == 1) + (player.teamNumber - 1);
    auto frameTableOffset = animation->getFrameTableOffset(player.direction, group);
    assert(frameTableOffset != kInvalidFrameTableOffset);
    if (frameTableOffset == kInvalidFrameTableOffset)
        return;

    player.animTable = animationTableOffset;
    player.frameIndicesTable = frameTableOffset;
    player.frameDelay = animation->frameDelay;
    player.startingDirection = player.direction;

    if (setPictureIndex) {
        auto frame = getFrameTableElement(frameTableOffset, player.frameIndex);
        if (frame >= 0)
            player.imageIndex = frame + player.frameOffset;
    } else {
        player.frameSwitchCounter = -1;
        player.frameIndex = -1;
        player.cycleFramesTimer = 1;
    }

}

void setPlayerAnimationTable(Sprite& player, int16_t animationTableOffset)
{
    setPlayerAnimationTable(player, animationTableOffset, false);
}

void setPlayerAnimationTableAndPictureIndex(Sprite& player, int16_t animationTableOffset)
{
    setPlayerAnimationTable(player, animationTableOffset, true);
}
