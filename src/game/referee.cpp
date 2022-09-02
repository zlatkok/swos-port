#include "referee.h"
#include "camera.h"
#include "random.h"
#include "gameSprites.h"
#include "comments.h"
#include "pitchConstants.h"

constexpr int kRefereeSpeed = 1'024;

constexpr int kRefereeHidingPlaceX = 276;
constexpr int kRefereeHidingPlaceY = 439;

constexpr int kRefereeLeavingTopDestY = 129;
constexpr int kRefereeLeavingBottomDestY = 770;

constexpr int kLastFrameLoopMarker = -999;
constexpr int kLastFrameHoldMarker = -101;
constexpr int kFrameLoopbackMarker = -100;

enum RefereeState
{
    kRefOffScreen = 0,
    kRefIncoming = 1,
    kRefWaitingPlayer = 2,
    kRefAboutToGiveCard = 3,
    kRefBooking = 4,
    kRefLeaving = 5,
};

enum CardHanding
{
    kNoCard = 0,
    kYellowCard = 1,
    kRedCard = 2,
    kSecondYellowCard = 3,
};

// set team number so he shows up in replays
static Sprite m_refereeSprite{ 3 };

static void updateRefereeState();
static void initRefereeAnimationTable(SwosDataPointer<void> animTable);
static void moveSprite(Sprite& sprite);
static void updateSpriteAnimation(Sprite& sprite);
static void updateSpriteDeltaAndDirection(Sprite& sprite);

void activateReferee()
{
    m_refereeSprite.destX = swos.foulXCoordinate + 28;
    m_refereeSprite.destY = swos.foulYCoordinate + 5;

    int xOffset = SWOS::rand() / 8;

    if (swos.foulXCoordinate >= kPitchCenterX)
        xOffset = -xOffset;

    int cameraY = static_cast<int>(getCameraY());
    int refStartY = cameraY - 20;

    if (swos.foulYCoordinate <= kPitchCenterY)
        refStartY = cameraY + 215;

    m_refereeSprite.x = m_refereeSprite.destX + xOffset;
    m_refereeSprite.y = refStartY;
    m_refereeSprite.speed = kRefereeSpeed;
    m_refereeSprite.show();

    initDisplaySprites();
    initRefereeAnimationTable(&swos.refComingAnimTable);

    swos.refState = kRefIncoming;
}

void SWOS::ActivateReferee()
{
    activateReferee();
}

bool refereeActive()
{
    return swos.refState != kRefOffScreen;
}

bool cardHandingInProgress()
{
    return swos.whichCard != kNoCard;
}

void updateReferee()
{
    if (m_refereeSprite.onScreen) {
        updateSpriteAnimation(m_refereeSprite);
        updateRefereeState();
    } else if (swos.gameStatePl == GameState::kInProgress) {
        moveSprite(m_refereeSprite);
    } else {
        updateRefereeState();
    }
}

Sprite *refereeSprite()
{
    return &m_refereeSprite;
}

static void removeReferee()
{
    swos.refState = kRefOffScreen;
    m_refereeSprite.hide();
    m_refereeSprite.x = kRefereeHidingPlaceX;
    m_refereeSprite.y = kRefereeHidingPlaceY;
    m_refereeSprite.z = 0;
    m_refereeSprite.speed = 0;
    m_refereeSprite.playerDownTimer = 0;
    m_refereeSprite.frameIndex = -1;
    m_refereeSprite.cycleFramesTimer = 1;
    m_refereeSprite.pictureIndex = -1;
    m_refereeSprite.direction = kFacingTop;
    m_refereeSprite.onScreen = 1;
    initRefereeAnimationTable(&swos.refWaitingAnimTable);
}

void SWOS::RemoveReferee()
{
    removeReferee();
}

static void setRefereeToLeavingState()
{
    int xOffset = SWOS::rand() / 4 - 32;    // -32..31
    int destY = swos.foulYCoordinate > kPitchCenterY ? kRefereeLeavingTopDestY : kRefereeLeavingBottomDestY;

    m_refereeSprite.destX = xOffset + m_refereeSprite.x.whole();
    m_refereeSprite.destY = destY;

    initRefereeAnimationTable(&swos.refComingAnimTable);

    swos.refState = kRefLeaving;
}

void SWOS::SetRefereeToLeavingState()
{
    setRefereeToLeavingState();
}

static void updateRefereeState()
{
    switch (swos.refState) {
    case kRefAboutToGiveCard:
        assert(swos.whichCard != kNoCard);
        swos.refState = kRefBooking;

        switch (swos.whichCard) {
        case kRedCard:
            initRefereeAnimationTable(&swos.refRedCardAnimTable);
            enqueueRedCardSample();
            break;
        case kYellowCard:
            initRefereeAnimationTable(&swos.refYellowCardAnimTable);
            enqueueYellowCardSample();
            break;
        case kSecondYellowCard:
            initRefereeAnimationTable(&swos.refSecondYellowAnimTable);
            enqueueRedCardSample();
            break;
        }
        break;

    case kRefLeaving:
        if (!m_refereeSprite.onScreen) {
            swos.refState = kRefOffScreen;
            m_refereeSprite.hide();
            initDisplaySprites();
            break;
        }
        // fall-through

    case kRefIncoming:
        m_refereeSprite.speed = kRefereeSpeed;
        auto oldDirection = m_refereeSprite.direction;
        updateSpriteDeltaAndDirection(m_refereeSprite);

        if (oldDirection != m_refereeSprite.direction)
{
            initRefereeAnimationTable(&swos.refComingAnimTable);
}

        moveSprite(m_refereeSprite);

        if (!m_refereeSprite.deltaX && !m_refereeSprite.deltaY) {
            swos.refState = kRefWaitingPlayer;
            m_refereeSprite.direction = kFacingLeft;
            initRefereeAnimationTable(&swos.refWaitingAnimTable);
        }
        break;
    }
}

static void initRefereeAnimationTable(SwosDataPointer<void> animTable)
{
    auto delay = animTable.as<RefereeAnimationTable *>()->numCycles;
    auto frameTable = animTable.asAligned<RefereeAnimationTable *>()->indicesTable;

    m_refereeSprite.frameDelay = delay;
    m_refereeSprite.frameIndicesTable = frameTable[m_refereeSprite.direction];
    assert(m_refereeSprite.frameIndicesTable.asAligned());

    m_refereeSprite.delayedFrameTimer = -1;
    m_refereeSprite.frameIndex = -1;
    m_refereeSprite.cycleFramesTimer = 1;
}

static void moveSprite(Sprite& sprite)
{
    if (sprite.deltaX) {
        sprite.x += sprite.deltaX;
        bool reachedDestination = sprite.deltaX > 0 ? sprite.destX <= sprite.x : sprite.destX >= sprite.x;
        if (reachedDestination) {
            sprite.x = sprite.destX;
            sprite.deltaX = 0;
        }
    }
    if (sprite.deltaY) {
        sprite.y += sprite.deltaY;
        bool reachedDestination = sprite.deltaY > 0 ? sprite.destY <= sprite.y : sprite.destY >= sprite.y;
        if (reachedDestination) {
            sprite.y = sprite.destY;
            sprite.deltaY = 0;
        }
    }
}

static void updateSpriteAnimation(Sprite& sprite)
{
    if (!--sprite.cycleFramesTimer) {
        sprite.frameIndex++;
        sprite.cycleFramesTimer = sprite.frameDelay;

        int frame;

        do {
            frame = sprite.frameIndicesTable.asAligned()[sprite.frameIndex];
            if (frame >= 0) {
                sprite.pictureIndex = frame;
            } else if (frame == kLastFrameLoopMarker) {
                sprite.frameIndex = 0;
            } else if (frame == kLastFrameHoldMarker) {
                sprite.frameIndex--;
                break;
            } else if (frame <= kFrameLoopbackMarker) {
                int offset = frame - kFrameLoopbackMarker;
                sprite.frameIndex += offset;
            } else {
                int newDelay = -frame;
                sprite.frameDelay = newDelay;
                sprite.cycleFramesTimer = newDelay;
                sprite.frameIndex++;
            }
        } while (frame < 0);
    }
}

static void updateSpriteDeltaAndDirection(Sprite& sprite)
{
    D0 = sprite.speed;
    D1 = sprite.destX;
    D2 = sprite.destY;
    D3 = sprite.x.whole();
    D4 = sprite.y.whole();

    CalculateDeltaXAndY();

    sprite.deltaX.setRaw(D1);
    sprite.deltaY.setRaw(D2);
    sprite.direction = ((D0.asWord() + 16) & 0xff) >> 5;
}
