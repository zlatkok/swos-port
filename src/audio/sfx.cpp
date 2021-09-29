#include "sfx.h"
#include "audio.h"
#include "chants.h"
#include "file.h"
#include "replays.h"

SfxSamplesArray m_sfxSamples;
static int m_crowdLoopChannel = -1;

void loadSoundEffects()
{
    if (!soundEnabled())
        return;

    logInfo("Loading sound effects...");

    swos.goodPassTimer = 0;  // why is this here... >_<

    auto prefix = std::string(kAudioDir) + getDirSeparator();

    int i = 0;
    for (auto p = swos.soundEffectsTable; *p != kSentinel; p++, i++) {
        if (m_sfxSamples[i].hasData())
            continue;

        auto filename = p->asPtr() + 4; // skip "sfx\" prefix
        m_sfxSamples[i].loadFromFile((prefix + filename).c_str());
    }

    assert(i == m_sfxSamples.size());
}

void clearSfxSamplesCache()
{
    for (auto& sample : m_sfxSamples)
        sample.free();
}

void initSfxBeforeTheGame()
{
    m_crowdLoopChannel = -1;
}

SfxSamplesArray& sfxSamples()
{
    return m_sfxSamples;
}

static bool isCrowdSample(SfxSampleIndex index)
{
    return index == kBackgroundCrowd || index == kHomeGoal || index == kMissGoal || index == kChant4l ||
        index == kChant10l || index == kChant8l;
}

static int playSfx(SfxSampleIndex index, int volume = MIX_MAX_VOLUME, int loopCount = 0, bool saveForHighlights = true)
{
    if (!soundEnabled() || swos.g_trainingGame && isCrowdSample(index))
        return -1;

    assert(index >= 0 && index < kNumSoundEffects);

    if (saveForHighlights)
        saveSfxForHighlights(index, volume);

    auto chunk = m_sfxSamples[index].chunk();
    if (chunk) {
        Mix_VolumeChunk(chunk, volume);
        return Mix_PlayChannel(-1, chunk, loopCount);
    } else {
        logWarn("Failed to load sound effect %d", index);
        return -1;
    }
}

void playCrowdNoise()
{
    if (areCrowdChantsEnabled() && !swos.g_trainingGame)
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

void playEndGameWhistleSample()
{
    playSfx(kEndGameWhistle, 42);
}

void playSfx(int sample, int volume)
{
    if (static_cast<size_t>(sample) < kNumSoundEffects)
        playSfx(static_cast<SfxSampleIndex>(sample), volume, 0, false);
    else
        logWarn("Got invalid SFX sample %d", sample);
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
