#pragma once

typedef uint32_t dword;
typedef uint16_t word;
typedef uint8_t byte;

#pragma pack(push, 1)
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
    word type1;
    union {
        void (*entryFunc)(word, word);
        word entryColor;
        word spriteIndex;
    } u1;
    word type2;
    word textColor;
    union {
        void (*entryFunc2)(word, word);
        char *string;
        word spriteIndex;
        void *stringTable;
        void *multiLineText;
        word number;
        void *spriteCopy;
    } u2;
    void (*onSelect)();
    word controlMask;
    void (*beforeDraw)();
    void (*afterDraw)();
};

struct Menu {
    void (*onInit)();
    void (*afterDraw)();
    void (*onDraw)();
    MenuEntry *selectedEntry;
    word numEntries;
    char *endOfMenuPtr;
};

/* sprite graphics structure - from dat files */
struct SpriteGraphics {
    char  *data;        /* pointer to actual graphics                       */
    char  unk1[6];      /* unknown                                          */
    int16_t width;      /* width                                            */
    int16_t nlines;     /* height                                           */
    int16_t wquads;     /* (number of bytes / 8) in one line                */
    int16_t centerX;    /* center x coordinate                              */
    int16_t centerY;    /* center y coordinate                              */
    byte unk2;          /* unknown                                          */
    byte nlinesDiv4;    /* nlines / 4                                       */
    short ordinal;      /* ordinal number in sprite.dat                     */

    int size() const {
        return sizeof(SpriteGraphics) + nlines * wquads * 8;
    }

    SpriteGraphics *next() const {
        return reinterpret_cast<SpriteGraphics *>((char *)this + size());
    }
};

struct Player {
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
    word visible;       // skip it when rendering if false
    int16_t pictureIndex; // -1 if none
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

struct TeamGeneralInfo {
    TeamGeneralInfo *opponentsTeam;
    word playerNumber;
    word plCoachNum;
    word isPlCoach;
    TeamGame *inGameTeamPtr;
    TeamStatsData *teamStatsPtr;
    word teamNumber;
    Player *(*players)[11];
    void *someTablePtr;
    word tactics;
    word tensTimer;
    Player *controlledPlayerSprite;
    Player *passToPlayerPtr;
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
    Player *lastHeadingPlayer;
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
    Player *passingKickingPlayer;
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

enum MenuEntryContentType {
    NO_FOREGROUND = 0,
    ENTRY_FUNC2 = 1,
    ENTRY_STRING = 2,
    ENTRY_SPRITE2 = 3,
    ENTRY_STRING_TABLE = 4,
    ENTRY_MULTI_LINE_TEXT = 5,
    ENTRY_NUMBER = 6,
    ENTRY_SPRITE_COPY = 7,
};

enum SpriteIndices {
    SPR_SQUARE_GRID_FOR_RESULT = 252,
    SPR_BIG_0 = 287,
    SPR_2BIG_0 = 297,
    SPR_BIG_DASH = 309,
    SPR_REPLAY_FRAME_00 = 1209,
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
