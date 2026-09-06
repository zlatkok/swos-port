#pragma once

#include "BaseTest.h"
#include "KeyConfig.h"
#include "resData.h"

#pragma pack(push, 1)
struct BenchDataV1 {
    byte pl1TapCount;
    byte pl2TapCount;
    int8_t pl1PreviousDirection;
    int8_t pl2PreviousDirection;
    byte pl1BlockWhileHoldingDirection;
    byte pl2BlockWhileHoldingDirection;
    int8_t controls;
    int8_t teamData;
    int8_t teamGame;
    byte state;
    int goToBenchTimer;
    byte bench1Called;
    byte bench2Called;
    byte blockDirections;
    int fireTimer;
    byte blockFire;
    int8_t lastDirection;
    int movementDelayTimer;
    bool trainingTopTeam;
    bool teamsSwapped;
    int alternateTeamsTimer;
    int8_t arrowPlayerIndex;
    int8_t selectedMenuPlayerIndex;
    int8_t playerToEnterGameIndex;
    int8_t playerToBeSubstitutedPos;
    int8_t playerToBeSubstitutedOrd;
    int8_t selectedFormationEntry;
    byte shirtNumberTable[2 * kNumPlayersInTeam];
};
struct BenchDataV2 : public BenchDataV1 {
    word pl1TapTimeoutCounter;
    word pl2TapTimeoutCounter;
};
#define BenchData BenchDataV2
#pragma pack(pop)

class RecordedDataTest : public BaseTest
{
protected:
    void init() override;
    void finish() override;
    void defaultCaseInit() override {};
    const char *name() const override;
    const char *displayName() const override;
    CaseList getCases() override;
    bool shouldRunLast() const override { return true; }

private:
    static constexpr int kFrameNumSprites = 33;

    enum ControlFlags : int16_t {
        kUp = 1,
        kDown = 2,
        kLeft = 4,
        kRight = 8,
        kFire = 32,
        kBench = 64,
        kEscape = 128,
    };

#pragma pack(push, 1)
    struct Header {
        byte major;
        byte minor;
        byte fixedRandomValue;
        int8_t pitchTypeOrSeason;
        int8_t pitchType;
        int8_t gameLength;
        int8_t autoReplays;
        int8_t allPlayerTeamsEqual;
        TeamFile topTeamFile;
        TeamFile bottomTeamFile;
        byte topTeamCoachNo;
        byte topTeamPlayerNo;
        byte bottomTeamCoachNo;
        byte bottomTeamPlayerNo;
        byte extraTime;
        byte penaltyShootout;
        int8_t topTeamMarkedPlayer;
        int8_t bottomTeamMarkedPlayer;
        TeamTactics userTactics[6];
        word inputControls;
        dword initialFrame;
    };
    struct HeaderV1p2 : public Header {
        dword trainingGameCareerSize;
        int8_t season;
        int8_t minSubstitutes;
        int8_t maxSubstitutes;
    };
    struct FrameV1p0 {
        dword frameNo;
        ControlFlags player1Controls;
        ControlFlags player2Controls;
        TeamGeneralInfo topTeam;
        char topTeamPlayers[11];
        TeamGeneralInfo bottomTeam;
        char bottomTeamPlayers[11];
        TeamGame topTeamGame;
        TeamGame bottomTeamGame;
        TeamStatsData topTeamStats;
        TeamStatsData bottomTeamStats;
        FixedPoint cameraX;
        FixedPoint cameraY;
        int8_t cameraXVelocity;
        int8_t cameraYVelocity;
        byte gameTime[4];
        byte team1Goals;
        byte team2Goals;
        byte gameState;
        byte gameStatePl;
        Sprite sprites[kFrameNumSprites];
    };
    struct FrameV1p3 : public FrameV1p0 {
        BenchDataV1 bench;
    };
    struct FrameV1p4 : public FrameV1p3 {
        char userKey;
        byte fireBlocked;
        word statsTimer;
        word resultTimer;
    };
    struct FrameV1p5 : public FrameV1p4 {
        word pl1TapTimeoutCounter;
        word pl2TapTimeoutCounter;
    };
    using Frame = FrameV1p5;
#pragma pack(pop)

    void setupRecordedDataVerification();
    void verifyRecordedData();
    void verifyCalculateDeltaXandY();
    void finalizeRecordedDataVerification();
    void setFrameInput();
    void verifyFrame();
    void verifyTeam(const TeamGeneralInfo& recTeam, const TeamGeneralInfo& team);
    void verifyTeamGame(const TeamGame& recTeam, const TeamGame& team);
    void verifySprites(const Sprite *sprites);
    void verifyBench();
    void verifyPlayerSpriteOrder(char *players, const TeamGeneralInfo& team);
    void verifyShotChanceTable(int recOffset, SwosDataPointer<const int16_t> table);
    void verifySpritePointer(SwosDataPointer<Sprite> recSprite, SwosDataPointer<Sprite> sprite);

    ResFilenameList m_files;

    std::pair<SDL_RWops *, HeaderV1p2> openDataFile(const std::string& file);
    Frame readNextFrame(SDL_RWops *f);
    static std::vector<SDL_Scancode> controlFlagsToKeys(ControlFlags flags, const DefaultKeySet& keySet);

    SDL_RWops *m_dataFile;
    HeaderV1p2 m_header;
    Frame m_frame;
    int m_frameSize;
    bool m_firstFrame;
    unsigned m_lastGameTick = 0;

    int m_screenWidth;
    int m_screenHeight;

    std::vector<SDL_Scancode> m_player1Keys;
    std::vector<SDL_Scancode> m_player2Keys;
};
