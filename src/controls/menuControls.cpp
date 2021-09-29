#include "menuControls.h"
#include "gameControlEvents.h"
#include "gameControls.h"
#include "controls.h"
#include "keyboard.h"
#include "mouse.h"
#include "joypads.h"
#include "keyBuffer.h"
#include "util.h"

static GameControlEvents m_clearEvents;
static bool m_fire;

static GameControlEvents getEventsFromAllControllers()
{
    auto events = eventsFromAllKeysets();
    return events | eventsFromAllJoypads();
}

void resetControls()
{
    m_clearEvents = getEventsFromAllControllers();

    resetMouse();
    //may be unnecessary if only called from menus

    // these can be removed once the edit tactics move ball routine is converted
    swos.shortFire = 0;
    swos.fire = 0;
    swos.left = 0;
    swos.right = 0;
    swos.up = 0;
    swos.down = 0;
    swos.menuControlsDirection = -1;
    swos.menuControlsDirection2 = -1;
}

// Only in a single frame.
void triggerMenuFire()
{
    m_fire = true;
}

static void handleFireReset()
{
    if (swos.fireResetFlag) {
        if (swos.fire)
            swos.fire = swos.shortFire = 0;
        else
            swos.fireResetFlag = 0;
    }
}

static void updateLongFireTimer()
{
    if (swos.shortFire) {
        swos.longFireTime = swos.longFireFlag = 0;
    } else if (swos.fire) {
        swos.longFireTime += swos.timerDifference;
        if (swos.longFireTime >= 24) {
            swos.longFireTime = 16;
            swos.longFireFlag++;
            swos.shortFire = 1;
        }
    }
}

static void handleControlDelay()
{
    static int s_controlsHeldTimer;
    static int s_lastControls;

    if (swos.menuControlsDirection != s_lastControls) {
        s_controlsHeldTimer = 0;
        s_lastControls = swos.menuControlsDirection;
    } else {
        s_controlsHeldTimer += swos.timerDifference;
        if (swos.fire) {
            if (s_controlsHeldTimer >= 24)
                s_controlsHeldTimer = 16;
            else
                swos.menuControlsDirection = -1;
        } else {
            if (s_controlsHeldTimer >= 12)
                s_controlsHeldTimer = 8;
            else
                swos.menuControlsDirection = -1;
        }
    }
}

static GameControlEvents filterResetEvents(GameControlEvents events)
{
    if (m_clearEvents) {
        auto newEventMask = m_clearEvents & events;
        events &= ~m_clearEvents;
        m_clearEvents = newEventMask;
    }

    return events;
}

// MenuCheckControls
//
// Handles controls in the menus. Highest level proc.
// Combines (ORs) the controls from all controllers into controls used in the menus.
//
void SWOS::MenuCheckControls()
{
    processControlEvents();

    auto events = getEventsFromAllControllers();
    if (events & kGameEventZoomIn)
        events |= kGameEventUp;
    if (events & kGameEventZoomOut)
        events |= kGameEventDown;

    events = filterResetEvents(events);
    auto shortFire = getShortFireAndBumpFireCounter((events & kGameEventNonMovementMask) != 0) || m_fire;
    m_fire = false;

    if (swos.fireCounter > 0)   // used in EditTactics menu in two functions
        swos.fireCounter = 0;

    flushKeyBuffer();

    swos.shortFire = shortFire;
    swos.fire = (events & kGameEventNonMovementMask) != 0;
    swos.left = (events & kGameEventLeft) != 0;
    swos.right = (events & kGameEventRight) != 0;
    swos.up = (events & (kGameEventUp | kGameEventZoomIn)) != 0;
    swos.down = (events & (kGameEventDown | kGameEventZoomOut)) != 0;

    auto direction = eventsToDirection(events);
    swos.menuControlsDirection = swos.menuControlsDirection2 = direction;

    handleFireReset();
    updateLongFireTimer();
    handleControlDelay();
}
