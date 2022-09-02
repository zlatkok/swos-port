#pragma once

#include "SoundSample.h"

class SampleTable
{
public:
    SampleTable(const char *dir);
    void reset();
    bool empty() const;
    const char *dir() const;
    Mix_Chunk *getRandomSample(const Mix_Chunk *lastPlayedSample, size_t lastPlayedHash);
    void loadSamples(const std::string& baseDir);
    void removeSample(int index);

private:
    int getRandomSampleIndex() const;
    void fixIndexToSkipLastPlayedComment(int& sampleIndex, const Mix_Chunk *lastPlayedSample, size_t lastPlayedHash);
    static bool isLastPlayedComment(SoundSample& sample, const Mix_Chunk *lastPlayedSample, size_t lastPlayedHash);
    static int parseSampleChanceMultiplier(const char *str, size_t len);

    const char *m_dir;
    std::vector<SoundSample> m_samples;
    int m_lastPlayedIndex = -1;
    int m_totalSampleChance = 0;
};
