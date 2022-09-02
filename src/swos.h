#pragma once

#include "fetch.h"
#include "FixedPoint.h"

using dword = uint32_t;
using word = uint16_t;
using byte = uint8_t;

#pragma pack(push, 1)
// dependency cycle breaker
namespace SwosVM {
    extern dword ptrToOffset(const void *ptr);
    extern char *offsetToPtr(dword offset);
    using VoidFunction = void(*)();
    extern VoidFunction fetchProc(int index);
    extern void invokeProc(int index);
    enum class Offsets : dword;
    enum class Procs : int;
}

template<typename Type>
class SwosDataPointer
{
public:
    SwosDataPointer(Type *t) {
        assert(reinterpret_cast<uintptr_t>(&m_offset) % 4 == 0);
        m_offset = SwosVM::ptrToOffset(t);
    }
    constexpr SwosDataPointer() : m_offset(0) {}
    constexpr SwosDataPointer(dword offset) : m_offset(offset) {}
    constexpr SwosDataPointer(SwosVM::Offsets offset) : m_offset(static_cast<dword>(offset)) {}
    SwosDataPointer& operator=(Type *t) {
        assert(reinterpret_cast<uintptr_t>(&m_offset) % 4 == 0);
        m_offset = SwosVM::ptrToOffset(t);
        return *this;
    }
    template<typename PtrType, std::enable_if_t<std::is_pointer<PtrType>::value, int> = 0>
    void loadFrom(PtrType ptr) {
        auto offset = ::fetch((dword *)ptr);
        assert(offset == -1 || (SwosVM::offsetToPtr(offset), true));
        store(&m_offset, offset);
    }
    void set(dword offset) {
        assert((SwosVM::offsetToPtr(offset), true));
        store(&m_offset, offset);
    }
    void setRaw(dword val) {
        store(&m_offset, val);
    }
    void set(Type *t) {
        auto offset = SwosVM::ptrToOffset(t);
        store(&m_offset, offset);
    }
    operator Type *() { return get(); }
    operator const Type *() const { return get(); }
    Type *operator->() { return get(); }
    const Type *operator->() const { return get(); }
    SwosDataPointer operator++(int) {
        auto tmp(*this);
        m_offset += sizeof(Type);
        assert((SwosVM::offsetToPtr(m_offset), true));
        return tmp;
    }
    SwosDataPointer operator++() {
        return operator+=(sizeof(Type));
    }
    SwosDataPointer operator--(int) {
        auto tmp(*this);
        m_offset -= sizeof(Type);
        assert((SwosVM::offsetToPtr(m_offset), true));
        return tmp;
    }
    SwosDataPointer& operator--() {
        return operator+=(-static_cast<int>(sizeof(Type)));
    }
    SwosDataPointer& operator+=(int inc) {
        m_offset += inc;
        assert((SwosVM::offsetToPtr(m_offset), true));
        return *this;
    }
    SwosDataPointer& operator-=(int inc) {
        return operator+=(-inc);
    }
    template <typename T> T as() { return reinterpret_cast<T>(get()); }
    template <typename T = Type *> T asAligned() const {
        auto offset = ::fetch(&m_offset);
        assert((SwosVM::offsetToPtr(offset), true));
        return reinterpret_cast<T>(SwosVM::offsetToPtr(offset));
    }
    std::decay_t<Type> fetch() const {
        return ::fetch(asAligned());
    }
    const Type *asConst() const { return get(); }
    Type *asPtr() const { return (Type *)get(); }
    const char *asCharPtr() const { return (const char *)get(); }
    const char *asAlignedConstCharPtr() const { return asAligned<const char *>(); }
    char *asAlignedCharPtr() { return asAligned<char *>(); }
    char *asCharPtr() { return (char *)get(); }
    const char *asConstCharPtr() const { return (char *)get(); }
    dword getRaw() const { return m_offset; }

private:
    Type *get() const {
        assert(reinterpret_cast<uintptr_t>(&m_offset) % 4 == 0);
        return reinterpret_cast<Type *>(SwosVM::offsetToPtr(m_offset));
    }

    dword m_offset;
};

static_assert(sizeof(SwosDataPointer<char>) == 4, "SWOS VM data pointers must be 32-bit");

class SwosProcPointer
{
    int32_t m_index;
public:
    SwosProcPointer() {}
    constexpr SwosProcPointer(int index) : m_index(index) {}
    constexpr SwosProcPointer(SwosVM::Procs index) : m_index(static_cast<int>(index)) {}
    int32_t index() const { return m_index; }
    SwosProcPointer& operator=(const SwosProcPointer *other) {
        assert(reinterpret_cast<uintptr_t>(&m_index) % 4 == 0);
        assert(!other || SwosVM::fetchProc(other->m_index));
        m_index = other ? other->m_index : -1;
        return *this;
    }
    template<typename PtrType, std::enable_if_t<std::is_pointer<PtrType>::value, int> = 0>
    void loadFrom(PtrType ptr) {
        auto index = fetch((int *)ptr);
        assert(SwosVM::fetchProc(index));
        store(&m_index, index);
    }
    void set(int index) {
        assert(SwosVM::fetchProc(index));
        store(&m_index, index);
    }
    void clearAligned() {
        store(&m_index, 0);
    }
    explicit operator bool() const { return !empty(); }
    bool empty() const { return !SwosVM::fetchProc(m_index); }
    void operator()() { SwosVM::invokeProc(m_index); }
    bool operator==(SwosVM::VoidFunction proc) const {
        return SwosVM::fetchProc(m_index) == proc;
    }
};

struct BaseMenu {};
struct PackedMenu : BaseMenu
{
    SwosProcPointer onInit;
    SwosProcPointer onReturn;
    SwosProcPointer onDraw;
    dword initialEntry;

    const int16_t *data() const {
        return (int16_t *)(this + 1);
    }
};

enum TextColors
{
    kWhiteText = 0,
    kGrayText = 1,
    kWhiteText2 = 2,
    kBlackText = 3,
    kBrownText = 4,
    kLightBrownText = 5,
    kOrangeText = 6,
    kDarkGrayText = 7,
    kNearBlackText = 8,
    kVeryDarkGreenText = 9,
    kRedText = 10,
    kBlueText = 11,
    kPurpleText = 12,
    kSoftBlueText = 13,
    kGreenText = 14,
    kYellowText = 15,
    kTextLeftAligned = 1 << 15, kTextRightAligned = 1 << 14, kShowText = 1 << 9, kBlinkText = 1 << 13,
    kBigText = 1 << 4, kBigFont = 1 << 4,
};

template<typename T>
const char **getTrailingStrings(const T *t)
{
    return (const char **)(t + 1);
}

struct StringTable
{
    SwosDataPointer<int16_t> index;
    int16_t startIndex;
    // followed by char pointers

    StringTable(int16_t *index, int16_t startIndex) : index(index), startIndex(startIndex) {}

    char *operator[](int index) const {
        auto stringOffset = fetch((dword *)(this + 1) + index);
        return SwosVM::offsetToPtr(stringOffset);
    }

    char *currentString() const {
        if (!index || *index < 0)
            return nullptr;
        else
            return (*this)[*index];
    }
};

struct MultilineText
{
    byte numLines;
    char text[1];

    int totalLenght() const {
        int len = 1;
        auto p = text;

        for (unsigned i = 0; i < numLines; i++)
            while (*p++)
                len++;

        return len;
    }
};

struct CharTable
{
    word unk1;
    word charHeight;
    dword unk2;
    word charSpacing;
    word spaceWidth;
    word spriteIndexOffset;
    char conversionTable[224];
};

constexpr int kStdMenuTextSize = 70;

/* sprite graphics structure - from *.dat files */
struct SpriteGraphics {
    SwosDataPointer<unsigned char> data; /* pointer to actual graphics              */
    char unk1[6];               /* unknown                                          */
    int16_t width;              /* width (in pixels)                                */
    int16_t height;             /* height                                           */
    int16_t wquads;             /* (number of bytes / 8) in one line                */
    int16_t centerX;            /* center x coordinate                              */
    int16_t centerY;            /* center y coordinate                              */
    byte unk2;                  /* unknown                                          */
    byte nlinesDiv4;            /* height / 4                                       */
    int16_t ordinal;            /* ordinal number in sprite.dat                     */

    int bytesPerLine() const {
        return wquads * 8;
    }
    int size() const {
        return sizeof(SpriteGraphics) + height * wquads * 8;
    }
    SpriteGraphics *next() const {
        return reinterpret_cast<SpriteGraphics *>((char *)this + size());
    }
};

static_assert(sizeof(SpriteGraphics) == 24, "SpriteGraphics is invalid");

enum class PlayerState : byte
{
    kNormal = 0,
    kTackling = 1,
    kTackled = 3,
    kGoalieCatchingBall = 4,
    kThrowIn = 5,
    kGoalieDivingHigh = 6,
    kGoalieDivingLow = 7,
    kNormal2 = 8,
    kHeading = 9,
    kDown = 10,
    kGoalieClaimed = 11,
    kBooked = 12,
    kInjured = 13,
    kSad = 14,
    kHappy = 15,
    kUnknown = 255,
};

struct ImageIndicesTable
{
    int16_t index[1];
};

struct PlayerAnimationTable
{
    word numCycles;
    // indexed as: indicesTable[player/goalkeeper][team1/2][direction]
    SwosDataPointer<ImageIndicesTable> indicesTable[2][2][8];
};

struct RefereeAnimationTable
{
    word numCycles;
    SwosDataPointer<int16_t> indicesTable[8];
};

enum Direction
{
    kNoDirection = -1,
    kFacingTop = 0,
    kFacingTopRight = 1,
    kFacingRight = 2,
    kFacingBottomRight = 3,
    kFacingBottom = 4,
    kFacingBottomLeft = 5,
    kFacingLeft = 6,
    kFacingTopLeft = 7,
};

struct Sprite
{
    word teamNumber;    // 1 or 2 for player controls, 0 for CPU
    word playerOrdinal; // 1-11 for players, 0 for other sprites
    word frameOffset;
    SwosDataPointer<PlayerAnimationTable> animTablePtr;
    word startingDirection;
    PlayerState state;
    byte playerDownTimer;
    word unk001;
    word unk002;
    SwosDataPointer<int16_t> frameIndicesTable;
    word frameIndex;
    word frameDelay;
    word cycleFramesTimer;
    word delayedFrameTimer;
    FixedPoint x;
    FixedPoint y;
    FixedPoint z;
    int16_t direction;
    int16_t speed;
    FixedPoint deltaX;
    FixedPoint deltaY;
    FixedPoint deltaZ;
    int16_t destX;
    int16_t destY;
    byte unk003[6];
    word visible;           // skip it when rendering if false
    int16_t pictureIndex;   // -1 if none
    word saveSprite;
    dword ballDistance;
    word unk004;
    word unk005;
    word fullDirection;
    word onScreen;
    word unk006;
    word unk007;
    word unk008;
    word playerDirection;
    word isMoving;
    word tackleState;
    word unk009;
    word unk010;
    int16_t cards;
    int16_t injuryLevel;
    word tacklingTimer;
    word sentAway;

    void hide() {
        visible = 0;
    }
    void show() {
        visible = 1;
    }
};

enum class PlayerPosition : int8_t
{
    kSubstituted = -1,
    kGoalkeeper = 0,
    kRightBack = 1,
    kLeftBack = 2,
    kDefender = 3,
    kRightWing = 4,
    kLeftWing = 5,
    kMidfielder = 6,
    kAttacker = 7,
};

struct PlayerGame
{
    byte substituted;
    byte index;
    byte goalsScored;
    byte shirtNumber;
    PlayerPosition position;
    byte face;
    byte isInjured;
    byte field_7;
    byte field_8;
    byte field_9;
    byte cards;
    byte field_B;
    char shortName[15];
    byte passing;
    byte shooting;
    byte heading;
    byte tackling;
    byte ballControl;
    byte speed;
    byte finishing;
    byte goalieDirection;
    byte injuriesBitfield;
    byte hasPlayed;
    byte face2;
    char fullName[23];

    bool canBeSubstituted() const {
        return !substituted && static_cast<int>(position) >= 0 && cards < 2;
    }
};

static_assert(sizeof(PlayerGame) == 61, "PlayerGame is invalid");

struct TeamStatsData
{
    word ballPossession;
    word cornersWon;
    word foulsConceded;
    word bookings;
    word sendingsOff;
    word goalAttempts;
    word onTarget;
};

enum TeamControls : byte
{
    kTeamNotSelected = 0,
    kComputerTeam = 1,
    kPlayerCoach = 2,
    kCoach = 3,
};

constexpr int kNumPlayersInTeam = 16;

struct TeamFileHeader
{
    byte teamFileNo;
    byte teamOrdinal;
    word globalTeamNumber;
    TeamControls teamControls;
    char teamName[17];
    byte writesZeroHere;
    byte andWith0xFE;
    byte tactics;
    byte league;
    byte prShirtType;
    byte prStripesColor;
    byte prBasicColor;
    byte prShortsColor;
    byte prSocksColor;
    byte secShirtTpe;
    byte secStripesColor;
    byte secBasicColor;
    byte secShortsColor;
    byte secSocksColor;
    byte coachName[23];
    byte someFlag;
    byte playerNumbers[kNumPlayersInTeam];
};

static_assert(sizeof(TeamFileHeader) == 76, "TeamFile header is invalid");

struct PlayerFile
{
    byte nationality;
    byte unk_1;
    byte shirtNumber;
    char playerName[23];
    byte positionAndFace;
    byte cardsInjuries;
    byte passing;
    byte shootingHeading;
    byte tacklingBallControl;
    byte speedFinishing;
    byte price;
    byte unk_2;
    byte unk_3;
    byte unk_4;
    byte unk_5;
    byte someFlag;
};

static_assert(sizeof(PlayerFile) == 38, "PlayerFile is invalid");

struct TeamFile : public TeamFileHeader
{
    PlayerFile players[16];
};

static_assert(sizeof(TeamFile) == 684, "TeamFile is invalid");

struct PlayerFileHeader : private TeamFileHeader, public PlayerFile {
};

static_assert(sizeof(PlayerFileHeader) == sizeof(TeamFileHeader) + sizeof(PlayerFile), "PlayerFileHeader invalid");

struct TeamGame
{
    word prShirtType;
    word prShirtCol;
    word prStripesCol;
    word prShortsCol;
    word prSocksCol;
    word secShirtType;
    word secShirtCol;
    word secStripesCol;
    word secShortsCol;
    word secSocksCol;
    int16_t markedPlayer;
    char teamName[17];
    byte unk_1;
    byte numOwnGoals;
    byte unk_2;
    PlayerGame players[kNumPlayersInTeam];
    byte unknownTail[686];
};

struct PlayerGameHeader : private TeamGame, public PlayerGame {
};

static_assert(sizeof(PlayerGameHeader) == sizeof(TeamGame) + sizeof(PlayerGame), "PlayerGameHeader invalid");

struct TeamGeneralInfo
{
    SwosDataPointer<TeamGeneralInfo> opponentTeam;
    word playerNumber;
    word plCoachNum;
    word isPlCoach;
    SwosDataPointer<TeamGame> inGameTeamPtr;
    SwosDataPointer<TeamStatsData> teamStatsPtr;
    word teamNumber;
    SwosDataPointer<SwosDataPointer<Sprite>> players;   // 11
    SwosDataPointer<void> shotChanceTable;
    word tactics;
    word tensTimer;
    SwosDataPointer<Sprite> controlledPlayerSprite;
    SwosDataPointer<Sprite> passToPlayerPtr;
    word playerHasBall;
    word playerHadBall;
    word currentAllowedDirection;
    word direction;
    byte quickFire;
    byte normalFire;
    byte firePressed;
    byte fireThisFrame;
    word headerOrTackle;
    word fireCounter;
    word allowedPlDirection;
    word shooting;
    byte ofs60;
    byte plVeryCloseToBall;
    byte plCloseToBall;
    byte plNotFarFromBall;
    byte ballLessEqual4;
    byte ball4To8;
    byte ball8To12;
    byte ball12To17;
    byte ballAbove17;
    byte prevPlVeryCloseToBall;
    word ofs70;
    SwosDataPointer<Sprite> lastHeadingPlayer;
    word goalkeeperSavedCommentTimer;
    word ofs78;
    word goalkeeperJumpingRight;
    word goalkeeperJumpingLeft;
    word ballOutOfPlayOrKeeper;
    word goaliePlayingOrOut;
    word passingBall;
    word passingToPlayer;
    word playerSwitchTimer;
    word ballInPlay;
    word ballOutOfPlay;
    word ballX;
    word ballY;
    word passKickTimer;
    SwosDataPointer<Sprite> passingKickingPlayer;
    word ofs108;
    word ballCanBeControlled;
    word ballControllingPlayerDirection;
    word ofs114;
    word ofs116;
    word spinTimer;
    word leftSpin;
    word rightSpin;
    word longPass;
    word longSpinPass;
    word passInProgress;
    word AITimer;
    word ofs134;
    word ofs136;
    word ofs138;    // timer
    word unkTimer;
    word goalkeeperPlaying;
    word resetControls;
    byte secondaryFire;
};
static_assert(sizeof(TeamGeneralInfo) == 145, "TeamGeneralInfo is invalid");

using PositionsTable = std::array<SwosDataPointer<byte>, 18>;
static_assert(sizeof(PositionsTable) == 72, "PositionsTable is invalid");

struct PlayerPositions
{
    byte positions[35];
};

struct TeamTactics
{
    char name[9];
    PlayerPositions positions[10];
    byte unkTable[10];
    byte ballOutOfPlayTactics;
};

static_assert(sizeof(TeamTactics) == 370, "Tactics are invalid");

#pragma pack(pop)

enum ShirtTypes
{
    kShirtOrdinary = 0,
    kShirtColoredSleeves = 1,
    kShirtVerticalStripes = 2,
    kShirtHorizontalStripes = 3,
    kNumShirtTypes = 4,
};

enum FaceTypes
{
    kWhite,
    kGinger,
    kBlack,
    kNumFaces,
};

enum SpriteIndices
{
    kSmallZeroSprite = 8,
    kSmallNineSprite = 17,
    kBlockSprite = 44,
    kLeftArrowSprite = 179,
    kRightArrowSprite = 175,
    kUpArrowSprite = 183,
    kDownArrowSprite = 184,
    kMaxMenuSprite = 226,
    kSquareGridForResultSprite = 252,
    kBenchRedCardSprite = 253,
    kBenchYellowCardSprite = 254,
    kTeam1PlayerNamesStartSprite = 255,
    kTeam1PlayerNamesEndSprite = 270,
    kTeam2PlayerNamesStartSprite = 271,
    kTeam2PlayerNamesEndSprite = 286,
    kBigZeroSprite = 287,
    kBigNineSprite = 296,
    kBigZero2Sprite = 297,
    kTeam1NameSprite = 307,
    kTeam2NameSprite = 308,
    kBigDashSprite = 309,
    kTeam1Scorer1NameSprite = 310,
    kTeam1Scorer8NameSprite = 317,
    kTeam2Scorer1NameSprite = 318,
    kTeam2Scorer8NameSprite = 325,
    kHalftimeSprite = 326,
    kFullTimeSprite = 327,
    kTimeSprite8Mins = 328,
    kTimeSprite88Mins = 329,
    kTimeSprite118Mins = 330,
    kBigTimeDigitSprite0 = 331,
    kBigTimeDigitSprite1 = 332,
    kTeam1WhitePlayerSpriteStart = 341,
    kTeam1GingerPlayerSpriteStart = 442,
    kTeam1BlackPlayerSpriteStart = 542,
    kTeam1BlackPlayerSpriteEnd = 643,
    kTeam2WhitePlayerSpriteStart = 644,
    kTeam2BlackPlayerSpriteEnd = 946,
    kTeam1MainGoalkeeperSpriteStart = 947,
    kTeam1MainGoalkeeperSpriteEnd = 1004,
    kTeam1ReserveGoalkeeperSpriteStart = 1005,
    kTeam1ReserveGoalkeeperSpriteEnd = 1062,
    kTeam2MainGoalkeeperSpriteStart = 1063,
    kTeam2MainGoalkeeperSpriteEnd = 1020,
    kTeam2ReserveGoalkeeperSpriteStart = 1121,
    kTeam2ReserveGoalkeeperSpriteEnd = 1178,
    kBallSprite1 = 1179,
    kBallSprite2 = 1180,
    kBallSprite3 = 1181,
    kBallSprite4 = 1182,
    kBallShadowSprite = 1183,
    kCornerFlagSpriteStart = 1184,
    kCornerFlagSpriteEnd = 1187,
    kSmallDigit1 = 1188,
    kSmallDigit16 = 1203,
    kPlayerMarkSprite = 1204,
    kTopGoalSprite = 1205,
    kBottomGoalSprite = 1206,
    kReplayFrame00Sprite = 1209,
    kBigSSpriteStart = 1241,
    kBigSSpriteEnd = 1272,
    kRefereeSpriteStart = 1273,
    kRefereeSpriteEnd = 1283,
    kSubstitutesArrowSprite = 1284,
    kSubstitutesLegendSprite = 1288,
    kTacticsLegendSprite = 1289,
    kRedCrossInjurySprite = 1290,
    kRedCardSprite = 1291,
    kYellowCardSprite = 1292,
    kWhiteFaceSprite = 1293,
    kGingerFaceSprite = 1294,
    kBlackFaceSprite = 1295,
    kCoach1SittingStartSprite = 1296,
    kCoach1StandingStartSprite = 1299,
    kCoach2SittingStartSprite = 1303,
    kTeam1BenchPlayerSpriteStart = 1310,
    kBenchGoalkeeperSittingWhite = 1310,
    kBenchGoalkeeperSittingGinger = 1311,
    kBenchGoalkeeperSittingBlack = 1312,
    kBenchPlayerSittingWhite = 1313,
    kBenchPlayerSittingGinger = 1314,
    kBenchPlayerSittingBlack = 1315,
    kBenchGoalkeeperStandingWhite = 1316,
    kBenchGoalkeeperStandingGinger = 1317,
    kBenchGoalkeeperStandingBlack = 1318,
    kBenchPlayerStandingWhite = 1319,
    kBenchPlayerStandingGinger = 1320,
    kBenchPlayerStandingBlack = 1321,
    kTeam2BenchPlayerSpriteStart = 1322,
    kNumSprites = 1334,
};

constexpr int kSmallDigitMaxWidth = 6;
constexpr int kSmallCursorWidth = 6;

enum GameTypes
{
    kGameTypeNoGame = 0,
    kGameTypeDiyCompetition = 1,
    kGameTypePresetCompetition = 2,
    kGameTypeCareer = 4,
    kGameTypeSeason = 3,
};

enum SwosControls
{
    kSwosKeyboardOnly = 1,
    kSwosJoypadOnly,
    kSwosJoypadAndKeyboard,
    kSwosKeyboardAndJoypad,
    kSwosTwoJoypads,
    kSwosMouse,
};

enum MenuControlMask
{
    kNoEventsMask = 0x00,
    kFireMask = 0x01,
    kLeftMask = 0x02,
    kRightMask = 0x04,
    kUpMask = 0x08,
    kDownMask = 0x10,
    kShortFireMask = 0x20,
    kUpLeftMask = 0x40,
    kDownRightMask = 0x80,
    kUpRightMask = 0x100,
    kDownLeftMask = 0x200,
};

enum PCKeys
{
    kKeyEscape     = 0x01,
    kKey1          = 0x02,
    kKey2          = 0x03,
    kKey3          = 0x04,
    kKey4          = 0x05,
    kKey5          = 0x06,
    kKey6          = 0x07,
    kKey7          = 0x08,
    kKey8          = 0x09,
    kKey9          = 0x0a,
    kKey0          = 0x0b,
    kKeyBackspace  = 0x0e,
    kKeyDelete     = 0x53,
    kKeyEnter      = 0x1c,
    kKeyLShift     = 0x2a,
    kKeyRShift     = 0x36,
    kKeyHome       = 0x47,
    kKeyEnd        = 0x4f,
    kKeyArrowLeft  = 0x4b,
    kKeyArrowRight = 0x4d,
    kKeyArrowUp    = 0x48,
    kKeyArrowDown  = 0x50,
    kKeyMinus      = 0x0c,
    kKeyKpMinus    = 0x4a,
    kKeyQ          = 0x10,
    kKeyY          = 0x15,
    kKeyN          = 0x31,
    kKeyF2         = 0x3c,
};

constexpr int kCursorChar = -1;

enum Nationalities
{
    kAlb = 0,     kPol = 26,    kSal = 52,    kGuy = 78,    kZim = 104,    kMly = 130,
    kAut = 1,     kPor = 27,    kGua = 53,    kPer = 79,    kEgy = 105,    kSau = 131,
    kBel = 2,     kRom = 28,    kHon = 54,    kAlg = 80,    kTan = 106,    kYem = 132,
    kBul = 3,     kRus = 29,    kBhm = 55,    kSaf = 81,    kNig = 107,    kKuw = 133,
    kCro = 4,     kSmr = 30,    kMex = 56,    kBot = 82,    kEth = 108,    kLao = 134,
    kCyp = 5,     kSco = 31,    kPan = 57,    kBfs = 83,    kGab = 109,    kNkr = 135,
    kTch = 6,     kSlo = 32,    kUsa = 58,    kBur = 84,    kSie = 110,    kOma = 136,
    kDen = 7,     kSwe = 33,    kBah = 59,    kLes = 85,    kBen = 111,    kPak = 137,
    kEng = 8,     kTur = 34,    kNic = 60,    kZai = 86,    kCon = 112,    kPhi = 138,
    kEst = 9,     kUkr = 35,    kBer = 61,    kZam = 87,    kGui = 113,    kChn = 139,
    kFar = 10,    kWal = 36,    kJam = 62,    kGha = 88,    kSrl = 114,    kSgp = 140,
    kFin = 11,    kYug = 37,    kTri = 63,    kSen = 89,    kMar = 115,    kMau = 141,
    kFra = 12,    kBls = 38,    kCan = 64,    kCiv = 90,    kGam = 116,    kMya = 142,
    kGer = 13,    kSvk = 39,    kBar = 65,    kTun = 91,    kMlw = 117,    kPap = 143,
    kGre = 14,    kEsp = 40,    kEls = 66,    kMli = 92,    kJap = 118,    kTad = 144,
    kHun = 15,    kArm = 41,    kSvc = 67,    kMdg = 93,    kTai = 119,    kUzb = 145,
    kIsl = 16,    kBos = 42,    kArg = 68,    kCmr = 94,    kInd = 120,    kQat = 146,
    kIsr = 17,    kAzb = 43,    kBol = 69,    kChd = 95,    kBan = 121,    kUae = 147,
    kIta = 18,    kGeo = 44,    kBra = 70,    kUga = 96,    kBru = 122,    kAus = 148,
    kLat = 19,    kSwi = 45,    kChl = 71,    kLib = 97,    kIra = 123,    kNzl = 149,
    kLit = 20,    kIrl = 46,    kCol = 72,    kMoz = 98,    kJor = 124,    kFij = 150,
    kLux = 21,    kMac = 47,    kEcu = 73,    kKen = 99,    kSri = 125,    kSol = 151,
    kMlt = 22,    kTrk = 48,    kPar = 74,    kSud = 100,   kSyr = 126,    kCus = 152,
    kHol = 23,    kLie = 49,    kSur = 75,    kSwa = 101,   kKor = 127,
    kNir = 24,    kMol = 50,    kUru = 76,    kAng = 102,   kIrn = 128,
    kNor = 25,    kCrc = 51,    kVen = 77,    kTog = 103,   kVie = 129,
};

enum Tactics
{
    kTacticDefault = 0,
    kTactic_541 = 1,
    kTactic_451 = 2,
    kTactic_532 = 3,
    kTactic_352 = 4,
    kTactic_433 = 5,
    kTactic_424 = 6,
    kTactic_343 = 7,
    kTacticSweep = 8,
    kTactic_523 = 9,
    kTacticAttack = 10,
    kTacticDefend = 11,
    kTacticUserA = 12,
    kTacticUserB = 13,
    kTacticUserC = 14,
    kTacticUserD = 15,
    kTacticUserE = 16,
    kTacticUserF = 17,
    kTacticImported = 1000,
};

enum class GameState : word
{
    kPlayersGoingToInitialPositions = 0,
    kKeeperHoldsTheBall = 3,
    kCornerLeft = 4,
    kCornerRight = 5,
    kThrowInForwardRight = 15,
    kThrowInCenterRight = 16,
    kThrowInBackRight = 17,
    kThrowInForwardLeft = 18,
    kThrowInCenterLeft = 19,
    kThrowInBackLeft = 20,
    kStartingGame = 21,
    kCameraGoingToShowers = 22,
    kGoingToHalftime = 23,
    kPlayersGoingToShower = 24,
    kResultOnHalftime = 25,
    kResultAfterTheGame = 26,
    kFirstExtraStarting = 27,
    kFirstExtraEnded = 28,
    kFirstHalfEnded = 29,
    kGameEnded = 30,
    kInProgress = 100,
    kStopped = 101,
    kWaitingOnPlayer = 102,
};

constexpr int kMenuScreenWidth = 320;
constexpr int kMenuScreenHeight = 200;
constexpr int kGameScreenWidth = 384;

constexpr int kVirtualScreenSize = 65'536;
constexpr int kMenuBackgroundOffset = 131'072;

static constexpr int kHilHeaderSize = 3'626;
constexpr int kSingleHighlightBufferSize = 19'000;

// can't keep this a constexpr in C++17 anymore, sigh...
static char *kSentinel = reinterpret_cast<char *>(-1);
