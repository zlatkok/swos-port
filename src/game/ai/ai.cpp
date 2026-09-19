#include "ai.h"
#include "amigaMode.h"
#include "pitch/pitchConstants.h"
#include "result.h"
#include "updateSprite.h"
#include "random.h"

enum class PitchEnd {
    kTop,
    kBottom,
};

struct CpuGoalContext {
    int ballX;
    int ballY;
    uint32_t squaredDistance;
    DeltasAndAngle vector;
};

static PitchEnd m_cpuAftertouchTargetGoal;

static uint16_t m_cpuShotAftertouchTimer;
static uint16_t m_cpuResumePlayTimer;
static uint16_t m_cpuMaximumStoppageTime;
static int16_t m_cpuResumePlayTurnStep = 1;
static constexpr std::array<DirectionMask, 6> kCpuThrowInDirections{3, 6, 12, 129, 192, 96};
static constexpr std::array<int16_t, 3> kCpuLeftSpinDirections{-1, -2, -3};
static constexpr std::array<int16_t, 3> kCpuRightSpinDirections{1, 2, 3};
static constexpr std::array<int16_t, 3> kCpuLongKickDirections{0, -999, 4};

static CpuGoalContext calculateCpuGoalContext(const TeamGeneralInfo& team);
static void updateCpuResumePlayTurnDirection(TeamGeneralInfo& team, Direction playerDirection);
static void setCpuGoalwardAftertouchDirection(TeamGeneralInfo& team);
static void clearCpuControls(TeamGeneralInfo& team);
static bool handleCpuResultScreen(TeamGeneralInfo& team);
static bool decideWhetherCpuPlayerFires(TeamGeneralInfo& team, const Sprite& player, Direction playerDirection);
static void updateCpuPreResumePlayDirection(TeamGeneralInfo& team, Direction playerDirection, uint16_t goalAngle);
static bool isCpuReadyToResumePlay(const TeamGeneralInfo& team);
static std::pair<Sprite *, uint32_t> findClosestTeammateFacing(const TeamGeneralInfo& team,
    const Sprite& controlledPlayer, Direction direction);
static std::pair<Sprite *, uint32_t> findClosestPlayerToBallFacing(const TeamGeneralInfo& team, int direction);

// Update the controls for one CPU-managed team. Stoppages and active play are
// deliberately handled as separate phases so each decision path is exclusive.
void updateCpuPlayerControls(TeamGeneralInfo& team)
{
    if (team.resetControls)
        return;

    if (m_cpuShotAftertouchTimer)
        m_cpuShotAftertouchTimer--;
    if (m_cpuResumePlayTimer)
        m_cpuResumePlayTimer--;

    const auto cpuRandom = static_cast<uint16_t>(SWOS::rand());
    team.cpuControlUpdateCounter++;
    clearCpuControls(team);

    if (swos.g_inSubstitutesMenu)
        return;

    auto player = team.controlledPlayer.asPtr();
    auto passTarget = team.passToPlayerPtr.asPtr();
    auto playerDirection = player ? player->direction : Direction::kNoDirection;
    auto goal = calculateCpuGoalContext(team);
    auto goalAngle = static_cast<uint8_t>(std::max(goal.vector.direction, 0));
    auto goalDirection = static_cast<Direction>((goalAngle + 16 & 0xff) >> 5);

    auto setDefaultDirection = [&] {
        team.currentAllowedDirection = team.plVeryCloseToBall ? goalDirection : playerDirection;
    };

    auto quickFireWithClosestPlayer = [&] {
        team.cpuDecisionCooldown = 0;
        if (!m_cpuResumePlayTimer && isCpuReadyToResumePlay(team)) {
            m_cpuResumePlayTimer = 15;
            team.currentAllowedDirection = playerDirection;
            team.quickFire = true;
            if (m_cpuMaximumStoppageTime <= swos.stoppageTimerActive)
                m_cpuMaximumStoppageTime = swos.stoppageTimerActive;
        }
    };

    auto findClosestForRestart = [&] {
        auto [closestPlayer, distance] = findClosestTeammateFacing(team, *player, playerDirection);
        if (closestPlayer)
            quickFireWithClosestPlayer();
        else
            updateCpuResumePlayTurnDirection(team, playerDirection);
    };

    auto applyRestartKick = [&] {
        if (playerDirection == Direction::kNoDirection)
            return;

        auto angleDifference = static_cast<int8_t>(
            static_cast<uint8_t>(static_cast<int>(playerDirection) << 5) - goalAngle);
        if (m_cpuMaximumStoppageTime <= swos.stoppageTimerActive)
            m_cpuMaximumStoppageTime = swos.stoppageTimerActive;
        m_cpuResumePlayTimer = 15;

        auto state = swos.gameState;
        uint16_t strength;
        if (state == GameState::kPenalty || state == GameState::kPenaltyShootout)
            strength = 0;
        else if (state == GameState::kCornerLeft || state == GameState::kCornerRight)
            strength = 1;
        else {
            auto freeKick = state >= GameState::kFreeKickOuterLeft &&
                state <= GameState::kFreeKickOuterRight;
            if (!freeKick && !(cpuRandom & 0x18))
                strength = 2;
            else if (goal.squaredDistance < 28800)
                strength = 0;
            else if (goal.squaredDistance < 57800)
                strength = 1;
            else
                strength = 2;
        }

        team.cpuAftertouchStrength = strength;
        team.currentAllowedDirection = playerDirection;
        team.normalFire = true;

        int16_t spin = 0;
        if (state == GameState::kCornerLeft || state == GameState::kCornerRight) {
            auto randomValue = cpuRandom & 7;
            auto testedDirection = Direction::kNoDirection;
            if (randomValue < 3) {
                spin = 1;
                testedDirection = static_cast<Direction>((static_cast<int>(playerDirection) + 1) & 7);
            } else if (randomValue < 6) {
                spin = -1;
                testedDirection = static_cast<Direction>((static_cast<int>(playerDirection) - 1) & 7);
            }
            if (testedDirection == Direction::kNoDirection ||
                !(swos.playerTurnFlags & makeDirectionMask(testedDirection)))
                spin = 0;
        } else if (state != GameState::kPenalty && state != GameState::kPenaltyShootout) {
            auto deriveFromAngle = true;
            if (state == GameState::kFoul) {
                auto ballY = swos.ballSprite.y.whole();
                deriveFromAngle = &team == &swos.topTeamData ? ballY < 682 : ballY > 216;
            }
            if (deriveFromAngle)
                spin = angleDifference < 0 ? 1 : -1;
        }
        team.cpuBallSpinDirection = spin;
    };

    auto applyBallAftertouch = [&] {
        auto strength = team.cpuAftertouchStrength;
        const std::array<int16_t, 3> *table = nullptr;

        if (!swos.playingPenalties && !swos.penalty && (cpuRandom & 1)) {
            auto spin = static_cast<int16_t>(team.cpuBallSpinDirection);
            if (spin < 0) {
                table = &kCpuLeftSpinDirections;
            } else if (spin > 0) {
                table = &kCpuRightSpinDirections;
            }
        }
        if (!table) {
            if (strength == 1) {
                team.currentAllowedDirection = Direction::kNoDirection;
                return;
            }
            table = &kCpuLongKickDirections;
        }

        assert(strength < table->size());
        auto offset = (*table)[strength];
        team.currentAllowedDirection = static_cast<Direction>(
            (static_cast<int>(team.controlledPlDirection) + offset) & 7);
    };

    auto tryNormalShot = [&](int8_t angleDifference) {
        if (!m_cpuResumePlayTimer && isCpuReadyToResumePlay(team)) {
            team.cpuAftertouchStrength = 0;
            m_cpuResumePlayTimer = 15;
            team.currentAllowedDirection = playerDirection;
            team.normalFire = true;
            team.cpuBallSpinDirection = angleDifference < 0 ? 1 : -1;
        }
    };

    auto turnAwayFromNearbyPlayer = [&] {
        if (!team.plVeryCloseToBall) {
            team.currentAllowedDirection = playerDirection;
        } else {
            auto step = swos.currentGameTick & 0x80 ? 1 : -1;
            team.currentAllowedDirection = static_cast<Direction>((static_cast<int>(playerDirection) + step) & 7);
        }
    };

    auto setDecisionCooldown = [&] {
        if (!team.cpuDecisionCooldown && (swos.frameCount & 0x7f) < 32) {
            team.cpuDecisionCooldown = 4;
            turnAwayFromNearbyPlayer();
        } else {
            setDefaultDirection();
        }
    };

    auto applyShotTurnBias = [&] {
        team.cpuAftertouchStrength = goal.squaredDistance > 115600 ? 2 : 1;
        team.currentAllowedDirection = playerDirection;
        team.normalFire = true;
        m_cpuResumePlayTimer = 15;

        auto difference = static_cast<int8_t>(
            static_cast<uint8_t>(static_cast<int>(playerDirection) << 5) - goalAngle);
        int16_t bias = difference < 0 ? 1 : -1;
        auto ballY = swos.ballSprite.y.whole();
        auto outsideGoalArea =
            (swos.ballSprite.deltaY.raw() < 0 && ballY < 342) ||
            (swos.ballSprite.deltaY.raw() >= 0 && ballY > 555);
        if (!outsideGoalArea) {
            auto phase = (swos.currentGameTick & 0x1c) >> 2;
            auto ballX = swos.ballSprite.x.whole();
            if (ballX >= 193 && ballX < 478) {
                if (!phase)
                    bias = 0;
                else if (phase >= 5)
                    bias = -bias;
            } else if (ballX >= 118 && ballX <= 553) {
                if (!phase)
                    bias = -bias;
                else if (phase < 4)
                    bias = 0;
            } else if (phase >= 4) {
                bias = 0;
            }
        }
        team.cpuBallSpinDirection = bias;
    };

    if (swos.gameStatePl != GameState::kInProgress) {
        if (swos.lastTeamPlayedBeforeBreak == &team && !handleCpuResultScreen(team)) {
            if (100 + (swos.currentGameTick & 0x3f) > swos.stoppageTimerActive) {
                updateCpuPreResumePlayDirection(team, playerDirection, goalAngle);
            } else {
                auto state = swos.gameState;
                auto freeKick = state >= GameState::kFreeKickOuterLeft &&
                    state <= GameState::kFreeKickOuterRight;

                if (swos.playingPenalties || state == GameState::kPenalty) {
                    auto randomDirection = static_cast<Direction>(cpuRandom & 7);
                    if (swos.playerTurnFlags & makeDirectionMask(randomDirection))
                        team.currentAllowedDirection = randomDirection;
                    else if (playerDirection != Direction::kTop && playerDirection != Direction::kBottom)
                        applyRestartKick();
                } else if (state == GameState::kKeeperHoldsTheBall ||
                    state == GameState::kGoalOutLeft || state == GameState::kGoalOutRight) {
                    auto rightHalf = swos.ballSprite.x.whole() > kPitchCenterX;
                    auto towardSideline = rightHalf ?
                        playerDirection == Direction::kBottomLeft || playerDirection == Direction::kTopLeft :
                        playerDirection == Direction::kTopRight || playerDirection == Direction::kBottomRight;
                    if ((cpuRandom & 1) && (playerDirection == swos.cameraDirection || towardSideline))
                        applyRestartKick();
                    else
                        findClosestForRestart();
                } else if (state == GameState::kPlayersGoingToInitialPositions) {
                    if (swos.stoppageTimerActive >= 150 && playerDirection == swos.cameraDirection)
                        applyRestartKick();
                    else
                        findClosestForRestart();
                } else if (freeKick || state == GameState::kFoul) {
                    if (!(cpuRandom & 15))
                        findClosestForRestart();
                    else if (goalDirection != playerDirection)
                        team.currentAllowedDirection = goalDirection;
                    else
                        applyRestartKick();
                } else if (state >= GameState::kThrowInForwardRight &&
                    state <= GameState::kThrowInBackLeft) {
                    auto index = static_cast<unsigned>(state) -
                        static_cast<unsigned>(GameState::kThrowInForwardRight);
                    auto allowed = kCpuThrowInDirections[index];
                    if (&team != &swos.bottomTeamData)
                        allowed = static_cast<uint8_t>((allowed >> 4) | (allowed << 4));
                    if (!(cpuRandom & 15) && (allowed & makeDirectionMask(playerDirection)))
                        applyRestartKick();
                    else
                        findClosestForRestart();
                } else {
                    applyRestartKick();
                }
            }
        }
        return;
    }

    if ((swos.playingPenalties || swos.penalty) && static_cast<int16_t>(team.spinTimer) >= 0) {
        applyBallAftertouch();
        return;
    }
    if (m_cpuShotAftertouchTimer)
        setCpuGoalwardAftertouchDirection(team);
    if (!player)
        return;
    if (static_cast<int16_t>(team.spinTimer) >= 0) {
        applyBallAftertouch();
        return;
    }

    if (!team.plVeryCloseToBall && !team.plCloseToBall) {
        if (decideWhetherCpuPlayerFires(team, *player, playerDirection)) {
            // The firing helper has already populated the controls.
        } else if (passTarget && static_cast<int32_t>(
            passTarget->ballDistance - player->ballDistance) >= 50) {
            std::swap(team.passToPlayerPtr, team.controlledPlayer);
        } else if (!passTarget || swos.topTeamData.playerNumber || swos.bottomTeamData.playerNumber) {
            auto opposite = static_cast<Direction>(
                ((static_cast<uint8_t>(player->fullDirection) + 128 + 16) & 0xff) >> 5);
            auto flip = opposite == playerDirection || player->ballDistance < 800 ||
                !(swos.currentGameTick & 0x0e);
            team.currentAllowedDirection = flip ? opposite : playerDirection;
        } else if (swos.currentGameTick & 0x18) {
            team.currentAllowedDirection = playerDirection;
            applyAmigaModeDirectionFlipBan(&team);
        } else {
            checkForAmigaModeDirectionFlipBan(player);
            constexpr std::array<int16_t, 2> kRandomRotation{-32, 32};
            auto rotation = kRandomRotation[(cpuRandom & 2) >> 1];
            auto fullDirection = static_cast<uint8_t>(player->fullDirection + rotation + 128);
            team.currentAllowedDirection = static_cast<Direction>((fullDirection + 16 & 0xff) >> 5);
        }
        return;
    }

    if (decideWhetherCpuPlayerFires(team, *player, playerDirection))
        return;

    auto useFallback = false;
    if (playerDirection == Direction::kRight || playerDirection == Direction::kLeft) {
        auto ballY = swos.ballSprite.y.whole();
        useFallback = &team == &swos.topTeamData ? ballY >= 740 : ballY <= 158;
    }
    if (!useFallback && goal.squaredDistance > 28800)
        useFallback = true;
    if (!useFallback && goal.squaredDistance >= 12800 && (cpuRandom & 3))
        useFallback = true;

    if (!useFallback && playerDirection != Direction::kNoDirection) {
        auto tolerance = goal.squaredDistance <= 3200 ? 50 : 15;
        auto difference = static_cast<int8_t>(
            static_cast<uint8_t>(static_cast<int>(playerDirection) << 5) - goalAngle);
        if (difference <= tolerance && difference > -tolerance) {
            tryNormalShot(difference);
            return;
        }
    }

    if (team.cpuDecisionCooldown) {
        team.cpuDecisionCooldown--;
        auto [closestPlayer, distance] = findClosestTeammateFacing(team, *player, playerDirection);
        if (closestPlayer)
            quickFireWithClosestPlayer();
        else
            turnAwayFromNearbyPlayer();
        return;
    }

    if (goal.squaredDistance < 9800) {
        setDefaultDirection();
        return;
    }

    auto& opponent = *team.opponentTeam;
    auto nearbyOpponent = opponent.controlledPlayer.asPtr();
    if (!nearbyOpponent || nearbyOpponent->ballDistance >= 5000)
        nearbyOpponent = opponent.passToPlayerPtr.asPtr();

    auto immediateShotOrCooldown = [&] {
        auto difference = (static_cast<int>(goalDirection) - static_cast<int>(playerDirection)) & 7;
        if (difference == 0 || difference == 1)
            applyShotTurnBias();
        else
            setDecisionCooldown();
    };

    if (!nearbyOpponent || nearbyOpponent->ballDistance >= 5000) {
        setDefaultDirection();
    } else if (nearbyOpponent->ballDistance < 800) {
        if (goal.squaredDistance > 180000) {
            immediateShotOrCooldown();
        } else {
            auto [closestPlayer, distance] = findClosestTeammateFacing(team, *player, playerDirection);
            if (closestPlayer)
                quickFireWithClosestPlayer();
            else
                setDecisionCooldown();
        }
    } else if (cpuRandom <= 8 && goal.squaredDistance > 48'400) {
        immediateShotOrCooldown();
    } else if (swos.currentGameTick & 0x0c) {
        setDefaultDirection();
    } else {
        auto [closestPlayer, distance] = findClosestTeammateFacing(team, *player, playerDirection);
        if (closestPlayer)
            quickFireWithClosestPlayer();
        else
            setDecisionCooldown();
    }
}

// Press fire for a CPU-controlled player near the ball when the opposing team has possession and
// its controlled player is facing sufficiently far away. In this context, fire initiates a tackle.
void cpuPlayerAttemptTackle(TeamGeneralInfo& team, const Sprite& player)
{
    constexpr uint32_t kMaximumTackleDistanceSquared = 200;

    if (player.ballDistance <= kMaximumTackleDistanceSquared) {
        const auto opponent = team.opponentTeam;
        if (opponent->playerHasBall && opponent->controlledPlayer) {
            // Compare the directions as wrapped signed full-direction angles.
            // Tackle when the opponent is facing more than one 45-degree sector away.
            constexpr int kFullDirectionSectorSize = 32;
            constexpr int kMaximumDirectionDifference = kFullDirectionSectorSize;

            auto opponentDirection = static_cast<uint8_t>(
                static_cast<int>(opponent->controlledPlayer->direction) * kFullDirectionSectorSize);
            auto playerFullDirection = static_cast<uint8_t>(static_cast<int>(player.direction) * kFullDirectionSectorSize);
            auto directionDifference = static_cast<int8_t>(opponentDirection - playerFullDirection);
            if (directionDifference < -kMaximumDirectionDifference ||
                directionDifference > kMaximumDirectionDifference) {
                team.currentAllowedDirection = player.direction;
                team.firePressed = true;
                team.fireThisFrame = true;
                team.normalFire = true;
            }
        }
    }
}

static CpuGoalContext calculateCpuGoalContext(const TeamGeneralInfo& team)
{
    constexpr int kTopGoalLineY = 129;
    constexpr int kBottomGoalLineY = 769;
    constexpr int kDirectionCalculationSpeed = 256;

    auto goalY = &team == &swos.bottomTeamData ? kTopGoalLineY : kBottomGoalLineY;
    auto ballX = swos.ballSprite.x.whole();
    auto ballY = swos.ballSprite.y.whole();
    auto deltaX = ballX - kPitchCenterX;
    auto deltaY = ballY - goalY;

    return { ballX, ballY, static_cast<uint32_t>(deltaX * deltaX + deltaY * deltaY),
        calculateDeltaXAndY(kDirectionCalculationSpeed, ballX, ballY, kPitchCenterX, goalY), };
}

static void updateCpuResumePlayTurnDirection(TeamGeneralInfo& team, Direction playerDirection)
{
    if (!(swos.currentGameTick & 0x0e)) {
        auto turnStep = m_cpuResumePlayTurnStep;
        int16_t fallbackTurnStep;
        if (turnStep < 0) {
            if (turnStep != -1) {
                m_cpuResumePlayTurnStep = ++turnStep;
                if (turnStep != -1)
                    return;
            }
            fallbackTurnStep = 1;
        } else {
            if (turnStep != 1) {
                m_cpuResumePlayTurnStep = --turnStep;
                if (turnStep != 1)
                    return;
            }
            fallbackTurnStep = -1;
        }

        auto direction = static_cast<Direction>((static_cast<int>(playerDirection) + turnStep) & 7);
        if (swos.playerTurnFlags & makeDirectionMask(direction)) {
            team.currentAllowedDirection = direction;
        } else {
            // Try rotating the other way during the next eligible update.
            m_cpuResumePlayTurnStep = fallbackTurnStep;
        }
    }
}

void resetCpuResumePlayTurnDirection()
{
    m_cpuResumePlayTurnStep = 1;
}

// Aim aftertouch toward the opposing goal, steering inward when the ball is
// outside the goal area (by x). Only the team that fired the shot may apply it.
static void setCpuGoalwardAftertouchDirection(TeamGeneralInfo& team)
{
    constexpr int kLeftGoalPostX = 300;
    constexpr int kRightGoalPostX = 371;

    if (m_cpuShotAftertouchTimer) {
        auto targetTeam = m_cpuAftertouchTargetGoal == PitchEnd::kTop ? &swos.bottomTeamData : &swos.topTeamData;
        if (&team == targetTeam) {
            auto direction = m_cpuAftertouchTargetGoal == PitchEnd::kTop ? Direction::kTop : Direction::kBottom;
            auto ballX = static_cast<uint16_t>(swos.ballSprite.x.whole());
            if (ballX < kLeftGoalPostX)
                direction = m_cpuAftertouchTargetGoal == PitchEnd::kTop ? Direction::kTopRight : Direction::kBottomRight;
            else if (ballX > kRightGoalPostX)
                direction = m_cpuAftertouchTargetGoal == PitchEnd::kTop ? Direction::kTopLeft : Direction::kBottomLeft;

            team.currentAllowedDirection = direction;
        }
    }
}

static void clearCpuControls(TeamGeneralInfo& team)
{
    team.currentAllowedDirection = Direction::kNoDirection;
    team.firePressed = false;
    team.fireThisFrame = false;
    team.quickFire = false;
    team.normalFire = false;
}

static bool handleCpuResultScreen(TeamGeneralInfo& team)
{
    auto state = swos.gameState;
    auto stateValue = static_cast<unsigned>(state);
    auto firstResultState = static_cast<unsigned>(GameState::kStartingGame);
    auto lastResultState = static_cast<unsigned>(GameState::kGameEnded);
    if (!swos.team1Computer || !swos.team2Computer || stateValue < firstResultState || stateValue > lastResultState)
        return false;

    auto interval = state == GameState::kResultOnHalftime ||
        state == GameState::kResultAfterTheGame ? clearResultInterval() : clearResultHalftimeInterval();
    if (swos.stoppageTimerTotal >= interval)
        team.firePressed = true;

    return true;
}

static bool decideWhetherCpuPlayerFires(TeamGeneralInfo& team, const Sprite& player, Direction playerDirection)
{
    constexpr uint32_t kMaximumShootingDistanceSquared = 18 * 18 + 18 * 18;
    constexpr int kMinimumRisingBallHeight = 8;
    constexpr int kMaximumRisingBallHeight = 14;
    constexpr int kMinimumFallingBallHeight = 12;
    constexpr int kMaximumFallingBallHeight = 20;

    if (player.isGoalkeeper())
        return false;

    bool facingOpposingGoal = &team == &swos.topTeamData ?
        isDownwardFacing(playerDirection) : isUpwardFacing(playerDirection);
    if (!facingOpposingGoal || player.ballDistance > kMaximumShootingDistanceSquared)
        return false;

    auto ballHeight = static_cast<uint16_t>(swos.ballSprite.z.whole());
    bool ballAtStrikingHeight = swos.ballSprite.deltaZ.raw() < 0 ?
        ballHeight >= kMinimumFallingBallHeight && ballHeight <= kMaximumFallingBallHeight :
        ballHeight >= kMinimumRisingBallHeight && ballHeight <= kMaximumRisingBallHeight;
    if (!ballAtStrikingHeight)
        return false;

    team.fireThisFrame = true;

    // Convert the 256-degrees direction to one of eight sectors. The first value is truncated and used
    // immediately; the rounded value must still match the player's current direction before the shot
    // is accepted.
    constexpr uint16_t kFullDirectionHalfTurn = 128;
    constexpr uint16_t kFullDirectionSectorHalfWidth = 16;
    constexpr uint16_t kFullDirectionSectorSize = 32;

    auto directionWithHalfTurn = static_cast<uint8_t>(player.fullDirection + kFullDirectionHalfTurn);
    team.currentAllowedDirection = static_cast<Direction>(directionWithHalfTurn / kFullDirectionSectorSize);

    auto roundedDirection = static_cast<Direction>(
        static_cast<uint8_t>(directionWithHalfTurn + kFullDirectionSectorHalfWidth) / kFullDirectionSectorSize);
    if (roundedDirection != playerDirection)
        return false;

    team.currentAllowedDirection = roundedDirection;
    m_cpuShotAftertouchTimer = 15;
    m_cpuAftertouchTargetGoal = &team == &swos.topTeamData ? PitchEnd::kBottom : PitchEnd::kTop;
    return true;
}

static void updateCpuPreResumePlayDirection(TeamGeneralInfo& team, Direction playerDirection, uint16_t goalAngle)
{
    if (!swos.resultTimer) {
        auto state = swos.gameState;
        bool directionalRestart =
            state == GameState::kGoalOutLeft ||
            state == GameState::kGoalOutRight ||
            state == GameState::kKeeperHoldsTheBall ||
            state >= GameState::kThrowInForwardRight && state <= GameState::kThrowInBackLeft;
        if (directionalRestart) {
            updateCpuResumePlayTurnDirection(team, playerDirection);
        } else {
            bool isFacingGoalState = state == GameState::kFoul ||
                state >= GameState::kFreeKickOuterLeft && state <= GameState::kFreeKickOuterRight;
            if (isFacingGoalState)
                team.currentAllowedDirection = static_cast<Direction>(static_cast<uint8_t>(goalAngle + 16) >> 5);
        }
    }
}

static bool isCpuReadyToResumePlay(const TeamGeneralInfo& team)
{
    return team.passKickTimer < 13;
}

static std::pair<Sprite *, uint32_t> findClosestTeammateFacing(const TeamGeneralInfo& team,
    const Sprite& controlledPlayer, Direction direction)
{
    auto [closestPlayer, distance] = findClosestPlayerToBallFacing(
        team, static_cast<int>(direction) << 5);

    if (!closestPlayer || closestPlayer->teamNumber != controlledPlayer.teamNumber)
        return { nullptr, distance };
    else
        return { closestPlayer, distance };
}

// Find player closest to ball facing specified direction. Search both teams.
static std::pair<Sprite *, uint32_t> findClosestPlayerToBallFacing(const TeamGeneralInfo& team, int direction)
{
    Sprite *closestPlayer{};
    auto closestDistance = std::numeric_limits<uint32_t>::max();

    for (auto currentTeam : { &swos.topTeamData, &swos.bottomTeamData }) {
        for (int i = 0; i < kNumPlayersInLineup; i++) {
            auto player = currentTeam->players[i];
            if (player != team.controlledPlayer && !player->sentAway && player->inNormalState()) {
                // Full directions wrap at 256; interpreting the low-byte difference as signed
                // gives the shortest angular distance around the wrap point.
                auto directionDifference = static_cast<int8_t>(static_cast<uint8_t>(player->fullDirection - direction));
                if (directionDifference >= -16 && directionDifference <= 16 && player->ballDistance < closestDistance) {
                    // Keep the first player on ties, matching the top-team-first assembly scan.
                    closestDistance = player->ballDistance;
                    closestPlayer = player;
                }
            }
        }
    }

    return { closestPlayer, closestDistance };
}
