#include "timer.h"
#include "audio.h"
#include "music.h"
#include "options.h"
#include "replays.h"
#include "sprites.h"
#include "pitch.h"
#include "controls.h"
#include "menuControls.h"
#include "menuBackground.h"
#include "menuMouse.h"
#include "file.h"
#include "util.h"
#include "mainMenu.h"

#ifndef SWOS_TEST
static_assert(offsetof(SwosVM::SwosVariables, g_selectedTeams) -
    offsetof(SwosVM::SwosVariables, competitionFileBuffer) == 5'191, "Load competition buffer broken");
static_assert(offsetof(SwosVM::SwosVariables, g_selectedTeams) -
    offsetof(SwosVM::SwosVariables, careerFileBuffer) == 95'153 &&
    sizeof(swos.g_selectedTeams) == 68'400, "Load career buffer broken");
#endif

#ifdef NDEBUG
constexpr int kSentinelSize = 0;
#else
constexpr char kSentinelMagic[] = "nick.slaughter@key.mariah.com";
constexpr int kSentinelSize = sizeof(kSentinelMagic);
#endif

// sizes of all SWOS buffers in DOS and extended memory
constexpr int kDosMemBufferSize = 21'890 + kSentinelSize;
// allocate buffer to hold every SWOS sprite (but only structs, no data)
constexpr int kSpritesBuffer = kNumSprites * sizeof(SpriteGraphics) + kSentinelSize;

constexpr int kExtendedMemoryBufferSize = 393'216 + kSentinelSize;
SDL_UNUSED constexpr int kTotalExtraMemorySize = kDosMemBufferSize + kSpritesBuffer + kExtendedMemoryBufferSize;

static void init();
static void initRandomSeed();
static void patchPlayersLeavingPitchDestinations();

void startMainMenuLoop()
{
    init();

    resetControls();

    startMenuSong();
    initRandomSeed();

    D0 = 0;
    Randomize2();

    swos.screenWidth = kVgaWidth;

    // flush controls
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);

#ifndef NDEBUG
    // verify that the string offsets are OK
    assert(!memcmp(swos.aChairmanScenes + 0x4dc1, "YUGOSLAVIA", 11));
    assert(!memcmp(swos.aChairmanScenes + 0x2e5, "THE PRESIDENT", 14));
    assert(!memcmp(swos.aChairmanScenes + 0x2f, "RETURN TO GAME", 15));
    assert(!memcmp(swos.aChairmanScenes + 0x30c, "EXCELLENT JOB", 13));
    assert(!memcmp(swos.aChairmanScenes + 0x58c3, "QUEENSLAND", 11));
    assert(!memcmp(swos.aChairmanScenes + 0x331, "APPRECIATE IT", 14));
    assert(!memcmp(swos.aChairmanScenes + 0x3d1c, "DISK FULL", 9));
#endif

#ifndef SWOS_TEST
    logInfo("Going to main menu");
    initFrameTicks();
    showMainMenu();
#endif
}

#ifdef DEBUG
static void verifyBlock(const char *array, size_t size)
{
    assert(!memcmp(array + size - kSentinelSize, kSentinelMagic, kSentinelSize));
}

void checkMemory()
{
    auto extraMemStart = SwosVM::getExtraMemoryArea();

    assert(swos.dosMemOfs60c00h == extraMemStart);
    verifyBlock(extraMemStart, kDosMemBufferSize);

    assert(*swos.g_spriteGraphicsPtr == reinterpret_cast<SpriteGraphics *>(extraMemStart + kDosMemBufferSize));
    verifyBlock(extraMemStart + kDosMemBufferSize, kSpritesBuffer);

    assert(swos.linAdr384k == extraMemStart + kDosMemBufferSize + kSpritesBuffer);
    verifyBlock(extraMemStart + kDosMemBufferSize + kSpritesBuffer, kExtendedMemoryBufferSize);

#ifdef _MSCVER
    _ASSERTE(_CrtCheckMemory());
#endif
}
#endif

static void printIntroString()
{
    auto str = swos.aSensibleWorldOfSoccerV2_0;
    while (*str != '\r')
        std::cout << *str++;

    std::cout << "[Win32 Port]\n";
}

static void setupExtraMemory()
{
    // make sure we have enough but don't waste too much either ;)
    static_assert(SwosVM::kExtendedMemSize >= kTotalExtraMemorySize && kTotalExtraMemorySize + 10'000 > SwosVM::kExtendedMemSize,
        "Too much or too little extra memory given");

    auto extraMemStart = SwosVM::getExtraMemoryArea();
    assert(reinterpret_cast<uintptr_t>(extraMemStart) % sizeof(void *) == 0);

    swos.dosMemOfs60c00h = extraMemStart;   // names don't match offsets anymore, but are left for historical preservation ;)

    *swos.g_spriteGraphicsPtr = reinterpret_cast<SpriteGraphics *>(extraMemStart + kDosMemBufferSize);

    swos.linAdr384k = extraMemStart + kDosMemBufferSize + kSpritesBuffer;

#ifdef DEBUG
    SwosVM::initSafeMemoryAreas();
    memcpy(swos.dosMemOfs60c00h + kDosMemBufferSize - kSentinelSize, kSentinelMagic, kSentinelSize);
    memcpy((*swos.g_spriteGraphicsPtr).asCharPtr() + kSpritesBuffer - kSentinelSize, kSentinelMagic, kSentinelSize);
    memcpy(swos.linAdr384k + kExtendedMemoryBufferSize - kSentinelSize, kSentinelMagic, kSentinelSize);
#endif
}

static void init()
{
    logInfo("Initializing the game...");

    printIntroString();

    logInfo("Setting up base and extended memory pointers");
    setupExtraMemory();

    patchPlayersLeavingPitchDestinations();

    initTimer();
    initAudio();
    initReplays();
    initMenuBackground();
    initSprites();
    initPitches();
    initMenuMouse();
}

static void initRandomSeed()
{
    auto tm = getCurrentTime();
    auto hundredths = tm.msec / 10;

    auto seed = static_cast<int>(hundredths * hundredths);
    auto lo = (seed & 0xff) ^ tm.min;
    seed = lo | (seed & 0xff00);

    assert(seed <= 0xffff);
    swos.randomSeed = seed;
}

static void patchPlayersLeavingPitchDestinations()
{
    constexpr int kPlayersOffscreenOffsetX = 39;

    // shift player destinations when leaving the pitch more to the right, since it looks bad if unzoomed,
    // they're visible on screen, and all converging into one point on the right side of the pitch
    for (size_t i = 0; i < std::size(swos.playersLeavingPitchBottom); i += 2) {
        swos.playersLeavingPitchBottom[i] += kPlayersOffscreenOffsetX;
        swos.playersLeavingPitchTop[i] -= kPlayersOffscreenOffsetX;
    }
}
