#include "music.h"
#include "audio.h"
#include "file.h"
#include "log.h"
#include "util.h"
#include "options.h"
#include <adlmidi.h>

#ifdef _WIN32
#pragma comment(lib, "winmm.lib")
#endif

static Mix_Music *m_titleMusic;
static Mix_Music *m_menuMusic;

static bool m_titleSongDone;
static bool m_playingMenuSong;
static bool m_failed;

static std::atomic<ADL_MIDIPlayer *> m_adlPlayer;
static std::atomic<bool> m_adlPlayerActive;
static std::thread m_adlPrefetchThread;

static std::atomic<int> m_adlBufferStart;
static std::atomic<int> m_adlBufferEnd;
static std::atomic<int> m_adlSongDone;

constexpr int kFadeOutMusicLength = 1'800;
static std::atomic<Uint32> m_startFadeMusicTicks;
static std::atomic<bool> m_midiMuted;

constexpr int kMidiNumBufferedChunks = 32;
static int16_t m_adlBuffer[kMidiNumBufferedChunks][kMenuChunkSize * 2];

static void finishAdl()
{
    if (m_adlPlayer.load()) {
        m_adlPlayerActive = false;

        if (m_adlPrefetchThread.joinable())
            m_adlPrefetchThread.join();

        Mix_HookMusic(nullptr, nullptr);

        if (m_adlPlayer.load()) {
            adl_close(m_adlPlayer);
            m_adlPlayer = nullptr;
        }
    }
}

void finishMusic()
{
    logInfo("Ending music");

    Mix_FreeMusic(m_menuMusic);
    m_menuMusic = nullptr;

    finishAdl();
}

static bool noMusic()
{
    return g_soundOff || g_musicOff || !g_menuMusic;
}

void fadeOutMusic()
{
    if (noMusic() || m_failed)
        return;

    if (m_adlPlayerActive) {
        while (m_startFadeMusicTicks)
            SDL_Delay(20);
        finishAdl();
    } else {
        if (Mix_FadingMusic() == MIX_FADING_OUT) {
            while (Mix_PlayingMusic())
                SDL_Delay(20);

            synchronizeSystemVolume();
        }
    }

    m_startFadeMusicTicks = 0;

    // set up to try again
    m_failed = false;
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

// keep prefetching chunks from ADL as fast as possible to keep the MIDI buffer filled to the max
static void adlPrefetch(bool loop)
{
    while (m_adlPlayerActive) {
        assert(m_adlBufferStart >= 0 && m_adlBufferStart < kMidiNumBufferedChunks &&
            m_adlBufferEnd >= 0 && m_adlBufferEnd < kMidiNumBufferedChunks);

        while ((m_adlBufferEnd + 1) % kMidiNumBufferedChunks == m_adlBufferStart) {
            // the buffer is full
            SDL_Delay(50);
            if (!m_adlPlayerActive)
                return;
        }

        m_adlSongDone = adl_atEnd(m_adlPlayer);
        if (m_adlSongDone && !loop)
            break;

        int actualSampleSize = adl_play(m_adlPlayer, 2 * kMenuChunkSize, m_adlBuffer[m_adlBufferEnd]);

        if (actualSampleSize < 2 * kMenuChunkSize) {
            if (loop) {
                adl_positionRewind(m_adlPlayer);
                adl_play(m_adlPlayer, 2 * kMenuChunkSize - actualSampleSize, m_adlBuffer[m_adlBufferEnd] + actualSampleSize);
            } else if (actualSampleSize) {
                memset(&m_adlBuffer[m_adlBufferEnd] + actualSampleSize, 0, (2 * kMenuChunkSize - actualSampleSize) * sizeof(int16_t));
            } else {
                m_adlSongDone = true;
                break;
            }
        }

        m_adlBufferEnd = (m_adlBufferEnd + 1) % kMidiNumBufferedChunks;
    }
}

static void initAdl()
{
    m_adlPlayer = adl_init(kMenuFrequency);

    if (!m_adlPlayer.load())
        errorExit("Couldn't initialize ADLMIDI: %s", adl_errorString());

    m_adlBufferStart = 0;
    m_adlBufferEnd = 0;
    m_adlSongDone = false;
    m_startFadeMusicTicks = 0;
    m_midiMuted = false;

    m_adlPlayerActive = true;

    adl_setDebugMessageHook(m_adlPlayer, [](void *, const char *format, ...) {
        va_list args;
        va_start(args, format);
        logv(kWarning, format, args);
        va_end(args);
    }, nullptr);

    adl_setBank(m_adlPlayer, midiBankNumber());

    logInfo("ADLMIDI initialized");
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

    return { nullptr, std::string() };
}

static bool fadeOutAdl(Uint8 *stream, int sampleCount)
{
    if (m_startFadeMusicTicks) {
        auto now = SDL_GetTicks();
        if (!m_midiMuted && now <= m_startFadeMusicTicks + kFadeOutMusicLength) {
            int volume = (kFadeOutMusicLength - (now - m_startFadeMusicTicks)) * getMusicVolume() / kFadeOutMusicLength;
            assert(volume >= 0 && volume <= MIX_MAX_VOLUME);

            auto dest = reinterpret_cast<int16_t *>(stream);

            for (int i = 0; i < sampleCount; i++)
                dest[i] = static_cast<int>(m_adlBuffer[m_adlBufferStart][i]) * volume / MIX_MAX_VOLUME;
        } else {
            m_startFadeMusicTicks = 0;
            m_midiMuted = true;
            memset(stream, 0, sampleCount * 2);
        }

        return true;
    }

    return false;
}

static void adlCustomHook(void *data, Uint8 *stream, int len)
{
    auto midiPlayer = reinterpret_cast<ADL_MIDIPlayer *>(data);

    int sampleCount = std::min(len / 2, 2 * kMenuChunkSize);
    assert(2 * sampleCount == sizeof(m_adlBuffer[m_adlBufferStart]));

    if (m_adlBufferStart != m_adlBufferEnd) {
        if (!fadeOutAdl(stream, sampleCount)) {
            int volume = getMusicVolume();
            assert(volume >= 0 && volume <= MIX_MAX_VOLUME);

            auto dest = reinterpret_cast<int16_t *>(stream);

            for (int i = 0; i < sampleCount; i++)
                dest[i] = static_cast<int>(m_adlBuffer[m_adlBufferStart][i]) * volume / MIX_MAX_VOLUME;
        }

        m_adlBufferStart = (m_adlBufferStart + 1) % kMidiNumBufferedChunks;
    } else {
        // aw no data ready :/
        memset(stream, 0, len);
    }
}

static bool loadXmi(const char *filename, bool loop)
{
    if (!m_adlPlayer.load())
        initAdl();

    auto path = rootDir() + filename;
    if (adl_openFile(m_adlPlayer, path.c_str()) >= 0) {
        // menu song plays too fast for some reason, this will make it approximately the same
        adl_setTempo(m_adlPlayer, 0.6);

        assert(!m_adlPrefetchThread.joinable());
        m_adlPrefetchThread = std::thread(adlPrefetch, loop);

        // give prefetch thread a small advantage
        SDL_Delay(25);

        Mix_HookMusic(adlCustomHook, m_adlPlayer);

        return true;
    }

    return false;
}

void SWOS::CDROM_StopAudio()
{
    if (!m_playingMenuSong) {
        m_titleSongDone = true;
        g_midiPlaying = false;
    }
}

static int cdromStatus()
{
    if (m_adlSongDone)
        m_titleSongDone = true;

    return m_titleSongDone ? 0 : 0x200;
}

__declspec(naked) void SWOS::IOCTL_CDROM_Read()
{
    __asm jmp cdromStatus
}

static void playXmi()
{
    if (!m_adlPlayer.load())
        initAdl();

    assert(Mix_GetMusicHookData() == m_adlPlayer);

    if (adl_totalTimeLength(m_adlPlayer) > 0)
        SDL_PauseAudio(0);
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
        bool isMidi = musicType == MUS_MID;

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

static bool playTitleSong()
{
    m_titleMusic = playMixSong("title", false, m_titleMusic, [] { m_titleSongDone = true; });

    if (m_titleMusic)
        return true;

    assert(!m_adlPlayer.load());

    for (auto song : { "title.xmi", aSwos_xmi }) {
        if (loadXmi(song, false)) {
            logInfo("Playing title music \"%s\"", song);
            playXmi();
            return true;
        }
    }

    logInfo("Couldn't find a suitable title music file");
    return false;
}

static void playMenuSong()
{
    m_playingMenuSong = true;

    finishAdl();

    m_menuMusic = playMixSong("menu", true, m_menuMusic);

    if (!m_menuMusic) {
        if (loadXmi(aMenu_xmi, true)) {
            logInfo("Playing menu music \"%s\"", aMenu_xmi);
            playXmi();
        } else {
            logInfo("Couldn't find a suitable menu music file");
            m_playingMenuSong = false;
            m_failed = true;
        }
    }
}

// called at startup, when initializing main menu
void SWOS::InitMenuMusic()
{
    if (noMusic())
        return;

    ensureMenuAudioFrequency();

    if (m_titleSongDone || !playTitleSong())
        playMenuSong();

    cdAudioPlaying = -1;
}

// called once when a song needs to be played
void SWOS::PlayMidi()
{
    if (noMusic())
        return;

    if (!m_failed && m_titleSongDone) {
        Mix_FreeMusic(m_titleMusic);
        m_titleMusic = nullptr;
        Mix_HookMusicFinished(nullptr);

        finishAdl();

        playMenuSong();
    }
}

void SWOS::StopMidiSequence()
{
    if (SDL_GetTicks() > m_startFadeMusicTicks + kFadeOutMusicLength) {
        finishAdl();
        Mix_HaltMusic();
    }
}

void SWOS::FadeOutXmidi()
{
    if (!noMusic()) {
        logInfo("Fading out music...");
        synchronizeMixVolume();
        Mix_FadeOutMusic(kFadeOutMusicLength);
        assert(!m_startFadeMusicTicks);
        m_startFadeMusicTicks = SDL_GetTicks();
    }
}
