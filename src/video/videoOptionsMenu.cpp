#include "videoOptionsMenu.h"
#include "joypads.h"
#include "windowManager.h"
#include "drawMenu.h"
#include "windowModeMenu.h"
#include "menuItemRenderer.h"
#include "render.h"
#include "overlay.h"
#include "pitch.h"

constexpr float kZoomIncrement = 0.1f;

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
#ifndef VIRTUAL_JOYPAD
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

static void setTouchTrailsEntryText()
{
#ifdef VIRTUAL_JOYPAD
    auto entry = A5.asMenuEntry();
    entry->copyString(getShowTouchTrails() ? "ON" : "OFF");
#endif
}

static void changeShowTouchTrails()
{
#ifdef VIRTUAL_JOYPAD
    logInfo("Changing touch trails to %s", getShowTouchTrails() ? "OFF" : "ON");
    setShowTouchTrails(!getShowTouchTrails());
#endif
}

static void setTransparentButtonsEntryText()
{
#ifdef VIRTUAL_JOYPAD
    auto entry = A5.asMenuEntry();
    entry->copyString(getTransparentVirtualJoypadButtons() ? "ON" : "OFF");
#endif
}

static void changeVirtualJoypadTransparentButtons()
{
#ifdef VIRTUAL_JOYPAD
    logInfo("Turning transparent virtual joypad buttons %s", getTransparentVirtualJoypadButtons() ? "OFF" : "ON");
    setTransparentVirtualJoypadButtons(!getTransparentVirtualJoypadButtons());
#endif
}

static void openWindowModeMenu()
{
    showWindowModeMenu();
}
