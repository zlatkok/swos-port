// This class holds data gathered during the game and required for replays
// (sprite coordinates, camera coordinates...) and handles its storage.

#pragma once

#include "stats.h"

struct HilV1Header;
struct HilV2Header;

class ReplayDataStorage
{
public:
    enum class FileStatus {
        kOk,
        kCorrupted,
        kUnsupportedVersion,
        kIoError,
    };

    enum class ObjectType {
        kStats,
        kSprite,
        kSfx,
        kUnknown,
    };

    struct Object {
        Object() : type(ObjectType::kUnknown) {}

        ObjectType type;
        union {
            struct {
                int pictureIndex;
                float x;
                float y;
            };
            GameStats stats;
            struct {
                int sampleIndex;
                int volume;
            };
        };
    };

    struct FrameData {
        FrameData() = default;
        FrameData(float cameraX, float cameraY, int team1Goals, int team2Goals, int gameTime)
            : cameraX(cameraX), cameraY(cameraY), team1Goals(team1Goals), team2Goals(team2Goals), gameTime(gameTime) {}
        float cameraX;
        float cameraY;
        int team1Goals;
        int team2Goals;
        int gameTime;
    };

    ReplayDataStorage();

    int numScenes() const;
    bool empty() const;
    bool isLegacyFormat() const;

    void startRecordingNewReplay(bool legacyFormat = false);
    void recordFrame(const FrameData& data);

    bool fetchFrameData(FrameData& data);
    void recordSprite(int spriteIndex, float x, float y);
    void recordStats(const GameStats& stats);
    void recordSfx(int sampleIndex, int volume);
    bool fetchObject(Object& obj);
    bool checkFetchRange(int numElements) const;

    bool hasAnotherFullFrame() const;
    float percentageAt() const;
    int currentScene() const;

    void saveHighlightsScene();

    void setupForCurrentSceneReplay();
    void setupForStoredSceneReplay(int sceneNumber);
    void setupForFullReplay();

    void skipFrames(int offset);

    FileStatus load(const char *filename, const char *dir, HilV2Header& header, bool isReplay);
    bool save(const char *filename, HilV2Header& header, bool isReplay, bool overwrite);

private:
#pragma pack(push, 1)
struct RawInt32 {
    RawInt32() {}
    RawInt32(int data) : data(data) {}
    RawInt32(int64_t data) : data(static_cast<int32_t>(data)) {}
    RawInt32(unsigned data) : data(data) {}
    RawInt32(float data) : dataF(data) {}
    operator int() const { return data; }
    explicit operator float() const { return dataF; }
    float asFloat() const { return dataF; }
    RawInt32& operator+=(int value) {
        data += value;
        return *this;
    }
    RawInt32& operator-=(int value) {
        data -= value;
        return *this;
    }
    union {
        int32_t data;
        float dataF;
    };
    static_assert(sizeof(float) == sizeof(int32_t), "Comet");
};
#pragma pack(pop)
    static_assert(sizeof(RawInt32) == sizeof(int32_t), "Life is full of obstacles");

    using DataStore = std::vector<RawInt32>;

    void updateFrameSceneOffsets();
    void updateSpriteSceneOffsets();
    void updateStatsSceneOffsets();
    void updateSfxSceneOffsets();
    void updateSceneOffsets(int numElements, bool newFrame = false);

    FileStatus loadHeader(SDL_RWops *f, HilV2Header& header, int& headerSize);
    bool loadSceneTable(SDL_RWops *f, int numScenes);
    bool saveSceneTable(SDL_RWops *f, bool isReplay);
    bool saveData(SDL_RWops *f, bool isReplay) const;
    DataStore highlightsData() const;
    bool highlightsNeedFixup() const;
    void fixupScene() const;
    size_t sceneOffsetTableSize() const;

    void convertLegacyData(const DataStore& legacyData, bool isReplay);

    DataStore m_replayData;
    bool m_legacyFormat = false;

    using SceneOffset = std::array<uint32_t, 2>;
    using SceneOffsetTable = std::vector<SceneOffset>;

    static int sceneSize(const SceneOffset& sceneOffset);

    SceneOffsetTable m_sceneOffsets;
    int m_currentFrameOffset = 0;
    int m_previousFrameOffset = 0;

    unsigned m_currentSceneStart = 0;
    unsigned m_currentSceneEnd = 0;
    int m_currentSceneNumber = 0;

    unsigned m_replayOffset = 0;
    unsigned m_replayStart = 0;
    unsigned m_replayLimit = 0;
    unsigned m_replayNextFrame = 0;

    friend class LegacyReplayConverter;
};
