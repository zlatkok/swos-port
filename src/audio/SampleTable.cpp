#include "SampleTable.h"
#include "file.h"
#include "util.h"
#include <dirent.h>

SampleTable::SampleTable(const char *dir) : m_dir(dir)
{
}

void SampleTable::reset()
{
    m_samples.clear();
    m_lastPlayedIndex = -1;
    m_totalSampleChance = 0;
}

bool SampleTable::empty() const
{
    return m_samples.empty();
}

const char *SampleTable::dir() const
{
    return m_dir;
}

Mix_Chunk *SampleTable::getRandomSample(const Mix_Chunk *lastPlayedSample, size_t lastPlayedHash)
{
    while (!m_samples.empty()) {
        auto sampleIndex = getRandomSampleIndex();
        fixIndexToSkipLastPlayedComment(sampleIndex, lastPlayedSample, lastPlayedHash);

        if (auto chunk = m_samples[sampleIndex].chunk()) {
            logInfo("Sample %d/%d (%s) selected for playback", sampleIndex + 1, m_samples.size(), m_dir);
            m_lastPlayedIndex = sampleIndex;
            return chunk;
        }

        logWarn("Failed to load comment %d from directory %s", sampleIndex, m_dir);
        m_totalSampleChance -= m_samples[sampleIndex].chanceModifier();
        m_samples.erase(m_samples.begin() + sampleIndex);
        m_lastPlayedIndex -= m_lastPlayedIndex >= static_cast<int>(sampleIndex);
        sampleIndex -= sampleIndex == m_samples.size();
    }

    return nullptr;
}

void SampleTable::loadSamples(const std::string& baseDir)
{
    m_lastPlayedIndex = -1;

    const auto& dirPrefix = joinPaths(baseDir.c_str(), m_dir) + getDirSeparator();
    const auto& dirFullPath = pathInRootDir(dirPrefix.c_str());

    if (auto dir = opendir(dirFullPath.c_str())) {
        for (dirent *entry; entry = readdir(dir); ) {
#ifdef _WIN32
            auto len = entry->d_namlen;
#else
            auto len = strlen(entry->d_name);
#endif
            if (len < 4)
                continue;

            auto samplePath = dirPrefix + entry->d_name;
            int chance = parseSampleChanceMultiplier(entry->d_name, len);
            SoundSample sample(samplePath.c_str(), chance);

            if (sample.hasData()) {
                if (std::find(m_samples.begin(), m_samples.end(), sample) == m_samples.end()) {
                    m_samples.push_back(std::move(sample));
                    m_totalSampleChance += chance;
#ifndef DEBUG
                    logInfo("`%s' loaded OK, chance: %d", samplePath.c_str(), chance);
#endif
                } else {
                    logInfo("Duplicate detected, rejecting: `%s'", samplePath.c_str());
                }
            } else {
                logWarn("Failed to load sample `%s'", samplePath.c_str());
            }
        }

        closedir(dir);
    }
}

void SampleTable::removeSample(int index)
{
}

int SampleTable::getRandomSampleIndex() const
{
    assert(m_totalSampleChance > 0 && m_totalSampleChance >= static_cast<int>(m_samples.size()));

    int randomSampleChance = getRandomInRange(0, std::max(0, m_totalSampleChance - 1));

    int index = 0;
    int size = static_cast<int>(m_samples.size());

    for (; index < size; index++) {
        randomSampleChance -= m_samples[index].chanceModifier();
        if (randomSampleChance < 0)
            break;
    }

    assert(index < size);
    return std::min(index, size - 1);
}


void SampleTable::fixIndexToSkipLastPlayedComment(int& sampleIndex, const Mix_Chunk *lastPlayedSample, size_t lastPlayedHash)
{
    if (m_samples.size() > 1 && (m_lastPlayedIndex == sampleIndex || isLastPlayedComment(m_samples[sampleIndex], lastPlayedSample, lastPlayedHash)))
        sampleIndex = (sampleIndex + 1) % m_samples.size();

    // theoretically, chosen comment might have been just played in a different category, and then we might bump into the one that
    // was last played from this category (it wouldn't be that bad since at least one comment was in between but let's be safe...)
    if (m_samples.size() > 2 && m_lastPlayedIndex == sampleIndex)
        sampleIndex = (sampleIndex + 1) % m_samples.size();
}

bool SampleTable::isLastPlayedComment(SoundSample& sample, const Mix_Chunk *lastPlayedSample, size_t lastPlayedHash)
{
    if (lastPlayedSample) {
        if (auto chunk = sample.chunk())
            return sample.hash() == lastPlayedHash && chunk->alen == lastPlayedSample->alen &&
                !memcmp(chunk->abuf, lastPlayedSample->abuf, chunk->alen);
    }

    return false;
}

int SampleTable::parseSampleChanceMultiplier(const char *str, size_t len)
{
    assert(str);
    assert(len == strlen(str));

    int frequency = 1;

    if (len >= 4) {
        auto end = str + len - 1;

        for (auto p = end; p >= str; p--) {
            if (*p == '.') {
                end = p - 1;
                break;
            }
        }

        if (end - str >= 4 && isdigit(*end)) {
            int value = 0;
            int power = 1;
            auto digit = end;

            while (digit >= str && isdigit(*digit)) {
                value += power * (*digit - '0');
                power *= 10;
                digit--;
            }

            if (digit >= str + 2 && *digit == 'x' && (digit[-1] == '_' || digit[-1] == '-'))
                frequency = value;
        }
    }

    return frequency;
}
