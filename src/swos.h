#pragma once

typedef uint32_t dword;
typedef uint16_t word;
typedef uint8_t byte;

#pragma pack(push, 1)
struct MenuEntry;

struct BaseMenu {};
struct PackedMenu : BaseMenu {};

struct Menu {
    void(*onInit)();
    void(*onReturn)();
    void(*onDraw)();
    MenuEntry *selectedEntry;
    word numEntries;
    char *endOfMenuPtr;
};

enum MenuEntryBackgroundType : word {
    kEntryNoBackground = 0,
    kEntryFunc1 = 1,
    kEntryFrameAndBackColor = 2,
    kEntrySprite1 = 3,
};

enum MenuEntryContentType : word {
    kEntryNoForeground = 0,
    kEntryFunc2 = 1,
    kEntryString = 2,
    kEntrySprite2 = 3,
    kEntryStringTable = 4,
    kEntryMultiLineText = 5,
    kEntryNumber = 6,
    kEntrySpriteCopy = 7,
};

struct StringTable {
    int16_t *index;
    int16_t initialValue;
    // followed by string pointers

    StringTable(int16_t *index, int16_t initialValue) : index(index), initialValue(initialValue) {}
    char **getStringTable() const {
        return (char **)((char *)this + sizeof(*this));
    }
    char *currentString() const {
        if (!index || *index < 0)
            return nullptr;
        else
            return getStringTable()[*index];
    }
};

struct CharTable {
    word unk1;
    word charHeight;
    dword unk2;
    word charSpacing;
    word spaceWidth;
    word spriteIndexOffset;
    char conversionTable[224];
};

struct MenuEntry {
    word drawn;
    word ordinal;
    word invisible;
    word disabled;
    byte leftEntry;
    byte rightEntry;
    byte upEntry;
    byte downEntry;
    byte leftDirection;
    byte rightDirection;
    byte upDirection;
    byte downDirection;
    byte leftEntryDis;
    byte rightEntryDis;
    byte upEntryDis;
    byte downEntryDis;
    int16_t x;
    int16_t y;
    word width;
    word height;
    MenuEntryBackgroundType type1;
    union {
        void (*entryFunc)(word, word);
        word entryColor;
        word spriteIndex;
    } u1;
    MenuEntryContentType type2;
    word textColor;
    union {
        void (*entryFunc2)(word, word);
        char *string;
        const char *constString;
        word spriteIndex;
        StringTable *stringTable;
        void *multiLineText;
        word number;
        void *spriteCopy;
    } u2;
    void (*onSelect)();
    int16_t controlMask;
    void (*beforeDraw)();
    void (*afterDraw)();

    enum Direction {
        kInitialDirection,
        kUp = 0,
        kRight,
        kDown,
        kLeft,
        kNumDirections,
    };

    const char *typeToString() const {
        switch (type2) {
        case kEntryNoForeground: return "empty";
        case kEntryFunc2: return "function";
        case kEntryString: return "string";
        case kEntrySprite2: return "sprite";
        case kEntryStringTable: return "string table";
        case kEntryMultiLineText: return "multi-line string";
        case kEntryNumber: return "number";
        case kEntrySpriteCopy: "sprite copy";
        default: assert(false); return "";
        }
    }

    bool visible() const {
        return !invisible;
    }
    void hide() {
        invisible = 1;
    }
    void show() {
        invisible = 0;
    }
    void setVisible(bool visible) {
        invisible = !visible;
    }
    void disable() {
        disabled = 1;
    }
    void enable() {
        disabled = 0;
    }
    void setEnabled(bool enabled) {
        disabled = !enabled;
    }
    bool selectable() const {
        return !invisible && !disabled;
    }
    bool isString() const {
        return type2 == kEntryString;
    }
    char *string() {
        assert(type2 == kEntryString);
        return u2.string;
    }
    const char *string() const {
        assert(type2 == kEntryString);
        return u2.string;
    }
    void setString(char *str) {
        assert(type2 == kEntryString);
        u2.string = str;
    }
    void setBackgroundColor(int color) {
        u1.entryColor = color;
    }
};

constexpr int kStdMenuTextSize = 70;

/* sprite graphics structure - from *.dat files */
struct SpriteGraphics {
    char *data;         /* pointer to actual graphics                       */
    char unk1[6];       /* unknown                                          */
    int16_t width;      /* width                                            */
    int16_t height;     /* height                                           */
    int16_t wquads;     /* (number of bytes / 8) in one line                */
    int16_t centerX;    /* center x coordinate                              */
    int16_t centerY;    /* center y coordinate                              */
    byte unk2;          /* unknown                                          */
    byte nlinesDiv4;    /* height / 4                                       */
    short ordinal;      /* ordinal number in sprite.dat                     */

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

struct Sprite {
    word teamNumber;    // 1 or 2 for player controls, 0 for CPU
    word shirtNumber;   // 1-11 for players, 0 for other sprites
    word frameOffset;
    void *animTablePtr;
    word startingDirection;
    byte playerState;
    byte playerDownTimer;
    word unk001;
    word unk002;
    void *frameIndicesTable;
    word frameIndex;
    word frameDelay;
    word cycleFramesTimer;
    word delayedFrameTimer;
    int16_t xFraction;      // fixed point, 16.16, signed, whole part high word
    int16_t x;
    int16_t yFraction;
    int16_t y;
    int16_t zFraction;
    int16_t z;
    int16_t direction;
    int16_t speed;
    dword deltaX;
    dword deltaY;
    dword deltaZ;
    int16_t destX;
    int16_t destY;
    byte unk003[6];
    word visible;           // skip it when rendering if false
    int16_t spriteIndex;    // -1 if none
    word saveSprite;
    dword ballDistance;
    word unk004;
    word unk005;
    word fullDirection;
    word beenDrawn;
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
};

struct PlayerGame {
    byte substituted;
    byte index;
    byte goalsScored;
    byte shirtNumber;
    int8_t position;
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
};

struct TeamStatsData {
    word ballPossession;
    word cornersWon;
    word foulsConceded;
    word bookings;
    word sendingsOff;
    word goalAttempts;
    word onTarget;
};

enum TeamControls : byte {
    kTeamNotSelected = 0,
    KComputerTeam = 1,
    kPlayerCoach = 2,
    kCoach = 3,
};

struct TeamFile {
    byte teamFileNo;
    byte teamOrdinal;
    word globalTeamNumber;
    TeamControls teamControls;
    byte teamName [17];
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
    byte playerNumbers[16];
};

static_assert(sizeof(TeamFile) == 76, "TeamFile is invalid");

struct PlayerFile {
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

struct PlayerFileHeader : private TeamFile, public PlayerFile {
};

static_assert(sizeof(PlayerFileHeader) == sizeof(TeamFile) + sizeof(PlayerFile), "PlayerFileHeader invalid");

struct TeamGame {
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
    PlayerGame players[16];
    byte unknownTail[686];
};

struct PlayerGameHeader : private TeamGame, public PlayerGame {
};

static_assert(sizeof(PlayerGameHeader) == sizeof(TeamGame) + sizeof(PlayerGame), "PlayerGameHeader invalid");

struct TeamGeneralInfo {
    TeamGeneralInfo *opponentsTeam;
    word playerNumber;
    word plCoachNum;
    word isPlCoach;
    TeamGame *inGameTeamPtr;
    TeamStatsData *teamStatsPtr;
    word teamNumber;
    Sprite *(*players)[11];
    void *someTablePtr;
    word tactics;
    word tensTimer;
    Sprite *controlledPlayerSprite;
    Sprite *passToPlayerPtr;
    word playerHasBall;
    word playerHadBall;
    word currentAllowedDirection;
    word direction;
    byte quickFire;
    byte normalFire;
    byte joyIsFiring;
    byte joyTriggered;
    word header;
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
    Sprite *lastHeadingPlayer;
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
    Sprite *passingKickingPlayer;
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
    byte joy1SecondaryFire;
};
static_assert(sizeof(TeamGeneralInfo) == 145, "TeamGeneralInfo is invalid");
#pragma pack(pop)

enum SpriteIndices {
    kUpArrowSprite = 183,
    kDownArrowSprite = 184,
    kSquareGridForResultSprite = 252,
    kBigZeroSprite = 287,
    kBigZero2Sprite = 297,
    kTeam1NameSprite = 307,
    kTeam2NameSprite = 308,
    kBigDashSprite = 309,
    kTimeSprite8Mins = 328,
    kTimeSprite88Mins = 329,
    kTimeSprite118Mins = 330,
    kBigTimeDigitSprite0 = 331,
    kReplayFrame00Sprite = 1209,
    kEndSprite = 1334,
};

enum GameTypes {
    kGameTypeNoGame = 0,
    kGameTypeDiyCompetition = 1,
    kGameTypePresetCompetition = 2,
    kGameTypeCareer = 4,
    kGameTypeSeason = 3,
};

enum TextColors {
    kWhiteText = 0,
    kDarkGrayText = 1,
    kWhiteText2 = 2,
    kBlackText = 3,
    kBrownText = 4,
    kLightBrownText = 5,
    kOrangeText = 6,
    kGrayText = 7,
    kNearBlackText = 8,
    kVeryDarkGreenText = 9,
    kRedText = 10,
    kBlueText = 11,
    kPurpleText = 12,
    kSoftBlueText = 13,
    kGreenText = 14,
    kYellowText = 15,
    kLeftAligned = 1 << 15, kRightAligned = 1 << 14, kShowText = 1 << 9, kBlinkText = 1 << 13,
    kBigText = 1 << 4, kBigFont = 1 << 4,
};

enum MenuEntryBackgrounds {
    kNoBackground = 0,
    kNoFrame = 0,
    kGray = 7,
    kDarkBlue = 3,
    kLightBrownWithOrangeFrame = 4,
    kLightBrownWithYellowFrame = 6,
    kRed = 10,
    kPurple = 11,
    kLightBlue = 13,
    kGreen = 14,
};

enum PCKeys {
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

enum Nationalities {
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

enum Tactics {
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

enum class GameState : word {
    kGameStarting = 0,
    kInProgress = 100,
    kStopped = 101,
    kWaitingOnPlayer = 102,
};

constexpr int kMenuScreenWidth = 320;
constexpr int kGameScreenWidth = 384;

constexpr int kVirtualScreenSize = 65'536;

static constexpr int kHilHeaderSize = 3'626;
constexpr int kSingleHighlightBufferSize = 19'000;

// can't keep this a constexpr in C++17 anymore, sigh...
#if _HAS_CXX17
static
#else
constexpr
#endif
char *kSentinel = reinterpret_cast<char *>(-1);
