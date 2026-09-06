#include "amigaMode.h"
#include "gameLoop.h"
#include "updatePlayers.h"
#include "timer.h"
#include "ball.h"

static bool m_enabled;
static bool m_preventDirectionFlip;

static const uint32_t kGoalkeeperDiveDeltasAmiga[8] = {
    0x3'0000, 0x3'8000, 0x4'0000, 0x4'8000, 0x5'0000, 0x5'8000, 0x6'0000, 0x6'8000
};
static const uint32_t kGoalkeeperDiveDeltasPC[8] = {
    0x2'8000, 0x3'0000, 0x3'8000, 0x4'0000, 0x4'8000, 0x5'0000, 0x5'8000, 0x6'0000
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
            setControlledBallSpeedReduction(16);
            setBallAirSpeedReduction(10);
            setBallAirFriction(4608);

            memcpy(swos.kGoalkeeperDiveDeltas, kGoalkeeperDiveDeltasAmiga, sizeof(kGoalkeeperDiveDeltasAmiga));

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
            setControlledBallSpeedReduction(13);
            setBallAirSpeedReduction(4);
            setBallAirFriction(3291);

            memcpy(swos.kGoalkeeperDiveDeltas, kGoalkeeperDiveDeltasPC, sizeof(kGoalkeeperDiveDeltasPC));

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
        SwosVM::flags.zero = !m_preventDirectionFlip;
    }
}

void writeAmigaModeDirectionFlip(TeamGeneralInfo *team)
{
    if (m_preventDirectionFlip) {
        team->currentAllowedDirection = -1;
        D0.lo16 = -1;
        SwosVM::ax = -1;
    }
}
