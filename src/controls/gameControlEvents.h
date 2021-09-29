#pragma once

#include "flags.h"

enum GameControlEvents : int32_t
{
    kNoGameEvents = 0,
    kGameEventUp = 1,
    kGameEventDown = 2,
    kGameEventLeft = 4,
    kGameEventRight = 8,
    kGameEventKick = 16,
    kGameEventBench = 32,
    kGameEventPause = 64,
    kGameEventReplay = 128,
    kGameEventSaveHighlight = 256,
    kGameEventZoomIn = 512,
    kGameEventZoomOut = 1'024,
    kMaxGameEvent = kGameEventSaveHighlight,
};

ENABLE_FLAGS(GameControlEvents)

constexpr auto kGameEventMovementMask = kGameEventUp | kGameEventDown | kGameEventLeft | kGameEventRight |
    kGameEventZoomIn | kGameEventZoomOut;
constexpr auto kGameEventNonMovementMask = ~kGameEventMovementMask;
constexpr auto kMinimumGameEventsMask = kGameEventUp | kGameEventDown | kGameEventLeft | kGameEventRight | kGameEventKick;
constexpr auto kGameEventAll = static_cast<GameControlEvents>(2 * kMaxGameEvent - 1);

constexpr int kNumDefaultGameControlEvents = 6;
using DefaultEventsPack = std::array<GameControlEvents, kNumDefaultGameControlEvents>;
using DefaultScancodesPack = std::array<SDL_Scancode, kNumDefaultGameControlEvents>;

constexpr DefaultEventsPack kDefaultGameControlEvents = {
    kGameEventUp, kGameEventDown, kGameEventLeft, kGameEventRight, kGameEventKick, kGameEventBench,
};
constexpr int kDefaultControlsKickIndex = 4;
constexpr int kDefaultControlsBenchIndex = 5;

std::pair<const char *, size_t> gameControlEventToString(GameControlEvents event);
int gameControlEventToString(GameControlEvents events, char *buffer, size_t bufferSize, const char *noneString = "N/A");
bool convertEvents(GameControlEvents& events, int intEvents);
void traverseEvents(GameControlEvents events, std::function<void(GameControlEvents)> f);
