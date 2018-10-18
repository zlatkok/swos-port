#include "audio.h"
#include "music.h"
#include "util.h"
#include "log.h"
#include "file.h"
#include "swos.h"
#include "audioOptions.mnu.h"

constexpr int kMaxVolume = MIX_MAX_VOLUME;
constexpr int kMinVolume = 0;

static int16_t m_volume = 100;                      // master sound volume
static std::atomic<int16_t> m_musicVolume = 100;    // atomic since ADL thread will need access to it

static int m_actualFrequency;
static int m_actualChannels;

static int m_refereeWhistleSampleChannel;
static int m_commentaryChannel;

#pragma pack(push, 1)
struct WaveFileHeader {
    dword id;
    dword size;
    dword format;
};

struct WaveFormatHeader {
    dword id;
    dword size;
    word format;
    word channels;
    dword frequency;
    dword byteRate;
    word sampleSize;
    word bitsPerSample;
};

struct WaveDataHeader {
    dword id;
    dword length;
};
#pragma pack(pop)

constexpr int kSizeofWaveHeader = sizeof(WaveFileHeader) + sizeof(WaveFormatHeader) + sizeof(WaveDataHeader);

template <typename T>
static void setVolume(T& dest, int volume, const char *desc)
{
    if (volume < 0 || volume > kMaxVolume)
        logWarn("Invalid value given for %s volume (%d), clamping", desc, volume);

    volume = std::min(volume, kMaxVolume);
    volume = std::max(volume, 0);

    dest = volume;
}

int getMasterVolume()
{
    return m_volume;
}

void setMasterVolume(int volume, bool apply /* = true */)
{
    setVolume(m_volume, volume, "master");

    if (apply && !g_soundOff)
        Mix_Volume(-1, m_volume);
}

int getMusicVolume()
{
    return m_musicVolume;
}

void setMusicVolume(int volume, bool apply /* = true */)
{
    setVolume(m_musicVolume, volume, "music");

    if (apply && !g_soundOff && !g_musicOff)
        Mix_VolumeMusic(m_musicVolume);
}


static void verifySpec(int frequency, int format, int channels)
{
    Uint16 actualFormat;

    Mix_QuerySpec(&m_actualFrequency, &actualFormat, &m_actualChannels);
    logInfo("Audio initialized at %dkHz, format: %d, number of channels: %d", m_actualFrequency, actualFormat, m_actualChannels);

    if (m_actualFrequency != frequency || actualFormat != format || m_actualChannels < channels)
        logWarn("Didn't get the desired audio specification, asked for frequency: %d, format: %d, channels: %d",
            frequency, format, channels);
}

static void resetMenuAudio()
{
    Mix_CloseAudio();

    if (Mix_OpenAudio(kMenuFrequency, MIX_DEFAULT_FORMAT, 2, kMenuChunkSize))
        sdlErrorExit("SDL Mixer failed to initialize for the menus");

    verifySpec(kMenuFrequency, MIX_DEFAULT_FORMAT, 2);

    synchronizeMixVolume();
}

void resetGameAudio()
{
    Mix_CloseAudio();

    if (Mix_OpenAudio(kGameFrequency, MIX_DEFAULT_FORMAT, 1, kGameChunkSize))
        sdlErrorExit("SDL Mixer failed to initialize for the game");

    Mix_Volume(-1, m_volume);
    verifySpec(kGameFrequency, MIX_DEFAULT_FORMAT, 1);
}

void ensureMenuAudioFrequency()
{
    int frequency;
    if (Mix_QuerySpec(&frequency, nullptr, nullptr) && frequency != kMenuFrequency)
        resetMenuAudio();
}

void initAudio()
{
    if (!g_soundOff)
        resetMenuAudio();
}

void finishAudio()
{
    finishMusic();
    Mix_CloseAudio();
}

static void channelFinished(int channel)
{
    logDebug("Channel %d finished playing", channel);

    if (channel == m_refereeWhistleSampleChannel)
        m_refereeWhistleSampleChannel = -1;
    else if (channel == m_commentaryChannel)
        m_commentaryChannel = -1;
    else if (channel == commentarySampleHandle)
        commentarySampleHandle = -1;
}

// switch from menu to game
static void initGameAudio()
{
    if (g_soundOff)
        return;

    fadeOutMusic();
    resetGameAudio();

    Mix_ChannelFinished(channelFinished);

    m_refereeWhistleSampleChannel = -1;
    m_commentaryChannel = -1;
    commentarySampleHandle = -1;
}

void SWOS::StopAudio()
{
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
}

void SWOS::PlayCrowdNoiseSample_OnEnter()
{
    initGameAudio();
}

static bool is11KhzSample(int size)
{
    // only two samples are always 11kHz: the whistle samples
    return size == 23'882 || size == 2'518;
}

static void fillWaveBuffer(char *waveBuffer, const void *sampleData, int size)
{
    static const char kWaveHeader[] = {
        'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E',                                 // file header
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 8, 0,    // format header
        'd', 'a', 't', 'a', 0, 0, 0, 0,                                                     // data header
    };
    static_assert(sizeof(kWaveHeader) == kSizeofWaveHeader, "Wave header format invalid");

    memcpy(waveBuffer, kWaveHeader, sizeof(kWaveHeader));

    auto waveHeader = reinterpret_cast<WaveFileHeader *>(waveBuffer);
    waveHeader->size = sizeof(kWaveHeader) + size - 8;

    auto waveFormatHeader = reinterpret_cast<WaveFormatHeader *>(reinterpret_cast<char *>(waveHeader) + sizeof(*waveHeader));
    waveFormatHeader->frequency = is11KhzSample(size) ? 11025 : 22050;
    waveFormatHeader->byteRate = waveFormatHeader->frequency;

    auto waveDataHeader = reinterpret_cast<WaveDataHeader *>(reinterpret_cast<char *>(waveFormatHeader) + sizeof(*waveFormatHeader));
    waveDataHeader->length = size;

    memcpy(waveBuffer + kSizeofWaveHeader, sampleData, size);
}

static Mix_Chunk *chunkFromBuffer(void *buffer, int size)
{
    Mix_Chunk *chunk = nullptr;

    if (auto rwOps = SDL_RWFromMem(buffer, kSizeofWaveHeader + size)) {
        chunk = Mix_LoadWAV_RW(rwOps, 1);
        if (!chunk)
            logWarn("Failed to load wave data");
    } else {
        logWarn("SDL_RWFromMem() failed");
    }

    return chunk;
}

static Mix_Chunk *convertSample(const void *buffer, int size)
{
    static std::unordered_map<int, Mix_Chunk *> waveCache;

    auto& chunk = waveCache[size];

    if (chunk)
        return chunk;

    char waveBuffer[400 * 1024];
    fillWaveBuffer(waveBuffer, buffer, size);

    chunk = chunkFromBuffer(waveBuffer, kSizeofWaveHeader + size);

    return chunk;
}

static int playSample(int *channel, void *buffer, int size, int volume, int loopCount)
{
    if (!g_soundOff) {
        auto chunk = convertSample(buffer, size);
        assert(chunk);

        if (chunk) {
            if (!volume)
                volume = 3 * MIX_MAX_VOLUME / 4;

            Mix_VolumeChunk(chunk, volume);
            auto newChannel = Mix_PlayChannel(channel ? *channel : -1, chunk, loopCount - 1);
            if (channel)
                *channel = newChannel;

            logDebug("Playing on channel %d, size %d, volume %d, loop count %d", newChannel, size, volume, loopCount);
            return newChannel;
        }
    }

    return -1;
}

static int playSfxSample(void *buffer, int size, int volume, int loopCount)
{
    return playSample(nullptr, buffer, size, volume, loopCount);
}

static int playChantSample(void *buffer, int size, int volume, int loopCount)
{
    return playSample(nullptr, buffer, size, volume, loopCount);
}

static int playCommentarySample(void *buffer, int size, int volume, int loopCount)
{
    return playSample(&m_commentaryChannel, buffer, size, volume, loopCount);
}

static int playRefereeWhistleSample(void *buffer, int size, int volume, int loopCount)
{
    return playSample(&m_refereeWhistleSampleChannel, buffer, size, volume, loopCount);
}

int playIntroSample(void *buffer, int size, int volume, int loopCount)
{
    assert(m_actualFrequency == kGameFrequency && m_actualChannels <= 2);
//    assert(size == kIntroBufferSize);
    assert(loopCount == 0);

    static Mix_Chunk *chunk;
    Mix_FreeChunk(chunk);

    char waveBuffer[8 * kIntroBufferSize];
    fillWaveBuffer(waveBuffer, buffer, size);

    chunk = chunkFromBuffer(waveBuffer, kSizeofWaveHeader + size);
//    static uint16_t stereoBuffer[kIntroBufferSize];

    return Mix_PlayChannel(0, chunk, 0);
}

inline __declspec(naked) void playSampleWrapper()
{
    __asm {
        test eax, eax
        jz   done       // there is at least one sample with 0 size

        push ecx
        push ebx
        push eax
        push edx
        call edi
        add  esp, 16

done:
        retn
    }
}

__declspec(naked) void SWOS::PlaySfxSample22Khz()
{
    __asm {
        mov edi, playSfxSample
        jmp playSampleWrapper
    }
}

__declspec(naked) void SWOS::PlayChantSample22Khz()
{
    __asm {
        mov edi, playChantSample
        jmp playSampleWrapper
    }
}

__declspec(naked) void SWOS::PlayCommentarySample22Khz()
{
    __asm {
        mov edi, playCommentarySample
        jmp playSampleWrapper
    }
}

__declspec(naked) void SWOS::PlayIntroSample()
{
    __asm {
        push ecx
        push ebx
        push eax
        push edx
        call playIntroSample
        add  esp, 16
        retn
    }
}

__declspec(naked) void SWOS::PlayRefereeWhistleSample()
{
    __asm {
        mov edi, playRefereeWhistleSample
        jmp playSampleWrapper
    }
}

static int isSamplePlaying(int channel)
{
    return channel >= 0 ? -Mix_Playing(channel) : 0;
}

__declspec(naked) void SWOS::IsSamplePlaying()
{
    __asm {
        push eax
        call isSamplePlaying
        add  esp, 4
        retn
    }
}

//
// audio options menu
//

void showAudioOptionsMenu()
{
    showMenu(audioOptionsMenu);
}

static void restartMusic()
{
    g_midiPlaying = 0;
    SWOS::CDROM_StopAudio();
    TryRestartingMusic();
}

void toggleMasterSound()
{
    g_soundOff = !g_soundOff;

    if (g_soundOff) {
        finishMusic();
        finishAudio();
    } else {
        initAudio();
        if (g_menuMusic)
            restartMusic();
    }

    DrawMenuItem();
}

void toggleMenuMusic()
{
    g_menuMusic = !g_menuMusic;
    g_musicOff = !g_menuMusic;

    if (g_menuMusic)
        restartMusic();
    else
        finishMusic();

    DrawMenuItem();
}

void increaseVolume()
{
    if (getMasterVolume() < kMaxVolume) {
        setMasterVolume(getMasterVolume() + 1);
        DrawMenuItem();
    }
}

void decreaseVolume()
{
    if (getMasterVolume() > 0) {
        setMasterVolume(getMasterVolume() - 1);
        DrawMenuItem();
    }
}

void volumeBeforeDraw()
{
    auto entry = A5.as<MenuEntry *>();
    entry->u2.number = m_volume;
}

void increaseMusicVolume()
{
    if (getMusicVolume() < kMaxVolume) {
        setMusicVolume(getMusicVolume() + 1);
        DrawMenuItem();
    }
}

void decreaseMusicVolume()
{
    if (getMusicVolume() > 0) {
        setMusicVolume(getMusicVolume() - 1);
        DrawMenuItem();
    }
}

void musicVolumeBeforeDraw()
{
    auto entry = A5.as<MenuEntry *>();
    entry->u2.number = getMusicVolume();
}
