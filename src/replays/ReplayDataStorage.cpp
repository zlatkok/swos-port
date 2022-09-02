#include "ReplayDataStorage.h"
#include "LegacyReplayConverter.h"
#include "hilFile.h"
#include "stats.h"
#include "file.h"
#include "util.h"

constexpr int kVersionMajor = 2;
constexpr int kVersionMinor = 0;

// make it bigger than the original, since we'll use 3 dwords to store the sprite data (vs. just 1)
// but also more data is being recorded (all the sprites, not just visible @320x200) so count that too
constexpr int kHighlightSceneNumElements = 39'000;

constexpr int kInitialReplayCapacity = 4'500'000;

constexpr int kObjectTypeMask = 7 << 29;
enum ObjectTypeMask
{
    kStatsMask = 1 << 31,
    kSfxMask = 1 << 30,
};

constexpr int kFrameNumElements = 6;
constexpr int kSpriteNumElements = 3;
constexpr int kStatsNumElements = 4 * 2;
constexpr int kSfxNumElements = 1;

constexpr int kNextFrameOffset = 0;
constexpr int kPrevFrameOffset = 1;

ReplayDataStorage::ReplayDataStorage()
{
    m_replayData.reserve(kInitialReplayCapacity);
}

int ReplayDataStorage::numScenes() const
{
    return m_sceneOffsets.size();
}

bool ReplayDataStorage::empty() const
{
    return m_replayData.empty();
}

bool ReplayDataStorage::isLegacyFormat() const
{
    return m_legacyFormat;
}

void ReplayDataStorage::startRecordingNewReplay(bool legacyFormat /* = false */)
{
    m_replayData.clear();
    m_sceneOffsets.clear();

    m_currentSceneStart = 0;
    m_currentSceneEnd = 0;

    m_currentFrameOffset = -1;

    m_legacyFormat = legacyFormat;
}

void ReplayDataStorage::recordFrame(const FrameData& data)
{
    assert(data.gameTime == -1 ||
        static_cast<size_t>((data.gameTime & 0xff) + 10 * ((data.gameTime >> 8) & 0xff) + (data.gameTime >> 16) <= 120));

    updateFrameSceneOffsets();

    m_replayData.push_back(m_replayData.size() + kFrameNumElements);
    m_replayData.push_back(m_previousFrameOffset);

    m_replayData.push_back(data.cameraX);
    m_replayData.push_back(data.cameraY);

    m_replayData.push_back(data.team1Goals | (data.team2Goals << 16));
    m_replayData.push_back(data.gameTime);
}

bool ReplayDataStorage::fetchFrameData(FrameData& data)
{
    if (m_replayOffset >= std::min<size_t>(m_replayLimit, m_replayData.size()))
        return false;

    m_previousFrameOffset = m_replayOffset;
    m_replayNextFrame = m_replayData[m_replayOffset++ + kNextFrameOffset];
    assert(m_replayNextFrame >= m_replayOffset - 1 + kFrameNumElements);

    if (m_replayOffset + kFrameNumElements - 1 > std::min<size_t>(m_replayLimit, m_replayData.size()))
        return false;

    m_replayOffset++;   // skip previous frame offset

    data.cameraX = m_replayData[m_replayOffset++].asFloat();
    data.cameraY = m_replayData[m_replayOffset++].asFloat();

    int goals = m_replayData[m_replayOffset++];
    data.team1Goals = goals & 0xffff;
    data.team2Goals = goals >> 16;

    data.gameTime = m_replayData[m_replayOffset++];

    return true;
}

void ReplayDataStorage::recordSprite(int spriteIndex, float x, float y)
{
    updateSpriteSceneOffsets();

    m_replayData.push_back(spriteIndex);
    m_replayData.push_back(x);
    m_replayData.push_back(y);
}

void ReplayDataStorage::recordStats(const GameStats& stats)
{
    updateStatsSceneOffsets();

    int objectMask = kStatsMask;

    for (auto teamStats : { &stats.team1, &stats.team2 }) {
        m_replayData.push_back(objectMask | teamStats->ballPossession | (teamStats->goalAttempts << 16));
        m_replayData.push_back(teamStats->onTarget | (teamStats->cornersWon << 16));
        m_replayData.push_back(teamStats->foulsConceded | (teamStats->bookings << 16));
        m_replayData.push_back(teamStats->sendingsOff);
        objectMask = 0;
    }
}

void ReplayDataStorage::recordSfx(int sampleIndex, int volume)
{
    updateSfxSceneOffsets();

    m_replayData.push_back(kSfxMask | sampleIndex | (volume << 8));
}

bool ReplayDataStorage::fetchObject(Object& obj)
{
    if (m_replayOffset >= std::min<size_t>(std::min(m_replayLimit, m_replayNextFrame), m_replayData.size()))
        return false;

    auto objectType = m_replayData[m_replayOffset] & kObjectTypeMask;

    switch (objectType) {
    case kStatsMask:
        if (!checkFetchRange(kStatsNumElements))
            return false;

        obj.type = ObjectType::kStats;

        for (auto teamStats : { &obj.stats.team1, &obj.stats.team2 }) {
            int data = m_replayData[m_replayOffset++];
            teamStats->ballPossession = data & 0xffff;
            teamStats->goalAttempts = (data & ~kObjectTypeMask) >> 16;
            data = m_replayData[m_replayOffset++];
            teamStats->onTarget = data & 0xffff;
            teamStats->cornersWon = data >> 16;
            data = m_replayData[m_replayOffset++];
            teamStats->foulsConceded = data & 0xffff;
            teamStats->bookings = data >> 16;
            teamStats->sendingsOff = m_replayData[m_replayOffset++];
        }
        break;

    case kSfxMask:
        if (!checkFetchRange(kSfxNumElements))
            return false;

        {
            int sfxData = m_replayData[m_replayOffset++] & ~kObjectTypeMask;
            obj.type = ObjectType::kSfx;
            obj.sampleIndex = sfxData & 0xff;
            obj.volume = sfxData >> 8;
        }
        break;

    default:
        if (!checkFetchRange(kSpriteNumElements))
            return false;

        obj.type = ObjectType::kSprite;
        obj.pictureIndex = m_replayData[m_replayOffset++] & ~kObjectTypeMask;

        obj.x = m_replayData[m_replayOffset++].asFloat();
        obj.y = m_replayData[m_replayOffset++].asFloat();
        break;
    }

    return true;
}

bool ReplayDataStorage::checkFetchRange(int numElements) const
{
    return m_replayOffset + numElements <= m_replayNextFrame;
}

bool ReplayDataStorage::hasAnotherFullFrame() const
{
    return m_replayNextFrame < std::min<size_t>(m_replayLimit, m_replayData.size()) &&
        static_cast<unsigned>(m_replayData[m_replayNextFrame + kNextFrameOffset]) <= std::min<size_t>(m_replayLimit, m_replayData.size());
}

float ReplayDataStorage::percentageAt() const
{
    assert(m_replayOffset >= m_replayStart && m_replayOffset <= m_replayLimit);

    if (m_replayOffset >= m_replayLimit)
        return 100;

    return (static_cast<float>(m_replayOffset) - m_replayStart) / (m_replayLimit - m_replayStart) * 100;
}

int ReplayDataStorage::currentScene() const
{
    return m_currentSceneNumber;
}

void ReplayDataStorage::saveHighlightsScene()
{
    assert(m_sceneOffsets.empty() ||
        m_currentSceneStart >= m_sceneOffsets.back()[0] && m_currentSceneEnd > m_sceneOffsets.back()[1]);

    m_sceneOffsets.push_back({ m_currentSceneStart, m_currentSceneEnd });
}

void ReplayDataStorage::setupForCurrentSceneReplay()
{
    assert(m_currentSceneStart <= m_currentSceneEnd && m_currentSceneEnd <= m_replayData.size());

    m_currentSceneNumber = -1;
    m_replayStart = m_replayOffset = std::min<size_t>(m_currentSceneStart, m_replayData.size());
    m_replayLimit = std::min<size_t>(m_currentSceneEnd, m_replayData.size());
}

void ReplayDataStorage::setupForStoredSceneReplay(int sceneNumber)
{
    m_currentSceneNumber = sceneNumber;
    m_replayStart = m_replayOffset = m_sceneOffsets[sceneNumber][0];
    m_replayLimit = m_sceneOffsets[sceneNumber][1];

    assert(m_replayLimit >= m_replayOffset);
}

void ReplayDataStorage::setupForFullReplay()
{
    assert(!m_replayData.empty());

    m_currentSceneNumber = 0;
    m_replayStart = m_replayOffset = 0;
    m_replayLimit = m_replayData.size();
}

void ReplayDataStorage::skipFrames(int offset)
{
    assert(m_replayNextFrame == m_replayOffset ||
        m_replayNextFrame > static_cast<unsigned>(m_replayData[m_replayOffset + kNextFrameOffset]));

    if (offset > 0) {
        // decrement first, as the current frame hasn't been drawn yet, so it's effectively the next frame
        while (--offset && m_replayData[m_replayOffset + kNextFrameOffset] < static_cast<int>(m_replayLimit))
            m_replayOffset = m_replayData[m_replayOffset + kNextFrameOffset];
    } else {
        offset = -offset + 1;

        if (m_replayOffset >= std::min<unsigned>(m_replayLimit, m_replayData.size())) {
            m_replayOffset = m_previousFrameOffset;
            offset--;
        }

        while (offset-- && m_replayData[m_replayOffset + kPrevFrameOffset] >= static_cast<int>(m_replayStart)) {
            assert(m_replayData[m_replayOffset + kPrevFrameOffset] >= 0);
            m_replayOffset = m_replayData[m_replayOffset + kPrevFrameOffset];
        }
    }
}

auto ReplayDataStorage::load(const char *filename, const char *dir, HilV2Header& header, bool isReplay) -> FileStatus
{
    auto path = joinPaths(dir, filename);

    logInfo("Loading replay file %s", path.c_str());

    auto f = openFile(path.c_str());
    if (!f)
        return FileStatus::kIoError;

    auto result = FileStatus::kOk;

    int headerSize;
    auto maxHeaderSize = std::max(sizeof(HilV1Header), sizeof(HilV2Header));

    auto size = SDL_RWsize(f);

    if (size < maxHeaderSize) {
        result = FileStatus::kCorrupted;
    } else {
        result = loadHeader(f, header, headerSize);
        if (result == FileStatus::kOk) {
            if (!loadSceneTable(f, header.numScenes))
                result = FileStatus::kCorrupted;

            if (result == FileStatus::kOk && SDL_RWseek(f, header.dataBufferOffset, RW_SEEK_SET) < 0)
                result = FileStatus::kIoError;

            if (result == FileStatus::kOk) {
                int remainingSize = static_cast<int>(size - header.dataBufferOffset);

                if (remainingSize > 0) {
                    int numElements = (remainingSize + sizeof(m_replayData[0]) - 1) / sizeof(m_replayData[0]);
                    if (m_legacyFormat) {
                        decltype(m_replayData) legacyData;
                        legacyData.resize(numElements);
                        if (SDL_RWread(f, legacyData.data(), remainingSize, 1) != 1)
                            result = FileStatus::kIoError;
                        else
                            convertLegacyData(legacyData, isReplay);
                    } else {
                        m_replayData.resize(numElements);
                        if (SDL_RWread(f, m_replayData.data(), remainingSize, 1) != 1)
                            result = FileStatus::kIoError;
                    }
                } else {
                    m_sceneOffsets.clear();
                }
            }
        }
    }

    if (result != FileStatus::kOk)
        logWarn("Failed to load replay file %s", path.c_str());

    SDL_RWclose(f);
    return result;
}

bool ReplayDataStorage::save(const char *filename, HilV2Header& header, bool isReplay, bool overwrite)
{
    logInfo("Saving replay file %s", filename);

    auto mode = overwrite ? "wb" : "ab";

    auto f = openFile(filename, mode);
    if (!f)
        return false;

    memcpy(header.magic, kHilV2Magic, sizeof(kHilV2Magic));
    header.major = kVersionMajor;
    header.minor = kVersionMinor;
    header.numScenes = static_cast<word>(m_sceneOffsets.size());
    header.dataBufferOffset = sizeof(header) + sceneOffsetTableSize();

    bool result = SDL_RWwrite(f, &header, sizeof(header), 1) == 1 &&
        saveSceneTable(f, isReplay) && saveData(f, isReplay);

    SDL_RWclose(f);

    if (!result)
        logWarn("Failed to save replay file %s", filename);

    return result;
}

void ReplayDataStorage::updateFrameSceneOffsets()
{
    updateSceneOffsets(kFrameNumElements, true);
}

void ReplayDataStorage::updateSpriteSceneOffsets()
{
    updateSceneOffsets(kSpriteNumElements);
}

void ReplayDataStorage::updateStatsSceneOffsets()
{
    updateSceneOffsets(kStatsNumElements);
}

void ReplayDataStorage::updateSfxSceneOffsets()
{
    updateSceneOffsets(kSfxNumElements);
}

void ReplayDataStorage::updateSceneOffsets(int numElements, bool newFrame /* = false */)
{
    if (newFrame) {
        while (m_currentSceneEnd - m_currentSceneStart > kHighlightSceneNumElements) {
            assert(static_cast<unsigned>(m_replayData[m_currentSceneStart]) > m_currentSceneStart);
            m_currentSceneStart = m_replayData[m_currentSceneStart + kNextFrameOffset];
            assert(m_currentSceneStart < m_replayData.size() && m_currentSceneEnd > m_currentSceneStart);
        }
        m_previousFrameOffset = m_currentFrameOffset;
        // mark the last frame offset index, so we can increment it to the next frame later
        m_currentFrameOffset = m_replayData.size();
    } else {
        m_replayData[m_currentFrameOffset] += numElements;
        assert(m_replayData[m_currentFrameOffset] > 0);
    }

    m_currentSceneEnd += numElements;
}

auto ReplayDataStorage::loadHeader(SDL_RWops *f, HilV2Header& header, int& headerSize) -> FileStatus
{
    assert(f);

    union {
        HilV1Header v1Header;
        HilV2Header v2Header;
    };

    auto result = FileStatus::kOk;

    if (SDL_RWread(f, &v1Header, 8, 1) != 1) {
        result = FileStatus::kIoError;
    } else {
        if (v1Header.magic1 == kHilV1Magic1 && v1Header.magic2 == kHilV1Magic2) {
            m_legacyFormat = true;
            headerSize = sizeof(HilV1Header);
            if (SDL_RWread(f, &v1Header.team1, sizeof(HilV1Header) - 8, 1) != 1)
                result = FileStatus::kIoError;
            else
                LegacyReplayConverter::convertLegacyHeader(v1Header, header);
        } else if (!memcmp(v2Header.magic, kHilV2Magic, sizeof(kHilV2Magic))) {
            m_legacyFormat = false;
            headerSize = sizeof(HilV2Header);
            if (v2Header.major > kVersionMajor) {
                result = FileStatus::kUnsupportedVersion;
            } else {
                if (SDL_RWread(f, &v2Header.dataBufferOffset, sizeof(HilV2Header) - 8, 1) != 1)
                    result = FileStatus::kIoError;
                else
                    header = v2Header;
            }
        } else {
            result = FileStatus::kCorrupted;
        }
    }

    return result;
}

bool ReplayDataStorage::loadSceneTable(SDL_RWops *f, int numScenes)
{
    assert(f);

    if (numScenes) {
        m_sceneOffsets.resize(numScenes);
        if (SDL_RWread(f, m_sceneOffsets.data(), numScenes * sizeof(m_sceneOffsets[0]), 1) != 1)
            return false;
    }

    return true;
}

bool ReplayDataStorage::saveSceneTable(SDL_RWops *f, bool isReplay)
{
    assert(f);

    auto result = true;

    if (!m_sceneOffsets.empty()) {
        if (isReplay) {
            result = SDL_RWwrite(f, m_sceneOffsets.data(), sceneOffsetTableSize(), 1) == 1;
        } else {
            int currentOffset = 0;
            SceneOffsetTable sequentialOffsets(m_sceneOffsets.size());

            for (size_t i = 0; i < m_sceneOffsets.size(); i++) {
                int len = sceneSize(m_sceneOffsets[i]);
                sequentialOffsets[i][0] = currentOffset;
                sequentialOffsets[i][1] = currentOffset += len;
            }

            result = SDL_RWwrite(f, sequentialOffsets.data(), vectorByteSize(sequentialOffsets), 1) == 1;
        }
    }

    return result;
}

bool ReplayDataStorage::saveData(SDL_RWops *f, bool isReplay) const
{
    if (isReplay) {
        return SDL_RWwrite(f, m_replayData.data(), m_replayData.size() * sizeof(m_replayData[0]), 1) == 1;
    } else {
        if (highlightsNeedFixup()) {
            const auto& data = highlightsData();
            return SDL_RWwrite(f, data.data(), data.size() * sizeof(m_replayData[0]), 1) == 1;
        } else {
            return SDL_RWwrite(f, m_replayData.data(), m_sceneOffsets.back().back() * sizeof(m_replayData[0]), 1) == 1;
        }
    }
}

auto ReplayDataStorage::highlightsData() const -> DataStore
{
    auto size = std::accumulate(m_sceneOffsets.begin(), m_sceneOffsets.end(), 0, [](int sum, const auto& sceneOffset) {
        return sum + sceneSize(sceneOffset);
    });

    DataStore data(size);

    int dataOffset = 0;
    int prevFrameOffset = -1;

    for (const auto& sceneOffset : m_sceneOffsets) {
        int len = sceneSize(sceneOffset);

        memcpy(&data[dataOffset], &m_replayData[sceneOffset.front()], len * sizeof(sceneOffset[0]));

        int base = sceneOffset.front();
        int diff = base - dataOffset;

        for (int i = 0; i < len; ) {
            auto& nextFrameOffset = data[dataOffset + i + kNextFrameOffset];
            nextFrameOffset -= diff;

            data[dataOffset + i + kPrevFrameOffset] = prevFrameOffset;
            prevFrameOffset = nextFrameOffset;

            i = m_replayData[sceneOffset.front() + i + kNextFrameOffset] - sceneOffset.front();
        }

        dataOffset += len;
    }

    assert(dataOffset == size);

    return data;
}

bool ReplayDataStorage::highlightsNeedFixup() const
{
    int previousEnd = 0;

    for (const auto& sceneOffset : m_sceneOffsets) {
        if (sceneOffset.front() != previousEnd)
            return true;
        previousEnd += sceneSize(sceneOffset);
    }

    return false;
}

size_t ReplayDataStorage::sceneOffsetTableSize() const
{
    return vectorByteSize(m_sceneOffsets);
}

void ReplayDataStorage::convertLegacyData(const DataStore& legacyData, bool isReplay)
{
    m_replayData.clear();
    m_replayData.reserve(4 * legacyData.size());

    startRecordingNewReplay(true);

    LegacyReplayConverter converter(*this);
    converter.convertLegacyData(legacyData, isReplay);
}

int ReplayDataStorage::sceneSize(const SceneOffset& sceneOffset)
{
    return (sceneOffset.back() - sceneOffset.front());
}
