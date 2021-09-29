#include "camera.h"
#include "bench.h"
#include "referee.h"
#include "render.h"
#include "random.h"
#include "pitchConstants.h"

constexpr float kTrainingGameStartX = 168;
constexpr float kTrainingGameStartY = 313;

constexpr float  kTopStartLocationY = 16;
constexpr float  kBottomStartLocationY = 664;
constexpr float kCenterX = 176;

constexpr float kPenaltyShootoutCameraX = 336;
constexpr float kPenaltyShootoutCameraY = 107;
constexpr float kLeavingBenchCameraDestX = 211;

// once the camera reaches this area it's allowed to slide all the way left
constexpr float kBenchSlideAreaStartY = 339;
constexpr float kBenchSlideAreaEndY = 359;

constexpr float kPlayersOutsidePitchX = 590;
constexpr float kTopGoalLine = 129;

constexpr float kPitchMaxX = 352;
constexpr float kPitchMinY = 16;
constexpr float kPitchMaxY = 664;
constexpr float kTrainingPitchMinY = 80;
constexpr float kTrainingPitchMaxY = 616;

constexpr float kPitchSideCameraLimitDuringBreak = 37;
constexpr float kPitchSideCameraLimitDuringGame = 63;
constexpr float kSubstituteCameraLimit = 51;

constexpr float kCameraMinX = 0;
constexpr float kCameraMaxX = kPitchMaxX;
constexpr float kCameraMinY = kPitchMinY;
constexpr float kCameraMaxY = 680;

static float m_cameraX;
static float m_cameraY;

float getCameraX()
{
    return m_cameraX;
}

float getCameraY()
{
    return m_cameraY;
}

void setCameraX(float value)
{
    m_cameraX = value;
}

void setCameraY(float value)
{
    m_cameraY = value;
}

struct CameraParams {
    CameraParams() {}
    CameraParams(float xDest, float yDest, float xLimit = 0, int xVelocity = 0, int yVelocity = 0)
        : xDest(xDest), yDest(yDest), xLimit(xLimit), xVelocity(xVelocity), yVelocity(yVelocity) {}

    float xDest;
    float yDest;
    float xLimit;
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

    if (cardHandingInProgress())
        params = bookingPlayerMode();
    else if (swos.playingPenalties)
        params = penaltyShootoutMode();
    else if (swos.g_waitForPlayerToGoInTimer)
        params = benchMode(true);
    else if (isCameraLeavingBench())
        params = leavingBenchMode();
    else if (inBench())
        params = benchMode(false);
    else
        params = standardMode();

    updateCameraCoordinates(params);
}

void setCameraToInitialPosition()
{
    float startX = kTrainingGameStartX;
    float startY = kTrainingGameStartY;

    if (!swos.g_trainingGame) {
        startX = kCenterX;
        startY = SWOS::rand() & 1 ? kBottomStartLocationY : kTopStartLocationY;
    }

    setCameraX(startX);
    setCameraY(startY);
}

static void clipCameraDestination(float& xDest, float& yDest, float xLimit)
{
    assert(xLimit >= 0);

    if (xDest < xLimit)
        xDest = xLimit;

    auto maxX = kPitchMaxX - xLimit;
    if (xDest > maxX)
        xDest = maxX;

    auto minY = swos.g_trainingGame ? kTrainingPitchMinY : kPitchMinY;
    auto maxY = swos.g_trainingGame ? kTrainingPitchMaxY : kPitchMaxY;

    if (yDest < minY)
        yDest = minY;
    if (yDest > maxY)
        yDest = maxY;
}

static void clipCameraMovement(float& deltaX, float& deltaY)
{
    constexpr float kMaxCameraMovement = 5;

    if (deltaX > kMaxCameraMovement)
        deltaX = kMaxCameraMovement;
    if (deltaX < -kMaxCameraMovement)
        deltaX = -kMaxCameraMovement;
    if (deltaY > kMaxCameraMovement)
        deltaY = kMaxCameraMovement;
    if (deltaY < -kMaxCameraMovement)
        deltaY = -kMaxCameraMovement;
}

static void boundCameraToPitch(float& cameraX, float& cameraY)
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

static void updateCameraCoordinates(const float& cameraX, const float& cameraY)
{
    setCameraX(cameraX);
    setCameraY(cameraY);
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

    auto cameraX = getCameraX();
    auto cameraY = getCameraY();

    auto deltaX = (xDest - cameraX) / 16;
    auto deltaY = (yDest - cameraY) / 16;

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
    return { kLeavingBenchCameraDestX, static_cast<float>(kPitchCenterY), kPitchSideCameraLimitDuringBreak };
}

static float getBenchCameraXLimit()
{
    auto limit = kPitchSideCameraLimitDuringBreak;

    bool cameraAtBenchLevel = getCameraY() >= kBenchSlideAreaStartY && getCameraY() <= kBenchSlideAreaEndY;

    // these sprite conditions are weird, I don't fully understand them so leaving them in
    if (cameraAtBenchLevel &&
        (swos.goal1TopSprite.pictureIndex == -1 || !swos.goal1TopSprite.onScreen) &&
        (swos.goal2BottomSprite.pictureIndex == -1 || !swos.goal2BottomSprite.onScreen))
        limit = swos.g_substituteInProgress ? kSubstituteCameraLimit : kCameraMinX;

    return limit;
}

static CameraParams benchMode(bool substitutingPlayer)
{
    auto limit = substitutingPlayer ? kSubstituteCameraLimit : getBenchCameraXLimit();

    return { benchCameraX(), static_cast<float>(kPitchCenterY), limit };
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

static CameraParams waitingForPlayersToLeaveCameraLocation(float limit)
{
    return { kPlayersOutsidePitchX, static_cast<float>(kPitchCenterY), limit };
}

static CameraParams showResultAtCenter(float limit)
{
    return { static_cast<float>(kPitchCenterX), static_cast<float>(kPitchCenterY), limit };
}

static CameraParams showResultAtTop(float limit)
{
    return { static_cast<float>(kPitchCenterX), kTopGoalLine, limit };
}

static CameraParams followTheBall(float limit, int xVelocity, int yVelocity)
{
    return { swos.ballSprite.x, swos.ballSprite.y, limit, xVelocity, yVelocity };
}

static CameraParams standardMode()
{
    int xDirection, yDirection;

    if (swos.gameStatePl != GameState::kInProgress) {
        std::tie(xDirection, yDirection) = getGameStoppedCameraDirections();
    } else {
        xDirection = swos.ballSprite.deltaX.whole();
        yDirection = swos.ballSprite.deltaY.whole();
    }

    int xVelocity, yVelocity;
    std::tie(xVelocity, yVelocity) = getStandardModeCameraVelocity(xDirection, yDirection);

    auto limit = kPitchSideCameraLimitDuringGame;

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
