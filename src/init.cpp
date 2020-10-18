#include "log.h"
#include "file.h"
#include "util.h"
#include "audio.h"
#include "render.h"
#include "options.h"
#include "replays.h"
#include "controls.h"

#ifdef NDEBUG
constexpr int kSentinelSize = 0;
#else
constexpr char kSentinelMagic[] = "nick.slaughter@key.mariah.com";
constexpr int kSentinelSize = sizeof(kSentinelMagic);
#endif

// sizes of all SWOS buffers in DOS and extended memory
constexpr int kDosMemBufferSize = 342'402 + kSentinelSize;
// allocate buffer big enough to hold every single pitch pattern (if each would be unique)
constexpr int kPitchPatternsBufferSize = (42 * 53 + 42) * 256 + kSentinelSize;

static SDL_RWops *m_soccerBinHandle;
static char *m_soccerBinPtr;

#ifdef DEBUG
constexpr int kExtendedMemoryBufferSize = 393'216 + kSentinelSize;
constexpr int kTotalExtraMemorySize = kDosMemBufferSize + kPitchPatternsBufferSize + kExtendedMemoryBufferSize;

void verifyBlock(const char *array, size_t size)
{
    assert(!memcmp(array + size - kSentinelSize, kSentinelMagic, kSentinelSize));
}

void checkMemory()
{
    auto extraMemStart = SwosVM::getExtraMemoryArea();

    assert(swos.dosMemLinAdr == extraMemStart);
    verifyBlock(extraMemStart, kDosMemBufferSize);

    assert(swos.g_pitchPatterns == extraMemStart + kDosMemBufferSize);
    verifyBlock(extraMemStart + kDosMemBufferSize, kPitchPatternsBufferSize);

    assert(swos.linAdr384k == extraMemStart + kDosMemBufferSize + kPitchPatternsBufferSize);
    verifyBlock(extraMemStart + kDosMemBufferSize + kPitchPatternsBufferSize, kExtendedMemoryBufferSize);
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
    assert(SwosVM::kExtendedMemSize >= kTotalExtraMemorySize && kTotalExtraMemorySize + 10'000 > SwosVM::kExtendedMemSize);

    auto extraMemStart = SwosVM::getExtraMemoryArea();
    assert(reinterpret_cast<uintptr_t>(extraMemStart) % sizeof(void *) == 0);

    swos.dosMemLinAdr = extraMemStart;
    swos.dosMemOfs30000h = extraMemStart + 0x30000;
    swos.dosMemOfs4fc00h = extraMemStart + 0x3d400; // names don't match offsets anymore, but are left for historical preservation ;)
    swos.dosMemOfs60c00h = extraMemStart + 0x4e400;

    swos.g_pitchPatterns = extraMemStart + kDosMemBufferSize;

    swos.linAdr384k = extraMemStart + kDosMemBufferSize + kPitchPatternsBufferSize;

    swos.g_memAllOk = 1;
    swos.g_gotExtraMemoryForSamples = 1;

#ifdef DEBUG
    SwosVM::initSafeMemoryAreas();
    memcpy(swos.dosMemLinAdr + kDosMemBufferSize - kSentinelSize, kSentinelMagic, kSentinelSize);
    memcpy(swos.g_pitchPatterns + kPitchPatternsBufferSize - kSentinelSize, kSentinelMagic, kSentinelSize);
    memcpy(swos.linAdr384k + kExtendedMemoryBufferSize - kSentinelSize, kSentinelMagic, kSentinelSize);
#endif
}

static void init()
{
    swos.skipIntro = 0;
    printIntroString();

    swos.setupDatBuffer = 0;    // disable this permanently
    swos.EGA_graphics = 0;
    swos.g_videoSpeedIndex = 100;

    swos.g_inputControls = 6;   // set this up for CheckControls to avoid touching g_*ScanCode variables

    logInfo("Setting up base and extended memory pointers");
    setupExtraMemory();

    initAudio();

    initReplays();
}

// showImageReel
//
// imageList -> array of char pointers to image names, terminated with -1
//
// returns: true if ended normally, false if user aborted
//
static bool showImageReel(SwosDataPointer<const char> *imageList)
{
    assert(imageList);

    constexpr int k256ImageSize = kVgaScreenSize + kVgaPaletteSize;

    auto imageBuffer = swos.linAdr384k;
    auto currentImageBuffer = imageBuffer + kVgaScreenSize;

    // 3 image buffer will be used, images 0 and 2 being empty
    memset(imageBuffer, 0, 3 * kVgaScreenSize);

    // first image palette is used for all scrolling images
    if (loadFile(*imageList, currentImageBuffer, -1) != k256ImageSize)
        return true;

    setPalette(currentImageBuffer + kVgaScreenSize);

    do {
        // clear palette area
        memset(currentImageBuffer + kVgaScreenSize, 0, kVgaPaletteSize);

        // scroll down line by line
        for (int lineOffset = 0; lineOffset < 2 * kVgaHeight * kVgaWidth; lineOffset += kVgaWidth) {
            frameDelay();
            updateScreen(imageBuffer + lineOffset);

            if (anyInputActive()) {
                // copy image at the proper position to linAdr384k so it can fade-out properly (when menu comes in)
                memmove(swos.linAdr384k, imageBuffer + lineOffset, kVgaScreenSize);
                return false;
            }
        }

        while (true) {
            if (*++imageList == (const char *)-1)
                return true;

            if (loadFile(*imageList, currentImageBuffer, -1, false) == k256ImageSize)
                break;
        }
    } while (true);

    return true;
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

static bool initIntroAnimation()
{
    resetGameAudio();

    m_soccerBinPtr = swos.dosMemLinAdr;
    swos.introCurrentSoundBuffer = swos.dosMemLinAdr + kVirtualScreenSize;
    swos.soccerBinSoundChunkBuffer = swos.introCurrentSoundBuffer + 47628;
    swos.introCurrentSoundBufferChunk = swos.soccerBinSoundChunkBuffer;
    swos.soccerBinSoundDataPtr = swos.soccerBinSoundChunkBuffer + 47628;
    swos.introScreenBuffer = swos.linAdr384k + kVirtualScreenSize;
    swos.introAborted = 0;

    do {
        if (m_soccerBinHandle = openFile(swos.aArcSoccer_bin)) {
            swos.introFrameDataOffset = 0;
            swos.introFrameDataChunkRemainder = 0;
            swos.introFirstChunkLoaded = 0;

            if (SDL_RWread(m_soccerBinHandle, m_soccerBinPtr, 20, 1) != 1)
                break;

            swos.introNumFrames = *reinterpret_cast<dword *>(m_soccerBinPtr + 12);
            auto size = *reinterpret_cast<word *>(m_soccerBinPtr + 16);

            if (SDL_RWread(m_soccerBinHandle, m_soccerBinPtr, size, 1) != 1)
                break;

            memcpy(swos.introCurrentSoundBuffer, m_soccerBinPtr, 47628);

            size = *reinterpret_cast<word *>(m_soccerBinPtr + 47628);
            if (SDL_RWread(m_soccerBinHandle, m_soccerBinPtr, size, 1) != 1)
                break;

            memcpy(swos.soccerBinSoundChunkBuffer, m_soccerBinPtr, 47628);

            size = *reinterpret_cast<word *>(m_soccerBinPtr + 47628);
            if (SDL_RWread(m_soccerBinHandle, m_soccerBinPtr, size, 1) != 1)
                break;

            memcpy(swos.soccerBinSoundDataPtr + 47628, m_soccerBinPtr, 47628);
            memcpy(swos.soccerBinSoundDataPtr, swos.soccerBinSoundChunkBuffer, 47628);

            swos.introSoundBufferIndex = 95256;
            swos.introCurrentFrameDataPtr = m_soccerBinPtr + 47628;
        }

        clearScreen();
        return false;
    } while (false);

    logWarn("Failed to read soccer.bin");
    SWOS::CloseSoccerBin();
    return true;
}

void SWOS::CloseSoccerBin()
{
    if (m_soccerBinHandle)
        SDL_RWclose(m_soccerBinHandle);
}

void SWOS::IntroEnsureFrameDataLoaded()
{
    constexpr int kChunkSize = 2048;

    int dest = 0;
    int numChunks = 0;
    int subSize = 0;
    auto wasFirstChunkLoaded = swos.introFirstChunkLoaded;

    if (!swos.introFirstChunkLoaded) {
        swos.introFirstChunkLoaded = 1;
        numChunks = 1;
    }

    if (wasFirstChunkLoaded || swos.introFrameDataSize > static_cast<size_t>(kChunkSize)) {
        swos.introFrameDataChunkRemainder = swos.introFrameDataOffset % kChunkSize;
        auto bytesUntilNextChunk = kChunkSize - swos.introFrameDataChunkRemainder;

        if (swos.introFrameDataChunkRemainder + swos.introFrameDataSize < kChunkSize)
            return;

        if (swos.introFrameDataChunkRemainder) {
            dest = swos.introFrameDataChunkRemainder;
            memcpy(m_soccerBinPtr + dest, m_soccerBinPtr + swos.introFrameDataOffset, bytesUntilNextChunk);
            dest += bytesUntilNextChunk;
            subSize = bytesUntilNextChunk;
            if (swos.introFrameDataSize == bytesUntilNextChunk)
                return;
        } else {
            swos.introFrameDataChunkRemainder = 0;
        }

        auto res = std::div(swos.introFrameDataSize - subSize, kChunkSize);
        numChunks = res.quot + (res.rem > 0);
    }

    SDL_RWread(m_soccerBinHandle, m_soccerBinPtr + dest, numChunks * kChunkSize, 1);
    swos.introFrameDataOffset = swos.introFrameDataChunkRemainder;
}

static void introDelay()
{
    SDL_Delay(35);
}

static char *introDrawFrame(char *frameData)
{
    swos.introCurrentFrameDataPtr = frameData;

    if (!swos.introAnimationTimer) {
        setPalette(frameData, kVgaPaletteNumColors / 2);
        frameData += kVgaPaletteSize / 2;

        int index = 0;
        auto dest = swos.introScreenBuffer;

        do {
            auto pixel = *frameData++;
            if (pixel < 0) {
                pixel &= 0x7f;

                auto count = *frameData++;
                if (!count)
                    break;

                do {
                    dest[index] = pixel;
                    index += 2;
                    if (index >= kVgaWidth) {
                        index = 0;
                        dest += 2 * kVgaWidth;
                    }
                } while (--count);
            } else {
                dest[index] = pixel;
                index += 2;
                if (index >= kVgaWidth) {
                    index = 0;
                    dest += 2 * kVgaWidth;
                }
            }
        } while (true);

        auto src = swos.introScreenBuffer;
        for (int y = 0; y < kVgaHeight / 2; y++) {
            for (int x = 0; x < kVgaWidth / 2; x++) {
                src[1] = (src[0] + src[2]) / 2;
                src[kVgaWidth] = (src[0] + src[2 * kVgaWidth]) / 2;
                src[kVgaWidth + 1] = (src[0] + src[2 * kVgaWidth + 2]) / 2;
                src += 2;
            }
            src += kVgaWidth;
        }

        updateScreen(swos.introScreenBuffer, 32, 148);
        while (swos.introCountDownTimer) {
            introDelay();
            timerProc();
        }
        timerProc();
        swos.introCountDownTimer = swos.introWaitInterval;
    } else {
        frameData += swos.introFrameDataSize - 1768;
        if (swos.introAnimationTimer < 4) {
            while (swos.introCountDownTimer > 4 - swos.introAnimationTimer) {
                introDelay();
                timerProc();
            }
            swos.introCountDownTimer = 4;
            swos.introAnimationTimer = 0;
        } else {
            while (!swos.introCountDownTimer) {
                introDelay();
                timerProc();
            }
            swos.introCountDownTimer = 4;
            swos.introAnimationTimer = 4;
        }
        timerProc();
    }

    SAFE_INVOKE(IntroFillSoundBuffer);
    frameData += 1764;

    return frameData;
}

__declspec(naked) void SWOS::IntroDrawFrame()
{
#ifdef SWOS_VM
    auto frameData = SwosVM::esi.asPtr();
    auto result = introDrawFrame(frameData);
    SwosVM::esi = result;
#else
    __asm {
        push esi
        call introDrawFrame
        add  esp, 4
        mov  esi, eax
        retn
    }
#endif
}

static void showImageReelsAndIntro()
{
    bool aborted = false;
    if (!disableImageReels()) {
        logInfo("Showing image reels");
        aborted = !showImageReel(swos.partOneImages) || !showImageReel(swos.partTwoImages);
    }

    getPalette(swos.g_fadePalette);
    SAFE_INVOKE(FadeOutToBlack);

    if (!disableIntro() && !aborted && initIntroAnimation()) {
        logInfo("Playing intro");
        PlayIntroAnimation();
        getPalette(swos.g_fadePalette);
        SAFE_INVOKE(FadeOutToBlack);
    }
}

// called from SWOS at the start
void SWOS::SWOS()
{
    init();

#if !defined(SWOS_TEST) && defined(NDEBUG)
    showImageReelsAndIntro();
#endif
//initIntroAnimation();
//PlayIntroAnimation();

    swos.controlWord = 0;
    swos.g_pl2Status = 0;
    swos.g_pl1Status = 0;
    swos.pl1FinalStatus = -1;
    swos.pl2FinalStatus = -1;

    logInfo("Loading title and menu music");
    SAFE_INVOKE(LoadTitleAndStartMusic);
    initRandomSeed();

    swos.someColorConversionFlag = 1;    // not sure what this is

    swos.goalBasePtr = swos.currentHilBuffer;
    swos.nextGoalPtr = reinterpret_cast<dword *>(swos.hilFileBuffer);

    loadFile(swos.aSprite_dat, swos.spritesIndex);
    loadFile(swos.aCharset_dat, swos.dosMemOfs30000h);

    logInfo("Fixing up initial charset");
    SAFE_INVOKE(FixupInitialCharset);
    D0 = 0;
    SAFE_INVOKE(ChainFont);
    SAFE_INVOKE(Randomize2);

    swos.vsPtr = swos.linAdr384k;
    swos.screenWidth = kVgaWidth;
    swos.spriteFixupFlag = 0;

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
    MainMenu();
#endif
}
