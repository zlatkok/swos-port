#pragma once

void loadCommentary();
void playEndGameCrowdSampleAndComment();
void initCommentsBeforeTheGame();
void enqueueThrowInSample();
void enqueueCornerSample();
void enqueueTacticsChangedSample();
void enqueueSubstituteSample();
void enqueueYellowCardSample();
void enqueueRedCardSample();
void playEnqueuedSamples();
void playPostHitComment();
void playBarHitComment();
void playNearMissComment();
void playGoalComment();
void playOwnGoalComment();
bool commenteryOnChannelFinished(int channel);
void toggleMuteCommentary();
void playHeaderComment(const TeamGeneralInfo& team);
void playInjuryComment(const TeamGeneralInfo& team);
void playGoodTackleComment();
void clearPenaltyFlag();

namespace SWOS {
    void PlayNearMissComment();
}

#ifdef SWOS_TEST
void clearCommentsSampleCache();
void setEnqueueTimers(const std::vector<int>& values);
#endif
