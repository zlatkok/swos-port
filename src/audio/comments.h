#pragma once

void loadCommentary();
void playEndGameCrowdSampleAndComment();
void clearCommentsSampleCache();
void initCommentsBeforeTheGame();
void enqueueTacticsChangedSample();
void enqueueSubstituteSample();
void enqueueYellowCardSample();
void enqueueRedCardSample();
void playEnqueuedSamples();
bool commenteryOnChannelFinished(int channel);
void toggleMuteCommentary();

#ifdef SWOS_TEST
void setEnqueueTimers(const std::vector<int>& values);
#endif
