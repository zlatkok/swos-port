#include "player.h"
#include "animation.h"
#include "ball.h"
#include "team.h"
#include "comments.h"
#include "sfx.h"
#include "pitchConstants.h"
#include "random.h"

static constexpr auto kGoalkeeperStrongDeflectBallSpeed = 3.0_speed;
static constexpr auto kGoalkeeperMediumDeflectBallSpeed = 2.0_speed;
static constexpr auto kGoalkeeperWeakDeflectBallSpeed = 1.0_speed;
static constexpr auto kGoalkeeperDeflectDeltaZ = 0.75_fp;
static constexpr int16_t kHeaderSpeedIncrease[] = {
    -0.65625_speed,
    -0.5625_speed,
    -0.46875_speed,
    -0.375_speed,
    -0.28125_speed,
    -0.1875_speed,
    -0.09375_speed,
    0.0_speed,
    1.001953125_speed,
    2.005859375_speed,
    3.009765625_speed,
    4.013671875_speed,
    5.017578125_speed,
};

enum class GoodPassCommand {
    kDoNothing = 0,
    kPlaySample = -1,
    kStopSample = -2,
} m_goodPassCommand = GoodPassCommand::kDoNothing;

enum class BallTackleStrength {
    kWeak,
    kStrong,
};

struct {
    int8_t x;
    int8_t y;
} static const kControlledBallJiggleOffsets[] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
};

static void doFlyingHeader(Sprite& headingPlayer, Sprite& ballSprite);
static void doLobHeader(Sprite& player, Sprite& ball);
static std::pair<Sprite *, uint32_t> getClosestNonControlledPlayerInDirection(
    Direction direction, const TeamGeneralInfo& team);
static void setPlayerJumpHeaderHitAnimationTable(Sprite& player);
static void playStopGoodPassSampleIfNeeded();
static void stopGoodPassSample();
static void enqueuePlayingGoodPassSample();
static void setBallDirectionAfterTackle(TeamGeneralInfo& team, const Sprite& player);
static uint32_t getTacklingPlayersDistanceMetric(const Sprite& player, const Sprite& opponent, BallTackleStrength strength);
static void tackleBall(TeamGeneralInfo& team, Sprite& player, BallTackleStrength strength);
static void doFlyingHeader(Sprite& headingPlayer, Sprite& ballSprite);

void updatePlayerSpeedAndFrameDelay(const TeamGeneralInfo& team, Sprite& player)
{
    if (player.state != PlayerState::kNormal ||
        (swos.gameStatePl == GameState::kInProgress && player.isGoalkeeper() && &player != team.controlledPlayer))
        return;

    static constexpr int16_t kPlayerSpeedsGameInProgress[] = {
        1.8125_speed, 1.90234375_speed, 1.9921875_speed, 2.08203125_speed,
        2.171875_speed, 2.26171875_speed, 2.3515625_speed, 2.44140625_speed,
    };
    static constexpr int16_t kPlayerSpeedsGameStopped[] = {
        2.21875_speed, 2.25_speed, 2.28125_speed, 2.3125_speed,
        2.34375_speed, 2.375_speed, 2.40625_speed, 2.4375_speed,
    };

    static_assert(std::size(kPlayerSpeedsGameInProgress) == std::size(kPlayerSpeedsGameStopped));

    auto speedTable = swos.gameStatePl == GameState::kInProgress ? kPlayerSpeedsGameInProgress : kPlayerSpeedsGameStopped;
    auto& playerInfo = getPlayerPointerFromShirtNumber(team, player);

    assert(static_cast<unsigned>(playerInfo.speed) < std::size(kPlayerSpeedsGameInProgress));
    player.speed = speedTable[playerInfo.speed];

    if (swos.runSlower) {
        // set speed to 62.5% (running slower when goal is scored)
        player.speed = 5 * player.speed / 8;
    }

    if ((team.playerNumber || team.playerCoachNumber) && player.injuryLevel) {
        static constexpr int16_t kInjuriesSpeedHandicap[] = {
            0.0_speed, -0.1875_speed, -0.25_speed, -0.3125_speed,
            -0.375_speed, -0.4375_speed, -0.5_speed, -0.5625_speed,
        };
        assert(player.injuryLevel / 32 < std::size(kInjuriesSpeedHandicap));
        player.speed += kInjuriesSpeedHandicap[player.injuryLevel / 32];
    }

    if (&player == team.controlledPlayer && team.playerHasBall) {
        // speed down to 87.5% - this is the player that controls the ball
        player.speed -= player.speed / 8;
    }

    if (team.playerNumber && &player == team.passToPlayerPtr && team.passingToPlayer &&
        (team.longPass || team.leftSpin || team.rightSpin) && swos.ballSprite.speed)
    {
        assert(swos.ballSprite.fullDirection <= 255 && player.fullDirection <= 255);
        int8_t directionDiff = swos.ballSprite.fullDirection - player.fullDirection;
        if (directionDiff <= 7 && directionDiff >= -7) {
            // set slower or faster speed depending on how much the player and ball direction overlap
            auto speed = directionDiff >= -5 && directionDiff <= 5 ? 0.5_speed : 1.0_speed;
            player.speed = speed;
        }
    }

    if (swos.gameStatePl != GameState::kInProgress) {
        if (swos.gameState == GameState::kFirstHalfEnded || swos.gameState == GameState::kGameEnded) {
            // slow down, and stop players gradually; slowdown in greater steps than accelerate when leaving
            player.speed = std::max(player.speed - swos.stoppageTimerTotal * 32, 0);
        } else if (swos.gameState == GameState::kGoingToHalftime || swos.gameState == GameState::kPlayersGoingToShower) {
            // player speed weighted with time elapsed from game/half end
            int leavePitchSlowdownFactor = std::min(swos.stoppageTimerTotal * 4, 100);
            // make players run gradually faster overt time when they're leaving pitch
            player.speed = player.speed * leavePitchSlowdownFactor / 100;
        }
    }

    // subtract from max, so slow players would have greater animation delay
    constexpr auto kMaxSpeed = 2.5_speed;
    player.frameDelay = std::max(kMaxSpeed - player.speed, 0) / 128 + 6;
}

//
// Set coordinates of player that has the ball. Make him dodge
// a little, by varying x and y coordinates by 1.
//
void updatePlayerWithBall(Sprite& player)
{
    struct {
        int8_t x;
        int8_t y;
    } static const kPlayerWithBallOffsets[] = {
        {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}
    };

    auto direction = static_cast<size_t>(player.direction);
    int destX = swos.ballSprite.x.whole() + kPlayerWithBallOffsets[direction].x;
    int destY = swos.ballSprite.y.whole() + kPlayerWithBallOffsets[direction].y;
    player.x.setWhole(destX);
    player.y.setWhole(destY);
    player.destX = destX;
    player.destY = destY;
    resetBothTeamSpinTimers();
}

//
// Set ball coordinates in regard to player that controls it.
// Ball is always about a pixel in front of player, wherever he may be turned.
//
static Sprite& positionBallInFrontOfPlayer(const Sprite& player)
{
    auto direction = static_cast<size_t>(player.direction);
    assert(direction < std::size(kControlledBallJiggleOffsets));

    auto& ball = swos.ballSprite;
    int destX = player.x.whole() + kControlledBallJiggleOffsets[direction].x;
    int destY = player.y.whole() + kControlledBallJiggleOffsets[direction].y;
    ball.speed = 0;
    ball.x.setWhole(destX);
    ball.y.setWhole(destY);
    ball.destX = destX;
    ball.destY = destY;
    ball.deltaZ >>= 1;
    resetBothTeamSpinTimers();

    return ball;
}

void updateBallWithControllingPlayer(const Sprite& player)
{
    auto& ball = positionBallInFrontOfPlayer(player);
    ball.z.setWhole(0);
}

void updateBallWithControllingGoalkeeper(const Sprite& player)
{
    auto& ball = positionBallInFrontOfPlayer(player);
    // if the ball is in the air, steady it down
    if (ball.deltaZ.raw() > 0)
        ball.deltaZ = -ball.deltaZ;
}

// Updates ball movement while a running player takes control. If both teams
// currently claim the ball, first resolve which team receives the possession
// lock; otherwise make the ball move just ahead of the player.
void calculateIfPlayerWinsBall(TeamGeneralInfo& team, Sprite& player, Direction direction)
{
    static constexpr uint8_t kLowerRatedPlayerWinThreshold[] = { 16, 17, 18, 19, 20, 21, 22, 23 };
    static constexpr int16_t kControlledBallSpeedIncrease[] = {
        0.25390625_speed, 0.2265625_speed, 0.19921875_speed, 0.171875_speed,
        0.14453125_speed, 0.1171875_speed, 0.08984375_speed, 0.0625_speed,
    };
    static constexpr uint16_t kDirectionChangeLimits[] = { 4, 5, 6, 8, 11, 14, 17, 21 };
    constexpr uint16_t kPossessionLockDuration = 12;
    constexpr uint16_t kDirectionChangeLimitReachedDuration = 8;
    constexpr int kBallNudgeDistance = 1;
    constexpr int kBallNudgeRange = 4;
    constexpr auto kAlignedMovementSpeedIncrease = 0.5_speed;

    assert(direction >= Direction::kLowestDirection && direction < Direction::kNumDirections);
    team.passInProgress = false;

    auto& opponent = *team.opponentTeam;
    if (!opponent.wonTheBallTimer && opponent.playerHasBall &&
        opponent.currentAllowedDirection != Direction::kNoDirection &&
        opponent.controlledPlayer) {
        // This apparently odd lookup is intentional: SWOS uses the opponent's
        // controlled-player ordinal to select a player from each team's roster.
        const auto& teamPlayer = getPlayerPointerFromShirtNumber(team, *opponent.controlledPlayer);
        const auto& opponentPlayer = getPlayerPointerFromShirtNumber(opponent, *opponent.controlledPlayer);
        auto teamRating = static_cast<uint8_t>(
            (static_cast<unsigned>(teamPlayer.tackling) + teamPlayer.ballControl) >> 1);
        auto opponentRating = static_cast<uint8_t>(
            (static_cast<unsigned>(opponentPlayer.tackling) + opponentPlayer.ballControl) >> 1);
        auto ratingDifference = static_cast<size_t>(teamRating < opponentRating ?
            opponentRating - teamRating : teamRating - opponentRating);
        assert(ratingDifference < std::size(kLowerRatedPlayerWinThreshold));

        // Preserve the original inverted-looking probability: the lower-rated
        // side wins when the random value is below 16 + the rating difference.
        auto lowerRatedTeam = teamRating < opponentRating ? &team : &opponent;
        auto winner = lowerRatedTeam;
        if ((SWOS::rand() & 0x1f) >= kLowerRatedPlayerWinThreshold[ratingDifference])
            winner = winner->opponentTeam.asPtr();

        winner->wonTheBallTimer = kPossessionLockDuration;

        auto& ball = swos.ballSprite;
        ball.speed = 0;
        ball.destX = ball.x.whole();
        ball.destY = ball.y.whole();
        return;
    }

    team.controlledPlDirection = direction;

    auto& ball = swos.ballSprite;
    // A rising ball is forced gently downward instead of retaining its upward
    // velocity when a running player takes control.
    if (ball.deltaZ.raw() > 0)
        ball.deltaZ.setRaw(-1);

    // When running straight upward and almost horizontally aligned with the
    // ball, move it one pixel away to prevent player and ball from overlapping.
    if (direction == Direction::kTop) {
        auto horizontalDistance = ball.x.whole() - player.x.whole();
        if (horizontalDistance > -kBallNudgeRange && horizontalDistance < kBallNudgeRange)
            ball.x.setWhole(ball.x.whole() + (horizontalDistance < 0 ? -kBallNudgeDistance : kBallNudgeDistance));
    }

    const auto& destination = getDefaultBallDestinations()[static_cast<size_t>(direction)];
    ball.destX = static_cast<int16_t>(player.x.whole() + destination.x);
    ball.destY = static_cast<int16_t>(player.y.whole() + destination.y);

    const auto& playerInfo = getPlayerPointerFromShirtNumber(team, player);
    assert(playerInfo.ballControl < std::size(kControlledBallSpeedIncrease));

    // Every other pair of ticks, lower ball-control skill lets the ball run
    // farther ahead. On the intervening ticks it exactly matches player speed.
    auto speedIncrease = swos.currentTick & 2 ?
        kControlledBallSpeedIncrease[playerInfo.ballControl] : 0;
    ball.speed = static_cast<int16_t>(
        static_cast<uint16_t>(player.speed) + static_cast<uint16_t>(speedIncrease));

    // fullDirection is continuous (0..255), while direction has eight sectors.
    // Add half a turn before the signed comparison to measure circular distance;
    // movement within 90 degrees of the facing direction receives another boost.
    auto centeredDirectionDifference = static_cast<int8_t>(
        static_cast<uint8_t>(player.fullDirection + 128) -
        static_cast<uint8_t>(static_cast<int>(player.direction) << 5));
    if (centeredDirectionDifference >= 64 || centeredDirectionDifference <= -64) {
        ball.speed = static_cast<int16_t>(
            static_cast<uint16_t>(ball.speed) + static_cast<uint16_t>(kAlignedMovementSpeedIncrease));
    }

    if (team.controlledPlDirection != player.direction) {
        team.ballDirectionChangeTimer++;
        if (team.ballDirectionChangeTimer >= kDirectionChangeLimits[playerInfo.ballControl])
            team.wonTheBallTimer = kDirectionChangeLimitReachedDuration;
    }

    resetBothTeamSpinTimers();
}

// Starts a normal kick and applies the player's shooting or finishing skill
// when the player is positioned and facing for a shot on goal.
void playerKickingBall(TeamGeneralInfo& team, const Sprite& player)
{
    static constexpr auto kBallKickingSpeed = 4.3125_speed;
    static constexpr auto kBallKickingDeltaZ = 1.25_fp;
    static constexpr int16_t kBallSpeedIncreaseByShooting[] = {
        -0.75_speed, -0.52734375_speed, -0.31640625_speed, -0.10546875_speed,
        0.10546875_speed, 0.31640625_speed, 0.52734375_speed, 0.75_speed,
    };
    static constexpr int16_t kBallSpeedIncreaseByFinishing[] = {
        -0.5625_speed, -0.3125_speed, -0.0625_speed, 0.1875_speed,
        0.4375_speed, 0.6875_speed, 0.9375_speed, 1.1875_speed,
    };
    constexpr int kUpperFinishingShotYLimit = 204;
    constexpr int kLowerFinishingShotYLimit = 694;

    static_assert(sizeof(kBallSpeedIncreaseByShooting) == sizeof(kBallSpeedIncreaseByFinishing));

    swos.stateGoal = 0;
    team.controlledPlDirection = player.direction;

    auto& ball = swos.ballSprite;
    assert(player.direction >= Direction::kLowestDirection && player.direction < Direction::kNumDirections);
    const auto& destination = getBallDestCoordinatesTable()[static_cast<size_t>(player.direction)];
    ball.destX = static_cast<int16_t>(ball.x.whole() + destination.x);
    ball.destY = static_cast<int16_t>(ball.y.whole() + destination.y);
    ball.speed = kBallKickingSpeed;
    ball.deltaZ = kBallKickingDeltaZ;

    resetBothTeamSpinTimers();

    bool throwInInProgress =
        swos.gameState >= GameState::kThrowInForwardRight && swos.gameState <= GameState::kThrowInBackLeft;
    if (swos.gameStatePl == GameState::kInProgress || !throwInInProgress) {
        auto ballY = ball.y.whole();
        bool shootingAtTopGoal = &team != &swos.topTeamData &&
            ballY <= kPitchUpperThirdYLimit && isUpwardFacing(player.direction);
        bool shootingAtBottomGoal = &team == &swos.topTeamData &&
            ballY >= kPitchMiddleThirdYLimit && isDownwardFacing(player.direction);

        if (shootingAtTopGoal || shootingAtBottomGoal) {
            const auto& playerInfo = getPlayerPointerFromShirtNumber(team, player);
            auto ballX = ball.x.whole();
            bool finishingShot = ballX > kGoalAttemptLeft && ballX <= kGoalAttemptRight &&
                (ballY < kUpperFinishingShotYLimit || ballY >= kLowerFinishingShotYLimit);

            const auto& speedIncreaseTable = finishingShot ? kBallSpeedIncreaseByFinishing : kBallSpeedIncreaseByShooting;
            auto skill = finishingShot ? playerInfo.finishing : playerInfo.shooting;
            assert(skill < std::size(speedIncreaseTable));
            ball.speed = static_cast<int16_t>(
                static_cast<uint16_t>(ball.speed) + static_cast<uint16_t>(speedIncreaseTable[skill]));
        }

        bool goalkeeperKickingSideways = player.isGoalkeeper() &&
            (player.direction == Direction::kRight || player.direction == Direction::kLeft);
        if (!goalkeeperKickingSideways && static_cast<int16_t>(team.opponentTeam->goalkeeperSavedCommentTimer) >= 0)
            team.spinTimer = 0;

        team.passInProgress = false;
        playKickSample();
    }
}

// Runs when a player makes contact with the ball while doing static header.
// Applies ball speed and destination, redirects its vertical movement and plays
// the kick sample.
void playerHittingStaticHeader(TeamGeneralInfo& team, Sprite& player)
{
    constexpr auto kStaticHeaderBallSpeed = 3.5_speed;

    team.passInProgress = false;

    if (player.animTable != getStaticHeaderHitAnimTable()) {
        auto directionDifference =
            (static_cast<int>(team.currentAllowedDirection) - static_cast<int>(player.direction)) & 7;

        if (directionDifference != 0 && directionDifference != 4) {
            auto turnStep = directionDifference < 4 ? 1 : -1;
            player.direction = static_cast<Direction>((static_cast<int>(player.direction) + turnStep) & 7);

            // A static header can turn at most 90 degrees toward the controls.
            if (player.direction != team.currentAllowedDirection) {
                player.direction = static_cast<Direction>((static_cast<int>(player.direction) + turnStep) & 7);
            }
        }
    }

    setPlayerAnimationTableAndPictureIndex(player, getStaticHeaderHitAnimTable());

    auto ballDirection = team.currentAllowedDirection;
    if (ballDirection == Direction::kNoDirection)
        ballDirection = player.direction;
    team.controlledPlDirection = ballDirection;

    auto& ball = swos.ballSprite;
    assert(static_cast<size_t>(ballDirection) < getDefaultBallDestinations().size());
    const auto& destination = getDefaultBallDestinations()[static_cast<size_t>(ballDirection)];
    ball.destX = static_cast<int16_t>(ball.x.whole() + destination.x);
    ball.destY = static_cast<int16_t>(ball.y.whole() + destination.y);

    const auto& playerInfo = getPlayerPointerFromShirtNumber(team, player);
    assert(playerInfo.heading < std::size(kHeaderSpeedIncrease));
    ball.speed = static_cast<int16_t>(
        static_cast<uint16_t>(kStaticHeaderBallSpeed) +
        static_cast<uint16_t>(kHeaderSpeedIncrease[playerInfo.heading]));

    auto negatedDeltaZ = -static_cast<int64_t>(ball.deltaZ.raw());
    auto halvedDeltaZ = negatedDeltaZ / 2;
    if (negatedDeltaZ < 0 && negatedDeltaZ % 2)
        halvedDeltaZ--;
    ball.deltaZ.setRaw(static_cast<int32_t>(halvedDeltaZ));

    player.isHeadingBall = true;
    playKickSample();
    resetBothTeamSpinTimers();
}

// Hitting flying or lob header. Only triggers on ball contact.
void playerHittingJumpHeader(TeamGeneralInfo& team, Sprite& player)
{
    constexpr auto kJumpHeaderBallDeltaZ = 0.625_fp;

    team.passInProgress = false;

    auto controlsDirection = team.currentAllowedDirection;
    if (controlsDirection == Direction::kNoDirection)
        controlsDirection = player.direction;

    auto& ball = swos.ballSprite;
    ball.deltaZ = kJumpHeaderBallDeltaZ;

    auto playerSpeed = static_cast<uint16_t>(player.speed);
    ball.speed = static_cast<int16_t>(playerSpeed + (playerSpeed >> 2));

    auto ballDirection = player.direction;
    if (team.currentAllowedDirection == Direction::kNoDirection) {
        doFlyingHeader(player, ball);
    } else {
        auto relativeDirection = static_cast<Direction>(
            (static_cast<int>(player.direction) - static_cast<int>(controlsDirection)) & 7);

        switch (relativeDirection) {
        case Direction::kTop:
            break;
        case Direction::kRight:
            doFlyingHeader(player, ball);
            [[fallthrough]];
        case Direction::kTopRight:
            ballDirection = static_cast<Direction>(static_cast<int>(ballDirection) - 1);
            break;
        case Direction::kBottomRight:
            doLobHeader(player, ball);
            ballDirection = static_cast<Direction>(static_cast<int>(ballDirection) - 1);
            break;
        case Direction::kBottom:
            doLobHeader(player, ball);
            break;
        case Direction::kBottomLeft:
            doLobHeader(player, ball);
            ballDirection = static_cast<Direction>(static_cast<int>(ballDirection) + 1);
            break;
        case Direction::kLeft:
            doFlyingHeader(player, ball);
            [[fallthrough]];
        case Direction::kTopLeft:
            ballDirection = static_cast<Direction>(static_cast<int>(ballDirection) + 1);
            break;
        default:
            assert(false);
        }
    }

    ballDirection = static_cast<Direction>(static_cast<int>(ballDirection) & 7);
    team.controlledPlDirection = ballDirection;

    const auto& destination = getDefaultBallDestinations()[static_cast<size_t>(ballDirection)];
    ball.destX = static_cast<int16_t>(ball.x.whole() + destination.x);
    ball.destY = static_cast<int16_t>(ball.y.whole() + destination.y);

    const auto& playerInfo = getPlayerPointerFromShirtNumber(team, player);
    assert(playerInfo.heading < std::size(kHeaderSpeedIncrease));
    ball.speed = static_cast<int16_t>(
        static_cast<uint16_t>(ball.speed) + static_cast<uint16_t>(kHeaderSpeedIncrease[playerInfo.heading]));

    player.speed = static_cast<int16_t>(static_cast<uint16_t>(player.speed) >> 1);
    player.isHeadingBall = true;
    playKickSample();
    resetBothTeamSpinTimers();
}

// Strong tackle: ball speed is 125% of player speed (100% for the CPU), then
// player speed is reduced to 50%.
void playerTackledTheBallStrong(TeamGeneralInfo& team, Sprite& player)
{
    tackleBall(team, player, BallTackleStrength::kStrong);
}

// Weak tackle: player speed goes down to about 50%, then about 75% of its
// original speed is transferred to the ball.
void playerTackledTheBallWeak(TeamGeneralInfo& team, Sprite& player)
{
    tackleBall(team, player, BallTackleStrength::kWeak);
}

void goalkeeperClaimedTheBall(TeamGeneralInfo& team, Sprite& goalKeeper, Sprite& ballSprite)
{
    swos.lastPlayerBeforeGoalkeeper = swos.lastPlayerPlayed;
    swos.lastTeamScored = swos.lastTeamPlayed;
    ballSprite.speed = 0;

    if (&team == &swos.bottomTeamData) {
        swos.cameraDirection = Direction::kTop;
        // everything except bottom-left, bottom, bottom-right
        swos.playerTurnFlags = makeDirectionMask(Direction::kTop, Direction::kTopRight, Direction::kRight,
            Direction::kLeft, Direction::kTopLeft);
    } else {
        swos.cameraDirection = Direction::kBottom;
        // everything except top-left, top, top-right
        swos.playerTurnFlags = makeDirectionMask(Direction::kRight, Direction::kBottomRight,
            Direction::kBottom, Direction::kBottomLeft, Direction::kLeft);
    }

    // prevent CPU from turning all the way left or right when the goalkeeper has the ball
    // probably so it doesn't shoot the ball non-sensically into the stands
    if (!team.playerNumber) {
        // everything except left and right
        auto mask = makeDirectionMask(Direction::kTop, Direction::kTopRight, Direction::kBottomRight,
            Direction::kBottom, Direction::kBottomLeft, Direction::kTopLeft);
        swos.playerTurnFlags &= mask;
    }

    auto keeperTeam = swos.forceLeftTeam == 1 ? &swos.topTeamData : &team;

    swos.gameState = GameState::kKeeperHoldsTheBall;
    swos.breakCameraMode = CameraBreakMode::kInactive;
    swos.foulXCoordinate = ballSprite.x.whole();
    swos.foulYCoordinate = ballSprite.y.whole();
    swos.gameStatePl = GameState::kStopped;
    swos.lastTeamPlayedBeforeBreak = keeperTeam;
    swos.stoppageTimerTotal = 0;
    swos.stoppageTimerActive = 0;

    stopAllPlayers();

    if (!keeperTeam->goalkeeperDivingRight && !keeperTeam->goalkeeperDivingLeft) {
        keeperTeam->controlledPlayer = &goalKeeper;
        keeperTeam->ballOutOfPlayOrKeeper = true;
        goalKeeper.direction = swos.cameraDirection;
        keeperTeam->goaliePlayingOrOut = true;
        updatePlayerWithBall(goalKeeper);
        swos.cameraXVelocity = 0;
        swos.cameraYVelocity = 0;
    }
}

void goalkeeperDeflectedBall(const TeamGeneralInfo& team, Sprite& ballSprite)
{
    auto direction = &team == &swos.bottomTeamData ? Direction::kTop : Direction::kBottom;
    const auto& destination = getDefaultBallDestinations()[static_cast<size_t>(direction)];
    ballSprite.destX = ballSprite.x.whole() + destination.x + ((swos.currentGameTick & 0x1f) << 5) - 512;
    ballSprite.destY = ballSprite.y.whole() + destination.y;

    const auto& shotChanceTable = *team.shotChanceTable.asConst();
    int chance = (swos.currentGameTick & 0x3c) >> 2;
    int deflectionSpeed;
    if (chance < shotChanceTable.strongDeflectionChance)
        deflectionSpeed = kGoalkeeperStrongDeflectBallSpeed;
    else if (chance - shotChanceTable.strongDeflectionChance < shotChanceTable.mediumDeflectionChance)
        deflectionSpeed = kGoalkeeperMediumDeflectBallSpeed;
    else
        deflectionSpeed = kGoalkeeperWeakDeflectBallSpeed;

    ballSprite.speed = deflectionSpeed + (swos.currentGameTick & 0x1ff) - 256;
    ballSprite.deltaZ.setRaw(kGoalkeeperDeflectDeltaZ.raw() + ((swos.currentGameTick & 0x7f) << 8) - 16384);
    resetBothTeamSpinTimers();
    playKickSample();
}

void doPass(TeamGeneralInfo& team, const Sprite& passingPlayer)
{
    // lower values in the table means higher chance to fail the pass, 0 = can't fail
    static constexpr uint8_t kAiFailedPassChance[] = { 6, 4, 3, 2, 1, 0, 0, 0 };
    // distance thresholds for each speed level from the table below, in pixels, squared
    static constexpr uint32_t kPassDistanceThresholds[] = { 2'500, 10'000, 22'500, 40'000, 62'500, 90'000, 122'500 };
    static constexpr int16_t kPassSpeeds[] = {
        3.0_speed, 3.25_speed, 3.5_speed, 3.666015625_speed,
        3.83203125_speed, 4.0_speed, 4.166015625_speed, 4.33203125_speed,
    };
    // how much the passing ball speed increases for each skill level, in pixels per tick
    static constexpr int16_t kPassSpeedIncreaseBySkill[] = {
        0.0_speed, 0.09375_speed, 0.1875_speed, 0.28125_speed,
        0.375_speed, 0.5_speed, 0.625_speed, 0.75_speed,
    };
    constexpr auto kFreePassSpeed = 3.5_speed;

    auto wrapAdd = [](int16_t lhs, int16_t rhs) {
        return static_cast<int16_t>(static_cast<uint16_t>(lhs) + static_cast<uint16_t>(rhs));
    };
    auto scale = [](int16_t value, unsigned shift) {
        return static_cast<int16_t>(static_cast<uint16_t>(value) << shift);
    };

    m_goodPassCommand = GoodPassCommand::kDoNothing;
    swos.stateGoal = 0;
    team.controlledPlDirection = passingPlayer.direction;

    const auto& playerInfo = getPlayerPointerFromShirtNumber(team, passingPlayer);
    assert(playerInfo.passing < std::size(kAiFailedPassChance));

    auto& ball = swos.ballSprite;
    auto [receiver, receiverDistance] = getClosestNonControlledPlayerInDirection(passingPlayer.direction, team);
    if (receiver) {
        team.passToPlayerPtr = receiver;
        team.passingBall = true;
        team.passingToPlayer = true;

        int16_t dx = receiver->x.whole() - ball.x.whole();
        int16_t dy = receiver->y.whole() - ball.y.whole();
        int aiFailureValue = (swos.currentGameTick & 0x1e) >> 1;
        // compare 0..7 pseudo-random variable based on the current game tick with value from the table
        bool botchedAiPass = swos.gameStatePl == GameState::kInProgress && !team.playerNumber &&
            aiFailureValue < kAiFailedPassChance[playerInfo.passing];

        if (botchedAiPass) {
            // CPU passing, there's a chance to fail the pass, unless the player has high passing skill
            // skew the pass by halving exactly one component before extending its trajectory
            m_goodPassCommand = GoodPassCommand::kStopSample;
            if (swos.currentGameTick & 0x20)
                dy >>= 1;
            else
                dx >>= 1;
            ball.destX = wrapAdd(ball.x.whole(), scale(dx, 5));
            ball.destY = wrapAdd(ball.y.whole(), scale(dy, 5));
        } else {
            // there's a receiver, extend the pass trajectory until it hits the pitch boundaries
            if (!dx && !dy)
                dx = 1;

            while (true) {
                auto nextX = wrapAdd(ball.x.whole(), dx);
                auto nextY = wrapAdd(ball.y.whole(), dy);
                if (nextX < 0 || nextX >= 672 || nextY < 0 || nextY >= 880)
                    break;
                dx = scale(dx, 1);
                dy = scale(dy, 1);
            }
            ball.destX = wrapAdd(ball.x.whole(), dx);
            ball.destY = wrapAdd(ball.y.whole(), dy);
        }

        // calculate the speed of the pass based on the distance to the receiver and the passing skill
        size_t speedIndex = 0;
        while (speedIndex < std::size(kPassDistanceThresholds) && receiverDistance >= kPassDistanceThresholds[speedIndex])
            speedIndex++;
        ball.speed = kPassSpeeds[speedIndex] + kPassSpeedIncreaseBySkill[playerInfo.passing];

        m_goodPassCommand = GoodPassCommand::kPlaySample;
    } else {
        // free form pass
        const auto& offset = getBallDestCoordinatesTable()[static_cast<int>(passingPlayer.direction)];
        ball.destX = ball.x.whole() + offset.x;
        ball.destY = ball.y.whole() + offset.y;
        ball.speed = kFreePassSpeed;
    }

    resetBothTeamSpinTimers();
    if (team.playerNumber)
        team.spinTimer = 0;
    team.passInProgress = true;

    // don't play kick sample for throw-in pass
    bool throwIn = swos.gameState >= GameState::kThrowInForwardRight && swos.gameState <= GameState::kThrowInBackLeft;
    if (swos.gameStatePl == GameState::kInProgress || !throwIn) {
        playKickSample();
        playStopGoodPassSampleIfNeeded();
    }
}

//
// Set how much time will player be lying on the ground after tackle.
// Better tackling skill means less time.
//
void setPlayerDowntimeAfterTackle(const TeamGeneralInfo& team, Sprite& player)
{
    static constexpr uint8_t kStrongTackleDownTime[] = { 30, 27, 24, 21, 18, 15, 12, 9 };
    constexpr uint8_t kWeakTackleDownTime = 3;

    const auto& playerInfo = getPlayerPointerFromShirtNumber(team, player);
    assert(playerInfo.tackling < std::size(kStrongTackleDownTime));
    player.playerDownTimer = player.tacklingTimer == static_cast<uint16_t>(-1) ?
        kWeakTackleDownTime : kStrongTackleDownTime[playerInfo.tackling];
}

void setJumpHeaderHitAnimTable(Sprite& player)
{
    // this is an animation switch from previous heading "intro" animation
    if (player.animTable != getJumpHeaderHitAnimTable() && !player.isHeadingBall &&
        player.playerDownTimer == 40 && static_cast<uint16_t>(player.frameSwitchCounter) <= 2 &&
        swos.currentGameTick & 0x0200)
    {
        setPlayerAnimationTableAndPictureIndex(player, getJumpHeaderHitAnimTable());
        player.speed >>= 1;
    }
}

const PlayerInfo& getPlayerPointerFromShirtNumber(const TeamGeneralInfo& team, const Sprite& player)
{
    assert(team.inGameTeamPtr);
    assert(player.playerOrdinal - 1 < std::size(team.inGameTeamPtr->players));

    return team.inGameTeamPtr->players[player.playerOrdinal - 1];
}

//
// Update ball speed and delta Z for a lob header, and switch player animation table.
//
static void doLobHeader(Sprite& player, Sprite& ball)
{
    constexpr auto kLobHeaderBallDeltaZ = 2.25_fp;

    ball.deltaZ = kLobHeaderBallDeltaZ;
    ball.speed -= ball.speed >> 4;  // speed = 93.75% of original speed
    setPlayerJumpHeaderHitAnimationTable(player);
}

//
// Return pointer to a player closest to the ball facing approximately given direction,
// and his distance from the ball. Can be null.
//
static std::pair<Sprite *, uint32_t> getClosestNonControlledPlayerInDirection(
    Direction direction, const TeamGeneralInfo& team)
{
    assert(direction >= Direction::kLowestDirection && direction < Direction::kNumDirections);

    auto closestDistance = std::numeric_limits<uint32_t>::max();
    Sprite *closestPlayer{};

    // expand the direction to full 0-224 range
    int8_t fullDirection = static_cast<int>(direction) * 32;

    for (int i = 0; i < kNumPlayersInLineup; i++) {
        auto player = team.players[i];
        if (team.controlledPlayer != player && !player->sentAway && player->state == PlayerState::kNormal) {
            int8_t directionDiff = static_cast<int8_t>(player->fullDirection) - fullDirection;
            if (directionDiff >= -16 && directionDiff <= 16) {
                if (player->ballDistance < closestDistance) {
                    closestDistance = player->ballDistance;
                    closestPlayer = player;
                }
            }
        }
    }

    return { closestPlayer, closestDistance };
}

//
// Switch into table for flying and lob headers.
//
static void setPlayerJumpHeaderHitAnimationTable(Sprite& player)
{
    if (player.frameSwitchCounter >= 0 && player.frameSwitchCounter <= 2)
        setPlayerAnimationTableAndPictureIndex(player, getJumpHeaderHitAnimTable());
}

static void playStopGoodPassSampleIfNeeded()
{
    if (m_goodPassCommand == GoodPassCommand::kPlaySample) {
        m_goodPassCommand = GoodPassCommand::kDoNothing;
        enqueuePlayingGoodPassSample();
    } else if (m_goodPassCommand == GoodPassCommand::kStopSample) {
        m_goodPassCommand = GoodPassCommand::kDoNothing;
        stopGoodPassSample();
    }
}

static void stopGoodPassSample()
{
    swos.playingGoodPassTimer = -1;
}

static void enqueuePlayingGoodPassSample()
{
    swos.playingGoodPassTimer = -1;
    if (++swos.goodPassTimer == 5) {
        swos.goodPassTimer = 0;
        swos.playingGoodPassTimer = 10;
    }
}

static void setBallDirectionAfterTackle(TeamGeneralInfo& team, const Sprite& player)
{
    auto controlsDirection = team.currentAllowedDirection;
    if (controlsDirection == Direction::kNoDirection)
        controlsDirection = player.direction;

    int direction = static_cast<int>(player.direction);
    int directionDifference = (direction - static_cast<int>(controlsDirection)) & 7;
    // If not facing the same or opposite direction, nudge the ball one step
    // from the player's direction toward the controls direction.
    if (directionDifference != 0 && directionDifference != 4)
        direction += directionDifference < 4 ? -1 : 1;

    auto ballDirection = static_cast<Direction>(direction & 7);
    team.controlledPlDirection = ballDirection;

    auto& ball = swos.ballSprite;
    const auto& destination = getDefaultBallDestinations()[static_cast<size_t>(ballDirection)];
    ball.destX = ball.x.whole() + destination.x;
    ball.destY = ball.y.whole() + destination.y;
}

static uint32_t getTacklingPlayersDistanceMetric(const Sprite& player, const Sprite& opponent, BallTackleStrength strength)
{
    int16_t deltaX = static_cast<int16_t>(player.x.whole() - opponent.x.whole());
    int16_t deltaY = static_cast<int16_t>(player.y.whole() - opponent.y.whole());

    uint32_t distanceSquared;
    if (strength == BallTackleStrength::kWeak) {
        // Preserve an original quirk: weak tackles sign-extend the X delta
        // for one operand and zero-extend it for the other.
        distanceSquared =
            static_cast<int32_t>(deltaX) * static_cast<uint16_t>(deltaX);
    } else {
        distanceSquared = static_cast<uint32_t>(static_cast<int32_t>(deltaX) * deltaX);
    }

    return distanceSquared + static_cast<uint32_t>(static_cast<int32_t>(deltaY) * deltaY);
}

static void tackleBall(TeamGeneralInfo& team, Sprite& player, BallTackleStrength strength)
{
    if (strength == BallTackleStrength::kWeak)
        team.passInProgress = false;

    setBallDirectionAfterTackle(team, player);

    auto& ball = swos.ballSprite;
    auto playerSpeed = static_cast<uint16_t>(player.speed);
    if (strength == BallTackleStrength::kStrong) {
        // A CPU tackle transfers 100% of the speed; a player-controlled
        // tackle transfers 125%. The tackling player is then slowed by 50%.
        ball.speed = static_cast<int16_t>(
            team.playerNumber ? playerSpeed + (playerSpeed >> 2) : playerSpeed);
        player.speed = static_cast<int16_t>(playerSpeed >> 1);
    } else {
        // A weak tackle first reduces the player speed by roughly half and
        // forces it odd, then transfers 150% of that speed to the ball.
        playerSpeed -= playerSpeed >> 1;
        playerSpeed |= 1;
        player.speed = static_cast<int16_t>(playerSpeed);
        ball.speed = static_cast<int16_t>(playerSpeed + (playerSpeed >> 1));
    }

    player.tackleState = TackleState::kTackling;

    auto opponent = team.opponentTeam->controlledPlayer.asPtr();
    if (opponent && opponent->ballDistance >= 9 && getTacklingPlayersDistanceMetric(player, *opponent, strength) > 32) {
        if (strength == BallTackleStrength::kStrong)
            playGoodTackleComment();
        player.tackleState = TackleState::kGoodTackle;
    }

    playKickSample();
    resetBothTeamSpinTimers();
}

static void doFlyingHeader(Sprite& headingPlayer, Sprite& ballSprite)
{
    static constexpr auto kFlyingHeaderBallDeltaZ = 2.0_fp;
    ballSprite.deltaZ = kFlyingHeaderBallDeltaZ;
    ballSprite.speed -= ballSprite.speed >> 2;  // reduce ball speed to 75%
    setPlayerJumpHeaderHitAnimationTable(headingPlayer);
}
