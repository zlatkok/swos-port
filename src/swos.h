#pragma once

#include "fetch.h"

using dword = uint32_t;
using word = uint16_t;
using byte = uint8_t;

#pragma pack(push, 1)
#ifdef SWOS_VM
// dependency cycle breaker
namespace SwosVM {
    extern inline dword ptrToOffset(const void *ptr);
    extern inline char *offsetToPtr(dword offset);
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
    constexpr SwosDataPointer(dword offset) : m_offset(offset) {}
    constexpr SwosDataPointer(SwosVM::Offsets offset) : m_offset(static_cast<dword>(offset)) {}
    SwosDataPointer& operator=(Type *t) {
        assert(reinterpret_cast<uintptr_t>(&m_offset) % 4 == 0);
        m_offset = SwosVM::ptrToOffset(t);
        return *this;
    }
    template<typename PtrType, std::enable_if_t<std::is_pointer<PtrType>::value, int> = 0>
    void loadFrom(PtrType ptr) {
        auto offset = fetch((dword *)ptr);
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
    SwosDataPointer& operator--(int) {
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
    template <typename T> T asAligned() const {
        auto offset = fetch(&m_offset);
        assert((SwosVM::offsetToPtr(offset), true));
        return reinterpret_cast<T>(SwosVM::offsetToPtr(offset));
    }
    const Type *asConst() const { return get(); }
    Type *asPtr() const { return (Type *)get(); }
    const char *asCharPtr() const { return (const char *)get(); }
    const char *asAlignedConstCharPtr() const { return asAligned<const char *>(); }
    char *asAlignedCharPtr() { return asAligned<char *>(); }
    char *asCharPtr() { return (char *)get(); }
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
    explicit operator bool() { return m_index && m_index != -1; }
    void operator()() { SwosVM::invokeProc(m_index); }
    bool operator==(SwosVM::VoidFunction proc) const {
        return SwosVM::fetchProc(m_index) == proc;
    }
};

#else
# error "Define SWOS Proc & Data Pointers"
#endif

struct MenuEntry;

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

struct Menu
{
    SwosProcPointer onInit;
    SwosProcPointer onReturn;
    SwosProcPointer onDraw;
    SwosDataPointer<MenuEntry> selectedEntry;
    word numEntries;
    SwosDataPointer<char> endOfMenuPtr;

    MenuEntry *entries() const {
        return (MenuEntry *)(this + 1);
    }
};

enum MenuEntryBackgroundType : word
{
    kEntryNoBackground = 0,
    kEntryFunc1 = 1,
    kEntryFrameAndBackColor = 2,
    kEntrySprite1 = 3,
};

enum MenuEntryContentType : word
{
    kEntryNoForeground = 0,
    kEntryFunc2 = 1,
    kEntryString = 2,
    kEntrySprite2 = 3,
    kEntryStringTable = 4,
    kEntryMultiLineText = 5,
    kEntryNumber = 6,
    kEntrySpriteCopy = 7,
};

template<typename T>
const char **getTrailingStrings(const T *t)
{
    return (const char **)(t + 1);
}

struct StringTable
{
    SwosDataPointer<int16_t> index;
    int16_t initialValue;
    // followed by char pointers

    StringTable(int16_t *index, int16_t initialValue) : index(index), initialValue(initialValue) {}
};

struct StringTableNative
{
    union {
        int16_t *nativeIndex;
        SwosDataPointer<int16_t> index;
    };
    int16_t initialValue;
    int16_t numPointers;
    // followed by char pointers
    // followed by bool array marking which pointer is native

    constexpr StringTableNative(int16_t *indexPtr, int16_t initialValue, int16_t numPointers)
        : nativeIndex(indexPtr), initialValue(initialValue), numPointers(numPointers) {}

    const char *operator[](size_t index) const {
        assert(index < static_cast<size_t>(numPointers));
        return fetch<char *>(getTrailingStrings(this) + index);
    }

    const bool *getNativeFlags() const {
        return reinterpret_cast<const bool *>(getTrailingStrings(this) + numPointers);
    }

    bool isIndexPointerNative() const {
        return getNativeFlags()[0];
    }

    bool isNativeString(size_t index) const {
        assert(index < static_cast<size_t>(numPointers));
        return getNativeFlags()[index + 1];
    }
};

struct MultiLineTextNative
{
    byte numLines;
    // followed by char pointers

    const char *operator[](size_t index) const {
        assert(index < numLines);
        return fetch<char *>(getTrailingStrings(this) + index);
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

struct MenuEntry
{
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
    union BackgroundData {
        BackgroundData() {}
        SwosProcPointer entryFunc;
        word entryColor;
        word spriteIndex;
    } u1;
    MenuEntryContentType type2;
    word stringFlags;
    union ContentData {
        ContentData() {}
        SwosProcPointer entryFunc2;
        SwosDataPointer<char> string;
        SwosDataPointer<const char> constString;
        word spriteIndex;
        SwosDataPointer<StringTable> stringTable;
        SwosDataPointer<void> multiLineText;
        word number;
        SwosDataPointer<void> spriteCopy;
    } u2;
    SwosProcPointer onSelect;
    int16_t controlMask;
    SwosProcPointer beforeDraw;
    SwosProcPointer onReturn;

    MenuEntry() {}

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
        case kEntrySpriteCopy: return "sprite copy";
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
        return u2.string.asAlignedCharPtr();
    }
    const char *string() const {
        assert(type2 == kEntryString);
        return u2.string.asAlignedConstCharPtr();
    }
    void setString(char *str) {
        assert(type2 == kEntryString);
        u2.string.set(str);
    }
    void setBackgroundColor(int color) {
        u1.entryColor = color;
    }
};

constexpr int kStdMenuTextSize = 70;

/* sprite graphics structure - from *.dat files */
struct SpriteGraphics {
    SwosDataPointer<char> data; /* pointer to actual graphics                       */
    char unk1[6];               /* unknown                                          */
    int16_t width;              /* width                                            */
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

struct Sprite
{
    word teamNumber;    // 1 or 2 for player controls, 0 for CPU
    word shirtNumber;   // 1-11 for players, 0 for other sprites
    word frameOffset;
    SwosDataPointer<void> animTablePtr;
    word startingDirection;
    byte playerState;
    byte playerDownTimer;
    word unk001;
    word unk002;
    SwosDataPointer<void> frameIndicesTable;
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

struct PlayerGame
{
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

struct TeamFile
{
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

struct PlayerFileHeader : private TeamFile, public PlayerFile {
};

static_assert(sizeof(PlayerFileHeader) == sizeof(TeamFile) + sizeof(PlayerFile), "PlayerFileHeader invalid");

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
    PlayerGame players[16];
    byte unknownTail[686];
};

struct PlayerGameHeader : private TeamGame, public PlayerGame {
};

static_assert(sizeof(PlayerGameHeader) == sizeof(TeamGame) + sizeof(PlayerGame), "PlayerGameHeader invalid");

struct TeamGeneralInfo
{
    SwosDataPointer<TeamGeneralInfo> opponentsTeam;
    word playerNumber;
    word plCoachNum;
    word isPlCoach;
    SwosDataPointer<TeamGame> inGameTeamPtr;
    SwosDataPointer<TeamStatsData> teamStatsPtr;
    word teamNumber;
    SwosDataPointer<Sprite[11]> players;
    SwosDataPointer<void> someTablePtr;
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
    byte joy1SecondaryFire;
};
constexpr int xsrf = sizeof(TeamGeneralInfo);
static_assert(sizeof(TeamGeneralInfo) == 145, "TeamGeneralInfo is invalid");
#pragma pack(pop)

enum SpriteIndices
{
    kUpArrowSprite = 183,
    kDownArrowSprite = 184,
    kMaxMenuSprite = 226,
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
    kNumSprites = 1334,
};

enum GameTypes
{
    kGameTypeNoGame = 0,
    kGameTypeDiyCompetition = 1,
    kGameTypePresetCompetition = 2,
    kGameTypeCareer = 4,
    kGameTypeSeason = 3,
};

enum TextColors
{
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

enum MenuEntryBackgrounds
{
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
static char *kSentinel = reinterpret_cast<char *>(-1);
