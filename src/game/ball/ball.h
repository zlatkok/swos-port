#pragma once

struct BallDestinationDelta
{
    int16_t x;
    int16_t y;
};

using BallDestinationTable = std::array<BallDestinationDelta, 8>;

void initBallSprites();
void initPitchDependentBallPhysics(int pitchBallSpeedReductionAdjustment,
    int ballSpeedBounceFactor, int ballZAxisDampenFactor);
void updateBall();
void applyBallAfterTouch();
void checkIfBallOutOfPlay();
void resetBothTeamSpinTimers();
Sprite& getBallSprite();
Sprite& getBallShadowSprite();
const BallDestinationTable& getBallDestCoordinatesTable();
const BallDestinationTable& getDefaultBallDestinations();
void setBallPosition(int x, int y);
int32_t getBallAirFriction();
void setBallAirFriction(int32_t friction);
int getControlledBallSpeedReduction();
void setControlledBallSpeedReduction(int reduction);
int getBallAirSpeedReduction();
void setBallAirSpeedReduction(int reduction);

#ifdef DEBUG
void verifyBallSprites();
#endif
