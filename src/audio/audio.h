#pragma once

constexpr int kMenuFrequency = 44'100;
constexpr int kMenuChunkSize = 8'192;

constexpr int kGameFrequency = 22'050;
constexpr int kGameChunkSize = 4'096;

constexpr int kIntroBufferSize = 95256;

constexpr char *kAudioDir = "audio";

int getMusicVolume();

void initAudio();
void finishAudio();
void initGameAudio();
void resetGameAudio();
void ensureMenuAudioFrequency();
int playIntroSample(void *buffer, int size, int volume, int loopCount);

void saveAudioOptions(CSimpleIni& ini);
void loadAudioOptions(const CSimpleIniA& ini);

void showAudioOptionsMenu();
