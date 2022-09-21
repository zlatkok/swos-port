#include "menuControls.h"
#include "menus.h"
#include "gameControlEvents.h"
#include "gameControls.h"
#include "controls.h"
#include "keyboard.h"
#include "mouse.h"
#include "joypads.h"
#include "keyBuffer.h"
#include "util.h"

static constexpr int kFireDelay = 303;
static constexpr int kFireRepeatRate = 229;
static constexpr int kMovementDelay = 151;
static constexpr int kMovementRepeatRate = 74;

static GameControlEvents m_clearEvents;
static bool m_fire;

static void handleFireReset();
static void updateLongFireTimer();
static void handleControlDelay();
static GameControlEvents filterResetEvents(GameControlEvents events);
static GameControlEvents getEventsFromAllControllers();

// MenuCheckControls
//
// Handles controls in the menus. Highest level proc.
// Combines (ORs) the controls from all controllers into controls used in the menus.
//
void menuCheckControls()
{
    processControlEvents();

    auto events = getEventsFromAllControllers();
    if (events & kGameEventZoomIn)
        events |= kGameEventUp;
    if (events & kGameEventZoomOut)
        events |= kGameEventDown;

    if (isLastKeyPressed(SDL_SCANCODE_RETURN) || isLastKeyPressed(SDL_SCANCODE_RETURN2) || isLastKeyPressed(SDL_SCANCODE_KP_ENTER))
        events |= kGameEventKick;

    if (isLastKeyPressed(SDL_SCANCODE_BACKSPACE) || isLastKeyPressed(SDL_SCANCODE_LEFT) && (SDL_GetModState() & KMOD_ALT))
        exitCurrentMenu();

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

void SWOS::MenuCheckControls()
{
    menuCheckControls();
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

// Prevents menu controls spamming wildly by inserting a delay before a control that's being held repeats.
// There is an initial delay before a key starts repeating, and another smaller one between repeated keys.
// Fire has a separate, longer delay, since effects of its spam can be more devastating.
static void handleControlDelay()
{
    static Uint32 s_noRepeatedInputUntil;
    static int s_lastControls;

    auto now = SDL_GetTicks();
    if (swos.menuControlsDirection != s_lastControls) {
        s_lastControls = swos.menuControlsDirection;
        auto initialDelay = swos.fire ? kFireDelay : kMovementDelay;
        s_noRepeatedInputUntil = now + initialDelay;
    } else {
        if (now >= s_noRepeatedInputUntil) {
            int repeatDelay = swos.fire ? kFireRepeatRate : kMovementRepeatRate;
            s_noRepeatedInputUntil = now + repeatDelay;
        } else {
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

static GameControlEvents getEventsFromAllControllers()
{
    auto events = eventsFromAllKeysets();
    return events | eventsFromAllJoypads();
}
