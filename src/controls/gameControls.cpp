#include "gameControls.h"
#include "keyboard.h"
#include "mouse.h"
#include "joypads.h"
#include "bench.h"
#include "pitch.h"
#include "game.h"
#include "gameLoop.h"
#include "replays.h"

// fire counter starts from -1 and counts downwards while the fire button is pressed
// if 0 don't touch it
// quick fire is activated after 1 and normal fire after 4 frames
// when the fire button is released, the counter is negated (becoming positive)
static int m_pl1FireCounter;
static int m_pl2FireCounter;

static GameControlEvents filterOverlappedEvents(PlayerNumber player, GameControlEvents events);
static void updateGameControls(PlayerNumber player, GameControlEvents events);
static bool handleStatsFireExit();
static void updateTeamControls(TeamGeneralInfo *team, PlayerNumber player, GameControlEvents events);
static void updatePlayerFire(PlayerNumber player, GameControlEvents events);

// Sets control related fields in team structure. Called once per frame.
// Handles one team per frame (next team in next frame).
//
void updateTeamControls()
{
    if (handleStatsFireExit())
        return;

    static int s_teamSwitchCounter;
    auto team = ++s_teamSwitchCounter & 1 ? &swos.topTeamData : &swos.bottomTeamData;

    A6 = team;
    UpdateControlledPlayer();
    UpdatePlayerBeingPassedTo();

    if (team->playerNumber) {
        auto player = team->playerNumber == 2 ? kPlayer2 : kPlayer1;
        auto events = getPlayerEvents(player);
        updateGameControls(player, events);
        updateTeamControls(team, player, events);
    }

    if (!team->resetControls) {
        if (inBench()) {
            team->currentAllowedDirection = kNoDirection;
            team->quickFire = 0;
            team->normalFire = 0;
            team->firePressed = 0;
            team->fireThisFrame = 0;
            team->fireCounter = 0;
        }

        UpdatePlayersAndBall();

        if (team->headerOrTackle) {
            team->headerOrTackle = 0;
            auto& fireCounter = team->playerNumber == 2 ? m_pl2FireCounter : m_pl1FireCounter;
            fireCounter = 0;
        }
    }
}

GameControlEvents getPlayerEvents(PlayerNumber player)
{
    assert(player == kPlayer1 || player == kPlayer2);

    auto events = kNoGameEvents;

    auto controls = player == kPlayer1 ? getPl1Controls() : getPl2Controls();

    switch (controls) {
    case kKeyboard1:
        assert(player == kPlayer1);
        events = keyboard1Events();
        break;
    case kKeyboard2:
        assert(player == kPlayer2);
        events = keyboard2Events();
        break;
    case kMouse:
        events = mouseEvents();
        break;
    case kJoypad:
        events = player == kPlayer1 ? pl1JoypadEvents() : pl2JoypadEvents();
        break;
    default:
        return events;
    }

    return filterOverlappedEvents(player, events);
}

bool isPlayerFiring(PlayerNumber player)
{
    auto events = getPlayerEvents(player);
    return (events & kGameEventKick) != 0;
}

bool getShortFireAndBumpFireCounter(bool currentFire, PlayerNumber player /* = kPlayer1 */)
{
    static bool s_pl1LastFired;
    static bool s_pl2LastFired;

    auto& fireCounter = player == kPlayer1 ? m_pl1FireCounter : m_pl2FireCounter;
    auto& lastFired = player == kPlayer1 ? s_pl1LastFired : s_pl2LastFired;

    bool shortFire = false;

    if (lastFired) {
        if (currentFire) {
            if (fireCounter)
                fireCounter--;
        } else {
            lastFired = false;
            fireCounter = -fireCounter;
        }
    } else if (currentFire) {
        shortFire = true;
        lastFired = true;
        fireCounter = -1;
    }

    return shortFire;
}

int16_t eventsToDirection(GameControlEvents events)
{
    int16_t direction = kNoDirection;

    bool left = (events & kGameEventLeft) != 0;
    bool right = (events & kGameEventRight) != 0;
    bool up = (events & kGameEventUp) != 0;
    bool down = (events & kGameEventDown) != 0;

    if (up && right)
        direction = kFacingTopRight;
    else if (down && right)
        direction = kFacingBottomRight;
    else if (down && left)
        direction = kFacingBottomLeft;
    else if (up && left)
        direction = kFacingTopLeft;
    else if (up)
        direction = kFacingTop;
    else if (right)
        direction = kFacingRight;
    else if (down)
        direction = kFacingBottom;
    else if (left)
        direction = kFacingLeft;

    return direction;
}

GameControlEvents directionToEvents(int16_t direction)
{
    switch (direction) {
    case kFacingTop:
        return kGameEventUp;
    case kFacingTopRight:
        return kGameEventUp | kGameEventRight;
    case kFacingRight:
        return kGameEventRight;
    case kFacingBottomRight:
        return kGameEventDown | kGameEventRight;
    case kFacingBottom:
        return kGameEventDown;
    case kFacingBottomLeft:
        return kGameEventDown | kGameEventLeft;
    case kFacingLeft:
        return kGameEventLeft;
    case kFacingTopLeft:
        return kGameEventUp | kGameEventLeft;
    default:
        assert(false);
    case kNoDirection:
        return kNoGameEvents;
    }
}

bool isAnyPlayerFiring()
{
    return ((getPlayerEvents(kPlayer1) | getPlayerEvents(kPlayer2)) & kGameEventKick) != 0;
}

// We must do this since without it there are problems with doing long kicks (seems the game processes
// events in a way that up always trumps down if they're both active at the same time).
static GameControlEvents filterOverlappedEvents(PlayerNumber player, GameControlEvents events)
{
    static GameControlEvents s_oldPl1Events;
    static GameControlEvents s_oldPl2Events;
    static GameControlEvents s_pl1LastVertical;
    static GameControlEvents s_pl1LastHorizontal;
    static GameControlEvents s_pl2LastVertical;
    static GameControlEvents s_pl2LastHorizontal;

    auto& oldEvents = player == kPlayer1 ? s_oldPl1Events : s_oldPl2Events;
    auto& forceVertical = player == kPlayer1 ? s_pl1LastVertical : s_pl2LastVertical;
    auto& forceHorizontal = player == kPlayer1 ? s_pl1LastHorizontal : s_pl2LastHorizontal;

    if ((events & kGameEventUp) && (events & kGameEventDown)) {
        events &= ~(kGameEventUp | kGameEventDown);
        if (!forceVertical) {
            if (oldEvents & kGameEventUp)
                forceVertical = kGameEventDown;
            else
                forceVertical = kGameEventUp;
        }
        events |= forceVertical;
    } else {
        forceVertical = kNoGameEvents;
    }

    if ((events & kGameEventLeft) && (events & kGameEventRight)) {
        events &= ~(kGameEventLeft | kGameEventRight);
        if (!forceHorizontal) {
            if (oldEvents & kGameEventLeft)
                forceHorizontal = kGameEventRight;
            else
                forceHorizontal = kGameEventLeft;
        }
        events |= forceHorizontal;
    } else {
        forceHorizontal = kNoGameEvents;
    }

    return oldEvents = events;
}

static void updateGameControls(PlayerNumber player, GameControlEvents events)
{
    static_assert(kMaxGameEvent == 256, "You missed a spot!");

    updatePlayerFire(player, events);

    if (events & kGameEventReplay)
        requestFadeAndInstantReplay();

    if (events & kGameEventSaveHighlight)
        requestFadeAndSaveReplay();

    if (events & kGameEventZoomIn)
        zoomIn();

    if (events & kGameEventZoomOut)
        zoomOut();
}

void SWOS::Player1StatusProc()
{
    auto events = getPlayerEvents(kPlayer1);
    updatePlayerFire(kPlayer1, events);
}

void SWOS::Player2StatusProc()
{
    auto events = getPlayerEvents(kPlayer2);
    updatePlayerFire(kPlayer2, events);
}

static bool handleStatsFireExit()
{
    if (swos.fireBlocked) {
        if (!((getPlayerEvents(kPlayer1) | getPlayerEvents(kPlayer2)) & kGameEventKick))
            swos.fireBlocked = 0;

        return true;
    }

    return false;
}

void updateTeamControls(TeamGeneralInfo *team, PlayerNumber player, GameControlEvents events)
{
    assert(team->playerNumber);

    auto direction = eventsToDirection(events);
    bool fire = (events & kGameEventKick) != 0;

    team->currentAllowedDirection = direction;
    team->direction = direction;
    team->fireThisFrame = getShortFireAndBumpFireCounter(fire, player);
    team->secondaryFire = (events & kGameEventBench) != 0;

    if (team->firePressed = fire)
        team->fireCounter++;
    else
        team->fireCounter = 0;

    team->quickFire = 0;
    team->normalFire = 0;

    auto& fireCounter = player == kPlayer1 ? m_pl1FireCounter : m_pl2FireCounter;

    if (fireCounter < 0) {
        if (fireCounter < -4) {
            team->normalFire = -1;
            fireCounter = 0;
        }
    } else if (fireCounter > 0) {
        if (fireCounter > 4)
            team->normalFire = -1;
        else
            team->quickFire = -1;

        fireCounter = 0;
    }
}

static void updatePlayerFire(PlayerNumber player, GameControlEvents events)
{
    auto& plFire = player == kPlayer1 ? swos.pl1Fire : swos.pl2Fire;
    plFire = -((events & kGameEventKick) != 0);
}
