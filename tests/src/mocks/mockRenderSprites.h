#pragma once

struct RenderedSprite {
    int index;
    int x;
    int y;
};

void initMockSpriteRenderer();
const std::vector<RenderedSprite>& renderedSprites();
const RenderedSprite *findRenderedSprite(int spriteIndex);
