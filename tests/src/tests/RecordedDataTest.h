#pragma once

#include "BaseTest.h"
#include "KeyConfig.h"
#include "file.h"

class RecordedDataTest : public BaseTest
{
    void init() override;
    void finish() override;
    void defaultCaseInit() override {};
    const char *name() const override;
    const char *displayName() const override;
    CaseList getCases() override;

private:
    static constexpr int kFrameNumSprites = 33;

    enum InputControls : word {
        kKeyboardOnly = 1,
        kJoypadOnly = 2,
        kJoypadAndKeyboard = 3,
        kKeyboardAndJoypad = 4,
        kTwoJoypads = 5,
        kMouse = 6,
    };

    enum ControlFlags : int16_t {
        kUp = 1,
        kDown = 2,
        kLeft = 4,
        kRight = 8,
        kFire = 32,
        kBench = 64,
    };

#pragma pack(push, 1)
    struct Header {
        byte major;
        byte minor;
        byte fixedRandomValue;
        TeamFile topTeamFile;
        TeamFile bottomTeamFile;
        TeamTactics userTactics[6];
        InputControls inputControls;
        dword initialFrame;
    };
    struct Frame {
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
        dword gameTime;
        Sprite sprites[kFrameNumSprites];
    };
#pragma pack(pop)

    void setupRecordedDataVerification();
    void verifyRecordedData();
    void finalizeRecordedDataVerification();
    void setFrameInput();
    void verifyFrame();
    void verifyTeam(const TeamGeneralInfo& recTeam, const TeamGeneralInfo& team);
    void verifyShotChanceTable(int recOffset, int offset);
    void verifySpritePointer(SwosDataPointer<Sprite> recSprite, SwosDataPointer<Sprite> sprite);
    std::pair<SDL_RWops *, Header> openDataFile(const char *path);
    Frame readNextFrame(SDL_RWops *f);
    static std::vector<SDL_Scancode> controlFlagsToKeys(ControlFlags flags, const DefaultKeySet& keySet);

    FoundFileList m_files;

    SDL_RWops *m_dataFile;
    Header m_header;
    Frame m_frame;

    std::vector<SDL_Scancode> m_player1Keys;
    std::vector<SDL_Scancode> m_player2Keys;
};
