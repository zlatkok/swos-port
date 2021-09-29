#include "render.h"
#include "mockRender.h"

static UpdateHook m_updateHook;

void setUpdateHook(UpdateHook updateHook)
{
    m_updateHook = updateHook;
}

void updateScreen(bool)
{
    if (m_updateHook)
        m_updateHook();
}

void initRendering() {}
void finishRendering() {}
SDL_Renderer *getRenderer() { return nullptr; }
SDL_Rect getViewport() { return {}; }
void setPalette(const char *, int) {}
void getPalette(char *) {}
void skipFrameUpdate() {}
void frameDelay(double) {}
void fadeIfNeeded() {}
void fadeIn(std::function<void()>, double) {}
void fadeOut(std::function<void()>, double) {}
void makeScreenshot() {}
void drawRectangle(int x, int y, int width, int height, const Color& color) {}
bool getLinearFiltering() { return false; }
void setLinearFiltering(bool) {}
