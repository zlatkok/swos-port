#pragma once

#include "SoundSample.h"

enum SfxSampleIndex {
    kBackgroundCrowd, kBounce, kHomeGoal, kKick, kWhistle, kMissGoal,
    kEndGameWhistle, kFoulWhistle, kChant4l, kChant10l, kChant8l,
    kNumSoundEffects,
};

using SfxSamplesArray = std::array<SoundSample, kNumSoundEffects>;

void loadSoundEffects();
void clearSfxSamplesCache();
void initSfxBeforeTheGame();
SfxSamplesArray& sfxSamples();
void playCrowdNoise();
void playBallBounceSample();
void playMissGoalSample();
void playHomeGoalSample();
void playAwayGoalSample();
void playRefereeWhistleSample();
void stopBackgroudCrowdNoise();
void playEndGameWhistleSample();
void playSfx(int sample, int volume);
