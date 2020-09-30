#include "audio.h"
#include "wavFormat.h"
#include "sfx.h"
#include "comments.h"
#include "chants.h"
#include "music.h"
#include "log.h"
#include "swos.h"
#include "options.h"
#include "audioOptions.mnu.h"

constexpr int kMaxVolume = MIX_MAX_VOLUME;
constexpr int kMinVolume = 0;

static int16_t m_volume = 100;                      // master sound volume
static std::atomic<int16_t> m_musicVolume = 100;    // atomic since ADL thread will need access to it

static int m_actualFrequency;
static int m_actualChannels;

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
    if (!swos.g_soundOff)
        resetMenuAudio();
}

void finishAudio()
{
    finishMusic();
    Mix_CloseAudio();
}

static void channelFinished(int channel);

// Called when switching from menus to the game.
void initGameAudio()
{
    if (swos.g_soundOff)
        return;

    fadeOutMusic();
    resetGameAudio();

    Mix_ChannelFinished(channelFinished);

    initCommentsBeforeTheGame();
    initSfxBeforeTheGame();
    initChantsBeforeTheGame();
}

static void channelFinished(int channel)
{
    if (!commenteryOnChannelFinished(channel) && !chantsOnChannelFinished(channel))
        logDebug("Channel %d finished playing", channel);
}

void SWOS::StopAudio()
{
    // SDL_mixer will crash if we call these before it's initialized
    if (Mix_QuerySpec(nullptr, nullptr, nullptr)) {
        Mix_HaltChannel(-1);
        Mix_HaltMusic();
    }
}

int playIntroSample(void *buffer, int size, int volume, int loopCount)
{
    assert(m_actualFrequency == kGameFrequency && m_actualChannels <= 2);
//    assert(size == kIntroBufferSize);
    assert(loopCount == 0);

//    static Mix_Chunk *chunk;
//    Mix_FreeChunk(chunk);

    //char waveBuffer[8 * kIntroBufferSize];
    //fillWaveBuffer(waveBuffer, buffer, size);

//    chunk = chunkFromBuffer(waveBuffer, kSizeofWaveHeader + size);
//    static uint16_t stereoBuffer[kIntroBufferSize];

//    return Mix_PlayChannel(0, chunk, 0);
    return -1;
}

__declspec(naked) void SWOS::PlaySoundSample()
{
#ifdef SWOS_VM
#else
    __asm {
    //    push ecx
    //    push ebx
    //    push eax
    //    push edx
    //    call playIntroSample
    //    add  esp, 16
        retn
    }
#endif
}

//
// option variables
//

static const std::array<Option<int16_t>, 4> kAudioOptions = {
    "soundOff", &swos.g_soundOff, 0, 1, 0,
    "musicOff", &swos.g_musicOff, 0, 1, 0,
    "commentary", &swos.g_commentary, 0, 1, 1,
    "crowdChants", &swos.g_crowdChantsOn, 0, 1, 1,
};

const char kAudioSection[] = "audio";
const char kMasterVolume[] = "masterVolume";
const char kMusicVolume[] = "musicVolume";

template <typename T>
static void setVolume(T& dest, int volume, const char *desc)
{
    if (volume < 0 || volume > kMaxVolume)
        logWarn("Invalid value given for %s volume (%d), clamping", desc, volume);

    volume = std::min(volume, kMaxVolume);
    volume = std::max(volume, 0);

    dest = volume;
}

static void setMasterVolume(int volume, bool apply = true)
{
    setVolume(m_volume, volume, "master");

    if (apply && !swos.g_soundOff)
        Mix_Volume(-1, m_volume);
}

int getMusicVolume()
{
    return m_musicVolume;
}

static void setMusicVolume(int volume, bool apply = true)
{
    setVolume(m_musicVolume, volume, "music");

    if (apply && !swos.g_soundOff && !swos.g_musicOff)
        Mix_VolumeMusic(m_musicVolume);
}

void loadAudioOptions(const CSimpleIniA& ini)
{
    loadOptions(ini, kAudioOptions, kAudioSection);

    swos.g_menuMusic = !swos.g_musicOff;

    auto volume = ini.GetLongValue(kAudioSection, kMasterVolume, 100);
    setMasterVolume(volume, false);

    auto musicVolume = ini.GetLongValue(kAudioSection, kMusicVolume, 100);
    setMusicVolume(volume, false);
}

void saveAudioOptions(CSimpleIni& ini)
{
    swos.g_musicOff = !swos.g_menuMusic;

    saveOptions(ini, kAudioOptions, kAudioSection);

    ini.SetLongValue(kAudioSection, kMasterVolume, m_volume);
    ini.SetLongValue(kAudioSection, kMusicVolume, m_musicVolume);
}

//
// audio options menu
//

void showAudioOptionsMenu()
{
    showMenu(audioOptionsMenu);
}

static void toggleMasterSound()
{
    swos.g_soundOff = !swos.g_soundOff;

    if (swos.g_soundOff) {
        finishMusic();
        finishAudio();
    } else {
        initAudio();
        if (swos.g_menuMusic)
            restartMusic();
    }

    DrawMenuItem();
}

static void toggleMenuMusic()
{
    swos.g_menuMusic = !swos.g_menuMusic;
    swos.g_musicOff = !swos.g_menuMusic;

    if (swos.g_menuMusic)
        restartMusic();
    else
        finishMusic();

    DrawMenuItem();
}

static void increaseVolume()
{
    if (m_volume < kMaxVolume) {
        setMasterVolume(m_volume + 1);
        DrawMenuItem();
    }
}

static void decreaseVolume()
{
    if (m_volume > 0) {
        setMasterVolume(m_volume - 1);
        DrawMenuItem();
    }
}

static void volumeBeforeDraw()
{
    auto entry = A5.as<MenuEntry *>();
    entry->u2.number = m_volume;
}

static void increaseMusicVolume()
{
    if (m_musicVolume < kMaxVolume) {
        setMusicVolume(getMusicVolume() + 1);
        DrawMenuItem();
    }
}

static void decreaseMusicVolume()
{
    if (getMusicVolume() > 0) {
        setMusicVolume(getMusicVolume() - 1);
        DrawMenuItem();
    }
}

static void musicVolumeBeforeDraw()
{
    auto entry = A5.as<MenuEntry *>();
    entry->u2.number = getMusicVolume();
}

static void toggleCrowdChants()
{
    if (!swos.g_crowdChantsOn)
        SWOS::TurnCrowdChantsOn();
    else
        SWOS::TurnCrowdChantsOff();
}
