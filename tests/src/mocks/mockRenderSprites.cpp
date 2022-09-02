#include "mockRenderSprites.h"
#include "mockRender.h"

using RenderedSprites = std::vector<RenderedSprite>;

RenderedSprites m_spritesA;
RenderedSprites m_spritesB;
RenderedSprites *m_currentSprites = &m_spritesA;
RenderedSprites *m_previousSprites = &m_spritesB;

void initMockSpriteRenderer()
{
    addUpdateHook([] {
        std::swap(m_currentSprites, m_previousSprites);
        m_currentSprites->clear();
    });
}

const std::vector<RenderedSprite>& renderedSprites()
{
    return *m_previousSprites;
}

const RenderedSprite *findRenderedSprite(int spriteIndex)
{
    auto sprite = std::find_if(m_previousSprites->begin(), m_previousSprites->end(), [spriteIndex](const auto& sprite) {
        return sprite.index == spriteIndex;
    });
    return sprite != m_previousSprites->end() ? &*sprite : nullptr;
}

#define drawMenuSprite drawMenuSprite_REAL
#include "renderSprites.cpp"
#undef drawMenuSprite

void drawMenuSprite(int spriteIndex, int x, int y)
{
    m_currentSprites->emplace_back(spriteIndex, x, y);
    drawMenuSprite_REAL(spriteIndex, x, y);
}
