#include "chants.h"
#include "audio.h"
#include "sfx.h"
#include "file.h"
#include "util.h"
#include "SoundSample.h"

constexpr int kChantsVolume = 55;

static SoundSample m_introChantSample;
static SoundSample m_resultSample;

static int m_fadeInChantTimer;
static int m_fadeOutChantTimer;
static int m_nextChantTimer;

enum ChantFunctionIndices {
    kPlayIntroChant, kPlayFansChant8l, kPlayFansChant10l, kPlayFansChant4l,
};
static std::array<void (*)(), 4> m_chants;

static void (*m_playCrowdChantsFunction)();

static int m_lastResultSampleIndex = -1;
static bool m_interestingGame;

static Mix_Chunk *m_chantsSample;
static int m_chantsChannel = -1;

static void setVolumeOrStopChants(int volume);

void initChantsBeforeTheGame()
{
    m_chantsChannel = -1;
    m_chantsSample = nullptr;

    m_resultSample.free();
    m_lastResultSampleIndex = -1;

    m_fadeInChantTimer = 0;
    m_fadeOutChantTimer = 0;
    m_nextChantTimer = 0;

    m_playCrowdChantsFunction = nullptr;
}

bool chantsOnChannelFinished(int channel)
{
    if (channel == m_chantsChannel) {
        logDebug("Chants finished playing on channel %d", channel);
        m_chantsChannel = -1;
        return true;
    }

    return false;
}

void playCrowdChants()
{
    if (swos.g_soundOff || !swos.g_crowdChantsOn || swos.g_trainingGame)
        return;

    if (m_fadeInChantTimer) {
        m_fadeInChantTimer--;
        int volume = (MIX_MAX_VOLUME - m_fadeInChantTimer) / 2;
        if (volume)
            setVolumeOrStopChants(volume);
    } else if (m_nextChantTimer) {
        m_nextChantTimer--;
    } else if (m_fadeOutChantTimer) {
        m_fadeOutChantTimer--;
        int volume = m_fadeOutChantTimer / 2;
        setVolumeOrStopChants(volume);
    } else if (getRandomInRange(0, 255) >= 128) {
        int pause = getRandomInRange(0, 255) * 2;
        m_nextChantTimer = pause;
    } else {
        if (!m_playCrowdChantsFunction || getRandomInRange(0, 255) >= 128) {
            // result chant will keep playing as long as the game is "interesting"
            int index = 2;
            if (!m_interestingGame)
                index = getRandomInRange(0, 3);

            logDebug("Picked chant function %d", index);
            m_playCrowdChantsFunction = m_chants[index];
        }

        assert(m_playCrowdChantsFunction);
        logDebug("Invoking crowd chant function");
        m_playCrowdChantsFunction();

        m_fadeInChantTimer = MIX_MAX_VOLUME;
        m_fadeOutChantTimer = MIX_MAX_VOLUME;

        m_nextChantTimer = getRandomInRange(0, 255) * 2 + 500;  // next chant: after 500-1012 ticks
    }
}

// Called when initializing the game loop.
void SWOS::LoadIntroChant()
{
    if (swos.g_soundOff)
        return;

    auto color = swos.leftTeamIngame.prShirtCol;
    assert(color < 16);

    static const int8_t kIntroTeamChantIndices[16] = {
        -1, -1, 3, -1, 0, 0, 0, 1, -1, 1, 2, 5, -1, 5, 1, 4
    };
    auto chantIndex = kIntroTeamChantIndices[color];

    std::string prefix;
    bool hasAudioDir = dirExists(kAudioDir);
    if (hasAudioDir)
        prefix = "audio"s + getDirSeparator();

    if (chantIndex >= 0) {
        assert(chantIndex < 6);
        auto introChantFile = swos.introTeamChantsTable[chantIndex].asPtr();
        m_introChantSample.free();

        logInfo("Picked intro chant %d `%s'", chantIndex, introChantFile);

        if (hasAudioDir)
            introChantFile += 4;    // skip "sfx\" prefix

        m_introChantSample.loadFromFile(hasAudioDir ? (prefix + introChantFile).c_str() : introChantFile);
        m_chants[kPlayIntroChant] = PlayIntroChantSample;
    } else {
        logInfo("Not playing intro chant");
        m_introChantSample.free();

        m_chants[kPlayIntroChant] = PlayFansChant4lSample;
    }

    m_chants[kPlayFansChant8l] = PlayFansChant8lSample;
    m_chants[kPlayFansChant10l] = PlayFansChant10lSample;
    m_chants[kPlayFansChant4l] = PlayFansChant4lSample;
}

static void playChant(Mix_Chunk *chunk)
{
    if (swos.g_soundOff || !swos.g_crowdChantsOn)
        return;

    if (chunk) {
        Mix_VolumeChunk(chunk, kChantsVolume);
        m_chantsChannel = Mix_PlayChannel(-1, chunk, -1);
        m_chantsSample = chunk;
    }
}

static void playChant(SfxSampleIndex index)
{
    if (swos.g_soundOff || !swos.g_crowdChantsOn)
        return;

    assert(index == kChant4l || index == kChant8l || index == kChant10l);

    logDebug("Playing chant index %d", index);
    auto& sample = sfxSamples()[index];
    auto chunk = sample.chunk();

    if (!chunk)
        logWarn("Sound effect %d failed to load");

    playChant(chunk);
}

void SWOS::PlayIntroChantSample()
{
    if (m_introChantSample.hasData()) {
        auto chunk = m_introChantSample.chunk();
        logDebug("Playing intro chant %#x", chunk);

        if (chunk)
            playChant(chunk);
        else
            logWarn("Failed to load intro chant");
    }
}

void SWOS::PlayFansChant4lSample()
{
    playChant(kChant4l);
}

void SWOS::PlayFansChant8lSample()
{
    playChant(kChant8l);
}

void SWOS::PlayFansChant10lSample()
{
    playChant(kChant10l);
}

static void stopChants()
{
    if (m_chantsChannel >= 0) {
        logDebug("Stopping the chants");
        Mix_HaltChannel(m_chantsChannel);
        m_chantsChannel = -1;
        m_chantsSample = nullptr;
    }
}

static void setVolumeOrStopChants(int volume)
{
    if (m_chantsChannel != -1) {
        volume &= 0xff;
        if (volume) {
            assert(m_chantsSample);
            Mix_VolumeChunk(m_chantsSample, volume);
        } else {
            stopChants();
        }
    }
}

void SWOS::EnqueueCrowdChantsReload()
{
    // fix SWOS bug where crowd chants only get reloaded when auto replays are on
    swos.loadCrowdChantSampleFlag = 1;
}

void SWOS::LoadCrowdChantSampleIfNeeded()
{
    // the code doesn't even get to test the flag unless the replay is started
    if (swos.loadCrowdChantSampleFlag) {
        LoadCrowdChantSample();
        swos.loadCrowdChantSampleFlag = 0;
    }
}

static int getChantSampleIndex();
static void loadChantSample(int sampleIndex);
static void fadeOutChantsIfGameTurnedBoring(bool wasInteresting);

void SWOS::LoadCrowdChantSample()
{
    if (swos.g_soundOff || !swos.g_commentary)
        return;

    bool wasInteresting = m_interestingGame;

    int sampleIndex = getChantSampleIndex();
    loadChantSample(sampleIndex);

    // a change from the original SWOS: we will reset chant function after each goal (SWOS would only reset if "interesting" game)
    // this is to prevent fans chanting "three nil" after the result has changed to 3-1
    m_playCrowdChantsFunction = nullptr;

    fadeOutChantsIfGameTurnedBoring(wasInteresting);
}

static int getChantSampleIndex()
{
    int sampleIndex = -1;

    if (!swos.statsTeam1Goals || !swos.statsTeam2Goals) {
        if (swos.statsTeam1Goals != swos.statsTeam2Goals) {
            // one team scored, other is losing with 0
            int goalDiff = std::max(swos.statsTeam1Goals, swos.statsTeam2Goals);
            if (goalDiff <= 6) {
                if (!getRandomInRange(0, 1)) {
                    // 50% chance of cheering up the losing team ;)
                    logDebug("Let's cheer up the losing team");
                    sampleIndex = 6;
                } else {
                    logDebug("Choosing %d-0 chant", goalDiff);
                    // otherwise pick x-nil chant sample, where x = [1..6]
                    sampleIndex = goalDiff - 1;
                }
            }
        }
    } else if (swos.statsTeam1Goals == swos.statsTeam2Goals) {
        // equalizer goal scored
        logDebug("Choosing equalizer chant");
        sampleIndex = 7;
    }

    return sampleIndex;
}

static void loadChantSample(int sampleIndex)
{
    if (sampleIndex >= 0) {
        assert(sampleIndex >= 0 && sampleIndex <= 7);

        stopChants();

        if (m_lastResultSampleIndex != sampleIndex) {
            m_resultSample.free();

            auto filename = swos.resultChantFilenames[sampleIndex].asPtr();

            std::string path;
            bool hasAudioDir = dirExists(kAudioDir);
            if (hasAudioDir)
                path = joinPaths("audio", "fx") + getDirSeparator() + (filename + 5);   // skip "hard\" part

            m_resultSample.loadFromFile(hasAudioDir ? path.c_str() : filename);
            m_lastResultSampleIndex = sampleIndex;
        }

        m_chants[kPlayFansChant10l] = SWOS::PlayResultSample;
        m_interestingGame = true;
    } else {
        // game not very interesting
        m_chants[kPlayFansChant10l] = SWOS::PlayFansChant10lSample;
        m_interestingGame = false;
    }
}

static void fadeOutChantsIfGameTurnedBoring(bool wasInteresting)
{
    if (wasInteresting && !m_interestingGame && m_chantsChannel >= 0 && Mix_Playing(m_chantsChannel)) {
        m_fadeOutChantTimer = m_fadeInChantTimer ? m_fadeInChantTimer : MIX_MAX_VOLUME;
        m_fadeInChantTimer = 0;
    }
}

void SWOS::PlayResultSample()
{
    auto chunk = m_resultSample.chunk();
    if (chunk)
        playChant(chunk);
    else
        logWarn("Failed to load result sample");
}

void SWOS::TurnCrowdChantsOn()
{
    m_chantsChannel = -1;
    m_fadeInChantTimer = 0;
    m_fadeOutChantTimer = 0;
    m_nextChantTimer = 0;
    swos.g_crowdChantsOn = 1;

    if (swos.screenWidth == kGameScreenWidth)
        playCrowdNoiseSample();
}

void SWOS::TurnCrowdChantsOff()
{
    swos.g_crowdChantsOn = 0;
    stopChants();
    stopBackgroudCrowdNoise();
}
