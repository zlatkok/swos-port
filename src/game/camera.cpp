#include "camera.h"
#include "bench.h"
#include "referee.h"
#include "render.h"
#include "random.h"
#include "pitchConstants.h"

constexpr int kTrainingGameStartX = 168;
constexpr int kTrainingGameStartY = 313;

constexpr int kTopStartLocationY = 16;
constexpr int kBottomStartLocationY = 664;
constexpr int kCenterX = 176;

constexpr int kPenaltyShootoutCameraY = 107;
constexpr int kLeavingBenchCameraDestX = 211;

// once the camera reaches this area it's allowed to slide all the way left
constexpr int kBenchSlideAreaStartY = 339;
constexpr int kBenchSlideAreaEndY = 359;

constexpr int kPlayersOutsidePitchX = 590;
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

static FixedPoint m_cameraX;
static FixedPoint m_cameraY;

static bool m_leavingBenchMode;

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

static CameraParams bookingPlayerMode();
static CameraParams penaltyShootoutMode();
static CameraParams benchMode(bool substitutingPlayer);
static CameraParams leavingBenchMode();
static CameraParams standardMode();
static void updateCameraLeaving();
static void updateCameraCoordinates(const CameraParams& params);
static void clipCameraDestination(FixedPoint& xDest, FixedPoint& yDest, int xLimit);
static void constrainCameraToPitch(FixedPoint& cameraX, FixedPoint& cameraY);
static void updateCameraCoordinates(const FixedPoint& cameraX, const FixedPoint& cameraY);
static int getBenchCameraXLimit();
static void clipCameraMovement(FixedPoint& deltaX, FixedPoint& deltaY);
static std::pair<int, int> getGameStoppedCameraDirections();
static std::pair<int, int> getStandardModeCameraVelocity(FixedPoint xDirection, FixedPoint yDirection);
static CameraParams waitingForPlayersToLeaveCameraLocation(int limit);
static CameraParams showResultAtCenter(int limit);
static CameraParams showResultAtTop(int limit);
static CameraParams followTheBall(int limit, int xVelocity, int yVelocity);

FixedPoint getCameraX()
{
    return m_cameraX;
}

FixedPoint getCameraY()
{
    return m_cameraY;
}

void setCameraX(FixedPoint value)
{
    m_cameraX = value;
}

void setCameraY(FixedPoint value)
{
    m_cameraY = value;
}

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
    else if (m_leavingBenchMode)
        params = leavingBenchMode();
    else if (inBench())
        params = benchMode(false);
    else
        params = standardMode();

    updateCameraCoordinates(params);
    updateCameraLeaving();
}

void setCameraToInitialPosition()
{
    auto startX = kTrainingGameStartX;
    auto startY = kTrainingGameStartY;

    if (!swos.g_trainingGame) {
        startX = kCenterX;
        startY = SWOS::rand() & 1 ? kBottomStartLocationY : kTopStartLocationY;
    }

    updateCameraCoordinates(startX, startY);
}

void switchCameraToLeavingBenchMode()
{
    m_leavingBenchMode = true;
}

static CameraParams bookingPlayerMode()
{
    return { swos.bookedPlayer->x, swos.bookedPlayer->y, kPitchSideCameraLimitDuringBreak };
}

static CameraParams penaltyShootoutMode()
{
    return { kPitchCenterX, kPenaltyShootoutCameraY };
}

static CameraParams benchMode(bool substitutingPlayer)
{
    auto limit = substitutingPlayer ? kSubstituteCameraLimit : getBenchCameraXLimit();
    return { benchCameraX(), kPitchCenterY, limit };
}

static CameraParams leavingBenchMode()
{
    return { kLeavingBenchCameraDestX, kPitchCenterY, kPitchSideCameraLimitDuringBreak };
}

static CameraParams standardMode()
{
    FixedPoint xDirection, yDirection;

    if (swos.gameStatePl == GameState::kInProgress) {
        xDirection = swos.ballSprite.deltaX;
        yDirection = swos.ballSprite.deltaY;
    } else {
        std::tie(xDirection, yDirection) = getGameStoppedCameraDirections();
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

static void updateCameraLeaving()
{
    constexpr int kCameraLeavingBenchXLimit = 35;

    if (m_leavingBenchMode) {
        bool benchVisibleByX = m_cameraX < kCameraLeavingBenchXLimit;
        m_leavingBenchMode = benchVisibleByX;
    }
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

#ifdef SWOS_TEST
    auto deltaX = FixedPoint(((xDest.whole() - cameraX.whole()) >> 2) * 0x4000, true);
    auto deltaY = FixedPoint(((yDest.whole() - cameraY.whole()) >> 2) * 0x4000, true);
#else
    auto deltaX = (xDest - cameraX) / 16;
    auto deltaY = (yDest - cameraY) / 16;
#endif

    clipCameraMovement(deltaX, deltaY);

    cameraX += deltaX;
    cameraY += deltaY;

    constrainCameraToPitch(cameraX, cameraY);
    updateCameraCoordinates(cameraX, cameraY);
}

static void clipCameraDestination(FixedPoint& xDest, FixedPoint& yDest, int xLimit)
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

static void clipCameraMovement(FixedPoint& deltaX, FixedPoint& deltaY)
{
    constexpr FixedPoint kMaxCameraMovement = 5;

    if (deltaX > kMaxCameraMovement)
        deltaX = kMaxCameraMovement;
    if (deltaX < -kMaxCameraMovement)
        deltaX = -kMaxCameraMovement;
    if (deltaY > kMaxCameraMovement)
        deltaY = kMaxCameraMovement;
    if (deltaY < -kMaxCameraMovement)
        deltaY = -kMaxCameraMovement;
}

static void constrainCameraToPitch(FixedPoint& cameraX, FixedPoint& cameraY)
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

static void updateCameraCoordinates(const FixedPoint& cameraX, const FixedPoint& cameraY)
{
    setCameraX(cameraX);
    setCameraY(cameraY);
}

static int getBenchCameraXLimit()
{
    auto limit = kPitchSideCameraLimitDuringBreak;

#ifdef SWOS_TEST
    bool cameraAtBenchLevel = getCameraY().whole() >= kBenchSlideAreaStartY && getCameraY().whole() <= kBenchSlideAreaEndY;
#else
    bool cameraAtBenchLevel = getCameraY() >= kBenchSlideAreaStartY && getCameraY() <= kBenchSlideAreaEndY;
#endif

    // These sprite conditions are weird, I don't fully understand them so leaving them in verbatim.
    // Image index should always be set, so it's something "if both goals are not visible". But at
    // this y range goals shouldn't be visible anyway -- is it a tautology?
    assert(!cameraAtBenchLevel || ((swos.goal1TopSprite.hasNoImage() || !swos.goal1TopSprite.onScreen) &&
        (swos.goal2BottomSprite.hasNoImage() || !swos.goal2BottomSprite.onScreen)));
    if (cameraAtBenchLevel &&
        (swos.goal1TopSprite.hasNoImage() || !swos.goal1TopSprite.onScreen) &&
        (swos.goal2BottomSprite.hasNoImage() || !swos.goal2BottomSprite.onScreen))
        limit = swos.g_substituteInProgress ? kSubstituteCameraLimit : kCameraMinX;

    return limit;
}

static std::pair<int, int> getGameStoppedCameraDirections()
{
    int xDirection = 0, yDirection = 0;

    Direction direction;
    bool gotDirection = false;

    if (swos.lastTeamPlayedBeforeBreak && swos.lastTeamPlayedBeforeBreak->controlledPlayer) {
        direction = swos.lastTeamPlayedBeforeBreak->controlledPlayer->direction;
        gotDirection = true;
    } else {
        direction = swos.cameraDirection;
    }

    if (gotDirection || direction != Direction::kNoDirection) {
        static const int8_t kNextCameraDirections[16] = {
            0, -1, 1, -1, 1, 0, 1, 1, 0, 1, -1, 1, -1, 0, -1, -1
        };
        xDirection = kNextCameraDirections[2 * static_cast<int>(direction)];
        yDirection = kNextCameraDirections[2 * static_cast<int>(direction) + 1];
    }

    return { xDirection, yDirection };
}

static std::pair<int, int> getStandardModeCameraVelocity(FixedPoint xDirection, FixedPoint yDirection)
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
    return { kPlayersOutsidePitchX, kPitchCenterY, limit };
}

static CameraParams showResultAtCenter(int limit)
{
    return { kPitchCenterX, kPitchCenterY, limit };
}

static CameraParams showResultAtTop(int limit)
{
    return { kPitchCenterX, kTopGoalLine, limit };
}

static CameraParams followTheBall(int limit, int xVelocity, int yVelocity)
{
    return { swos.ballSprite.x, swos.ballSprite.y, limit, xVelocity, yVelocity };
}
