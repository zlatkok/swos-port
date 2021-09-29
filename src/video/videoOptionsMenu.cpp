#include "videoOptionsMenu.h"
#include "joypads.h"
#include "VirtualJoypad.h"
#include "windowManager.h"
#include "windowModeMenu.h"
#include "pitch.h"

constexpr float kZoomIncrement = 0.1f;

static int16_t m_useLinearFiltering = 0;
static int16_t m_flashCursor = 1;
static int16_t m_showTouchTrails;
static int16_t m_transparentButtons = 1;
static int16_t m_showFps;

#include "videoOptions.mnu.h"

using namespace VideoOptionsMenu;

#ifndef VIRTUAL_JOYPAD
static void hideVirtualJoypadOptions();
#endif

void showVideoOptionsMenu()
{
    showMenu(videoOptionsMenu);
}

static void videoOptionsMenuOnInit()
{
    m_useLinearFiltering = getLinearFiltering();
    m_flashCursor = cursorFlashingEnabled();
    m_showFps = getShowFps();
#ifdef VIRTUAL_JOYPAD
    m_showTouchTrails = getShowTouchTrails();
    m_transparentButtons = getTransparentVirtualJoypadButtons();
#else
    hideVirtualJoypadOptions();
#endif
}

#ifndef VIRTUAL_JOYPAD
static void hideVirtualJoypadOptions()
{
    static_assert(transparentVirtualJoypadButtonsLabel > showTouchTrailsLabel &&
        transparentVirtualJoypadButtons > showTouchTrails, "Stop! Hammer time!");

    assert(transparentVirtualJoypadButtonsLabelEntry.y == transparentVirtualJoypadButtonsEntry.y &&
        showTouchTrailsLabelEntry.y == showTouchTrailsEntry.y &&
        transparentVirtualJoypadButtonsEntry.y > showTouchTrailsEntry.y);

    int diff = 2 * (transparentVirtualJoypadButtonsEntry.y - showTouchTrailsEntry.y);

    auto currentMenu = getCurrentMenu();
    for (auto item = currentMenu->entries(); item < &exitEntry; item++) {
        if (item == &transparentVirtualJoypadButtonsLabelEntry || item == &transparentVirtualJoypadButtonsEntry ||
            item == &showTouchTrailsLabelEntry || item == &showTouchTrailsEntry)
            item->hide();
        else if (item->y > showTouchTrailsEntry.y)
            item->y -= diff;
    }
}
#endif

static void zoomBeforeDraw()
{
    formatDoubleNoTrailingZeros(getZoomFactor(), zoomEntry.string(), kStdMenuTextSize, 4);
}

static void decreaseZoom()
{
    setZoomFactor(getZoomFactor() - kZoomIncrement);
}

static void increaseZoom()
{
    setZoomFactor(getZoomFactor() + kZoomIncrement);
}

static void enterZoom()
{
}

static void switchLinearFiltering()
{
    m_useLinearFiltering = !m_useLinearFiltering;
    logInfo("Linear filtering %s", m_useLinearFiltering ? "enabled" : "disabled");
    setLinearFiltering(m_useLinearFiltering != 0);
}

static void changeFlashCursor()
{
    m_flashCursor = !m_flashCursor;
    logInfo("Flash cursor changed to %s", m_flashCursor ? "ON" : "OFF");
    setFlashMenuCursor(m_flashCursor != 0);
    swos.menuCursorFrame = 0;
}

static void changeShowFps()
{
    m_showFps = !m_showFps;
    setShowFps(m_showFps != 0);
}

static void changeShowTouchTrails()
{
#ifdef VIRTUAL_JOYPAD
    m_showTouchTrails = !m_showTouchTrails;
    logInfo("Touch trails changed to %s", m_showTouchTrails ? "ON" : "OFF");
    setShowTouchTrails(m_showTouchTrails != 0);
    getVirtualJoypad().enableTouchTrails(m_showTouchTrails != 0);
#endif
}

static void changeVirtualJoypadTransparentButtons()
{
#ifdef VIRTUAL_JOYPAD
    m_transparentButtons = !m_transparentButtons;
    logInfo("Turning transparent virtual joypad buttons %s", m_transparentButtons ? "ON" : "OFF");
    setTransparentVirtualJoypadButtons(m_transparentButtons != 0);
    getVirtualJoypad().enableTransparentButtons(m_transparentButtons != 0);
#endif
}

static void openWindowModeMenu()
{
    showWindowModeMenu();
}
