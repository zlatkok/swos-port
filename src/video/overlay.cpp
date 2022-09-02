#include "overlay.h"
#include "windowManager.h"
#include "render.h"
#include "game.h"
#include "pitch.h"
#include "text.h"
#include "util.h"

constexpr int kShowZoomSolid = 900;
constexpr int kFadeZoom = 600;
constexpr int kShowZoomInterval = kShowZoomSolid + kFadeZoom;

constexpr int kInfoX = 290;
constexpr int kFpsY = 4;

constexpr Uint32 kInfoMessageInterval = 1'100;

static bool m_showFps;

static Uint32 m_infoMessageTimestamp;
static char m_infoBuffer[1024];

static void showFps();
static void showZoomFactor();
static void showInfoMessage();

void showOverlay()
{
    showFps();
    showZoomFactor();
    showInfoMessage();
}

bool getShowFps()
{
    return m_showFps;
}

void setShowFps(bool showFps)
{
    m_showFps = showFps;
}

void enqueueInfoMessage(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vsnprintf(m_infoBuffer, sizeof(m_infoBuffer), format, args);
    va_end(args);

    m_infoMessageTimestamp = SDL_GetTicks();
}

static void showFps()
{
    if (getShowFps()) {
        constexpr int kNumFramesForFps = 64;

        auto now = SDL_GetPerformanceCounter();

        static Uint64 s_lastFrameTick;
        if (s_lastFrameTick) {
            static std::array<Uint64, kNumFramesForFps> s_renderTimes;
            static int s_renderTimesIndex;

            auto frameTime = now - s_lastFrameTick;

            s_renderTimes[s_renderTimesIndex] = frameTime;
            s_renderTimesIndex = (s_renderTimesIndex + 1) % s_renderTimes.size();
            int numFrames = 0;

            auto totalRenderTime = std::accumulate(s_renderTimes.begin(), s_renderTimes.end(), 0ULL, [&](auto sum, auto current) {
                if (current) {
                    sum += current;
                    numFrames++;
                }
                return sum;
            });

            auto fps = .0;
            if (numFrames)
                fps = 1. / (static_cast<double>(totalRenderTime) / numFrames / SDL_GetPerformanceFrequency());

            char buf[32];
            formatDoubleNoTrailingZeros(fps, buf, sizeof(buf), 2);

            drawText(kInfoX, kFpsY, buf);
        }

        s_lastFrameTick = now;
    }
}

static void showZoomFactor()
{
    if (!isMatchRunning())
        return;

    auto now = SDL_GetTicks();
    auto interval = now - getLastZoomChangeTime();

    if (interval < kShowZoomInterval) {
        int alpha = 255;
        if (interval > kShowZoomSolid)
            alpha = 255 * (kFadeZoom - (interval - kShowZoomSolid)) / kFadeZoom;

        int y = kFpsY;
        if (getShowFps())
            y += kSmallFontHeight + 2;

        char buf[32];
        int len = formatDoubleNoTrailingZeros(getZoomFactor(), buf, sizeof(buf), 2);

        buf[len] = 'x';
        buf[len + 1 ] = '\0';

        drawText(kInfoX, y, buf, -1, kWhiteText, false, alpha);
    }
}

static void showInfoMessage()
{
    if (m_infoMessageTimestamp + kInfoMessageInterval >= SDL_GetTicks())
        drawTextCentered(kVgaWidth / 2, kVgaHeight / 2 - kSmallFontHeight / 2, m_infoBuffer);
}
