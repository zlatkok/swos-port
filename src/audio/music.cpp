#include "music.h"
#include "audio.h"
#include "file.h"
#include "log.h"
#include "util.h"
#include "options.h"

enum class State {
    kStart,
    kPlayingTitleSong,
    kTitleSongFadeOut,
    kPlayingMenuSong,
    kMenuSongFadeOut,
    kPlaybackError,
} static m_state = State::kStart;

static Mix_Music *m_titleMusic;
static Mix_Music *m_menuMusic;

static bool m_titleSongDone;

constexpr int kFadeOutMenuMusicLength = 1'800;

static bool noMusic();
static void playMenuSong();
static bool playTitleSong();

void initMusic()
{
    if (noMusic())
        return;

    static bool s_playedAtStart;

    ensureMenuAudioFrequency();

    if (!s_playedAtStart && (m_titleSongDone || !playTitleSong()))
        playMenuSong();

    s_playedAtStart = true;
}

void finishMusic()
{
    logInfo("Ending music");

    Mix_FreeMusic(m_menuMusic);
    m_menuMusic = nullptr;
}

// Initiates music fade and returns immediately.
void startFadingOutMusic()
{
    if (!noMusic()) {
        logInfo("Initiating music fade out");

        synchronizeMixVolume();
        Mix_FadeOutMusic(kFadeOutMenuMusicLength);
    }
}

// Blocks until the music fades out. Called when the match is about to start.
void waitForMusicToFadeOut()
{
    if (noMusic() || m_state == State::kPlaybackError)
        return;

    logInfo("Waiting for music to fade out...");

    if (Mix_FadingMusic() == MIX_FADING_OUT) {
        while (Mix_PlayingMusic())
            SDL_Delay(20);

        synchronizeSystemVolume();
    }

    // set up to try again
    m_state = State::kStart;
}

void synchronizeMixVolume()
{
#ifdef _WIN32
    // set mix volume according to the system volume
    //WAVEOUTCAPS caps;
    //if (::waveOutGetDevCaps(0, &caps, sizeof(caps)) == MMSYSERR_NOERROR && (caps.dwSupport & WAVECAPS_VOLUME)) {
    //    DWORD sysVolume;
    //    if (::waveOutGetVolume(nullptr, &sysVolume) == MMSYSERR_NOERROR) {
    //        int mixVolume = (sysVolume & 0xffff) * MIX_MAX_VOLUME / 0xffff;
    //        Mix_Volume(-1, mixVolume);
    //    }
    //}
#endif
}

void synchronizeSystemVolume()
{
#ifdef _WIN32
    // set the system volume according to mix volume
    int volume = getMusicVolume();
    DWORD sysVolume = volume * 0xffff / MIX_MAX_VOLUME;

    if (Mix_GetMusicType(nullptr) == MUS_MID)
        ::midiOutSetVolume(nullptr, sysVolume);
    else
        ::waveOutSetVolume(nullptr, MAKELONG(sysVolume, sysVolume));
#endif
}

// called each frame by MenuProc
void updateSongState()
{
    if (noMusic())
        return;

    if (m_state == State::kPlayingTitleSong && m_titleSongDone) {
        startMenuSong();
    } else if (m_state == State::kTitleSongFadeOut) {
        m_titleSongDone = !Mix_PlayingMusic();
        if (m_titleSongDone)
            startMenuSong();
    }
}

void stopTitleSong()
{
    if (noMusic())
        return;

    constexpr int kTitleSongFadeOutInterval = 1'200;

    if (m_state == State::kPlayingTitleSong) {
        Mix_FadeOutMusic(kTitleSongFadeOutInterval);
        m_state = State::kTitleSongFadeOut;
    }
}

// called once when a song needs to be played
void startMenuSong()
{
    if (noMusic())
        return;

    initMusic();

    if (m_state != State::kPlaybackError && m_state != State::kPlayingMenuSong && m_titleSongDone) {
        Mix_FreeMusic(m_titleMusic);
        m_titleMusic = nullptr;

        Mix_HookMusicFinished(nullptr);

        playMenuSong();
    }
}

void restartMusic()
{
    if (noMusic())
        return;

    if (m_state != State::kPlayingMenuSong)
        m_titleSongDone = true;

    startMenuSong();
}

static bool noMusic()
{
    return !soundEnabled() || !musicEnabled();
}

static std::pair<Mix_Music *, std::string> loadMixMusicFile(const char *name)
{
    std::string nameStr(name);
    nameStr += '.';

    for (const auto ext : { "mp3", "ogg", "wav", "flac", "mid", }) {
        auto filename = nameStr + ext;
        auto path = rootDir() + filename;

        auto music = Mix_LoadMUS(path.c_str());
        if (music)
            return { music, filename };
    }

    return {};
}

static Mix_Music *playMixSong(const char *basename, bool loop, Mix_Music *musicChunk = nullptr, void (onFinished)() = nullptr)
{
    static std::string filename;

    if (!musicChunk)
        std::tie(musicChunk, filename) = loadMixMusicFile(basename);

    if (musicChunk) {
        // OGG does a short pop when music starts playing again
        auto musicType = Mix_GetMusicType(musicChunk);
        bool isOgg = musicType == MUS_OGG;

        if (isOgg)
            Mix_VolumeMusic(0);

        logInfo("Playing %s music \"%s\"", basename, filename.c_str());
        Mix_PlayMusic(musicChunk, loop ? -1 : 0);

        if (!isOgg)
            Mix_VolumeMusic(getMusicVolume());

        synchronizeSystemVolume();

        Mix_HookMusicFinished(onFinished);

        if (isOgg) {
            SDL_Delay(200);
            Mix_VolumeMusic(getMusicVolume());
        }
    }

    return musicChunk;
}

static void playMenuSong()
{
    m_menuMusic = playMixSong("menu", true, m_menuMusic);

    if (m_menuMusic) {
        m_titleSongDone = true;
        m_state = State::kPlayingMenuSong;
    } else {
        logInfo("Couldn't find a suitable menu music file");
        m_state = State::kPlaybackError;
    }
}

// called each time when coming back from the match
void SWOS::TryMidiPlay()
{
    playMenuSong();
}

static bool playTitleSong()
{
    m_titleMusic = playMixSong("title", false, m_titleMusic, [] { m_titleSongDone = true; });
    m_state = State::kPlayingTitleSong;

    if (m_titleMusic) {
        return true;
    } else {
        logInfo("Couldn't find a suitable title music file");
        return false;
    }
}
