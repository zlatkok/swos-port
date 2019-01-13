#include "log.h"
#include "file.h"
#include "util.h"
#include "audio.h"
#include "render.h"
#include "options.h"
#include "controls.h"

#ifdef NDEBUG
constexpr int kSentinelSize = 0;
#else
constexpr char kSentinelMagic[] = "nick.slaughter@mariah.key.com";
constexpr int kSentinelSize = sizeof(kSentinelMagic);
#endif

// this buffer will masquerade as DOS memory ;)
// decreased by the size of pitch patterns buffer
static char dosMemBuffer[342'402 + kSentinelSize];

// allocate buffer big enough to hold every single pitch pattern (if each would be unique)
static char pitchPatternsBuffer[(42 * 53 + 42) * 256 + kSentinelSize];

// extended memory buffers
static char linBuf384k[393'216 + kSentinelSize];

#ifndef NDEBUG
template <size_t N>
void verifyBlock(char (&array)[N])
{
    assert(!memcmp(array + sizeof(array) - kSentinelSize, kSentinelMagic, kSentinelSize));
}

void checkMemory()
{
    verifyBlock(dosMemBuffer);
    verifyBlock(pitchPatternsBuffer);
    verifyBlock(linBuf384k);
}
#endif

static void printIntroString()
{
    auto str = aSensibleWorldOfSoccerV2_0;
    while (*str != '\r')
        std::cout << *str++;

    std::cout << "[Win32 Port]\n";
}

static void setupDosBaseMemory()
{
    g_pitchPatterns = pitchPatternsBuffer;

    dosMemLinAdr = dosMemBuffer;
    dosMemOfs30000h = dosMemBuffer + 0x30000;
    dosMemOfs4fc00h = dosMemBuffer + 0x3d400;   // names don't match offsets anymore, but oh well ;)
    dosMemOfs60c00h = dosMemBuffer + 0x4e400;

#ifndef NDEBUG
    memcpy(dosMemBuffer + sizeof(dosMemBuffer) - kSentinelSize, kSentinelMagic, kSentinelSize);
    memcpy(pitchPatternsBuffer + sizeof(pitchPatternsBuffer) - kSentinelSize, kSentinelMagic, kSentinelSize);
#endif
}

static void setupExtendedMemory()
{
    linAdr384k = linBuf384k;

    g_memAllOk = 1;
    g_gotExtraMemoryForSamples = 1;

#ifndef NDEBUG
    memcpy(linBuf384k + sizeof(linBuf384k) - kSentinelSize, kSentinelMagic, kSentinelSize);
#endif
}

static void init()
{
    skipIntro = 0;
    printIntroString();

    setupDatBuffer = 0;     // disable this permanently
    EGA_graphics = 0;
    g_videoSpeedIndex = 100;

    g_inputControls = 6;    // set this up for CheckControls to avoid touching g_*ScanCode variables

    logInfo("Setting up base and extended memory pointers");
    setupDosBaseMemory();
    setupExtendedMemory();

    initAudio();
}

// showImageReel
//
// imageList -> array of char pointers to image names, terminated with -1
//
// returns: true if ended normally, false if user aborted
//
static bool showImageReel(const char **imageList)
{
    assert(imageList);

    constexpr int k256ImageSize = kVgaScreenSize + kVgaPaletteSize;

    auto imageBuffer = linAdr384k;
    auto currentImageBuffer = imageBuffer + kVgaScreenSize;

    // 3 image buffer will be used, images 0 and 2 being empty
    memset(imageBuffer, 0, 3 * kVgaScreenSize);

    // first image palette is used for all scrolling images
    if (loadFile(*imageList, currentImageBuffer, false) != k256ImageSize)
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
                memmove(linAdr384k, imageBuffer + lineOffset, kVgaScreenSize);
                return false;
            }
        }

        while (true) {
            if (*++imageList == (const char *)-1)
                return true;

            if (loadFile(*imageList, currentImageBuffer, false) == k256ImageSize)
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
    randomSeed = seed;
}

static bool initIntroAnimation()
{
    resetGameAudio();

    soccerBinPtr = dosMemLinAdr;
    introCurrentSoundBuffer = dosMemLinAdr + 65536;
    soccerBinSoundChunkBuffer = introCurrentSoundBuffer + 47628;
    introCurrentSoundBufferChunk = soccerBinSoundChunkBuffer;
    soccerBinSoundDataPtr = soccerBinSoundChunkBuffer + 47628;
    introScreenBuffer = linAdr384k + 65536;
    introAborted = 0;

    do {
        if (soccerBinHandle = openFile(aArcSoccer_bin)) {
            introFrameDataOffset = 0;
            introFrameDataChunkRemainder = 0;
            introFirstChunkLoaded = 0;

            if (fread(soccerBinPtr, 20, 1, soccerBinHandle) != 1)
                break;

            introNumFrames = *reinterpret_cast<dword *>(soccerBinPtr + 12);
            auto size = *reinterpret_cast<word *>(soccerBinPtr + 16);

            if (fread(soccerBinPtr, size, 1, soccerBinHandle) != 1)
                break;

            memcpy(introCurrentSoundBuffer, soccerBinPtr, 47628);

            size = *reinterpret_cast<word *>(soccerBinPtr + 47628);
            if (fread(soccerBinPtr, size, 1, soccerBinHandle) != 1)
                break;

            memcpy(soccerBinSoundChunkBuffer, soccerBinPtr, 47628);

            size = *reinterpret_cast<word *>(soccerBinPtr + 47628);
            if (fread(soccerBinPtr, size, 1, soccerBinHandle) != 1)
                break;

            memcpy(soccerBinSoundDataPtr + 47628, soccerBinPtr, 47628);
            memcpy(soccerBinSoundDataPtr, soccerBinSoundChunkBuffer, 47628);

            introSoundBufferIndex = 95256;
            introCurrentFrameDataPtr = soccerBinPtr + 47628;
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
    if (soccerBinHandle)
        fclose(soccerBinHandle);
}

void SWOS::IntroEnsureFrameDataLoaded()
{
    constexpr int kChunkSize = 2048;

    int dest = 0;
    int numChunks = 0;
    int subSize = 0;
    auto wasFirstChunkLoaded = introFirstChunkLoaded;

    if (!introFirstChunkLoaded) {
        introFirstChunkLoaded = 1;
        numChunks = 1;
    }

    if (wasFirstChunkLoaded || introFrameDataSize > kChunkSize) {
        introFrameDataChunkRemainder = introFrameDataOffset % kChunkSize;
        auto bytesUntilNextChunk = kChunkSize - introFrameDataChunkRemainder;

        if (introFrameDataChunkRemainder + introFrameDataSize < kChunkSize)
            return;

        if (introFrameDataChunkRemainder) {
            dest = introFrameDataChunkRemainder;
            memcpy(soccerBinPtr + dest, soccerBinPtr + introFrameDataOffset, bytesUntilNextChunk);
            dest += bytesUntilNextChunk;
            subSize = bytesUntilNextChunk;
            if (introFrameDataSize == bytesUntilNextChunk)
                return;
        } else {
            introFrameDataChunkRemainder = 0;
        }

        auto res = std::div(introFrameDataSize - subSize, kChunkSize);
        numChunks = res.quot + (res.rem > 0);
    }

    fread(soccerBinPtr + dest, numChunks * kChunkSize, 1, soccerBinHandle);
    introFrameDataOffset = introFrameDataChunkRemainder;
}

static void introDelay()
{
    SDL_Delay(35);
}

static char *introDrawFrame(char *frameData)
{
    introCurrentFrameDataPtr = frameData;

    if (!introAnimationTimer) {
        setPalette(frameData, kVgaPaletteNumColors / 2);
        frameData += kVgaPaletteSize / 2;

        int index = 0;
        auto dest = introScreenBuffer;

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

        auto src = introScreenBuffer;
        for (int y = 0; y < kVgaHeight / 2; y++) {
            for (int x = 0; x < kVgaWidth / 2; x++) {
                src[1] = (src[0] + src[2]) / 2;
                src[kVgaWidth] = (src[0] + src[2 * kVgaWidth]) / 2;
                src[kVgaWidth + 1] = (src[0] + src[2 * kVgaWidth + 2]) / 2;
                src += 2;
            }
            src += kVgaWidth;
        }

        updateScreen(introScreenBuffer, 32, 148);
        while (introCountDownTimer) {
            introDelay();
            timerProc();
        }
        timerProc();
        introCountDownTimer = introWaitInterval;
    } else {
        frameData += introFrameDataSize - 1768;
        if (introAnimationTimer < 4) {
            while (introCountDownTimer > 4 - introAnimationTimer) {
                introDelay();
                timerProc();
            }
            introCountDownTimer = 4;
            introAnimationTimer = 0;
        } else {
            while (!introCountDownTimer) {
                introDelay();
                timerProc();
            }
            introCountDownTimer = 4;
            introAnimationTimer = 4;
        }
        timerProc();
    }

    SAFE_INVOKE(IntroFillSoundBuffer);
    frameData += 1764;

//    if (introCurrentSoundBufferNum == 26)
//        playIntroSample(soccerBinSoundDataPtr + introCurrentSoundBufferChunkOffset, kIntroBufferSize / 2, 127, 0);

    return frameData;
}

__declspec(naked) void SWOS::IntroDrawFrame()
{
    __asm {
        push esi
        call introDrawFrame
        add  esp, 4
        mov  esi, eax
        retn
    }
}

// called from SWOS at the start
void SWOS::SWOS()
{
    init();

#ifdef NDEBUG
    bool aborted = false;
    if (!disableImageReels()) {
        logInfo("Showing image reels");
        aborted = !showImageReel(partOneImages) || !showImageReel(partTwoImages);
    }

    getPalette(g_fadePalette);
    SAFE_INVOKE(FadeOutToBlack);

    if (!disableIntro() && !aborted && initIntroAnimation()) {
        logInfo("Playing intro");
        PlayIntroAnimation();
        getPalette(g_fadePalette);
        SAFE_INVOKE(FadeOutToBlack);
    }

#endif
//initIntroAnimation();
//PlayIntroAnimation();

    controlWord = 0;
    g_pl2Status = 0;
    g_pl1Status = 0;
    pl1FinalStatus = -1;
    pl2FinalStatus = -1;

    logInfo("Loading title and menu music");
    SAFE_INVOKE(LoadTitleAndStartMusic);
    initRandomSeed();

    someColorConversionFlag = 1;    // not sure what this is

    goalBasePtr = currentHilBuffer;
    nextGoalPtr = reinterpret_cast<dword *>(hilFileBuffer);

    loadFile(aSprite_dat, spritesIndex);
    loadFile(aCharset_dat, dosMemOfs30000h);

    logInfo("Fixing up initial charset");
    SAFE_INVOKE(FixupInitialCharset);
    D0 = 0;
    SAFE_INVOKE(ChainFont);
    SAFE_INVOKE(Randomize2);

    vsPtr = linAdr384k;
    screenWidth = kVgaWidth;
    spriteFixupFlag = 0;

    // flush controls
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);

#ifndef NDEBUG
    // verify that the string offsets are OK
    assert(!memcmp(aChairmanScenes + 0x4dc1, "YUGOSLAVIA", 11));
    assert(!memcmp(aChairmanScenes + 0x2e5, "THE PRESIDENT", 14));
    assert(!memcmp(aChairmanScenes + 0x2f, "RETURN TO GAME", 15));
    assert(!memcmp(aChairmanScenes + 0x30c, "EXCELLENT JOB", 13));
    assert(!memcmp(aChairmanScenes + 0x58c3, "QUEENSLAND", 11));
    assert(!memcmp(aChairmanScenes + 0x331, "APPRECIATE IT", 14));
    assert(!memcmp(aChairmanScenes + 0x3d1c, "DISK FULL", 9));
#endif

    logInfo("Going to main menu");
    SAFE_INVOKE(MainMenu);
}
