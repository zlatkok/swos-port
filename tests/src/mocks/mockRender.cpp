#include "render.h"
#include "mockRender.h"

static UpdateHook m_updateHook;

void setUpdateHook(UpdateHook updateHook)
{
    m_updateHook = updateHook;
}

void updateScreen()
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
void timerProc() {}
void fadeIfNeeded() {}
void makeScreenshot() {}
bool getLinearFiltering() { return false; }
void setLinearFiltering(bool) {}

void SWOS::Flip() {}
