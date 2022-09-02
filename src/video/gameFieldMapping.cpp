#include "gameFieldMapping.h"
#include "render.h"
#include "pitch.h"
#include "windowManager.h"

static float m_gameScale;
static float m_gameOffsetX;
static float m_gameOffsetY;

static float m_fieldWidth;
static float m_fieldHeight;

bool mapCoordinatesToGameArea(int& x, int& y)
{
    int windowWidth, windowHeight;

    if (isInFullScreenMode()) {
        std::tie(windowWidth, windowHeight) = getFullScreenDimensions();
    } else {
        auto viewPort = getViewport();
        x -= viewPort.x;
        y -= viewPort.y;
    }

    if (x < m_gameOffsetX || y < m_gameOffsetY)
        return false;

    x = static_cast<int>(std::round((x - m_gameOffsetX) / m_gameScale));
    y = static_cast<int>(std::round((y - m_gameOffsetY) / m_gameScale));

    return true;
}

void updateLogicalScreenSize()
{
    int width, height;
    std::tie(width, height) = getWindowSize();
    m_fieldWidth = width / m_gameScale;
    m_fieldHeight = height / m_gameScale;

    auto renderer = getRenderer();
    if (m_fieldWidth > kPitchWidth || m_fieldHeight > kPitchHeight) {
        m_fieldWidth = std::min(m_fieldWidth, static_cast<float>(kPitchWidth));
        m_fieldHeight = std::min(m_fieldHeight, static_cast<float>(kPitchHeight));
        SDL_RenderSetLogicalSize(renderer, std::lroundf(m_fieldWidth * m_gameScale), std::lroundf(m_fieldHeight * m_gameScale));
    } else {
        SDL_RenderSetLogicalSize(renderer, 0, 0);
    }

    m_gameOffsetX = (m_fieldWidth - kVgaWidth) / 2 * m_gameScale;
    m_gameOffsetY = (m_fieldHeight - kVgaHeight) / 2 * m_gameScale;

    assert(m_gameOffsetX >= -0.001 && m_gameOffsetY >= -0.001);
}

void updateGameScaleFactor(int width, int height)
{
    auto scaleX = static_cast<float>(width) / kVgaWidth;
    auto scaleY = static_cast<float>(height) / kVgaHeight;
    m_gameScale = std::min(scaleX, scaleY);
}

float getGameScale()
{
    return m_gameScale;
}

float getGameScreenOffsetX()
{
    return m_gameOffsetX;
}

float getGameScreenOffsetY()
{
    return m_gameOffsetY;
}

float getFieldWidth()
{
    return m_fieldWidth;
}

float getFieldHeight()
{
    return m_fieldHeight;
}

SDL_FRect mapRect(int x, int y, int width, int height)
{
    return { m_gameOffsetX + x * m_gameScale, m_gameOffsetY + y * m_gameScale, width * m_gameScale, height * m_gameScale };
}
