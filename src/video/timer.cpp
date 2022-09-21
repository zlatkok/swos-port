#include "timer.h"
#include "game.h"

static constexpr int kMenuTargetFps = 30;
static constexpr int kMaxLastFrames = 32;

static double m_targetFps = kTargetFpsPC;

static Sint64 m_frequency;

static Uint64 m_lastFrameTicks;
static double m_ticksPerFrame;

static Uint64 m_frameStartTime;
static Uint64 m_overShoot;

static std::deque<Uint64> m_lastFramesDelay;

static std::array<Uint64, kMaxLastFrames> m_renderTimes;
static int m_renderTimesIndex;

static void sleep(Uint64 sleepTicks);
static Uint64 getAverageRenderTime();

void initTimer()
{
    m_frequency = SDL_GetPerformanceFrequency();
    m_ticksPerFrame = m_frequency / targetFps();
}

void initFrameTicks()
{
    m_lastFrameTicks = SDL_GetPerformanceCounter();
    m_renderTimes.fill(0);
}

double targetFps()
{
    return m_targetFps;
}

void setTargetFps(double fps)
{
    m_targetFps = fps;
}

// Simulates SWOS procedure executed at each interrupt 8 tick.
// This will have to change and be calculated dynamically based on time elapsed since the last call.
void timerProc(int factor /* = 1 */)
{
    auto now = SDL_GetPerformanceCounter();
    auto ticksElapsed = now - m_frameStartTime;

    int framesElapsed = std::max(std::lround(ticksElapsed / m_ticksPerFrame), 1l);
    framesElapsed *= factor;
    framesElapsed = std::min(framesElapsed, 6);

    swos.currentTick += framesElapsed;

    if (!isGamePaused())
        swos.stoppageTimer += framesElapsed;
}

void markFrameStartTime()
{
    m_frameStartTime = SDL_GetPerformanceCounter();
}

void menuFrameDelay()
{
    // don't use busy wait in menus and subtract estimated render time to keep things simple
    constexpr int kEstimatedMenuRenderTime = 4;
    auto delay = std::max(1'000 / kMenuTargetFps - kEstimatedMenuRenderTime, 0);
    SDL_Delay(delay);
}

void gameFrameDelay()
{
    Uint64 desiredDelay = std::lround(m_frequency / targetFps());
    desiredDelay -= getAverageRenderTime();

    auto diff = SDL_GetPerformanceCounter() - m_frameStartTime;

    if (diff < desiredDelay) {
        sleep(desiredDelay - diff);

        Uint64 startTicks;
        do {
            startTicks = SDL_GetPerformanceCounter();
        } while (m_frameStartTime + desiredDelay > startTicks);

        m_overShoot = startTicks - m_frameStartTime - desiredDelay;
    }

    timerProc();
}

void measureRendering(std::function<void()> render)
{
    auto start = SDL_GetPerformanceCounter();
    render();
    auto time = SDL_GetPerformanceCounter() - start;

    m_renderTimes[m_renderTimesIndex] = time;
    m_renderTimesIndex = (m_renderTimesIndex + 1) % kMaxLastFrames;

    m_frameStartTime = SDL_GetPerformanceCounter() - m_overShoot;
}

static void sleep(Uint64 sleepTicks)
{
    // measure deviation from the desired sleep value and what the system actually delivers
    constexpr int kNumFramesForSlackValue = 64;
    static std::array<Uint64, kNumFramesForSlackValue> s_slackValues;
    static int s_slackValueIndex;

    auto slackValue = std::accumulate(s_slackValues.begin(), s_slackValues.end(), 0LL);
    slackValue = (slackValue + (slackValue > 0 ? kNumFramesForSlackValue : -kNumFramesForSlackValue) / 2) / kNumFramesForSlackValue;

    if (static_cast<Sint64>(sleepTicks) > slackValue) {
        auto intendedDelay = 1'000 * (sleepTicks - slackValue) / m_frequency;

        auto delayStart = SDL_GetPerformanceCounter();
        SDL_Delay(static_cast<Uint32>(intendedDelay));
        auto actualDelay = SDL_GetPerformanceCounter() - delayStart;

        s_slackValues[s_slackValueIndex] = actualDelay - intendedDelay * m_frequency / 1'000;
        s_slackValueIndex = (s_slackValueIndex + 1) % kNumFramesForSlackValue;
    }
}

static Uint64 getAverageRenderTime()
{
    auto sum = std::accumulate(m_renderTimes.begin(), m_renderTimes.end(), 0LL);
    return (sum + m_renderTimes.size() / 2) / m_renderTimes.size();
}
