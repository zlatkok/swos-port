#include "camera.h"
#include "render.h"

constexpr int kPenaltyShootoutCameraX = 336;
constexpr int kPenaltyShootoutCameraY = 107;
constexpr int kLeavingBenchCameraDestX = 211;

// once the camera reaches this area it's allowed to slide all the way left
constexpr int kBenchSlideAreaStartY = 339;
constexpr int kBenchSlideAreaEndY = 359;

constexpr int kRightThrowInLine = 590;
constexpr int kCenterLine = 449;
constexpr int kPitchMiddleX = 336;
constexpr int kTopGoalLine = 129;

constexpr int kPitchMaxX = 352;
constexpr int kPitchMinY = 16;
constexpr int kPitchMaxY = 664;
constexpr int kTrainingPitchMinY = 80;
constexpr int kTrainingPitchMaxY = 616;

constexpr int kPitchSideCameraLimitDuringBreak = 37;
constexpr int kPitchSideCameraLimitDuringGame = 63;
constexpr int kSubstituteCameraLimit = 51;

constexpr int kCameraMinX = 0;
constexpr int kCameraMaxX = kPitchMaxX;
constexpr int kCameraMinY = kPitchMinY;
constexpr int kCameraMaxY = 680;

FixedPoint getCameraX()
{
    return FixedPoint(swos.g_cameraX, swos.g_cameraXFraction);
}

FixedPoint getCameraY()
{
    return FixedPoint(swos.g_cameraY, swos.g_cameraYFraction);
}

void setCameraX(FixedPoint value)
{
    swos.g_cameraX = value.whole();
    swos.g_cameraXFraction = value.fraction();
}

void setCameraY(FixedPoint value)
{
    swos.g_cameraY = value.whole();
    swos.g_cameraYFraction = value.fraction();
}

struct CameraParams {
    CameraParams() {}
    CameraParams(FixedPoint xDest, FixedPoint yDest, int xLimit = 0, int xVelocity = 0, int yVelocity = 0)
        : xDest(xDest), yDest(yDest), xLimit(xLimit), xVelocity(xVelocity), yVelocity(yVelocity) {}

    FixedPoint xDest;
    FixedPoint yDest;
    int xLimit;
    int xVelocity;
    int yVelocity;
};

static void updateCameraCoordinates(const CameraParams& params);
static CameraParams bookingPlayerMode();
static CameraParams penaltyShootoutMode();
static CameraParams leavingBenchMode();
static CameraParams benchMode(bool substitutingPlayer);
static CameraParams standardMode();

void moveCamera()
{
    if (swos.showFansCounter)
        return;

    CameraParams params;

    if (swos.whichCard)
        params = bookingPlayerMode();
    else if (swos.playingPenalties)
        params = penaltyShootoutMode();
    else if (swos.waitForPlayerToGoInTimer)
        params = benchMode(true);
    else if (swos.g_leavingSubsMenu)
        params = leavingBenchMode();
    else if (swos.g_inSubstitutesMenu)
        params = benchMode(false);
    else
        params = standardMode();

    updateCameraCoordinates(params);
}

void SWOS::MoveCamera()
{
    moveCamera();
}

static void clipCameraDestination(FixedPoint& xDest, FixedPoint& yDest, int xLimit)
{
    assert(xLimit >= 0);

    if (xDest < xLimit)
        xDest = xLimit;

    int maxX = kPitchMaxX - xLimit;
    if (xDest > maxX)
        xDest = maxX;

    int minY = swos.g_trainingGame ? kTrainingPitchMinY : kPitchMinY;
    int maxY = swos.g_trainingGame ? kTrainingPitchMaxY : kPitchMaxY;

    if (yDest < minY)
        yDest = minY;
    if (yDest > maxY)
        yDest = maxY;
}

void clipCameraMovement(FixedPoint& deltaX, FixedPoint& deltaY)
{
    constexpr FixedPoint kMaxCameraMovement(5, 0);

    if (deltaX > kMaxCameraMovement)
        deltaX = kMaxCameraMovement;
    if (deltaX < -kMaxCameraMovement)
        deltaX = -kMaxCameraMovement;
    if (deltaY > kMaxCameraMovement)
        deltaY = kMaxCameraMovement;
    if (deltaY < -kMaxCameraMovement)
        deltaY = -kMaxCameraMovement;
}

static void boundCameraToPitch(FixedPoint& cameraX, FixedPoint& cameraY)
{
    if (cameraX < kCameraMinX)
        cameraX = kCameraMinX;
    if (cameraX > kCameraMaxX)
        cameraX = kCameraMaxX;
    if (cameraY < kCameraMinY)
        cameraY = kCameraMinY;
    if (cameraY > kCameraMaxY)
        cameraY = kCameraMaxY;
}

static void updateCameraCoordinates(FixedPoint& cameraX, FixedPoint& cameraY)
{
    swos.g_cameraX = cameraX.whole();
    swos.g_cameraXFraction = cameraX.fraction();
    swos.g_cameraY = cameraY.whole();
    swos.g_cameraYFraction = cameraY.fraction();
}

static void updateCameraCoordinates(const CameraParams& params)
{
    swos.cameraXVelocity = params.xVelocity;
    swos.cameraYVelocity = params.yVelocity;

    auto xDest = params.xDest - kVgaWidth / 2;
    auto yDest = params.yDest - kVgaHeight / 2;
    xDest += params.xVelocity;
    yDest += params.yVelocity;

    clipCameraDestination(xDest, yDest, params.xLimit);

    auto cameraX = FixedPoint(swos.g_cameraX, swos.g_cameraXFraction);
    auto cameraY = FixedPoint(swos.g_cameraY, swos.g_cameraYFraction);

    auto deltaX = xDest - cameraX;
    auto deltaY = yDest - cameraY;

    deltaX >>= 4;
    deltaY >>= 4;

    clipCameraMovement(deltaX, deltaY);

    cameraX += deltaX;
    cameraY += deltaY;

    boundCameraToPitch(cameraX, cameraY);
    updateCameraCoordinates(cameraX, cameraY);
}

static CameraParams bookingPlayerMode()
{
    return { swos.bookedPlayer->x, swos.bookedPlayer->y, kPitchSideCameraLimitDuringBreak };
}

static CameraParams penaltyShootoutMode()
{
    return { kPenaltyShootoutCameraX, kPenaltyShootoutCameraY };
}

static CameraParams leavingBenchMode()
{
    return { kLeavingBenchCameraDestX, kCenterLine, kPitchSideCameraLimitDuringBreak };
}

static int getBenchCameraXLimit()
{
    int limit = kPitchSideCameraLimitDuringBreak;

    bool cameraAtBenchLevel = swos.g_cameraY >= kBenchSlideAreaStartY && swos.g_cameraY <= kBenchSlideAreaEndY;

    // these sprite conditions are weird, I don't fully understand them so leaving them in
    if (cameraAtBenchLevel &&
        (swos.goal1TopSprite.pictureIndex == -1 || !swos.goal1TopSprite.beenDrawn) &&
        (swos.goal2BottomSprite.pictureIndex == -1 || !swos.goal2BottomSprite.beenDrawn))
        limit = swos.g_substituteInProgress ? kSubstituteCameraLimit : kCameraMinX;

    return limit;
}

static CameraParams benchMode(bool substitutingPlayer)
{
    int limit = substitutingPlayer ? kSubstituteCameraLimit : getBenchCameraXLimit();

    return { swos.g_cameraLeavingSubsLimit, kCenterLine, limit };
}

static std::pair<int, int> getGameStoppedCameraDirections()
{
    int xDirection = 0, yDirection = 0;

    int direction;
    bool gotPlayerDirection = false;

    if (swos.lastTeamPlayedBeforeBreak && swos.lastTeamPlayedBeforeBreak->controlledPlayerSprite) {
        direction = swos.lastTeamPlayedBeforeBreak->controlledPlayerSprite->direction;
        gotPlayerDirection = true;
    } else {
        direction = swos.cameraDirection;
    }

    if (gotPlayerDirection || direction != -1) {
        static const int8_t kNextCameraDirections[16] = {
            0, -1, 1, -1, 1, 0, 1, 1, 0, 1, -1, 1, -1, 0, -1, -1
        };
        xDirection = kNextCameraDirections[2 * direction];
        yDirection = kNextCameraDirections[2 * direction + 1];
    }

    return { xDirection, yDirection };
}

static std::pair<int, int> getStandardModeCameraVelocity(int xDirection, int yDirection)
{
    constexpr int kVelocityIncrement = 2;
    constexpr int kMaxVelocity = 40;

    int xVelocity = swos.cameraXVelocity;
    int yVelocity = swos.cameraYVelocity;

    if (xDirection < 0 && xVelocity != -kMaxVelocity)
        xVelocity -= kVelocityIncrement;
    else if (xDirection > 0 && xVelocity != kMaxVelocity)
        xVelocity += kVelocityIncrement;

    if (yDirection < 0 && yVelocity != -kMaxVelocity)
        yVelocity -= kVelocityIncrement;
    else if (yDirection > 0 && yVelocity != kMaxVelocity)
        yVelocity += kVelocityIncrement;

    return { xVelocity, yVelocity };
}

static CameraParams waitingForPlayersToLeaveCameraLocation(int limit)
{
    return { kRightThrowInLine, kCenterLine, limit };
}

static CameraParams showResultAtCenter(int limit)
{
    return { kPitchMiddleX, kCenterLine, limit };
}

static CameraParams showResultAtTop(int limit)
{
    return { kPitchMiddleX, kTopGoalLine, limit };
}

static CameraParams followTheBall(int limit, int xVelocity, int yVelocity)
{
    return { swos.ballSprite.x, swos.ballSprite.y, limit, xVelocity, yVelocity };
}

static CameraParams standardMode()
{
    int xDirection, yDirection;

    if (swos.gameStatePl != GameState::kInProgress) {
        std::tie(xDirection, yDirection) = getGameStoppedCameraDirections();
    } else {
        xDirection = swos.ballSprite.deltaX;
        yDirection = swos.ballSprite.deltaY;
    }

    int xVelocity, yVelocity;
    std::tie(xVelocity, yVelocity) = getStandardModeCameraVelocity(xDirection, yDirection);

    int limit = kPitchSideCameraLimitDuringGame;

    if (swos.gameStatePl != GameState::kInProgress) {
        bool cornerOrThrowIn = swos.gameState == GameState::kCornerLeft || swos.gameState == GameState::kCornerRight ||
            swos.gameState >= GameState::kThrowInForwardRight && swos.gameState <= GameState::kThrowInBackLeft;
        if (cornerOrThrowIn)
            limit = kPitchSideCameraLimitDuringBreak;
    }

    if (swos.gameState >= GameState::kStartingGame && swos.gameState <= GameState::kGameEnded) {
        switch (swos.gameState) {
        case GameState::kStartingGame:
        case GameState::kCameraGoingToShowers:
        case GameState::kGoingToHalftime:
        case GameState::kPlayersGoingToShower:
            return waitingForPlayersToLeaveCameraLocation(limit);

        case GameState::kResultAfterTheGame:
            return showResultAtTop(limit);

        case GameState::kGameEnded:
            if (swos.penaltiesState < 0) {
        default:
                return showResultAtCenter(limit);
            }
            break;

        case GameState::kFirstHalfEnded:
            break;
        }
    }

    return followTheBall(limit, xVelocity, yVelocity);
}
