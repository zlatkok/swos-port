#pragma once

constexpr int kMaxVolume = MIX_MAX_VOLUME;
constexpr int kMinVolume = 0;

constexpr int kMenuFrequency = 44'100;
constexpr int kMenuChunkSize = 8'192;

constexpr int kGameFrequency = 22'050;
constexpr int kGameChunkSize = 4'096;

constexpr char kAudioDir[] = "audio";

void initAudio();
void finishAudio();
void stopAudio();
void initGameAudio();
void resetGameAudio();
void ensureMenuAudioFrequency();

bool soundEnabled();
void initSoundEnabled(bool enabled);
void setSoundEnabled(bool enabled);

bool musicEnabled();
void initMusicEnabled(bool enabled);
void setMusicEnabled(bool enabled);

bool commentaryEnabled();
void setCommentaryEnabled(bool enabled);

int getMasterVolume();
void initMasterVolume(int volume);
void setMasterVolume(int volume);

int getMusicVolume();
void initMusicVolume(int volume);
void setMusicVolume(int volume);
