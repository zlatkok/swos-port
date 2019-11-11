#include "sdlProcs.h"
#include "SdlAddressTableFetcher.h"
#include <SDL2/SDL.h>

static void **m_table;
static std::unique_ptr<void *[]> m_originalTable;
static uint32_t m_tableSize;

static std::vector<SDL_DisplayMode> m_displayModes;
static std::deque<SDL_Event> m_eventQueue;
static int m_mouseX;
static int m_mouseY;
static Uint32 m_mouseButtonFlags;

static SDL_Scancode m_scancodes[SDL_NUM_SCANCODES];

static int m_getTicksDelta;
static Uint32 m_lastGetTicks;
static bool m_timeFrozen;

static void SDL_Delay_NOP(Uint32) {}
static int SDL_PeepEvents_NOP(SDL_Event *, int, SDL_eventaction, Uint32) { return 0; }

static int dummyIntProc(int) { return 0; }

struct EnvSetter {
    EnvSetter() {
        _putenv("SDL_DYNAMIC_API=" SDL_ADDRESS_FETCHER_DLL);
    }
};

// set SDL environment variable before any SDL function gets called to make sure it will load our DLL
static EnvSetter e;

// Hooks all functions that might change state of the system.
static void mockFunctions()
{
    setSdlProc(SDL_SetCursorIndex, dummyIntProc);
}

bool initSdlApiTable()
{
    m_table = GetFunctionTable(&m_tableSize);
    if (m_table) {
        m_originalTable = std::make_unique<void *[]>(m_tableSize);
        memcpy(m_originalTable.get(), m_table, m_tableSize * sizeof(void *));
        mockFunctions();
    }

    return m_table != nullptr;
}

void *getSdlProc(SdlApiIndex index)
{
    if (m_table && index < m_tableSize)
        return m_table[index];

    return nullptr;
}

void *setSdlProc(SdlApiIndex index, void *hook)
{
    if (m_table && index < m_tableSize) {
        auto prevValue = m_table[index];
        m_table[index] = hook;
        return prevValue;
    }

    return nullptr;
}

void restoreSdlProc(SdlApiIndex index)
{
    if (m_originalTable && index < m_tableSize)
        m_table[index] = m_originalTable[index];
}

void restoreOriginalSdlFunctionTable()
{
    memcpy(m_table, m_originalTable.get(), m_tableSize * sizeof(void *));
}

static int getEventFromQueue(SDL_Event *event)
{
    if (!m_eventQueue.empty()) {
        *event = m_eventQueue.front();
        m_eventQueue.pop_front();

        if (event->type == SDL_MOUSEBUTTONUP)
            m_mouseButtonFlags = 0;

        return 1;
    }

    return 0;
}

static Uint32 getMouseState(int *x, int *y)
{
    if (x)
        *x = m_mouseX;
    if (y)
        *y = m_mouseY;

    return m_mouseButtonFlags;
}

static void pumpEvents()
{
    if (m_mouseButtonFlags) {
        queueSdlMouseButtonEvent(true);
        m_mouseButtonFlags = 0;
    }
}

static int getNumJoypads()
{
    return 0;
}

static Uint8 getJoystickButton(SDL_Joystick *, int)
{
    return 0;
}

static Uint8 *getKeyboardState(int *numKeys)
{
    if (numKeys)
        *numKeys = std::size(m_scancodes);

    return reinterpret_cast<Uint8 *>(&m_scancodes);
}

void takeOverInput()
{
    m_table[SDL_PollEventIndex] = getEventFromQueue;
    m_table[SDL_WaitEventIndex] = getEventFromQueue;
    m_table[SDL_GetMouseStateIndex] = getMouseState;
    m_table[SDL_PeepEventsIndex] = pumpEvents;
    m_table[SDL_PumpEventsIndex] = pumpEvents;
    m_table[SDL_NumJoysticksIndex] = getNumJoypads;
    m_table[SDL_GetKeyboardStateIndex] = getKeyboardState;
    m_table[SDL_JoystickGetButtonIndex] = getJoystickButton;
}

void queueSdlEvent(const SDL_Event& event)
{
    m_eventQueue.push_back(event);
}

void queueSdlMouseWheelEvent(int direction)
{
    SDL_Event event;

    event.wheel.x = 0;
    event.wheel.y = direction;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    event.wheel.type = SDL_MOUSEWHEEL;

    queueSdlEvent(event);
}

void queueSdlMouseMotionEvent(int x, int y)
{
    SDL_Event event;

    event.motion.x = x;
    event.motion.y = y;
    event.motion.xrel = x - m_mouseX;
    event.motion.yrel = y - m_mouseX;
    event.motion.state = m_mouseButtonFlags;
    event.motion.type = SDL_MOUSEMOTION;

    queueSdlEvent(event);
}

void queueSdlMouseButtonEvent(bool mouseUp /* = false */, int button /* = 1 */)
{
    SDL_Event event;

    event.button.type = mouseUp ? SDL_MOUSEBUTTONUP : SDL_MOUSEBUTTONDOWN;
    event.button.button = button;
    event.button.state = SDL_PRESSED;
    event.button.clicks = 1;
    event.button.x = m_mouseX;
    event.button.y = m_mouseY;

    queueSdlEvent(event);
}

static void queueSdlKeyEvent(SDL_Scancode keyCode, bool keyDown)
{
    SDL_Event event {};
    event.type = keyDown ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.state = keyDown ? SDL_PRESSED : SDL_RELEASED;
    event.key.keysym.scancode = keyCode;
    queueSdlEvent(event);
}

void queueSdlKeyDown(SDL_Scancode keyCode)
{
    queueSdlKeyEvent(keyCode, true);
}

void queueSdlKeyUp(SDL_Scancode keyCode)
{
    queueSdlKeyEvent(keyCode, false);
}

void setSdlMouseState(int x, int y, bool leftClick /* = false */, bool rightClick /* = false */)
{
    if (x != m_mouseX || y != m_mouseY)
        queueSdlMouseMotionEvent(x, y);

    m_mouseX = x;
    m_mouseY = y;

    auto mouseButtonFlags = 0;
    if (leftClick)
        mouseButtonFlags |= SDL_BUTTON(SDL_BUTTON_LEFT);
    if (rightClick)
        mouseButtonFlags |= SDL_BUTTON(SDL_BUTTON_LEFT);

    if (mouseButtonFlags && !m_mouseButtonFlags)
        queueSdlMouseButtonEvent();
    else if (!mouseButtonFlags && m_mouseButtonFlags)
        queueSdlMouseButtonEvent(true);

    m_mouseButtonFlags = mouseButtonFlags;
}

void resetSdlInput()
{
    m_eventQueue.clear();
    m_mouseButtonFlags = 0;
    m_mouseX = 0;
    m_mouseY = 0;
}

void killSdlDelay()
{
    m_table[SDL_DelayIndex] = SDL_Delay_NOP;
}

static int fakeGetNumDisplayModes(int)
{
    auto numModes = m_displayModes.size();
    return numModes ? numModes : -1;
}

static int fakeGetDisplayMode(int, int modeIndex, SDL_DisplayMode *mode)
{
    if (mode && modeIndex < static_cast<int>(m_displayModes.size())) {
        *mode = m_displayModes[modeIndex];
        return 0;
    }

    return -1;
}

void setFakeDisplayModes(const std::vector<SDL_DisplayMode>& displayModes)
{
    if (m_table) {
        m_displayModes = displayModes;
        m_table[SDL_GetNumDisplayModesIndex] = fakeGetNumDisplayModes;
        m_table[SDL_GetDisplayModeIndex] = fakeGetDisplayMode;
    }
}

void restoreRealDisplayModes()
{
    if (m_table) {
        m_table[SDL_GetNumDisplayModesIndex] = m_originalTable[SDL_GetNumDisplayModesIndex];
        m_table[SDL_GetDisplayModeIndex] = m_originalTable[SDL_GetDisplayModeIndex];
    }
}

static Uint32 getFakeTicks()
{
    if (m_timeFrozen) {
        return 0;
    } else {
        m_lastGetTicks += m_getTicksDelta;
        return m_lastGetTicks;
    }
}

void setSetTicksDelta(int delta)
{
    m_getTicksDelta = delta;
    m_timeFrozen = false;

    if (m_table)
        m_table[SDL_GetTicksIndex] = getFakeTicks;
}

void freezeSdlTime()
{
    m_timeFrozen = true;
    killSdlDelay();

    if (m_table)
        m_table[SDL_GetTicksIndex] = getFakeTicks;
}
