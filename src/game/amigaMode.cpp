#include "amigaMode.h"
#include "gameLoop.h"
#include "result.h"
#include "updatePlayers.h"
#include "timer.h"
#include "ball.h"

static bool m_enabled;
static bool m_preventDirectionFlip;

static constexpr std::array<FixedPoint, 8> kGoalkeeperDiveDeltasAmiga = {
    3.0_fp, 3.5_fp, 4.0_fp, 4.5_fp, 5.0_fp, 5.5_fp, 6.0_fp, 6.5_fp,
};
static constexpr std::array<FixedPoint, 8> kGoalkeeperDiveDeltasPC = {
    2.5_fp, 3.0_fp, 3.5_fp, 4.0_fp, 4.5_fp, 5.0_fp, 5.5_fp, 6.0_fp,
};

bool amigaModeActive()
{
    return m_enabled;
}

void setAmigaModeEnabled(bool enable)
{
    if (enable != m_enabled) {
        if (m_enabled = enable) {
            logInfo("Switching to Amiga game mode");
            setPenaltiesInterval(100);
            setInitalKickInterval(750);
            setGoalCameraInterval(50);
            setAllowPlayerControlCameraInterval(500);
            setPlayerDownTacklingInterval(50);
            setPlayerDownHeadingInterval(50);
            setClearResultInterval(600);
            setClearResultHalftimeInterval(350);
            swos.kKeeperSaveDistance = 24;
            setControlledBallSpeedReduction(0.03125_speed);
            setBallAirSpeedReduction(0.01953125_speed);
            setBallAirFriction(4608);

            std::copy(kGoalkeeperDiveDeltasAmiga.begin(), kGoalkeeperDiveDeltasAmiga.end(), swos.kGoalkeeperDiveDeltas);

            setTargetFps(kTargetFpsAmiga);
        } else {
            logInfo("Switching to PC game mode");
            setPenaltiesInterval(110);
            setInitalKickInterval(825);
            setGoalCameraInterval(55);
            setAllowPlayerControlCameraInterval(550);
            setPlayerDownTacklingInterval(55);
            setPlayerDownHeadingInterval(55);
            setClearResultInterval(660);
            setClearResultHalftimeInterval(385);
            swos.kKeeperSaveDistance = 16;
            setControlledBallSpeedReduction(0.025390625_speed);
            setBallAirSpeedReduction(0.0078125_speed);
            setBallAirFriction(3291);

            std::copy(kGoalkeeperDiveDeltasPC.begin(), kGoalkeeperDiveDeltasPC.end(), swos.kGoalkeeperDiveDeltas);

            setTargetFps(kTargetFpsPC);
        }
    }
}

void checkForAmigaModeDirectionFlipBan(const Sprite *sprite)
{
    m_preventDirectionFlip = false;
    if (amigaModeActive()) {
        m_preventDirectionFlip = sprite->x.whole() >= 273 && sprite->x.whole() <= 398 &&
            (sprite->y.whole() <= 158 || sprite->y.whole() >= 740);
    }
}

void applyAmigaModeDirectionFlipBan(TeamGeneralInfo *team)
{
    if (m_preventDirectionFlip)
        team->currentAllowedDirection = Direction::kNoDirection;
}
