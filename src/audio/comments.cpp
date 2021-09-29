#include "comments.h"
#include "util.h"
#include "file.h"
#include "audio.h"
#include "chants.h"
#include "comments.h"
#include "SoundSample.h"
#include <dirent.h>

constexpr int kEnqueuedSampleDelay = 70;
constexpr int kEnqueuedCardSampleDelay = 100;

static SoundSample m_endGameCrowdSample;

static Mix_Chunk *m_lastPlayedComment;
static size_t m_lastPlayedCommentHash;
static int m_lastPlayedCategory;

static int m_commentaryChannel = -1;

static bool m_muteCommentary;

static bool m_performingPenalty;

static int m_tacticsChangedSampleTimer;
static int m_substituteSampleTimer;
static int m_playingYellowCardTimer;
static int m_playingRedCardTimer;

static void playGoodPassComment();
static void playYellowCardSample();
static void playRedCardSample();
static void playCornerSample();
static void playTacticsChangeSample();
static void playThrowInSample();
static void playSubstituteSample();
static void loadAndPlayEndGameCrowdSample(int index);
static void playDrawComment();
static void playSensationalGameComment();
static void playItsBeenACompleteRoutComment();

//
// sample loading
//

struct SampleTable {
    const char *dir;
    std::vector<SoundSample> samples;
    int lastPlayedIndex = -1;
    int totalSampleChance = 0;

    SampleTable(const char *dir) : dir(dir) {}

    void reset() {
        samples.clear();
        lastPlayedIndex = -1;
        totalSampleChance = 0;
    }

    unsigned getRandomSampleIndex() const {
        assert(totalSampleChance > 0 && totalSampleChance >= static_cast<int>(samples.size()));

        int randomSampleChance = getRandomInRange(0, std::max(0, totalSampleChance - 1));

        unsigned index = 0;
        for (; index < samples.size(); index++) {
            randomSampleChance -= samples[index].chanceModifier();
            if (randomSampleChance < 0)
                break;
        }

        assert(index < samples.size());
        return index;
    }
};

// these must match exactly with directories in the sample tables below
enum CommentarySampleTableIndex {
    kCorner, kDirtyTackle, kEndGameRout, kEndGameSensational, kEndGameSoClose, kFoul, kGoal, kGoodPass, kGoodTackle,
    kHeader, kHitBar, kHitPost, kInjury, kKeeperClaimed, kKeeperSaved, kNearMiss, kOwnGoal, kPenalty, kPenaltyMissed,
    kPenaltySaved, kPenaltyGoal, kRedCard, kSubstitution, kChangeTactics, kThrowIn, kYellowCard,
    kNumSampleTables,
};

// all the categories of comments heard in the game
static std::array<SampleTable, kNumSampleTables> m_sampleTables = {{
    { "corner" },
    { "dirty_tackle" },
    { "end_game_rout" },
    { "end_game_sensational" },
    { "end_game_so_close" },
    { "free_kick" },
    { "goal" },
    { "good_play" },
    { "good_tackle" },
    { "header" },
    { "hit_bar" },
    { "hit_post" },
    { "injury" },
    { "keeper_claimed" },
    { "keeper_saved" },
    { "near_miss" },
    { "own_goal" },
    { "penalty" },
    { "penalty_missed" },
    { "penalty_saved" },
    { "penalty_scored" },
    { "red_card" },
    { "substitution" },
    { "tactic_changed" },
    { "throw_in" },
    { "yellow_card" },
}};

static void loadCustomCommentary();

void loadCommentary()
{
    if (!soundEnabled() || !commentaryEnabled() || swos.g_trainingGame)
        return;

    logInfo("Loading commentary...");

    loadCustomCommentary();

    m_muteCommentary = !commentaryEnabled();
}

void loadAndPlayEndGameComment()
{
    if (!soundEnabled() || !commentaryEnabled())
        return;

    int index;
    // weird way of comparing, might it be like this because of team positions?
    if (swos.team1GoalsDigit2 == swos.team2GoalsDigit2)
        index = 2;
    else if (swos.team2GoalsDigit2 < swos.team1GoalsDigit2)
        index = 0;
    else
        index = 1;

    loadAndPlayEndGameCrowdSample(index);

    int goalDiff = std::abs(swos.statsTeam1Goals - swos.statsTeam2Goals);

    if (goalDiff >= 3)
        playItsBeenACompleteRoutComment();
    else if (!goalDiff)
        playDrawComment();
    else if (swos.statsTeam1Goals + swos.statsTeam2Goals >= 4)
        playSensationalGameComment();
}

void clearCommentsSampleCache()
{
    for (auto& table : m_sampleTables)
        table.reset();
}

void initCommentsBeforeTheGame()
{
    m_commentaryChannel = -1;

    m_lastPlayedCategory = -1;
    m_lastPlayedComment = nullptr;

    m_endGameCrowdSample.free();
}

void enqueueTacticsChangedSample()
{
    m_tacticsChangedSampleTimer = kEnqueuedSampleDelay;
}

void enqueueSubstituteSample()
{
    m_substituteSampleTimer = kEnqueuedSampleDelay;
}

void enqueueYellowCardSample()
{
    m_playingYellowCardTimer = kEnqueuedCardSampleDelay;
}

void enqueueRedCardSample()
{
    m_playingRedCardTimer = kEnqueuedCardSampleDelay;
}

void playEnqueuedSamples()
{
    if (m_playingYellowCardTimer >= 0 && !--m_playingYellowCardTimer) {
        playYellowCardSample();
        m_playingYellowCardTimer = -1;
    } else if (m_playingRedCardTimer >= 0 && !--m_playingRedCardTimer) {
        playRedCardSample();
        m_playingRedCardTimer = -1;
    } else if (swos.playingGoodPassTimer >= 0 && !--swos.playingGoodPassTimer) {
        playGoodPassComment();
        swos.playingGoodPassTimer = -1;
    } else if (swos.playingThrowInSample >= 0 && !--swos.playingThrowInSample) {
        playThrowInSample();
        swos.playingThrowInSample = -1;
    } else if (swos.playingCornerSample >= 0 && !--swos.playingCornerSample) {
        playCornerSample();
        swos.playingCornerSample = -1;
    } else if (m_substituteSampleTimer >= 0 && !--m_substituteSampleTimer) {
        playSubstituteSample();
        m_substituteSampleTimer = -1;
    } else if (m_tacticsChangedSampleTimer >= 0 && !--m_tacticsChangedSampleTimer) {
        playTacticsChangeSample();
        m_tacticsChangedSampleTimer = -1;
    }

    // strange place to decrement this...
    if (swos.goalCounter)
        swos.goalCounter--;

    playCrowdChants();
}

bool commenteryOnChannelFinished(int channel)
{
    if (channel == m_commentaryChannel) {
        logDebug("Commentary finished playing on channel %d", channel);
        m_commentaryChannel = -1;
        return true;
    }

    return false;
}

void toggleMuteCommentary()
{
    m_muteCommentary = !m_muteCommentary;
}

static int parseSampleChanceMultiplier(const char *str, size_t len)
{
    assert(str);
    assert(len == strlen(str));

    int frequency = 1;

    if (len >= 4) {
        auto end = str + len - 1;

        for (auto p = end; p >= str; p--) {
            if (*p == '.') {
                end = p - 1;
                break;
            }
        }

        if (end - str >= 4 && isdigit(*end)) {
            int value = 0;
            int power = 1;
            auto digit = end;

            while (digit >= str && isdigit(*digit)) {
                value += power * (*digit - '0');
                power *= 10;
                digit--;
            }

            if (digit >= str + 2 && *digit == 'x' && (digit[-1] == '_' || digit[-1] == '-'))
                frequency = value;
        }
    }

    return frequency;
}

static void loadCustomCommentary()
{
    assert(m_sampleTables.size() == kNumSampleTables);
    assert(!strcmp(m_sampleTables[kEndGameSoClose].dir, "end_game_so_close"));
    assert(!strcmp(m_sampleTables[kYellowCard].dir, "yellow_card"));

    const std::string audioPath = joinPaths("audio", "commentary") + getDirSeparator();

    for (int i = 0; i < kNumSampleTables; i++) {
        auto& table = m_sampleTables[i];
        table.lastPlayedIndex = -1;

        if (!table.samples.empty())
            continue;

        const auto& dirPath = audioPath + table.dir + getDirSeparator();
        const auto& dirFullPath = pathInRootDir((audioPath + table.dir).c_str());
        auto dir = opendir(dirFullPath.c_str());

        if (dir) {
            for (dirent *entry; entry = readdir(dir); ) {
#ifdef _WIN32
                auto len = entry->d_namlen;
#else
                auto len = strlen(entry->d_name);
#endif

                if (len < 4)
                    continue;

                auto samplePath = dirPath + entry->d_name;
                int chance = parseSampleChanceMultiplier(entry->d_name, len);
                SoundSample sample(samplePath.c_str(), chance);

                if (sample.hasData()) {
                    if (std::find(table.samples.begin(), table.samples.end(), sample) == table.samples.end()) {
                        table.samples.push_back(std::move(sample));
                        table.totalSampleChance += chance;
#ifndef DEBUG
                        logInfo("`%s' loaded OK, chance: %d", samplePath.c_str(), chance);
#endif
                    } else {
                        logInfo("Duplicate detected, rejecting: `%s'", samplePath.c_str());
                    }
                } else {
                    logWarn("Failed to load sample `%s'", samplePath.c_str());
                }
            }

            closedir(dir);
        }
    }
}

static bool commentPlaying()
{
    return m_commentaryChannel >= 0 && Mix_Playing(m_commentaryChannel);
}

static void playComment(Mix_Chunk *chunk, bool interrupt = true)
{
    assert(chunk);

    if (commentPlaying()) {
        if (!interrupt) {
            logDebug("Playing comment aborted since the previous comment is still playing");
            return;
        }

        logDebug("Interrupting previous comment");
        Mix_HaltChannel(m_commentaryChannel);
    }

    Mix_VolumeChunk(chunk, MIX_MAX_VOLUME);
    m_commentaryChannel = Mix_PlayChannel(-1, chunk, 0);

    m_lastPlayedComment = chunk;
    m_lastPlayedCommentHash = hash(chunk->abuf, chunk->alen);
}

static bool isLastPlayedComment(SoundSample& sample)
{
    assert(sample.isChunkLoaded());

    if (m_lastPlayedComment) {
        auto chunk = sample.chunk();
        return sample.hash() == m_lastPlayedCommentHash && chunk->alen == m_lastPlayedComment->alen &&
            !memcmp(chunk->abuf, m_lastPlayedComment->abuf, chunk->alen);
    }

    return false;
}

static bool fixIndexToSkipLastPlayedComment(unsigned& sampleIndex, unsigned lastPlayedIndex, std::vector<SoundSample>& samples)
{
     if (samples.size() > 1 && (lastPlayedIndex == sampleIndex || isLastPlayedComment(samples[sampleIndex]))) {
         sampleIndex = (sampleIndex + 1) % samples.size();
         return true;
     }

     // theoretically, chosen comment might have been just played in a different category, and then we might bump into the one that
     // was last played from this category (it wouldn't be that bad since at least one comment was in between but let's be safe...)
     if (samples.size() > 2 && lastPlayedIndex == sampleIndex) {
         sampleIndex = (sampleIndex + 1) % samples.size();
         return true;
     }

     return false;
}

static void playComment(CommentarySampleTableIndex tableIndex, bool interrupt = true)
{
    if (!soundEnabled() || !commentaryEnabled() || m_muteCommentary || swos.g_trainingGame)
        return;

    auto& table = m_sampleTables[tableIndex];
    auto& samples = table.samples;

    if (!samples.empty()) {
        auto sampleIndex = table.getRandomSampleIndex();

        while (!samples.empty()) {
            auto chunk = samples[sampleIndex].chunk();

            if (!chunk) {
                logWarn("Failed to load comment %d from category %d", sampleIndex, tableIndex);
                samples.erase(samples.begin() + sampleIndex);
                table.lastPlayedIndex -= table.lastPlayedIndex >= static_cast<int>(sampleIndex);
                sampleIndex -= sampleIndex == samples.size();
                continue;
            }

            if (fixIndexToSkipLastPlayedComment(sampleIndex, table.lastPlayedIndex, samples))
                continue;

            logDebug("Playing comment %d from category %d", sampleIndex, tableIndex);
            playComment(chunk, interrupt);
            table.lastPlayedIndex = sampleIndex;
            m_lastPlayedCategory = tableIndex;
            break;
        }
    } else {
        logWarn("Comment table %d empty", tableIndex);
    }
}

#ifdef __ANDROID__
static void vibrate()
{
    if (auto env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv())) {
        if (auto activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity())) {
            if (auto swosClass = env->FindClass("com/sensible_portware/SWOS")) {
                if (auto vibrateMethodId = env->GetMethodID(swosClass, "vibrate", "()V"))
                    env->CallVoidMethod(activity, vibrateMethodId);
                env->DeleteLocalRef(swosClass);
            }
            env->DeleteLocalRef(activity);
        }
    }
}
#endif

//
// SWOS sound hooks
//

// in:
//     A6 -> player's team
//
void SWOS::PlayHeaderComment()
{
    auto team = A6.as<TeamGeneralInfo *>();
    assert(team);

    if (team->playerNumber)
        playComment(kHeader, false);
}

// in:
//     A6 -> player's team
//
void SWOS::PlayInjuryComment()
{
    auto team = A6.as<TeamGeneralInfo *>();
    assert(team);

    if (team->playerNumber)
        playComment(kInjury);
}

static void playPenaltyGoalComment()
{
    playComment(kPenaltyGoal);
    m_performingPenalty = false;
}

static void playPenaltyMissComment()
{
    playComment(kPenaltyMissed);
    m_performingPenalty = false;
}

static void playPenaltySavedComment()
{
    playComment(kPenaltySaved);
    m_performingPenalty = false;
}

void SWOS::PlayPenaltyComment()
{
    m_performingPenalty = true;
    playComment(kPenalty);
}

void SWOS::PlayFoulComment()
{
    playComment(kFoul);
}

void SWOS::PlayDangerousPlayComment()
{
    playComment(kDirtyTackle);
}

static void playGoodPassComment()
{
    playComment(kGoodPass, false);
}

void SWOS::PlayGoodTackleComment()
{
    playComment(kGoodTackle, false);
}

void SWOS::PlayPostHitComment()
{
    playComment(kHitPost);
}

void SWOS::PlayBarHitComment()
{
    playComment(kHitBar);
}

void SWOS::PlayKeeperClaimedComment()
{
    if (m_performingPenalty) {
        playComment(kPenaltySaved);
        m_performingPenalty = false;
    } else {
        // don't interrupt penalty saved comments
        if (m_lastPlayedCategory != kPenaltySaved || !commentPlaying())
            playComment(kKeeperClaimed);
    }
}

void SWOS::PlayNearMissComment()
{
    if (m_performingPenalty || swos.penaltiesState == -1)
        playPenaltyMissComment();
    else
        playComment(kNearMiss);
}

void SWOS::PlayGoalkeeperSavedComment()
{
    if (m_performingPenalty || swos.penaltiesState == -1)
        playPenaltySavedComment();
    else
        playComment(kKeeperSaved);
}

// fix original SWOS bug where penalty flag remains set when penalty is missed, but it's not near miss
void SWOS::FixPenaltyBug()
{
    m_performingPenalty = 0;
}

void SWOS::PlayOwnGoalComment()
{
    playComment(kOwnGoal);
#ifdef __ANDROID__
    vibrate();
#endif
}

void SWOS::PlayGoalComment()
{
    if (m_performingPenalty || swos.penaltiesState == -1)
        playPenaltyGoalComment();
    else
        playComment(kGoal);
#ifdef __ANDROID__
    vibrate();
#endif
}

static void playYellowCardSample()
{
    playComment(kYellowCard);
}

static void playRedCardSample()
{
    playComment(kRedCard);
}

static void playCornerSample()
{
    playComment(kCorner);
}

static void playTacticsChangeSample()
{
    playComment(kChangeTactics);
}

static void playThrowInSample()
{
    playComment(kThrowIn);
}

static void playSubstituteSample()
{
    playComment(kSubstitution);
}

static void loadAndPlayEndGameCrowdSample(int index)
{
    if (swos.g_trainingGame || !commentaryEnabled())
        return;

    assert(index >= 0 && index <= 3);

    // no need to worry about this still being played
    m_endGameCrowdSample.free();

    auto filename = swos.endGameCrowdSamples[index].asPtr();

    std::string path;
    path = joinPaths("audio", "fx") + getDirSeparator() + (filename + 5);   // skip "hard\" part
    m_endGameCrowdSample.loadFromFile(path.c_str());

    auto chunk = m_endGameCrowdSample.chunk();
    if (chunk) {
        Mix_VolumeChunk(chunk, MIX_MAX_VOLUME);
        Mix_PlayChannel(-1, chunk, 0);
    } else {
        logWarn("Failed to load end game sample %s", path.c_str());
    }
}

static void playDrawComment()
{
    playComment(kEndGameSoClose);
}

static void playSensationalGameComment()
{
    playComment(kEndGameSensational);
}

static void playItsBeenACompleteRoutComment()
{
    playComment(kEndGameRout);
}
