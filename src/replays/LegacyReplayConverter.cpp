#include "LegacyReplayConverter.h"
#include "ReplayDataStorage.h"
#include "hilFile.h"

constexpr int kLegacySceneBufferSize = 19'000;
constexpr int kLegacySceneBufferMaxIndex = kLegacySceneBufferSize / 4;

void LegacyReplayConverter::convertLegacyData(const ReplayDataStorage::DataStore& legacyData, bool isReplay)
{
    int sceneStart = 0;
    int dataLimit = legacyData.size();
    int sceneNo = 0;

    while (sceneStart < dataLimit) {
        int sceneEnd = dataLimit;
        if (!isReplay)
            sceneEnd = std::min(sceneEnd, sceneStart + kLegacySceneBufferMaxIndex);

        int current = sceneStart;
        int elementsProcessed = findLegacyFrameStart(legacyData, current, sceneStart, sceneEnd);

        if (elementsProcessed < 0) {
            logWarn("Failed to find legacy replay file scene %d start", sceneNo);
        } else {
            int sceneLength = sceneEnd - sceneStart;
            while (elementsProcessed < sceneLength && legacyData[current] != -1) {
                if (legacyData[current] < 0)
                    elementsProcessed += processLegacyFrame(legacyData, current, sceneStart, sceneEnd);
                else
                    elementsProcessed += processLegacySprite(legacyData, current, sceneStart, sceneEnd);
            }
            if (isReplay)
                break;

            m_storage.saveHighlightsScene();
            m_storage.m_currentSceneStart = m_storage.m_currentSceneEnd;
        }

        sceneStart += kLegacySceneBufferMaxIndex;
        sceneNo++;
    }
}

int LegacyReplayConverter::processLegacyFrame(const ReplayDataStorage::DataStore& legacyData, int& current, int sceneStart, int sceneEnd)
{
    assert(legacyData[current] < 0);

    ReplayDataStorage::FrameData frameData;

    frameData.cameraX = static_cast<float>(legacyData[current] & 0xffff);
    frameData.cameraY = static_cast<float>((legacyData[current] >> 16) & 0x7fff);
    wrappedIncrement(current, sceneStart, sceneEnd);

    frameData.team1Goals = legacyData[current] & 0xffff;
    frameData.team2Goals = legacyData[current] >> 16;
    wrappedIncrement(current, sceneStart, sceneEnd);

    frameData.gameTime = -1;
    wrappedIncrement(current, sceneStart, sceneEnd);    // skip animated patterns state

    m_storage.recordFrame(frameData);

    return 3;
}

int LegacyReplayConverter::processLegacySprite(const ReplayDataStorage::DataStore& legacyData, int& current, int sceneStart, int sceneEnd)
{
    uint32_t packedSprite = legacyData[current];

    int spriteIndex = packedSprite >> 20;
    int x = signExtend<10>((packedSprite >> 10) & 0x3ff);
    int y = signExtend<10>(packedSprite & 0x3ff);
    m_storage.recordSprite(spriteIndex, static_cast<float>(x), static_cast<float>(y));

    wrappedIncrement(current, sceneStart, sceneEnd);

    return 1;
}

void LegacyReplayConverter::convertLegacyHeader(const HilV1Header& v1Header, HilV2Header& v2Header)
{
    v2Header.dataBufferOffset = sizeof(HilV1Header);
    v2Header.team1 = v1Header.team1;
    v2Header.team2 = v1Header.team2;
    memcpy(v2Header.gameName, v1Header.gameName, sizeof(v2Header.gameName));
    memcpy(v2Header.gameRound, v1Header.gameRound, sizeof(v2Header.gameRound));
    v2Header.team1Goals = v1Header.team1Goals;
    v2Header.team2Goals = v1Header.team2Goals;
    v2Header.pitchType = v1Header.pitchType;
    v2Header.pitchNumber = v1Header.pitchNumber;
    v2Header.numMaxSubstitutes = v1Header.numMaxSubstitutes;
}

int LegacyReplayConverter::findLegacyFrameStart(const ReplayDataStorage::DataStore& legacyData, int& current, int sceneStart, int sceneEnd)
{
    while (current < sceneEnd && (legacyData[current] >= 0 || legacyData[current] == -1))
        current++;

    return current < sceneEnd ? current - sceneStart : -1;
}

void LegacyReplayConverter::wrappedIncrement(int& current, int sceneStart, int sceneEnd)
{
    if (++current >= sceneEnd)
        current = sceneStart;
}

template<size_t N> int LegacyReplayConverter::signExtend(int value)
{
    return static_cast<int32_t>(value) << (32 - N) >> (32 - N);
}
