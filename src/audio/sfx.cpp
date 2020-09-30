#include "sfx.h"
#include "audio.h"
#include "file.h"

SfxSamplesArray m_sfxSamples;
static int m_crowdLoopChannel = -1;

void initSfxBeforeTheGame()
{
    m_crowdLoopChannel = -1;
}

SfxSamplesArray& sfxSamples()
{
    return m_sfxSamples;
}

static int playSfx(SfxSampleIndex index, int volume = MIX_MAX_VOLUME, int loopCount = 0)
{
    if (swos.g_soundOff)
        return -1;

    assert(index >= 0 && index < kNumSoundEffects);

    auto chunk = m_sfxSamples[index].chunk();
    if (chunk) {
        Mix_VolumeChunk(chunk, volume);
        return Mix_PlayChannel(-1, chunk, loopCount);
    } else {
        logWarn("Failed to load sound effect %d", index);
        return -1;
    }
}

void playCrowdNoiseSample()
{
    m_crowdLoopChannel = playSfx(kBackgroundCrowd, 100, -1);
}

void stopBackgroudCrowdNoise()
{
    if (m_crowdLoopChannel >= 0) {
        logDebug("Stopping background crowd noise loop");
        Mix_HaltChannel(m_crowdLoopChannel);
        m_crowdLoopChannel = -1;
    }
}

void SWOS::LoadSoundEffects()
{
    if (swos.g_soundOff)
        return;

    logInfo("Loading sound effects...");

    swos.goodPassTimer = 0;  // why is this here... >_<

    std::string prefix;
    bool hasAudioDir = dirExists(kAudioDir);
    if (hasAudioDir)
        prefix = std::string(kAudioDir) + getDirSeparator();

    int i = 0;
    for (auto p = swos.soundEffectsTable; *p != kSentinel; p++, i++) {
        if (m_sfxSamples[i].hasData())
            continue;

        auto filename = p->asPtr();
        if (hasAudioDir)
            filename += 4;  // skip "sfx\" prefix

        m_sfxSamples[i].loadFromFile(hasAudioDir ? (prefix + filename).c_str() : filename);
    }

    assert(i == m_sfxSamples.size());
}

void SWOS::PlayCrowdNoiseSample()
{
    // this is the first audio function to be called right before the game
    initGameAudio();

    if (swos.g_crowdChantsOn)
        playCrowdNoiseSample();
}

void SWOS::PlayMissGoalSample()
{
    playSfx(kMissGoal);
}

void SWOS::PlayHomeGoalSample()
{
    playSfx(kHomeGoal);
}

void SWOS::PlayAwayGoalSample()
{
    // playing same sample as home goal, but perfectly able to separate if needed
    playSfx(kHomeGoal);
}

void SWOS::PlayRefereeWhistleSample()
{
    playSfx(kWhistle, 42);
}

void SWOS::PlayEndGameWhistleSample()
{
    playSfx(kEndGameWhistle, 42);
}

void SWOS::PlayFoulWhistleSample()
{
    playSfx(kFoulWhistle, 42);
}

void SWOS::PlayKickSample()
{
    playSfx(kKick, 25);
}

void SWOS::PlayBallBounceSample()
{
    playSfx(kBounce, 42);
}
