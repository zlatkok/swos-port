#include "ball.h"
#include "player.h"
#include "team.h"
#include "sprites.h"
#include "updateSprite.h"
#include "animation.h"
#include "pitchConstants.h"
#include "comments.h"
#include "sfx.h"
#include "result.h"
#include "random.h"
#include "playerDirection.h"

static constexpr int kControlledBallSpeedReduction = 13;
static constexpr int kBallAirSpeedReduction = 4;

static constexpr int32_t kBallAirFriction = 3291;

static constexpr int kLeftInnerGoalCollisionX = kLeftInnerGoalPost + 1;
static constexpr int kRightInnerGoalCollisionX = kRightInnerGoalPost + 1;
static constexpr int kRightOuterGoalCollisionX = kRightOuterGoalPost + 1;

static constexpr auto kAllowUpwardPlayerDirections = allowPlayerDirections(PlayerDirection::kLeft,
    PlayerDirection::kUpLeft, PlayerDirection::kUp, PlayerDirection::kUpRight, PlayerDirection::kRight);
static constexpr auto kAllowDownwardPlayerDirections = allowPlayerDirections(PlayerDirection::kLeft,
    PlayerDirection::kDownLeft, PlayerDirection::kDown, PlayerDirection::kDownRight, PlayerDirection::kRight);
static constexpr auto kAllowLeftwardPlayerDirections = allowPlayerDirections(PlayerDirection::kUp,
    PlayerDirection::kUpLeft, PlayerDirection::kLeft, PlayerDirection::kDownLeft, PlayerDirection::kDown);
static constexpr auto kAllowRightwardPlayerDirections = allowPlayerDirections(PlayerDirection::kUp,
    PlayerDirection::kUpRight, PlayerDirection::kRight, PlayerDirection::kDownRight, PlayerDirection::kDown);

static constexpr int kSpinDuration = 10;
static constexpr int kKickSetupFrame = 4;
static constexpr int16_t kHighKickBallSpeed = 2688;   // Q7.9: 5.25 pixels/tick
static constexpr int16_t kNormalKickBallSpeed = 2560; // Q7.9: 5 pixels/tick

static constexpr int16_t kSpinMultiplierFactors[kSpinDuration] = { 5, 4, 3, 2, 2, 2, 2, 1, 1, 1 };

// Each direction has left-spin X/Y followed by right-spin X/Y.
static constexpr int16_t kKickSpinFactors[8][4] = {
    {-32, 0, 32, 0}, {0, -23, 23, 0}, {0, -32, 0, 32}, {23, 0, 0, 23},
    {32, 0, -32, 0}, {0, 23, -23, 0}, {0, 32, 0, -32}, {-23, 0, 0, -23},
};
static constexpr int16_t kPassingSpinFactors[8][4] = {
    {-16, 0, 16, 0}, {0, -11, 11, 0}, {0, -16, 0, 16}, {11, 0, 0, 11},
    {16, 0, -16, 0}, {0, 11, -11, 0}, {0, 16, 0, -16}, {-11, 0, 0, -11},
};

enum class SpinDirection
{
    kNone,
    kLeft,
    kRight,
};

struct BallStoppage
{
    TeamGeneralInfo *team;
    int x;
    int y;
    int cameraDirection;
    uint8_t playerTurnFlags;
};

static Sprite m_ballShadowSprite;

static int16_t m_ballNextX; // predicted ball location on the ground
static int16_t m_ballNextY;

// when the player controls the ball, the speed is reduced by this much
static int m_controlledBallSpeedReduction = kControlledBallSpeedReduction;
static int m_ballAirSpeedReduction = kBallAirSpeedReduction;
static int32_t m_ballAirFriction = kBallAirFriction;

std::array<int16_t, 3> ballStaticFrames = {kBallSprite4, kBallSprite4, kInvalidSprite};
std::array<int16_t, 5> ballMovingFrames = {kBallSprite4, kBallSprite3, kBallSprite2, kBallSprite1, kInvalidSprite};

static const BallDestinationTable kLeftThrowInBallDestDelta = {
    250, -1000, 1000, -1000, 1000, 0, 1000, 1000, 250, 1000, -1000, 1000, -1000, 0, -1000, -1000,
};
static const BallDestinationTable kRightThrowInBallDestDelta = {
    -250, -1000, 1000, -1000, 1000, 0, 1000, 1000, -250, 1000, -1000, 1000, -1000, 0, -1000, -1000,
};
static const BallDestinationTable kPenaltyBallDestDelta = {
    0, -1000, 500, -1000, 1000, 0, 500, 1000, 0, 1000, -500, 1000, -1000, 0, -500, -1000,
};
static const BallDestinationTable kUpperLeftCornerBallDestDelta = {
    0, -1000, 1000, -1000, 1000, 150, 1000, 300, 250, 1000, -1000, 1000, -1000, 0, -1000, -1000,
};
static const BallDestinationTable kUpperRightCornerBallDestDelta = {
    0, -1000, 1000, -1000, 1000, 0, 1000, 1000, -250, 1000, -1000, 350, -1000, 150, -1000, -1000,
};
static const BallDestinationTable kLowerLeftCornerBallDestDelta = {
    250, -1000, 1000, -350, 1000, -150, 1000, 1000, 0, 1000, -1000, 1000, -1000, 0, -1000, -1000,
};
static const BallDestinationTable kLowerRightCornerBallDestDelta = {
    -250, -1000, 1000, -1000, 1000, 0, 1000, 1000, 0, 1000, -1000, 1000, -1000, -150, -1000, -350,
};

int m_pitchBallSpeedReductionAdjustment;
int m_ballSpeedBounceFactor;
int m_ballZAxisDampenFactor;

static void updateBallAnimation();
static void updateBallSpeedAndXYCoordinates();
static void updateBallZCoordinate();
static void bounceOffInvisibleWalls(FixedPoint initialX, FixedPoint initialY, FixedPoint initialZ);
static void handleGoalFrameCollisions(FixedPoint& previousX, FixedPoint& previousY, FixedPoint previousZ);
static void handlePostAndCrossbarCollisions(FixedPoint& previousX, FixedPoint& previousY, FixedPoint previousZ);
static void checkUpdatedBallOutOfPlay();
static void updateBallShadow();
static void calculateNextBallPosition();
static void updateBallQuadrants();
static void applyPassAfterTouch(TeamGeneralInfo& team);
static void applyKickAfterTouch(TeamGeneralInfo& team);
static void applyStoppage(const BallStoppage& stoppage);
static BallStoppage handleGoal(TeamGeneralInfo& goalSide, int teamNumber);
static BallStoppage makeCorner(TeamGeneralInfo& team, bool upper, bool left);
static BallStoppage makeGoalOut(TeamGeneralInfo& team, bool upper, bool left);
static BallStoppage makeThrowIn(int ballX, int ballY);
static GameState getThrowInState(bool rightHalf, bool topTeam, int ballY);
static void setNextBallSpriteFrame();

void initBallSprites()
{
    swos.ballSprite.init();
    m_ballShadowSprite.teamNumber = 3;
    m_ballShadowSprite.init();
}

void initPitchDependentBallPhysics(int pitchBallSpeedReductionAdjustment,
    int ballSpeedBounceFactor, int ballZAxisDampenFactor)
{
    m_pitchBallSpeedReductionAdjustment = pitchBallSpeedReductionAdjustment;
    m_ballSpeedBounceFactor = ballSpeedBounceFactor;
    m_ballZAxisDampenFactor = ballZAxisDampenFactor;
}

void updateBall()
{
    // Collision responses restore one or more coordinates to the position from the
    // beginning of the tick, so preserve the full fixed-point values before moving.
    auto initialX = swos.ballSprite.x;
    auto initialY = swos.ballSprite.y;
    auto initialZ = swos.ballSprite.z;

    updateBallAnimation();
    updateBallSpeedAndXYCoordinates();
    updateBallZCoordinate();
    bounceOffInvisibleWalls(initialX, initialY, initialZ);
    handleGoalFrameCollisions(initialX, initialY, initialZ);
    handlePostAndCrossbarCollisions(initialX, initialY, initialZ);
    checkUpdatedBallOutOfPlay();
    updateBallShadow();
    calculateNextBallPosition();
    updateBallQuadrants();
}

// Apply directional input after a kick or pass. Spin weakens over ten frames;
// kicks can become high kicks, while passes can become long-pass variants.
void applyBallAfterTouch()
{
    auto& team = A6.as<TeamGeneralInfo&>();

    if (team.passInProgress)
        applyPassAfterTouch(team);
    else
        applyKickAfterTouch(team);
}

void checkIfBallOutOfPlay()
{
    swos.stateGoal = 0;
    if (swos.gameStatePl == GameState::kStopped)
        return;

    bool playWhistle = true;
    int ballX = swos.ballSprite.x.whole();
    int ballY = swos.ballSprite.y.whole();
    int ballZ = swos.ballSprite.z.whole();

    bool insideGoal = ballZ <= kCrossbarHeight &&
        ballX >= kLeftInnerGoalCollisionX && ballX <= kRightInnerGoalCollisionX;
    if (insideGoal) {
        // The end of the pitch identifies the scoring side. goalScored() uses its
        // team number together with goalTypeScored to handle ordinary and own goals.
        auto goalSide = ballY <= kPitchCenterY ? &swos.bottomTeamData : &swos.topTeamData;
        applyStoppage(handleGoal(*goalSide, goalSide->teamNumber));
        return;
    }

    constexpr int kNearMissMinimumBallSpeed = 768;
    constexpr int kNearMissLeftX = 290;
    constexpr int kNearMissRightX = 381;
    constexpr int kNearMissMaximumZ = 23; // SWOS tests ball Z + 2 against 25.
    bool nearMiss = static_cast<uint16_t>(swos.ballSprite.speed) >= kNearMissMinimumBallSpeed &&
        ballX >= kNearMissLeftX && ballX <= kNearMissRightX &&
        ballZ <= kNearMissMaximumZ;
    if (nearMiss) {
        playNearMissComment();
        playMissGoalSample();
        // The miss sample already contains the appropriate reaction, so the
        // original suppresses the normal out-of-play whistle in this case.
        playWhistle = false;
    }

    // Fix the original SWOS bug where the penalty flag remains set after a
    // missed penalty which was not classified as a near miss.
    clearPenaltyFlag();

    BallStoppage stoppage{};
    bool left = ballX < kPitchCenterX;
    if (ballY < kTopPitchLine) {
        // The last touch decides between a corner and a goalkeeper restart.
        bool corner = swos.lastTeamPlayed == &swos.topTeamData;
        stoppage = corner ?
            makeCorner(swos.bottomTeamData, true, left) :
            makeGoalOut(swos.topTeamData, true, left);
    } else if (ballY > kBottomPitchLine) {
        bool corner = swos.lastTeamPlayed == &swos.bottomTeamData;
        bool useUpperRestart = swos.forceLeftTeam == 1;
        stoppage = corner ?
            makeCorner(swos.topTeamData, useUpperRestart, left) :
            makeGoalOut(swos.bottomTeamData, useUpperRestart, left);
    } else {
        stoppage = makeThrowIn(ballX, ballY);
    }

    applyStoppage(stoppage);
    if (playWhistle)
        playRefereeWhistleSample();
}

void resetBothTeamSpinTimers()
{
    swos.topTeamData.spinTimer = -1;
    swos.bottomTeamData.spinTimer = -1;
}

Sprite& getBallSprite()
{
    return swos.ballSprite;
}

Sprite& getBallShadowSprite()
{
    return m_ballShadowSprite;
}

// out:
//      ball factor table, depending on game state
//
// Even indices are delta x, odd are delta y. They are added to ball
// delta x and y. Tables are different for each type of game halt.
// Called when player kicks the ball or does a pass.
//
const BallDestinationTable& getBallDestCoordinatesTable()
{
    if (swos.gameState >= GameState::kThrowInForwardRight && swos.gameState <= GameState::kThrowInBackLeft) {
        return swos.foulXCoordinate > kPitchCenterX ?
            kRightThrowInBallDestDelta : kLeftThrowInBallDestDelta;
    } else if (swos.gameState == GameState::kPenalty || swos.gameState == GameState::kPenaltyShootout) {
        return kPenaltyBallDestDelta;
    } else if (swos.gameState == GameState::kCornerLeft || swos.gameState == GameState::kCornerRight) {
        if (swos.foulYCoordinate <= kPitchCenterY) {
            return swos.foulXCoordinate > kPitchCenterX ?
                kUpperRightCornerBallDestDelta : kUpperLeftCornerBallDestDelta;
        } else {
            return swos.foulXCoordinate > kPitchCenterX ?
                kLowerRightCornerBallDestDelta : kLowerLeftCornerBallDestDelta;
        }
    } else {
        return *reinterpret_cast<BallDestinationTable *>(swos.kDefaultDestinations);
    }
}

// Besides setting x and y coordinates, stops the ball and puts it to the ground (z = 0).
void setBallPosition(int x, int y)
{
    swos.ballSprite.speed = 0;
    swos.ballSprite.x = x;
    swos.ballSprite.y = y;
#ifdef SWOS_TEST
    swos.ballSprite.z.setWhole(0);
#else
    swos.ballSprite.z = 0;
#endif
    swos.ballSprite.destX = x;
    swos.ballSprite.destY = y;
    swos.ballSprite.deltaZ = 0;
}

int32_t getBallAirFriction()
{
    return m_ballAirFriction;
}

void setBallAirFriction(int32_t friction)
{
    m_ballAirFriction = friction;
}

int getControlledBallSpeedReduction()
{
    return m_controlledBallSpeedReduction;
}

void setControlledBallSpeedReduction(int reduction)
{
    m_controlledBallSpeedReduction = reduction;
}

int getBallAirSpeedReduction()
{
    return m_ballAirSpeedReduction;
}

void setBallAirSpeedReduction(int reduction)
{
    m_ballAirSpeedReduction = reduction;
}

#ifdef DEBUG
void verifyBallSprites()
{
    assert(swos.ballSprite.hasNoImage() ||
        (swos.ballSprite.imageIndex >= kBallSprite1 && swos.ballSprite.imageIndex <= kBallSprite4));
    assert(m_ballShadowSprite.hasNoImage() || m_ballShadowSprite.imageIndex == kBallShadowSprite);
}
#endif

static bool afterTouchAllowed(const TeamGeneralInfo& team)
{
    const auto& opponent = *team.opponentTeam;
    // A negative save-comment timer marks an active goalkeeper save. After-touch
    // is otherwise allowed during play, and while the keeper-holds-ball state is
    // being left behind.
    return static_cast<int16_t>(opponent.goalkeeperSavedCommentTimer) >= 0 &&
        (swos.gameStatePl == GameState::kInProgress || swos.gameState != GameState::kKeeperHoldsTheBall);
}

static PlayerDirection relativeControlDirection(const TeamGeneralInfo& team)
{
    // Center the eight-way direction circle on the player's facing direction.
    return static_cast<PlayerDirection>((team.allowedPlDirection - team.currentAllowedDirection) & 7);
}

static SpinDirection startOrGetSpin(TeamGeneralInfo& team)
{
    // Once chosen, spin direction remains latched for the rest of this ten-tick
    // after-touch sequence. Only its strength decays.
    if (team.leftSpin)
        return SpinDirection::kLeft;
    if (team.rightSpin)
        return SpinDirection::kRight;
    if (static_cast<int16_t>(team.currentAllowedDirection) < 0)
        return SpinDirection::kNone;

    auto relativeDirection = relativeControlDirection(team);
    if (relativeDirection == PlayerDirection::kUp || relativeDirection == PlayerDirection::kDown)
        return SpinDirection::kNone;

    if (relativeDirection < PlayerDirection::kDown) {
        team.leftSpin = 1;
        return SpinDirection::kLeft;
    }

    team.rightSpin = 1;
    return SpinDirection::kRight;
}

static void addSpinToBall(int direction, SpinDirection spin, const int16_t factors[8][4], int spinTimer)
{
    assert(direction >= 0 && direction < 8);
    assert(spin != SpinDirection::kNone);
    assert(spinTimer >= 0 && spinTimer < kSpinDuration);

    auto factorIndex = spin == SpinDirection::kRight ? 2 : 0;
    auto multiplier = kSpinMultiplierFactors[spinTimer];
    auto deltaX = static_cast<int16_t>(factors[direction][factorIndex] * multiplier);
    auto deltaY = static_cast<int16_t>(factors[direction][factorIndex + 1] * multiplier);

    // Preserve the original 16-bit additions to the ball destination.
    swos.ballSprite.destX = static_cast<int16_t>(
        static_cast<uint16_t>(swos.ballSprite.destX) + static_cast<uint16_t>(deltaX));
    swos.ballSprite.destY = static_cast<int16_t>(
        static_cast<uint16_t>(swos.ballSprite.destY) + static_cast<uint16_t>(deltaY));
}

static void advanceSpinTimer(TeamGeneralInfo& team)
{
    team.spinTimer = static_cast<uint16_t>(team.spinTimer + 1);
    // -1 is the original inactive sentinel; the field is unsigned in the VM view.
    if (team.spinTimer == kSpinDuration)
        team.spinTimer = static_cast<uint16_t>(-1);
}

static void adjustKickSpeedForDirection(int direction)
{
    auto speed = static_cast<uint16_t>(swos.ballSprite.speed);

    // Vertical kicks retain 3/4 speed, diagonals 7/8, and horizontal kicks full speed.
    if (direction == 0 || direction == 4)
        swos.ballSprite.speed = static_cast<int16_t>(speed - (speed >> 2));
    else if (direction & 1)
        swos.ballSprite.speed = static_cast<int16_t>(speed - (speed >> 2) + (speed >> 3));
}

static void applyKickAfterTouch(TeamGeneralInfo& team)
{
    if (!afterTouchAllowed(team)) {
        team.spinTimer = static_cast<uint16_t>(-1);
        return;
    }

    auto spinTimer = static_cast<int16_t>(team.spinTimer);
    if (spinTimer < 0)
        return;

    if (!spinTimer) {
        team.leftSpin = 0;
        team.rightSpin = 0;
    }

    auto spin = startOrGetSpin(team);
    if (spin != SpinDirection::kNone)
        addSpinToBall(team.allowedPlDirection, spin, kKickSpinFactors, spinTimer);

    if (spinTimer == kKickSetupFrame) {
        // SWOS waits until the fifth after-touch update before deciding whether
        // directional input should lift and/or strengthen the kick.
        auto pressedDirection = static_cast<int16_t>(team.currentAllowedDirection);
        bool updateKick = false;
        bool highKick = false;

        if (pressedDirection < 0) {
            updateKick = true;
        } else {
            auto relativeDirection = relativeControlDirection(team);
            if (relativeDirection == PlayerDirection::kRight || relativeDirection == PlayerDirection::kLeft) {
                updateKick = true;
            } else if (relativeDirection >= PlayerDirection::kDownRight && relativeDirection <= PlayerDirection::kDownLeft) {
                // Back-left, back and back-right turn the normal kick into a high kick.
                updateKick = true;
                highKick = true;
            }
        }

        if (updateKick) {
            swos.ballSprite.deltaZ = highKick ? 2_fp : 1.375_fp;
            swos.ballSprite.speed = highKick ? kHighKickBallSpeed : kNormalKickBallSpeed;
            adjustKickSpeedForDirection(team.allowedPlDirection);
        }
    }

    advanceSpinTimer(team);
}

static void increasePassSpeed()
{
    auto speed = static_cast<uint16_t>(swos.ballSprite.speed);
    swos.ballSprite.speed = static_cast<int16_t>(speed + (speed >> 3));
}

static void applyPassAfterTouch(TeamGeneralInfo& team)
{
    if (!afterTouchAllowed(team)) {
        team.spinTimer = static_cast<uint16_t>(-1);
        return;
    }

    auto spinTimer = static_cast<int16_t>(team.spinTimer);
    if (spinTimer < 0)
        return;

    if (!spinTimer) {
        team.leftSpin = 0;
        team.rightSpin = 0;
        team.longPass = 0;
        team.longSpinPass = 0;
    }

    auto spin = startOrGetSpin(team);
    if (spin != SpinDirection::kNone)
        addSpinToBall(swos.ballSprite.direction, spin, kPassingSpinFactors, spinTimer);

    // Assembly reads longPass and longSpinPass together as one dword. Once either
    // mode is selected, subsequent frames only apply spin and advance the timer.
    if (!team.longPass && !team.longSpinPass) {
        auto pressedDirection = static_cast<int16_t>(team.currentAllowedDirection);
        if (pressedDirection < 0) {
            team.longPass = 1;
            increasePassSpeed();
        } else {
            auto relativeDirection = relativeControlDirection(team);
            if (relativeDirection == PlayerDirection::kRight || relativeDirection == PlayerDirection::kLeft) {
                // Holding left or right after passing selects a long pass.
                team.longPass = 1;
                increasePassSpeed();
            } else if (relativeDirection >= PlayerDirection::kDownRight && relativeDirection <= PlayerDirection::kDownLeft) {
                // Holding backwards selects the alternate long, spinning pass.
                team.longSpinPass = 1;
                increasePassSpeed();
            }
        }
    }

    advanceSpinTimer(team);
}

static Sprite *getGoalScorer()
{
    auto scorer = swos.lastPlayerPlayed.asPtr();
    // Outfield players are stored directly in lastPlayerPlayed. For goalkeepers
    // that slot identifies the goalie sprite, while lastKeeperPlayed identifies
    // the actual player record used by the goal statistics code.
    if (!swos.lastKeeperPlayed.isNull() &&
        (scorer == &swos.goalie1Sprite || scorer == &swos.goalie2Sprite))
        scorer = swos.lastKeeperPlayed.asPtr();

    return scorer;
}

static int getGoalCelebrationTime(int previousGoalDifference)
{
    int duration = (SWOS::rand() >> 1) + 100;
    if (swos.playingPenalties)
        return duration;

    int currentGoalDifference = static_cast<int16_t>(swos.statsTeam1Goals - swos.statsTeam2Goals);
    int baseDuration;
    // Equalizers and goals which first establish a lead get longer celebrations;
    // extending a one-goal lead to two receives the middle duration.
    if (currentGoalDifference == 0) {
        baseDuration = 200;
    } else {
        if (previousGoalDifference == 0)
            baseDuration = 300;
        else if ((previousGoalDifference == 1 || previousGoalDifference == -1) &&
            (currentGoalDifference == 2 || currentGoalDifference == -2))
            baseDuration = 200;
        else
            baseDuration = 100;
    }

    return baseDuration + SWOS::rand();
}

static void applyStoppage(const BallStoppage& stoppage)
{
    swos.gameStatePl = GameState::kStopped;
    swos.foulXCoordinate = stoppage.x;
    swos.foulYCoordinate = stoppage.y;
    swos.cameraDirection = stoppage.cameraDirection;
    swos.playerTurnFlags = stoppage.playerTurnFlags;
    // forceLeftTeam is a debug/compatibility override from SWOS: it forces the
    // upper team to take the restart regardless of the normal decision above.
    auto team = swos.forceLeftTeam == 1 ? &swos.topTeamData : stoppage.team;
    swos.lastTeamPlayedBeforeBreak = team;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;
    stopAllPlayers();
    swos.cameraXVelocity = 0;
    swos.cameraYVelocity = 0;
}

static BallStoppage handleGoal(TeamGeneralInfo& goalSide, int teamNumber)
{
    int previousGoalDifference =
        static_cast<int16_t>(swos.statsTeam1Goals - swos.statsTeam2Goals);
    goalScored(teamNumber, *getGoalScorer());

    swos.goalCameraMode = 1;
    swos.teamScoredDataPtr = &goalSide;
    swos.teamScoredGamePtr = goalSide.inGameTeamPtr;

    auto& opposingSide = *goalSide.opponentTeam;
    BallStoppage stoppage = &opposingSide == &swos.topTeamData ?
        BallStoppage{&opposingSide, kPitchCenterX, kPitchCenterY, 4, kAllowDownwardPlayerDirections} :
        BallStoppage{&opposingSide, kPitchCenterX, kPitchCenterY, 0, kAllowUpwardPlayerDirections};

    if (swos.goalTypeScored == static_cast<int>(GoalType::kOwnGoal))
        playOwnGoalComment();
    else
        playGoalComment();

    if (teamNumber == 2)
        playAwayGoalSample();
    else
        playHomeGoalSample();

    swos.stateGoal = static_cast<uint16_t>(-1);
    swos.goalCounter = getGoalCelebrationTime(previousGoalDifference);
    swos.patternsGoalCounter = 1;
    swos.gameState = GameState::kPlayersGoingToInitialPositions;
    swos.breakCameraMode = -1;
    return stoppage;
}

static BallStoppage makeCorner(TeamGeneralInfo& team, bool upper, bool left)
{
    // Restart positions sit just inside the four pitch corners. Direction masks
    // permit only input which sends the ball back onto the pitch.
    BallStoppage stoppage{
        &team,
        left ? 86 : 585,
        upper ? 134 : 764,
        left ? 2 : 6,
        static_cast<uint8_t>(upper ? (left ? 0x1c : 0x70) : (left ? 0x07 : 0xc1)),
    };

    // Corner state names are relative to the attacking team, hence their apparent
    // left/right reversal at the lower end of the pitch.
    if (upper)
        swos.gameState = left ? GameState::kCornerLeft : GameState::kCornerRight;
    else
        swos.gameState = left ? GameState::kCornerRight : GameState::kCornerLeft;

    swos.breakCameraMode = -1;
    ++team.teamStatsPtr->cornersWon;
    enqueueCornerSample();
    return stoppage;
}

static BallStoppage makeGoalOut(TeamGeneralInfo& team, bool upper, bool left)
{
    BallStoppage stoppage{
        &team,
        left ? 276 : 396,
        upper ? 154 : 744,
        upper ? 4 : 0,
        upper ? kAllowDownwardPlayerDirections : kAllowUpwardPlayerDirections,
    };

    if (upper)
        swos.gameState = left ? GameState::kGoalOutRight : GameState::kGoalOutLeft;
    else
        swos.gameState = left ? GameState::kGoalOutLeft : GameState::kGoalOutRight;

    swos.breakCameraMode = -1;
    // Human-controlled teams may use the full inward-facing arc. CPU teams are
    // prevented from turning exactly sideways along the goal line.
    if (!team.playerNumber)
        stoppage.playerTurnFlags &= static_cast<uint8_t>(
            ~allowPlayerDirections(PlayerDirection::kLeft, PlayerDirection::kRight));
    swos.goalOut = 1;
    return stoppage;
}

static BallStoppage makeThrowIn(int ballX, int ballY)
{
    auto& team = *swos.lastTeamPlayed->opponentTeam;
    bool rightHalf = ballX >= kPitchCenterX;
    swos.gameState = getThrowInState(rightHalf, &team == &swos.topTeamData, ballY);
    swos.breakCameraMode = -1;
    enqueueThrowInSample();

    return {
        &team,
        rightHalf ? kRightThrowInLine : kLeftThrowInLine,
        ballY,
        rightHalf ? 6 : 2,
        rightHalf ? kAllowLeftwardPlayerDirections : kAllowRightwardPlayerDirections,
    };
}

static GameState getThrowInState(bool rightHalf, bool topTeam, int ballY)
{
    // Separate states select the forward, central or defensive throw-in setup for
    // each team. The labels are team-relative rather than screen-relative.
    bool upperThird = ballY < 342;
    bool middleThird = !upperThird && ballY < 556;

    if (rightHalf) {
        if (topTeam)
            return upperThird ? GameState::kThrowInForwardRight :
                middleThird ? GameState::kThrowInCenterRight : GameState::kThrowInBackRight;
        return upperThird ? GameState::kThrowInBackLeft :
            middleThird ? GameState::kThrowInCenterLeft : GameState::kThrowInForwardLeft;
    }

    if (topTeam)
        return upperThird ? GameState::kThrowInForwardLeft :
            middleThird ? GameState::kThrowInCenterLeft : GameState::kThrowInBackLeft;
    return upperThird ? GameState::kThrowInBackRight :
        middleThird ? GameState::kThrowInCenterRight : GameState::kThrowInForwardRight;
}

// Advances to next frame for ball sprite, and sets the image to it. Next frame is simply next
// positive index in the frame table. First negative element makes the index go back to zero,
// and loop around.
static void setNextBallSpriteFrame()
{
    auto frameTable = getFrameTable(swos.ballSprite.frameIndicesTable);
    assert(frameTable[0] >= 0);

    int frame = frameTable[swos.ballSprite.frameIndex];
    if (frame < 0) {
        swos.ballSprite.frameIndex = 0;
        frame = frameTable[0];
    }

    swos.ballSprite.setImage(frame >= 0 ? frame : -1);
}

static void updateBallAnimation()
{
    if (!swos.hideBall) {
        m_ballShadowSprite.setImage(kBallShadowSprite);
        int frameTable = swos.ballSprite.stationary() ?
            getBallStaticFrameTableOffset() : getBallMovingFrameTableOffset();

        if (swos.ballSprite.frameIndicesTable != frameTable) {
            swos.ballSprite.frameIndicesTable = frameTable;
            swos.ballSprite.frameIndex = 0;
        }

        if (swos.ballSprite.speed) {
            constexpr int kWholeSpriteSpeed = 512;

            // Speed is Q7.9. Faster travel subtracts more from the frame timer,
            // making the four ball images rotate proportionally faster.
            // speed 256(0.5): -= 1, 1024(2.0) -= 3, 1824(3.5625) -= 4, 2208(4.3125) -= 5, 2592(5.0625) -= 6
            int wholeSpeedPartRoundedUp = swos.ballSprite.speed / kWholeSpriteSpeed + 1;
            auto cycleFramesTimer = static_cast<int16_t>(
                swos.ballSprite.cycleFramesTimer - wholeSpeedPartRoundedUp);
            swos.ballSprite.cycleFramesTimer = cycleFramesTimer;
            if (cycleFramesTimer < 0) {
                swos.ballSprite.frameIndex++;
                swos.ballSprite.cycleFramesTimer = swos.ballSprite.frameDelay;
                setNextBallSpriteFrame();
            }
        }

        if (!swos.ballSprite.hasImage())
            setNextBallSpriteFrame();
    } else {
        swos.ballSprite.clearImage();
        m_ballShadowSprite.clearImage();
    }
}

static void updateBallSpeedAndXYCoordinates()
{
    const auto& result = calculateDeltaXAndY(swos.ballSprite.speed,
        swos.ballSprite.x.whole(), swos.ballSprite.y.whole(), swos.ballSprite.destX, swos.ballSprite.destY);

    swos.ballSprite.deltaX = result.deltaX;
    swos.ballSprite.deltaY = result.deltaY;

    int direction = result.direction;
    if (direction >= 0) {
        swos.ballSprite.fullDirection = direction;
        direction = ((direction + 16) & 0xff) >> 5;
    }
    swos.ballSprite.direction = direction;

    // Planar speed loses a fixed Q7.9 amount each tick. Ground friction also
    // includes the selected pitch's adjustment unless a player controls the ball.
    int speedReduction = m_ballAirSpeedReduction;
    if (!swos.ballSprite.z.whole()) {
        speedReduction = m_controlledBallSpeedReduction;
        if (!swos.topTeamData.playerHasBall && !swos.bottomTeamData.playerHasBall)
            speedReduction += m_pitchBallSpeedReductionAdjustment;
    }

    swos.ballSprite.speed = std::max(0, swos.ballSprite.speed - speedReduction);
    swos.ballSprite.x += swos.ballSprite.deltaX;
    swos.ballSprite.y += swos.ballSprite.deltaY;
}

static void updateBallZCoordinate()
{
    if (swos.gameState == GameState::kKeeperHoldsTheBall) {
        // settle z to keeper's hand (= 5)
        bool substitutedKeeperHasNoControlledPlayer = swos.g_substituteInProgress &&
            swos.teamThatSubstitutes == swos.lastTeamPlayedBeforeBreak &&
            !swos.teamThatSubstitutes->controlledPlayer;

        if (!substitutedKeeperHasNoControlledPlayer) {
            if (swos.ballSprite.speed && swos.lastTeamPlayedBeforeBreak->controlledPlayer)
                updateBallWithControllingGoalkeeper(*swos.lastTeamPlayedBeforeBreak->controlledPlayer);

            constexpr int kBallHeightInGoalkeepersHands = 5;
            if (swos.ballSprite.z.whole() == kBallHeightInGoalkeepersHands) {
                swos.ballSprite.deltaZ = 0;
            } else {
                // note that it goes up two times faster
                swos.ballSprite.deltaZ = swos.ballSprite.z.whole() < kBallHeightInGoalkeepersHands ?
                    2.00006103515625_fp : -1.000030517578125_fp;
            }
        } else {
            swos.ballSprite.z.setWhole(0);
            swos.ballSprite.deltaZ = 0;
        }
    } else if (swos.ballSprite.deltaZ) {
        // Preserve SWOS's odd raw vertical delta. The low bit prevents a tiny
        // nonzero trajectory from becoming zero prematurely through quantization.
        swos.ballSprite.deltaZ.setRaw((swos.ballSprite.deltaZ.raw() - m_ballAirFriction) | 1);
    }

    if (swos.ballSprite.deltaZ) {
        swos.ballSprite.z += swos.ballSprite.deltaZ;
        if (swos.ballSprite.z < 0) {
            int speedLoss = swos.ballSprite.speed * m_ballSpeedBounceFactor >> 8;
            swos.ballSprite.speed -= speedLoss;
            swos.ballSprite.z.setWhole(0);

            int32_t deltaZ = -swos.ballSprite.deltaZ.raw();
            deltaZ -= (deltaZ >> 8) * m_ballZAxisDampenFactor;
            deltaZ |= 1;

            constexpr auto kBounceLimit = 0.625_fp;
            if (FixedPoint::fromRaw(deltaZ) > kBounceLimit)
                playBallBounceSample();
            else
                deltaZ = 0;

            swos.ballSprite.deltaZ.setRaw(deltaZ);
        }
    }
}

// While the game is stopped, invisible walls keep the ball near the presentation area. The ball bounces off them,
// changes direction (destination reversed by axis), then gets its coordinates reset to their pre-movement values.
void bounceOffInvisibleWalls(FixedPoint initialX, FixedPoint initialY, FixedPoint initialZ)
{
    auto restorePreviousPosition = [&] {
        swos.ballSprite.x = initialX;
        swos.ballSprite.y = initialY;
        swos.ballSprite.z = initialZ;
    };

    // reverse the ball direction, e.g. if traveling +30 pixels by x, make it -30, when bouncing back
    auto reverseXDestination = [](int x) {
        swos.ballSprite.destX = 2 * x - swos.ballSprite.destX;
    };
    auto reverseYDestination = [](int y) {
        swos.ballSprite.destY = 2 * y - swos.ballSprite.destY;
    };

    if (swos.gameStatePl != GameState::kInProgress) {
        static constexpr int kLeftInvisibleBarrier = 53;
        static constexpr int kRightInvisibleBarrier = 618;

        int x = swos.ballSprite.x.whole();
        if (x < kLeftInvisibleBarrier || x > kRightInvisibleBarrier) {
            reverseXDestination(x);
            swos.ballSprite.speed >>= 1;
            restorePreviousPosition();
        }

        static constexpr int kTopInvisibleBarrier = 100;
        static constexpr int kBottomInvisibleBarrier = 799;

        int y = swos.ballSprite.y.whole();
        if (y < kTopInvisibleBarrier || y > kBottomInvisibleBarrier) {
            reverseYDestination(y);
            swos.ballSprite.speed >>= 1;
            restorePreviousPosition();
        }
    }
}

static void handleGoalFrameCollisions(FixedPoint& previousX, FixedPoint& previousY, FixedPoint previousZ)
{
    // This pass handles the goal structure behind the pitch line: rear netting,
    // side netting and the roof. Posts and the front crossbar are handled below.
    static constexpr int kTopCrossbarTransitionY = kTopPitchLine - 6;
    static constexpr int kTopNetLine = kTopPitchLine - 10;
    static constexpr int kBottomGoalFrontLine = kBottomPitchLine + 1;
    static constexpr int kBottomNetLine = kBottomPitchLine + 9;
    static constexpr int kRearTopNetHeight = 10;
    static constexpr int kFrameDeflectionDistance = 1'000;
    static constexpr int kFrameDeflectionSpeed = 512;

    int ballY = swos.ballSprite.y.whole();
    if (ballY < kTopPitchLine || ballY > kBottomPitchLine) {
        int ballX = swos.ballSprite.x.whole();
        int ballZ = swos.ballSprite.z.whole();

        enum class GoalFrameCollision {
            kNone,
            kNet,
            kSide,
            kTop,
        };

        GoalFrameCollision collision = GoalFrameCollision::kNone;

        if (ballY < kTopPitchLine) {
            bool withinUpperGoalDepth = ballY > kTopGoalBackLine;
            bool withinGoalWidth = ballX > kLeftOuterGoalPost &&
                ballX <= kRightOuterGoalCollisionX;
            if (withinUpperGoalDepth && withinGoalWidth && ballZ <= kGoalHeight) {
                bool touchingTop = ballY > kTopCrossbarTransitionY ?
                    ballZ > kCrossbarHeight : ballZ > kRearTopNetHeight;
                if (touchingTop) {
                    collision = GoalFrameCollision::kTop;
                } else {
                    bool withinNetWidth = ballX > kLeftInnerGoalPost &&
                        ballX <= kRightInnerGoalCollisionX;
                    if (!withinNetWidth)
                        collision = GoalFrameCollision::kSide;
                    else if (ballY < kTopNetLine)
                        collision = GoalFrameCollision::kNet;
                }
            }
        } else if (ballY >= kBottomGoalFrontLine && ballY < kBottomGoalBackLine) {
            bool withinGoalWidth = ballX > kLeftOuterGoalPost &&
                ballX <= kRightOuterGoalCollisionX;
            if (withinGoalWidth && ballZ <= kGoalHeight) {
                if (ballZ > kCrossbarHeight) {
                    collision = GoalFrameCollision::kTop;
                } else {
                    bool withinNetWidth = ballX > kLeftInnerGoalPost &&
                        ballX <= kRightInnerGoalCollisionX;
                    if (!withinNetWidth)
                        collision = GoalFrameCollision::kSide;
                    else if (ballY > kBottomNetLine)
                        collision = GoalFrameCollision::kNet;
                }
            }
        }

        switch (collision) {
        case GoalFrameCollision::kNet:
            swos.ballSprite.destY = 2 * previousY.whole() - swos.ballSprite.destY;
            resetBothTeamSpinTimers();
            swos.ballSprite.speed >>= 3;
            swos.ballSprite.x = previousX;
            swos.ballSprite.y = previousY;
            break;

        case GoalFrameCollision::kSide:
            swos.ballSprite.destX = 2 * previousX.whole() - swos.ballSprite.destX;
            resetBothTeamSpinTimers();
            swos.ballSprite.speed >>= 2;
            swos.ballSprite.x = previousX;
            swos.ballSprite.y = previousY;
            break;

        case GoalFrameCollision::kTop:
            swos.ballSprite.deltaZ.setRaw(1);
            if (previousZ.whole() <= kCrossbarHeight) {
                swos.ballSprite.speed = 0;
                swos.ballSprite.x = previousX;
                swos.ballSprite.y = previousY;
                swos.ballSprite.z = previousZ;
            } else {
                bool inTopGoal = ballY < kTopPitchLine;
                swos.ballSprite.destY = swos.ballSprite.y.whole() +
                    (inTopGoal ? -kFrameDeflectionDistance : kFrameDeflectionDistance);
                swos.ballSprite.speed = kFrameDeflectionSpeed;
                swos.ballSprite.z = previousZ;
            }
            break;

        case GoalFrameCollision::kNone:
            break;
        }
    }
}

static void handlePostAndCrossbarCollisions(FixedPoint& previousX, FixedPoint& previousY, FixedPoint previousZ)
{
    if (swos.gameStatePl != GameState::kInProgress)
        return;

    constexpr int kGoalLineCollisionMargin = 3;

    int ballX = swos.ballSprite.x.whole();
    int ballY = swos.ballSprite.y.whole();
    int ballZ = swos.ballSprite.z.whole();

    bool crossingGoalLine = ballY >= kTopPitchLine && ballY <= kTopPitchLine + kGoalLineCollisionMargin ||
        ballY >= kBottomPitchLine - kGoalLineCollisionMargin && ballY <= kBottomPitchLine;
    bool withinOuterGoalWidth = ballX > kLeftOuterGoalPost &&
        ballX <= kRightOuterGoalCollisionX;

    if (!crossingGoalLine || !withinOuterGoalWidth || ballZ > kGoalHeight)
        return;

    bool hitCrossbar = ballZ > kCrossbarHeight;
    bool hitPost = !hitCrossbar &&
        (ballX <= kLeftInnerGoalPost || ballX > kRightInnerGoalCollisionX);
    if (!hitCrossbar && !hitPost)
        return;

    // The original temporarily reuses goalTypeScored as a collision-kind marker.
    // It is restored after a handled rebound and never reaches goal accounting.
    swos.goalTypeScored = static_cast<int>(hitCrossbar ? GoalType::kPenalty : GoalType::kOwnGoal);

    constexpr int kDeflectionRandomMask = 0x1f;
    constexpr int kDeflectionRandomScaleShift = 4;
    constexpr int kDeflectionRandomCenter = 256;

    int randomDeflection = ((swos.currentGameTick & kDeflectionRandomMask) << kDeflectionRandomScaleShift) -
        kDeflectionRandomCenter;
    bool shallowVerticalMovement = swos.ballSprite.deltaY >= -0.3125_fp && swos.ballSprite.deltaY <= 0.3125_fp;
    bool collisionHandled = false;

    if (hitCrossbar && shallowVerticalMovement) {
        swos.ballSprite.deltaZ = -swos.ballSprite.deltaZ;
        swos.ballSprite.z = previousZ;
        previousY += 1;
        collisionHandled = true;
    } else if (hitPost && shallowVerticalMovement) {
        swos.ballSprite.destY = 2 * previousY.whole() - swos.ballSprite.destY;
        resetBothTeamSpinTimers();
        swos.ballSprite.destY += randomDeflection;
        collisionHandled = true;
    } else {
        bool movingTowardFrame = ballY <= kPitchCenterY ?
            swos.ballSprite.deltaY < 0 : swos.ballSprite.deltaY > 0;
        if (movingTowardFrame) {
            swos.ballSprite.destY = 2 * previousY.whole() - swos.ballSprite.destY;
            resetBothTeamSpinTimers();
            swos.ballSprite.destX += randomDeflection;
            collisionHandled = true;
        }
    }

    if (collisionHandled) {
        if ((SWOS::rand() & 1) && !hitPost)
            playBarHitComment();
        else
            playPostHitComment();
        playMissGoalSample();

        swos.goalTypeScored = static_cast<int>(GoalType::kRegular);
        swos.ballSprite.speed -= swos.ballSprite.speed >> 2;
        swos.ballSprite.x = previousX;
        swos.ballSprite.y = previousY;
    }
}

static void checkUpdatedBallOutOfPlay()
{
    if (swos.gameStatePl == GameState::kInProgress) {
        int x = swos.ballSprite.x.whole();
        int y = swos.ballSprite.y.whole();
        if (x < kLeftThrowInLine || x > kRightThrowInLine || y < kTopPitchLine || y > kBottomPitchLine)
            checkIfBallOutOfPlay();
    }
}

void updateBallShadow()
{
    constexpr int kBallShadowZ = -10;

    // Height shifts the shadow diagonally to fake the original game's oblique
    // projection; the shadow sprite itself stays on a fixed drawing layer.
    m_ballShadowSprite.x.setWhole(swos.ballSprite.x.whole() + swos.ballSprite.z.whole() / 2 + 1);
    m_ballShadowSprite.y.setWhole(swos.ballSprite.y.whole() + swos.ballSprite.z.whole() / 4 + 1 + kBallShadowZ);
    m_ballShadowSprite.z.setWhole(kBallShadowZ);
}

static void calculateNextBallPosition()
{
    auto predictedX = swos.ballSprite.x;
    auto predictedY = swos.ballSprite.y;

    // SWOS does not predict any movement for a ball whose planar speed is zero, even if deltaZ is nonzero.
    if (swos.ballSprite.speed) {
        int stepShift;
        if (swos.ballSprite.deltaZ.raw() > 0) {
            stepShift = 3;
        } else {
            constexpr uint16_t kNearGroundPredictionHeight = 20;
            constexpr uint16_t kLowBallPredictionHeight = 30;
            constexpr uint16_t kHighBallPredictionHeight = 35;

            // The assembly compares the 16-bit whole part as unsigned. Ball height is normally nonnegative here.
            uint16_t height = static_cast<uint16_t>(swos.ballSprite.z.whole());
            if (height <= kNearGroundPredictionHeight)
                stepShift = 0;
            else if (height <= kLowBallPredictionHeight)
                stepShift = 1;
            else if (height <= kHighBallPredictionHeight)
                stepShift = 2;
            else
                stepShift = 3;
        }

        int simulationStep = 1 << stepShift;
        auto deltaX = FixedPoint::fromRaw(swos.ballSprite.deltaX.raw() * simulationStep);
        auto deltaY = FixedPoint::fromRaw(swos.ballSprite.deltaY.raw() * simulationStep);
        auto deltaZ = swos.ballSprite.deltaZ;
        auto gravity = FixedPoint::fromRaw(m_ballAirFriction * simulationStep);

        // The original divides only the initial height by the simulation step. Together with scaled planar deltas
        // and gravity, this reproduces its deliberately coarse landing-position estimate.
        auto predictedZ = FixedPoint::fromRaw(swos.ballSprite.z.raw() >> stepShift);

        do {
            predictedX += deltaX;
            predictedY += deltaY;
            deltaZ -= gravity;
            predictedZ += deltaZ;
        } while (predictedZ >= 0);
    }

    m_ballNextX = predictedX.whole();
    m_ballNextY = predictedY.whole();
}

void updateBallQuadrants()
{
    int16_t quadrantX;
    int16_t quadrantY;

    if (swos.gameStatePl == GameState::kInProgress) {
        // During play, player positioning reacts to the estimated landing point
        // rather than chasing the ball's current airborne position.
        quadrantX = m_ballNextX;
        quadrantY = m_ballNextY;
    } else if (swos.gameState == GameState::kKeeperHoldsTheBall || swos.gameState == GameState::kGoalOutLeft ||
        swos.gameState == GameState::kGoalOutRight) {
        quadrantX = kPitchCenterX;
        quadrantY = kPitchCenterY;
    } else {
        quadrantX = swos.foulXCoordinate;
        quadrantY = swos.foulYCoordinate;
    }

    static constexpr int16_t kBallXQuadrantLimits[] = { kLeftThrowInLine, 183, 285, 387, 489 };
    static constexpr int16_t kBallYQuadrantLimits[] = { kTopPitchLine, 220, 312, 403, 495, 586, 678 };
    constexpr int kHalfQuadrantWidth = 51;
    constexpr int kHalfQuadrantHeight = 45;
    constexpr int kNumHorizontalQuadrants = 5;
    constexpr int kPlayerOffsetScaleNumerator = 5;
    constexpr int kPlayerOffsetScaleDenominator = 15;

    int xQuadrant = 0;
    while (xQuadrant + 1 < std::size(kBallXQuadrantLimits) &&
        static_cast<uint16_t>(quadrantX) >= static_cast<uint16_t>(kBallXQuadrantLimits[xQuadrant + 1]))
        xQuadrant++;

    int yQuadrant = 0;
    while (yQuadrant + 1 < std::size(kBallYQuadrantLimits) &&
        static_cast<uint16_t>(quadrantY) >= static_cast<uint16_t>(kBallYQuadrantLimits[yQuadrant + 1]))
        yQuadrant++;

    auto xOffset = static_cast<int16_t>(quadrantX - kBallXQuadrantLimits[xQuadrant] - kHalfQuadrantWidth);
    auto yOffset = static_cast<int16_t>(quadrantY - kBallYQuadrantLimits[yQuadrant] - kHalfQuadrantHeight);
    swos.playerXQuadrantOffset = xOffset * kPlayerOffsetScaleNumerator / kPlayerOffsetScaleDenominator;
    swos.playerYQuadrantOffset = yOffset * kPlayerOffsetScaleNumerator / kPlayerOffsetScaleDenominator;
    swos.ballQuadrantIndex = xQuadrant + kNumHorizontalQuadrants * yQuadrant;
}
