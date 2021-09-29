#include "benchControls.h"
#include "bench.h"
#include "pitchConstants.h"
#include "gameControlEvents.h"
#include "gameControls.h"
#include "gameSprites.h"
#include "referee.h"
#include "comments.h"
#include "util.h"

constexpr int kEnterBenchDelay = 15;
constexpr int kPlayerGoingInDelay = 100;

constexpr int kNumTapsForBench = 3;
constexpr int kTapTimeoutTicks = 15;

constexpr int kLeavingSubsDelay = 55;
constexpr int kSubstituteFireTicks = 8;

constexpr int kMaxSubstitutes = 5;

struct TapCounterState {
    int tapCount;
    int tapTimeoutCounter;
    GameControlEvents previousDirection;
    bool blockWhileHoldingDirection;

    void reset() {
        tapCount = 0;
        tapTimeoutCounter = 0;
        previousDirection = kNoGameEvents;
        blockWhileHoldingDirection = false;
    }
    bool anyDirectionPressedLastFrame() const {
        return (previousDirection & kGameEventMovementMask) != kNoGameEvents;
    }
    bool holdingSameDirectionAsLastFrame(GameControlEvents controls) const {
        return previousDirection == (controls & kGameEventMovementMask);
    }
};

static TapCounterState m_pl1TapState, m_pl2TapState;
static GameControlEvents m_controls;

TeamGeneralInfo *m_team;
TeamGame *m_teamData;

static BenchState m_state;
static int m_goToBenchTimer;

static bool m_bench1Called;
static bool m_bench2Called;

static bool m_blockDirections;
static int m_fireTimer;
static bool m_blockFire;

static bool m_trainingTopTeam;
static bool m_teamsSwapped;

static int m_arrowPlayerIndex;
static int m_selectedMenuPlayerIndex;   // in menu
static int m_playerToEnterGameIndex;

static int m_playerToBeSubstitutedPos;
static int m_playerToBeSubstitutedOrd;

static std::array<std::array<byte, kNumPlayersInTeam>, 2> m_shirtNumberTable;

static int m_selectedFormationEntry;

static void handleMenuControls();
static void initBenchVars();
static void handleBenchArrowSelection();
static void selectPlayerToSubstituteMenuHandler();
static void handleFormationMenuControls();
static void markPlayersMenuHandler();
static void updateSelectedMenuPlayer();
static void showFormationMenu();
static void firePressedInSubsMenu();
static void selectOrSwapPlayers();
static void updatePlayerToBeSubstitutedPosition();
static void initiateSubstitution();
static void substitutePlayer();
static void changeTactics(int newTactics);
static void leaveBench();
static void leaveBenchFromMenu();
static bool benchBlocked();
static bool benchUnavailable();
static void updateBenchTeam();
static bool updateNonBenchControls();
static void updateBenchControls();
static bool bumpGoToBenchTimer();
static bool filterControls();
static bool benchInvoked();
static void findInitialPlayerToBeSubstituted();
static void markPlayer();
static void swapPlayerShirtNumbers(int ord1, int ord2);
static void increasePlayerIndex();
static void decreasePlayerIndex(bool allowBenchSwitch);
static void increasePlayerToSubstitute();
static void decreasePlayerToSubstitute();
static bool isPlayerOkToSelect();
static bool isPlayerOkToSubstitute();
static void trainingSwapBenchTeams();

void initBenchControls()
{
    m_teamsSwapped = false;
    m_trainingTopTeam = false;

    m_pl1TapState.reset();
    m_pl2TapState.reset();

    m_goToBenchTimer = 0;

    m_blockDirections = false;
    m_blockFire = false;
    m_fireTimer = 0;

    m_controls = kNoGameEvents;

    m_state = BenchState::kInitial;

    m_arrowPlayerIndex = 0;
    m_playerToEnterGameIndex = 0;

    for (int i = 0; i < kNumPlayersInTeam; i++) {
        m_shirtNumberTable[0][i] = i;
        m_shirtNumberTable[1][i] = i;
    }
}

// Returns true if bench needs to be invoked, but false if it's already showing.
bool benchCheckControls()
{
    if (inBench()) {
        updateBenchControls();

        if (!bumpGoToBenchTimer()) {
            if (newPlayerAboutToGoIn())
                substitutePlayer();
            else if (!filterControls())
                handleMenuControls();
        }
    } else {
        setBenchOff();

        if (!benchBlocked() && !benchUnavailable()) {
            updateBenchTeam();

            auto benchCalled = m_bench1Called || m_bench2Called;
            if (benchCalled || updateNonBenchControls() && benchInvoked()) {
                initBenchVars();
                return true;
            }
        }
    }

    return false;
}

BenchState getBenchState()
{
    return m_state;
}

bool trainingTopTeam()
{
    return m_trainingTopTeam;
}

void setTrainingTopTeam(bool value)
{
    m_trainingTopTeam = value;
}

void requestBench1()
{
    m_bench1Called = true;
}

void requestBench2()
{
    m_bench2Called = true;
}

int getBenchPlayerIndex()
{
    return m_arrowPlayerIndex;
}

int getBenchPlayerShirtNumber(bool topTeam, int index)
{
    assert(index >= 0 && index < sizeof(m_shirtNumberTable[0]));
    return m_shirtNumberTable[topTeam][index];
}

int getBenchMenuSelectedPlayer()
{
    return m_selectedMenuPlayerIndex;
}

int getSelectedFormationEntry()
{
    return m_selectedFormationEntry;
}

bool inBenchOrGoingTo()
{
    // without m_goToBenchTimer, cutout for replays is too sharp
    return !m_goToBenchTimer && swos.g_inSubstitutesMenu;
}

bool goingToBenchDelay()
{
    return m_goToBenchTimer != 0;
}

int playerToEnterGameIndex()
{
    return m_playerToEnterGameIndex;
}

int playerToBeSubstitutedIndex()
{
    return m_playerToBeSubstitutedOrd;
}

int playerToBeSubstitutedPos()
{
    return m_playerToBeSubstitutedPos;
}

bool substituteInProgress()
{
    return swos.g_substituteInProgress != 0;
}

bool newPlayerAboutToGoIn()
{
    return swos.g_substituteInProgress < 0;
}

void setSubstituteInProgress()
{
    swos.g_substituteInProgress = 1;
}

const TeamGeneralInfo *getBenchTeam()
{
    return m_team;
}

const TeamGame *getBenchTeamData()
{
    return m_teamData;
}

static void handleMenuControls()
{
    if (substituteInProgress())
        return;

    switch (m_state) {
    case BenchState::kInitial:
        handleBenchArrowSelection();
        break;
    case BenchState::kAboutToSubstitute:
        selectPlayerToSubstituteMenuHandler();
        break;
    case BenchState::kFormationMenu:
        handleFormationMenuControls();
        break;
    case BenchState::kMarkingPlayers:
        markPlayersMenuHandler();
        break;
    }
}

static void initBenchVars()
{
    m_bench1Called = false;
    m_bench2Called = false;

    m_teamData = m_team->teamNumber == 2 ? swos.bottomTeamPtr : swos.topTeamPtr;

    m_playerToBeSubstitutedPos = -1;
    m_arrowPlayerIndex = 0;

    m_state = BenchState::kInitial;
    m_goToBenchTimer = kEnterBenchDelay;

    m_blockFire = true;
    m_blockDirections = true;
}

static void handleBenchArrowSelection()
{
    auto benchRecalled = m_team == &swos.topTeamData ? m_bench1Called : m_bench2Called;

    if (benchRecalled || (m_controls & (kGameEventLeft | kGameEventRight))) {
        leaveBench();
    } else if (m_controls & (kGameEventUp | kGameEventDown)) {
        bool increaseIndex = true;
        bool allowIfKeeperHolds = false;

        if (m_controls & kGameEventUp) {
            increaseIndex = false;
            if (swos.g_trainingGame) {
                if (m_trainingTopTeam)
                    increaseIndex = true;
                else
                    allowIfKeeperHolds = true;
            }
        } else if (m_controls & kGameEventDown) {
            if (swos.g_trainingGame) {
                if (m_trainingTopTeam) {
                    increaseIndex = false;
                    allowIfKeeperHolds = true;
                }
            }
        }

        if (!allowIfKeeperHolds && swos.gameState == GameState::kKeeperHoldsTheBall)
            return;

        m_blockDirections = true;

        increaseIndex ? increasePlayerIndex() : decreasePlayerIndex(allowIfKeeperHolds);
    } else if (m_controls & kGameEventKick) {
        firePressedInSubsMenu();
    }
}

static void selectPlayerToSubstituteMenuHandler()
{
    if (m_controls & kGameEventKick)
        initiateSubstitution();
    else
        updateSelectedMenuPlayer();
}

static void handleFormationMenuMovement()
{
    if (m_controls & (kGameEventLeft | kGameEventRight)) {
        leaveBenchFromMenu();
    } else if (m_controls & kGameEventUp) {
        m_blockDirections = true;
        if (m_selectedFormationEntry > 0)
            m_selectedFormationEntry--;
    } else if (m_controls & kGameEventDown) {
        m_blockDirections = true;
        if (m_selectedFormationEntry < 0)
            m_selectedFormationEntry = 0;
        else if (m_selectedFormationEntry < kNumFormationEntries - 1)
            m_selectedFormationEntry++;
    }
}

static void handleFormationMenuControls()
{
    if (m_controls & kGameEventKick) {
        assert(m_selectedFormationEntry >= 0 && m_selectedFormationEntry < kNumFormationEntries);
        changeTactics(m_selectedFormationEntry);
    } else {
        handleFormationMenuMovement();
    }
}

static void markPlayersMenuHandler()
{
    const PlayerGame *player{};

    if (m_playerToBeSubstitutedPos >= 0) {
        assert(m_playerToBeSubstitutedPos <= 10);
        player = &m_teamData->players[m_playerToBeSubstitutedPos];
    }

    if (!player || player->cards >= 2 || m_fireTimer < kSubstituteFireTicks) {
        if (m_controls & kGameEventKick) {
            if (m_playerToBeSubstitutedOrd < 0)
                showFormationMenu();
            else if (m_playerToBeSubstitutedOrd > 0)
                selectOrSwapPlayers();
        } else {
            updateSelectedMenuPlayer();
        }
    } else {
        markPlayer();
    }
}

static void updateSelectedMenuPlayer()
{
    if (m_controls & (kGameEventLeft | kGameEventRight))
        leaveBenchFromMenu();
    else if (m_state == BenchState::kMarkingPlayers || m_playerToEnterGameIndex != 11 || swos.gameMaxSubstitutes <= 5) {
        if (m_controls & kGameEventUp) {
            m_blockDirections = true;
            if (m_state == BenchState::kMarkingPlayers && (m_playerToBeSubstitutedOrd < 0 || m_playerToBeSubstitutedOrd == 1)) {
                // skip goalkeeper when going up, and jump to formation entry
                m_playerToBeSubstitutedOrd = -1;
                m_playerToBeSubstitutedPos = -1;
            } else if (m_playerToBeSubstitutedOrd) {
                decreasePlayerToSubstitute();
            }
        } else if (m_controls & kGameEventDown) {
            m_blockDirections = true;
            if (m_state == BenchState::kMarkingPlayers && m_playerToBeSubstitutedOrd < 0) {
                // jump from formation entry to first player (skip goalkeeper)
                m_playerToBeSubstitutedOrd = 1;
                updatePlayerToBeSubstitutedPosition();
            } else if (m_playerToBeSubstitutedOrd != 10) {
                increasePlayerToSubstitute();
            }
        }
    }
}

static void showFormationMenu()
{
    m_state = BenchState::kFormationMenu;
    m_blockFire = true;
    m_selectedFormationEntry = m_team->tactics;
}

static void firePressedInSubsMenu()
{
    if (!m_arrowPlayerIndex) {
        m_state = BenchState::kMarkingPlayers;
        m_blockFire = true;
        m_playerToBeSubstitutedOrd = -1;
        m_playerToBeSubstitutedPos = -1;
        m_selectedMenuPlayerIndex = -1;
    } else {
        const auto& player = m_teamData->players[m_arrowPlayerIndex + 10];
        if (player.canBeSubstituted()) {
            m_state = BenchState::kAboutToSubstitute;
            m_blockFire = true;
            m_playerToEnterGameIndex = m_arrowPlayerIndex + 10;
            if (swos.gameMaxSubstitutes <= kMaxSubstitutes || m_playerToEnterGameIndex != 11)
                findInitialPlayerToBeSubstituted();
            else
                m_playerToBeSubstitutedPos = 0;
        }
    }
}

static void maintainMarkedPlayer()
{
    auto& markedPlayer = m_teamData->markedPlayer;

    if (markedPlayer == m_playerToBeSubstitutedPos)
        markedPlayer = -1;
    if (markedPlayer == m_playerToEnterGameIndex) {
        markedPlayer = -1;
        if (m_playerToBeSubstitutedPos)
            markedPlayer = m_playerToBeSubstitutedPos;
    }
}

static void swapMarkedPlayer()
{
    auto& markedPlayer = m_teamData->markedPlayer;
    if (markedPlayer == m_playerToBeSubstitutedPos)
        markedPlayer = m_selectedMenuPlayerIndex;
    else if (markedPlayer == m_selectedMenuPlayerIndex)
        markedPlayer = m_playerToBeSubstitutedPos;
}

static void selectOrSwapPlayers()
{
    if (m_selectedMenuPlayerIndex < 0) {
        if (m_fireTimer == 1)
            m_selectedMenuPlayerIndex = m_playerToBeSubstitutedPos;
    } else {
        if (m_selectedMenuPlayerIndex == m_playerToBeSubstitutedPos) {
            if (m_fireTimer == 1)
                m_selectedMenuPlayerIndex = -1;
        } else {
            assert(m_playerToBeSubstitutedPos >= 0 && m_playerToBeSubstitutedPos < 11);
            assert(m_selectedMenuPlayerIndex >= 0 && m_selectedMenuPlayerIndex < 11);

            swapMarkedPlayer();
            swapPlayerShirtNumbers(m_playerToBeSubstitutedPos, m_selectedMenuPlayerIndex);
            std::swap(m_teamData->players[m_playerToBeSubstitutedPos], m_teamData->players[m_selectedMenuPlayerIndex]);
            std::swap(m_team->players[m_playerToBeSubstitutedPos], m_team->players[m_selectedMenuPlayerIndex]);
            std::swap(m_team->players[m_playerToBeSubstitutedPos]->playerOrdinal,
                m_team->players[m_selectedMenuPlayerIndex]->playerOrdinal);

            initializePlayerSpriteFrameIndices();
            ApplyTeamTactics();

            m_selectedMenuPlayerIndex = -1;

            leaveBenchFromMenu();
        }
    }
}

static void updatePlayerToBeSubstitutedPosition()
{
    m_playerToBeSubstitutedPos = getBenchPlayerPosition(m_playerToBeSubstitutedOrd);
}

static void initiateSubstitution()
{
    constexpr int kSubstitutedPlayerX = 39;
    constexpr int kSubstitutedPlayerY = kPitchCenterY;

    assert(m_playerToBeSubstitutedPos >= 0 && m_playerToBeSubstitutedPos <= 10);

    setSubstituteInProgress();
    auto& numSubs = m_team->teamNumber == 1 ? swos.team1NumSubs : swos.team2NumSubs;
    numSubs++;
    m_blockFire = true;
    m_state = BenchState::kInitial;

    // must set this sprite so the game waits for him to arrive at the destination before continuing
    swos.substitutedPlSprite = m_team->players[m_playerToBeSubstitutedPos];
    swos.teamThatSubstitutes = m_team;

    swos.substitutedPlSprite->cards = 0;
    swos.substitutedPlDestX = kSubstitutedPlayerX;
    swos.substitutedPlDestY = kSubstitutedPlayerY;
    swos.plSubstitutedX = kSubstitutedPlayerX;
    swos.plSubstitutedY = kSubstitutedPlayerY;
}

// Old player has left the field, new one is about to go it. Does the actual swap of the player data.
static void substitutePlayer()
{
    swos.substitutedPlSprite->injuryLevel = 0;
    swos.substitutedPlSprite->sentAway = 0;

    maintainMarkedPlayer();
    swapPlayerShirtNumbers(m_playerToEnterGameIndex, m_playerToBeSubstitutedPos);

    m_teamData->players[m_playerToBeSubstitutedPos].position = PlayerPosition::kSubstituted;
    std::swap(m_teamData->players[m_playerToEnterGameIndex], m_teamData->players[m_playerToBeSubstitutedPos]);

    initializePlayerSpriteFrameIndices();
    ApplyTeamTactics();

    swos.g_waitForPlayerToGoInTimer = kPlayerGoingInDelay;
    m_blockFire = true;

    enqueueSubstituteSample();
    leaveBench();
}

static void changeTactics(int newTactics)
{
    assert(static_cast<unsigned>(newTactics) < std::size(swos.g_tacticsTable));

    auto& tactics = m_team->teamNumber == 1 ? swos.pl1Tactics : swos.pl2Tactics;
    tactics = newTactics;
    m_team->tactics = newTactics;

    A4 = m_teamData;
    ApplyTeamTactics();

    enqueueTacticsChangedSample();
    leaveBenchFromMenu();
}

static void leaveBench()
{
    setBenchOff();
    setCameraLeavingBench();
    swos.g_cameraLeavingSubsTimer = kLeavingSubsDelay;

    m_bench1Called = false;
    m_bench2Called = false;

    m_teamsSwapped = false;
    m_state = BenchState::kInitial;
    m_playerToBeSubstitutedPos = -1;

    m_pl1TapState.reset();
    m_pl2TapState.reset();
}

static void leaveBenchFromMenu()
{
    m_state = BenchState::kInitial;
    m_blockDirections = true;
    m_blockFire = true;
}

static bool benchBlocked()
{
    if (swos.g_waitForPlayerToGoInTimer) {
        swos.g_waitForPlayerToGoInTimer--;
        return true;
    } else if (swos.g_cameraLeavingSubsTimer) {
        swos.g_cameraLeavingSubsTimer--;
        return true;
    } else {
        return swos.g_substituteInProgress || refereeActive() || swos.statsTimer;
    }
}

static bool benchUnavailable()
{
    if (swos.gameStatePl == GameState::kInProgress || cardHandingInProgress() || swos.playingPenalties ||
        swos.gameState >= GameState::kStartingGame && swos.gameState <= GameState::kGameEnded) {
        m_pl1TapState.reset();
        m_pl2TapState.reset();
        m_bench1Called = false;
        m_bench2Called = false;
        return true;
    }

    return false;
}

static void updateBenchTeam()
{
    static int s_alternateTeamsTimer;

    if (m_bench1Called)
        m_team = &swos.topTeamData;
    else if (m_bench2Called)
        m_team = &swos.bottomTeamData;
    else
        m_team = ++s_alternateTeamsTimer & 1 ? &swos.topTeamData : &swos.bottomTeamData;

    m_teamData = m_team == &swos.topTeamData ? swos.topTeamPtr : swos.bottomTeamPtr;
}

static bool updateNonBenchControls()
{
    if (m_team->playerNumber) {
        m_controls = directionToEvents(m_team->direction);
        if (m_team->firePressed)
            m_controls |= kGameEventKick;
        if (m_team->secondaryFire)
            m_controls |= kGameEventBench;
    } else {
        switch (m_team->plCoachNum) {
        case 1:
            m_controls = getPlayerEvents(kPlayer1);
            break;
        case 2:
            m_controls = getPlayerEvents(kPlayer2);
            break;
        default:
            assert(false);
        case 0:
            return false;
        }
    }

    return true;
}

static void updateBenchControls()
{
    assert(inBench() && m_team);

    auto team = m_teamsSwapped ? m_team->opponentsTeam.asPtr() : m_team;
    m_controls = team->playerNumber == 1 ? getPlayerEvents(kPlayer1) : getPlayerEvents(kPlayer2);
}

static bool bumpGoToBenchTimer()
{
    if (m_goToBenchTimer > 0)
        m_goToBenchTimer--;

    return m_goToBenchTimer > 0;
}

// Returns true if the controls were filtered (blocked).
static bool filterControls()
{
    constexpr int kMovementDelay = 4;

    static GameControlEvents s_lastDirection;
    static int s_movementDelayTimer;

    auto currentDirection = m_controls & kGameEventMovementMask;
    bool sameDirectionHeld = currentDirection && m_blockDirections && s_lastDirection == currentDirection;

    // this part keeps the menu navigation at a reasonable speed; if not present up/down would fly super fast
    if (sameDirectionHeld &&
        (getBenchState() == BenchState::kInitial ||
        !(s_lastDirection & (kGameEventUp | kGameEventDown)) ||
        ++s_movementDelayTimer != kMovementDelay))
        return true;

    m_blockDirections = false;

    s_movementDelayTimer = 0;
    s_lastDirection = currentDirection;

    bool firing = (m_controls & kGameEventKick) != 0;

    if (firing)
        m_fireTimer++;
    else
        m_fireTimer = 0;

    if (m_blockFire && firing)
        return true;
    else
        m_blockFire = false;

    return false;
}

// Game is stopped and bench call is possible, check if it's actually invoked.
// Secondary fire needs to be pressed, or any one direction tapped three times quickly.
static bool benchInvoked()
{
    assert(m_team);

    if (m_controls & kGameEventBench)
        return true;

    auto& state = m_team == &swos.topTeamData ? m_pl1TapState : m_pl2TapState;

    if ((m_controls & kGameEventMovementMask) == kNoGameEvents) {
        state.blockWhileHoldingDirection = false;
        if (++state.tapTimeoutCounter >= kTapTimeoutTicks)
            state.reset();
    } else if (!state.blockWhileHoldingDirection) {
        state.tapTimeoutCounter = 0;
        if (state.anyDirectionPressedLastFrame()) {
            if (state.holdingSameDirectionAsLastFrame(m_controls)) {
                if (++state.tapCount >= kNumTapsForBench - 1)
                    return true;
                else
                    state.blockWhileHoldingDirection = true;
            } else {
                state.previousDirection = kNoGameEvents;
                state.tapCount = 0;
            }
        } else {
            state.previousDirection = m_controls & kGameEventMovementMask;
            state.blockWhileHoldingDirection = true;
        }
    }

    return false;
}

static void findInitialPlayerToBeSubstituted()
{
    assert(m_playerToEnterGameIndex >= 0);

    auto position = m_teamData->players[m_playerToEnterGameIndex].position;

    int exactMatch = -1;
    int approximateMatch = -1;
    int firstAvailablePlayer = -1;

    for (int i = 0; i < 11; i++) {
        const auto& player = getBenchPlayer(i);
        if (player.canBeSubstituted()) {
            if (firstAvailablePlayer < 0)
                firstAvailablePlayer = i;
            if (approximateMatch < 0) {
                static const auto kDefencePositions = {
                    PlayerPosition::kDefender, PlayerPosition::kRightBack, PlayerPosition::kLeftBack
                };
                static const auto kMidfieldPositions = {
                    PlayerPosition::kMidfielder, PlayerPosition::kRightWing, PlayerPosition::kLeftWing
                };
                auto isPositionIn = [](PlayerPosition position, const auto& positions) {
                    return std::find(positions.begin(), positions.end(), position);
                };
                if (isPositionIn(position, kDefencePositions) && isPositionIn(player.position, kDefencePositions) ||
                    isPositionIn(position, kMidfieldPositions) && isPositionIn(player.position, kMidfieldPositions))
                    approximateMatch = i;
            }
            if (exactMatch < 0 && position == player.position)
                exactMatch = i;
        }
    }

    if (exactMatch >= 0)
        m_playerToBeSubstitutedOrd = exactMatch;
    else if (approximateMatch >= 0)
        m_playerToBeSubstitutedOrd = approximateMatch;
    else
        m_playerToBeSubstitutedOrd = firstAvailablePlayer;

    m_playerToBeSubstitutedPos = getBenchPlayerPosition(m_playerToBeSubstitutedOrd);
}

static void markPlayer()
{
    bool playerAlreadyMarked = m_teamData->markedPlayer == m_playerToBeSubstitutedPos;
    m_teamData->markedPlayer = playerAlreadyMarked ? -1 : m_playerToBeSubstitutedPos;
    m_selectedMenuPlayerIndex = -1;
}

static void swapPlayerShirtNumbers(int ord1, int ord2)
{
    bool topTeam = getBenchTeamData() == &swos.topTeamIngame;
    std::swap(m_shirtNumberTable[topTeam][ord1], m_shirtNumberTable[topTeam][ord2]);
}

static void increasePlayerIndex()
{
    while (++m_arrowPlayerIndex <= kMaxSubstitutes && !isPlayerOkToSelect())
        ;

    if (m_arrowPlayerIndex > kMaxSubstitutes)
        decreasePlayerIndex(false);
}

static void decreasePlayerIndex(bool allowBenchSwitch)
{
    if (m_arrowPlayerIndex) {
        while (--m_arrowPlayerIndex > 0 && !isPlayerOkToSelect())
            ;
    } else if (allowBenchSwitch) {
        trainingSwapBenchTeams();
    }
}

static void increasePlayerToSubstitute()
{
    do {
        if (++m_playerToBeSubstitutedOrd >= 11)
            decreasePlayerToSubstitute();
        updatePlayerToBeSubstitutedPosition();
    } while (!isPlayerOkToSubstitute());
}

static void decreasePlayerToSubstitute()
{
    do {
        if (--m_playerToBeSubstitutedOrd < 0)
            increasePlayerToSubstitute();
        updatePlayerToBeSubstitutedPosition();
    } while (!isPlayerOkToSubstitute());
}

static bool isPlayerOkToSelect()
{
    assert(m_team);

    auto numSubs = m_team->teamNumber == 1 ? swos.team1NumSubs : swos.team2NumSubs;

    if (swos.gameMinSubstitutes == numSubs) {
        return false;
    } else {
        const auto& player = m_teamData->players[m_arrowPlayerIndex + 10];
        if (!player.canBeSubstituted())
            return false;
    }

    return true;
}

static bool isPlayerOkToSubstitute()
{
    if (m_state == BenchState::kMarkingPlayers)
        return m_playerToBeSubstitutedOrd != 0;
    else
        return m_teamData->players[m_playerToBeSubstitutedPos].cards < 2;
}

static void trainingSwapBenchTeams()
{
    m_teamsSwapped = !m_teamsSwapped;
    m_trainingTopTeam = !m_trainingTopTeam;
    swapBenchWithOpponent();
    m_team = m_team->opponentsTeam;
    m_teamData = m_team->teamNumber == 2 ? swos.bottomTeamPtr : swos.topTeamPtr;
}
