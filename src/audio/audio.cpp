#include "audio.h"
#include "wavFormat.h"
#include "sfx.h"
#include "comments.h"
#include "chants.h"
#include "music.h"
#include "util.h"

constexpr int kDefaultVolume = 64;

static int16_t m_volume = kDefaultVolume;   // master sound volume
static int16_t m_musicVolume = kDefaultVolume;

static bool m_soundEnabled = true;
static bool m_musicEnabled = true;
static bool m_commentaryEnabled = true;

static int m_actualFrequency;
static int m_actualChannels;

static void verifySpec(int frequency, int format, int channels);
static void resetMenuAudio();
static void channelFinished(int channel);
static void setMasterVolume(int volume, bool apply);
static void setMusicVolume(int volume, bool apply);

void initAudio()
{
    if (m_soundEnabled)
        resetMenuAudio();
}

void finishAudio()
{
    finishMusic();
    Mix_CloseAudio();
}

void stopAudio()
{
    // SDL_mixer will crash if we call these before it's initialized
    if (Mix_QuerySpec(nullptr, nullptr, nullptr)) {
        Mix_HaltChannel(-1);
        Mix_HaltMusic();
    }
}

// Called when switching from menus to the game.
void initGameAudio()
{
    if (!m_soundEnabled)
        return;

    waitForMusicToFadeOut();
    resetGameAudio();

    Mix_ChannelFinished(channelFinished);

    initCommentsBeforeTheGame();
    initSfxBeforeTheGame();
    initChantsBeforeTheGame();
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

bool soundEnabled()
{
    return m_soundEnabled;
}

void initSoundEnabled(bool enabled)
{
    m_soundEnabled = enabled;
}

void setSoundEnabled(bool enabled)
{
    if (m_soundEnabled != enabled) {
        if (m_soundEnabled = enabled) {
            initAudio();
            if (m_musicEnabled)
                restartMusic();
        } else {
            finishAudio();
        }
    }
}

bool musicEnabled()
{
    return m_musicEnabled;
}

void initMusicEnabled(bool enabled)
{
    m_musicEnabled = enabled;
}

void setMusicEnabled(bool enabled)
{
    if (m_musicEnabled != enabled) {
        if (m_musicEnabled = enabled)
            restartMusic();
        else
            finishMusic();
    }
}

bool commentaryEnabled()
{
    return m_commentaryEnabled;
}

void setCommentaryEnabled(bool enabled)
{
    m_commentaryEnabled = enabled;
}

int getMasterVolume()
{
    return m_volume;
}

void initMasterVolume(int volume)
{
    setMasterVolume(volume, false);
}

void setMasterVolume(int volume)
{
    setMasterVolume(volume, true);
}

int getMusicVolume()
{
    return m_musicVolume;
}

void initMusicVolume(int volume)
{
    setMusicVolume(volume, false);
}

void setMusicVolume(int volume)
{
    setMusicVolume(volume, true);
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

static void channelFinished(int channel)
{
    if (!commenteryOnChannelFinished(channel) && !chantsOnChannelFinished(channel))
        logDebug("Channel %d finished playing", channel);
}

template <typename T>
static void setVolume(T& dest, int volume, const char *desc)
{
    if (volume < kMinVolume || volume > kMaxVolume)
        logWarn("Invalid value given for %s volume (%d), clamping", desc, volume);

    volume = std::min(volume, kMaxVolume);
    volume = std::max(volume, 0);

    dest = volume;
}

static void setMasterVolume(int volume, bool apply)
{
    setVolume(m_volume, volume, "master");

    if (apply && m_soundEnabled)
        Mix_Volume(-1, m_volume);
}

static void setMusicVolume(int volume, bool apply)
{
    setVolume(m_musicVolume, volume, "music");

    if (m_soundEnabled && m_musicEnabled)
        Mix_VolumeMusic(m_musicVolume);
}
