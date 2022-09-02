#include "comments.h"
#include "audio.h"
#include "chants.h"
#include "comments.h"
#include "file.h"
#include "util.h"
#include "SampleTable.h"
#include "SoundSample.h"

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

void playEndGameCrowdSampleAndComment()
{
    if (!soundEnabled() || !commentaryEnabled())
        return;

    auto team2Losing = [] {
        return swos.team2GoalsDigit1 < swos.team1GoalsDigit1 ||
            swos.team2GoalsDigit1 == swos.team1GoalsDigit1 && swos.team2GoalsDigit2 < swos.team1GoalsDigit2;
    };

    int index;
    if (swos.team1GoalsDigit1 == swos.team2GoalsDigit1 && swos.team1GoalsDigit2 == swos.team2GoalsDigit2)
        index = 2;
    else if (team2Losing())
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

#ifdef SWOS_TEST
void setEnqueueTimers(const std::vector<int>& values)
{
    assert(values.size() >= 7);

    m_playingYellowCardTimer = values[0];
    m_playingRedCardTimer = values[1];
    swos.playingGoodPassTimer = values[2];
    swos.playingThrowInSample = values[3];
    swos.playingCornerSample = values[4];
    m_substituteSampleTimer = values[5];
    m_tacticsChangedSampleTimer = values[6];
}
#endif

static void loadCustomCommentary()
{
    assert(m_sampleTables.size() == kNumSampleTables);
    assert(!strcmp(m_sampleTables[kEndGameSoClose].dir(), "end_game_so_close"));
    assert(!strcmp(m_sampleTables[kYellowCard].dir(), "yellow_card"));

    const std::string audioPath = joinPaths("audio", "commentary");

    for (int i = 0; i < kNumSampleTables; i++) {
        auto& table = m_sampleTables[i];
        if (table.empty())
            table.loadSamples(audioPath);
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

static void playComment(CommentarySampleTableIndex tableIndex, bool interrupt = true)
{
    if (!soundEnabled() || !commentaryEnabled() || m_muteCommentary || swos.g_trainingGame)
        return;

    auto& table = m_sampleTables[tableIndex];

    if (!table.empty()) {
        if (auto chunk = table.getRandomSample(m_lastPlayedComment, m_lastPlayedCommentHash)) {
            playComment(chunk, interrupt);
            m_lastPlayedCategory = tableIndex;
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
