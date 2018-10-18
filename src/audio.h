#pragma once

constexpr int kMenuFrequency = 44'100;
constexpr int kMenuChunkSize = 8'192;

constexpr int kGameFrequency = 22'050;
constexpr int kGameChunkSize = 4'096;

constexpr int kIntroBufferSize = 95256;

int getMasterVolume();
void setMasterVolume(int volume, bool apply = true);
int getMusicVolume();
void setMusicVolume(int volume, bool apply = true);

void initAudio();
void finishAudio();
void resetGameAudio();
void ensureMenuAudioFrequency();
int playIntroSample(void *buffer, int size, int volume, int loopCount);

void showAudioOptionsMenu();
