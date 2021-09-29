#include "spinningLogo.h"
#include "renderSprites.h"
#include "windowManager.h"
#include "bench.h"
#include "stats.h"

constexpr float kLogoXEdgeDist = 35;
constexpr float kLogoY = 14;

static bool m_enabled;
static int m_frameIndex;

static int m_pictureIndex;

void updateSpinningLogo()
{
    if (m_enabled) {
        bool logoSpinning = !inBenchMenus() && !showingPostGameStats();
        if (logoSpinning && (swos.stoppageTimer & 2))
            m_frameIndex = (m_frameIndex + 1) & 0x3f;

        m_pictureIndex = kBigSSpriteStart + m_frameIndex / 2;
    }
}

void drawSpinningLogo()
{
    if (m_enabled)
        drawSprite(m_pictureIndex, kVgaWidth + getScreenXOffset() - kLogoXEdgeDist, kLogoY, false, 0, 0);
}

void toggleSpinningLogo()
{
    m_enabled = !m_enabled;
}
