#include "audio.h"

static bool m_soundEnabled = true;
static bool m_commentaryEnabled = true;

void saveAudioOptions(CSimpleIni& ini) {}
void loadAudioOptions(const CSimpleIniA& ini) {}

void initAudio() {}
void stopAudio() {}
void finishAudio() {}
void initGameAudio() {}
void resetGameAudio() {}
void ensureMenuAudioFrequency() {}

bool soundEnabled()
{
    return m_soundEnabled;
}

void initSoundEnabled(bool enabled)
{
    m_soundEnabled = enabled;
}

void setSoundEnabled(bool enabled)
{
    m_soundEnabled = enabled;
}

bool musicEnabled() { return true; }
void initMusicEnabled(bool) {}
void setMusicEnabled(bool) {}

bool commentaryEnabled()
{
    return m_commentaryEnabled;
}

void setCommentaryEnabled(bool enabled)
{
    m_commentaryEnabled = enabled;
}

int getMasterVolume() { return 0; }
void setMasterVolume(int) {}
int getMusicVolume() { return 0; }
void setMusicVolume(int) {}
