#include "comments.h"
#include "util.h"
#include "file.h"
#include "audio.h"
#include "chants.h"
#include "SoundSample.h"
#include <dirent.h>

static std::array<SoundSample, 76 + 52> m_originalCommentarySamples;

static bool m_hasAudioDir;

static SoundSample m_endGameCrowdSample;

static Mix_Chunk *m_lastPlayedComment;
static size_t m_lastPlayedCommentHash;
static int m_lastPlayedCategory;

static int m_commentaryChannel = -1;

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

bool commenteryOnChannelFinished(int channel)
{
    if (channel == m_commentaryChannel) {
        logDebug("Commentary finished playing on channel %d", channel);
        m_commentaryChannel = -1;
        return true;
    }

    return false;
}

SwosDataPointer<const char> *getOnDemandSampleTable()
{
    // unique samples used in tables of on-demand comment filenames
    static SwosDataPointer<const char> kOnDemandSamples[] = {
        swos.aHardM10_v__raw, swos.aHardM10_w__raw, swos.aHardM10_y__raw, swos.aHardM313_1__ra, swos.aHardM313_2__ra, swos.aHardM313_3__ra,
        swos.aHardM10_5__raw, swos.aHardM10_7__raw, swos.aHardM10_8__raw, swos.aHardM10_9__raw, swos.aHardM10_a__raw, swos.aHardM10_b__raw,
        swos.aHardM233_j__ra, swos.aHardM233_k__ra, swos.aHardM233_l__ra, swos.aHardM233_m__ra, swos.aHardM10_3__raw, swos.aHardM10_4__raw,
        swos.aHardM196_8__ra, swos.aHardM196_9__ra, swos.aHardM196_a__ra, swos.aHardM196_b__ra, swos.aHardM196_c__ra, swos.aHardM196_d__ra,
        swos.aHardM196_e__ra, swos.aHardM196_f__ra, swos.aHardM196_g__ra, swos.aHardM196_h__ra, swos.aHardM196_i__ra, swos.aHardM196_j__ra,
        swos.aHardM406_f__ra, swos.aHardM406_g__ra, swos.aHardM406_h__ra, swos.aHardM406_i__ra, swos.aHardM406_j__ra, swos.aHardM443_7__ra,
        swos.aHardM443_8__ra, swos.aHardM443_9__ra, swos.aHardM443_a__ra, swos.aHardM443_b__ra, swos.aHardM443_c__ra, swos.aHardM443_d__ra,
        swos.aHardM443_e__ra, swos.aHardM443_f__ra, swos.aHardM443_g__ra, swos.aHardM443_h__ra, swos.aHardM443_i__ra, swos.aHardM443_j__ra,
        swos.aHardM406_3__ra, swos.aHardM406_8__ra, swos.aHardM406_7__ra, swos.aHardM406_9__ra,
        kSentinel,
    };

    return kOnDemandSamples;
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

static void mapOriginalSamples()
{
    // save double chance sample indices since they'll get moved from original samples table
    std::vector<std::tuple<CommentarySampleTableIndex, int, int>> doubleChanceSamples;
    doubleChanceSamples.reserve(20);

    std::array<std::pair<CommentarySampleTableIndex, int>, m_originalCommentarySamples.size()> movedSamples;
    // -1 = uninitialized, -2 = failed
    movedSamples.fill({ kNumSampleTables, -1 });

    auto isSampleMoved = [&](int i) { return movedSamples[i].first < kNumSampleTables && movedSamples[i].second >= 0; };
    auto didSampleFailToLoad = [&](int i) { return movedSamples[i].second == -2; };
    auto markSampleAsFailed = [&](int i) { movedSamples[i].second = -2; };

    struct {
        CommentarySampleTableIndex tableIndex;
        std::pair<int, int> sampleIndexRange;
        std::pair<int, int> doubleChanceSamples;
    } static const kSampleMapping[] = {
        // preloaded comments
        { kGoal, { 0, 6 }, { 0, 2 }, },
        { kKeeperSaved, { 6, 13 }, { 6, 7 }, },
        { kOwnGoal, { 13, 19 }, { 15, 17 }, },
        { kNearMiss, { 19, 21 } },
        { kNearMiss, { 28, 30 } },
        { kKeeperClaimed, { 21, 28 } },
        { kHitPost, { 28, 36 } },
        { kHitBar, { 36, 38 } },
        { kGoodTackle, { 38, 42 } },
        { kGoodPass, { 42, 46 } },
        { kDirtyTackle, { 46, 49 }, { 46, 47 }, },
        { kFoul, { 46, 53 }, { 46, 47 }, },
        { kPenalty, { 53, 57 } },
        { kPenaltySaved, { 57, 60 } },
        { kPenaltySaved, { 6, 8 } },
        { kPenaltySaved, { 9, 10 } },
        { kPenaltySaved, { 11, 13 } },
        { kPenaltyMissed, { 60, 64 } },
        { kPenaltyGoal, { 64, 68 } },
        { kHeader, { 68, 76 } },
        // on-demand comments
        { kCorner, { 76, 82 }, { 77, 79 }, },
        { kChangeTactics, { 82, 88 }, { 82, 84 }, },
        { kSubstitution, { 88, 94 }, { 88, 90 }, },
        { kRedCard, { 94, 106 }, { 94, 98 }, },
        { kEndGameSoClose, { 106, 108 } },
        { kEndGameRout, { 108, 109 } },
        { kEndGameSensational, { 109, 111 } },
        { kYellowCard, { 111, 124 }, { 111, 114 }, },
        { kThrowIn, { 124, 128 } },
    };

    for (const auto& mapping : kSampleMapping) {
        auto& table = m_sampleTables[mapping.tableIndex];
        const auto& range = mapping.sampleIndexRange;

        for (int i = range.first; i < range.second; i++) {
            if (isSampleMoved(i)) {
                auto tableIndex = movedSamples[i].first;
                auto sampleIndex = movedSamples[i].second;
                table.samples.push_back(m_sampleTables[tableIndex].samples[sampleIndex]);
            } else if (!didSampleFailToLoad(i)) {
                auto& sample = m_originalCommentarySamples[i];
                if (sample.hasData()) {
                    movedSamples[i] = { mapping.tableIndex, static_cast<int>(table.samples.size()) };
                    table.samples.push_back(std::move(sample));
                } else {
                    markSampleAsFailed(i);
                }
            }

            if (!didSampleFailToLoad(i)) {
                table.totalSampleChance++;
                if (i >= mapping.doubleChanceSamples.first && i < mapping.doubleChanceSamples.second) {
                    table.samples.back().setChanceModifier(2);
                    table.totalSampleChance++;
                }
            }
        }
    }

    // emplace a null sample for the keeper claimed comment, it's the only "silent" comment we got
    m_sampleTables[kKeeperClaimed].samples.emplace_back(SoundSample::createNullSample());
    m_sampleTables[kKeeperClaimed].totalSampleChance++;
}

static void loadOriginalSamples()
{
    const auto onDemandSamples = getOnDemandSampleTable();

    int i = 0;
    for (auto ptr : { swos.commentaryTable, onDemandSamples }) {
        for (; *ptr != kSentinel; ptr++, i++) {
            auto& sampleData = m_originalCommentarySamples[i];
            if (!sampleData.hasData())
                sampleData.loadFromFile(*ptr);
        }
    }

    assert(i == m_originalCommentarySamples.size());

    mapOriginalSamples();
}

void SWOS::LoadCommentary()
{
    if (swos.g_soundOff || !swos.g_commentary)
        return;

    logInfo("Loading commentary...");

    auto audioDirPath = pathInRootDir("audio");
    m_hasAudioDir = dirExists(audioDirPath.c_str());

    m_hasAudioDir ? loadCustomCommentary() : loadOriginalSamples();
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
    if (swos.g_soundOff || !swos.g_commentary || swos.g_muteCommentary)
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

void SWOS::PlayPenaltyGoalComment()
{
    playComment(kPenaltyGoal);
    swos.performingPenalty = 0;
}

void SWOS::PlayPenaltyMissComment()
{
    playComment(kPenaltyMissed);
    swos.performingPenalty = 0;
}

void SWOS::PlayPenaltySavedComment()
{
    playComment(kPenaltySaved);
    swos.performingPenalty = 0;
}

void SWOS::PlayPenaltyComment()
{
    swos.performingPenalty = -1;
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

void SWOS::PlayGoodPassComment()
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
    if (swos.performingPenalty) {
        playComment(kPenaltySaved);
        swos.performingPenalty = 0;
    } else {
        // don't interrupt penalty saved comments
        if (m_lastPlayedCategory != kPenaltySaved || !commentPlaying())
            playComment(kKeeperClaimed);
    }
}

void SWOS::PlayNearMissComment()
{
    if (swos.performingPenalty || swos.penaltiesState == -1)
        PlayPenaltyMissComment();
    else
        playComment(kNearMiss);
}

void SWOS::PlayGoalkeeperSavedComment()
{
    if (swos.performingPenalty || swos.penaltiesState == -1)
        PlayPenaltySavedComment();
    else
        playComment(kKeeperSaved);
}

// fix original SWOS bug where penalty flag remains set when penalty is missed, but it's not near miss
void SWOS::FixPenaltyBug()
{
    swos.performingPenalty = 0;
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
    if (swos.performingPenalty || swos.penaltiesState == -1)
        PlayPenaltyGoalComment();
    else
        playComment(kGoal);
#ifdef __ANDROID__
    vibrate();
#endif
}

void SWOS::PlayYellowCardSample()
{
    playComment(kYellowCard);
}

void SWOS::PlayRedCardSample()
{
    playComment(kRedCard);
}

void SWOS::PlayCornerSample()
{
    playComment(kCorner);
}

void SWOS::PlayTacticsChangeSample()
{
    playComment(kChangeTactics);
}

void SWOS::PlaySubstituteSample()
{
    playComment(kSubstitution);
}

void SWOS::PlayThrowInSample()
{
    playComment(kThrowIn);
}

void SWOS::PlayEnqueuedSamples()
{
    if (swos.playingYellowCardTimer >= 0 && !--swos.playingYellowCardTimer) {
        PlayYellowCardSample();
        swos.playingYellowCardTimer = -1;
    } else if (swos.playingRedCardTimer >= 0 && !--swos.playingRedCardTimer) {
        PlayRedCardSample();
        swos.playingRedCardTimer = -1;
    } else if (swos.playingGoodPassTimer >= 0 && !--swos.playingGoodPassTimer) {
        PlayGoodPassComment();
        swos.playingGoodPassTimer = -1;
    } else if (swos.playingThrowInSample >= 0 && !--swos.playingThrowInSample) {
        PlayThrowInSample();
        swos.playingThrowInSample = -1;
    } else if (swos.playingCornerSample >= 0 && !--swos.playingCornerSample) {
        PlayCornerSample();
        swos.playingCornerSample = -1;
    } else if (swos.substituteSampleTimer >= 0 && !--swos.substituteSampleTimer) {
        PlaySubstituteSample();
        swos.substituteSampleTimer = -1;
    } else if (swos.tacticsChangedSampleTimer >= 0 && !--swos.tacticsChangedSampleTimer) {
        PlayTacticsChangeSample();
        swos.tacticsChangedSampleTimer = -1;
    }

    // strange place to decrement this...
    if (swos.goalCounter)
        swos.goalCounter--;

    playCrowdChants();
}

static void loadAndPlayEndGameCrowdSample(int index)
{
    if (swos.g_trainingGame || !swos.g_commentary)
        return;

    assert(index >= 0 && index <= 3);

    // no need to worry about this still being played
    m_endGameCrowdSample.free();

    auto filename = swos.endGameCrowdSamples[index].asPtr();

    std::string path;
    if (m_hasAudioDir)
        path = joinPaths("audio", "fx") + getDirSeparator() + (filename + 5);   // skip "hard\" part

    m_endGameCrowdSample.loadFromFile(m_hasAudioDir ? path.c_str() : filename);

    auto chunk = m_endGameCrowdSample.chunk();
    if (chunk) {
        Mix_VolumeChunk(chunk, MIX_MAX_VOLUME);
        Mix_PlayChannel(-1, chunk, 0);
    } else {
        logWarn("Failed to load end game sample %s", filename);
    }
}

void SWOS::LoadAndPlayEndGameComment()
{
    if (swos.g_soundOff || !swos.g_commentary)
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
        PlayItsBeenACompleteRoutComment();
    else if (!goalDiff)
        PlayDrawComment();
    else if (swos.statsTeam1Goals + swos.statsTeam2Goals >= 4)
        PlaySensationalGameComment();
}

void SWOS::PlayDrawComment()
{
    playComment(kEndGameSoClose);
}

void SWOS::PlaySensationalGameComment()
{
    playComment(kEndGameSensational);
}

void SWOS::PlayItsBeenACompleteRoutComment()
{
    playComment(kEndGameRout);
}
