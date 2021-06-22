#include "videoOptionsMenu.h"
#include "joypads.h"
#include "VirtualJoypad.h"
#include "windowManager.h"
#include "windowModeMenu.h"
#include "render.h"

static int16_t m_useLinearFiltering = 0;
static int16_t m_flashCursor = 1;
static int16_t m_showTouchTrails;
static int16_t m_transparentButtons = 1;

#include "videoOptions.mnu.h"

using namespace VideoOptionsMenu;

void showVideoOptionsMenu()
{
    showMenu(videoOptionsMenu);
}

static void videoOptionsMenuOnInit()
{
    m_useLinearFiltering = getLinearFiltering();
    m_flashCursor = cursorFlashingEnabled();
#ifdef VIRTUAL_JOYPAD
    m_showTouchTrails = getShowTouchTrails();
    m_transparentButtons = getTransparentVirtualJoypadButtons();
#else
    int diff;
    auto label = &firstLabelEntry;
    auto changer = &firstChangerItemEntry;

    static_assert(transparentVirtualJoypadButtonsLabel > showTouchTrailsLabel, "Stop! Hammer time!");

    while (label != &firstChangerItemEntry) {
        if (label->ordinal == showTouchTrailsLabel) {
            diff = 2 * (label == &firstChangerItemEntry ? label[1].y - label->y : label->y - label[-1].y);
            label->hide();
            changer->hide();
        } else if (label->ordinal == transparentVirtualJoypadButtonsLabel) {
            label->hide();
            changer->hide();
        } else if (label->ordinal > showTouchTrailsLabel) {
            label->y -= diff;
            changer->y -= diff;
        }
        label++;
        changer++;
    }

    showWindowModeEntry.y -= diff;
#endif
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
