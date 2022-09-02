#pragma once

void loadIntroChant();
void loadCrowdChantSample();
void initChantsBeforeTheGame();
bool chantsOnChannelFinished(int channel);
void playCrowdChants();
void playFansChant4lSample();
bool areCrowdChantsEnabled();
void initCrowdChantsEnabled(bool crowdChantsEnabled);
void setCrowdChantsEnabled(bool crowdChantsEnabled);

#ifdef SWOS_TEST
auto getPlayChants10lFunction() -> void (*)();
#endif
