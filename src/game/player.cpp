#include "player.h"
#include "animation.h"
#include "ball.h"
#include "team.h"
#include "comments.h"

using namespace SwosVM;

static const std::array<int8_t, 16> kControlledBallJiggleOffsets = {
    0, -1, 1, -1, 1, 0, 1, 1, 0, 1, -1, 1, -1, 0, -1, -1,
};
static constexpr int kGoalkeeperStrongDeflectBallSpeed = 1536;
static constexpr int kGoalkeeperMediumDeflectBallSpeed = 1024;
static constexpr int kGoalkeeperWeakDeflectBallSpeed = 512;
static constexpr int kGoalkeeperDeflectDeltaZ = 49152;

static void doLobHeader();
static void getClosestNonControlledPlayerInDirection();
static void setPlayerJumpHeaderHitAnimationTable();
static void playStopGoodPassSampleIfNeeded();
static void stopGoodPassSample();
static void enqueuePlayingGoodPassSample();

void updatePlayerSpeedAndFrameDelay(const TeamGeneralInfo& team, Sprite& player)
{
    if (player.state != PlayerState::kNormal ||
        (swos.gameStatePl == GameState::kInProgress && player.playerOrdinal == 1 && &player != team.controlledPlayer))
        return;

    static const int kPlayerSpeedsGameInProgress[] = { 928, 974, 1020, 1066, 1112, 1158, 1204, 1250 };
    static const int kPlayerSpeedsGameStopped[] = { 1136, 1152, 1168, 1184, 1200, 1216, 1232, 1248 };

    static_assert(std::size(kPlayerSpeedsGameInProgress) == std::size(kPlayerSpeedsGameStopped));

    auto speedTable = swos.gameStatePl == GameState::kInProgress ? kPlayerSpeedsGameInProgress : kPlayerSpeedsGameStopped;
    auto& playerInfo = getPlayerPointerFromShirtNumber(team, player);

    assert(static_cast<unsigned>(playerInfo.speed) < std::size(kPlayerSpeedsGameInProgress));
    player.speed = speedTable[playerInfo.speed];

    if (swos.runSlower) {
        // set speed to 62.5% (running slower when goal is scored)
        player.speed = 5 * player.speed / 8;
    }

    if ((team.playerNumber || team.playerCoachNumber) && player.injuryLevel) {
        static const int kInjuriesSpeedHandicap[] = { 0, -96, -128, -160, -192, -224, -256, -288 };
        assert(player.injuryLevel / 32 < std::size(kInjuriesSpeedHandicap));
        player.speed += kInjuriesSpeedHandicap[player.injuryLevel / 32];
    }

    if (&player == team.controlledPlayer && team.playerHasBall) {
        // speed down to 87.5% - this is the player that controls the ball
        player.speed -= player.speed / 8;
    }

    if (team.playerNumber && &player == team.passToPlayerPtr && team.passingToPlayer &&
        (team.longPass || team.leftSpin || team.rightSpin) && swos.ballSprite.speed)
    {
        assert(swos.ballSprite.fullDirection <= 255 && player.fullDirection <= 255);
        int8_t directionDiff = swos.ballSprite.fullDirection - player.fullDirection;
        if (directionDiff <= 7 && directionDiff >= -7) {
            // set slower or faster speed depending on how much the player and ball direction overlap
            int speed = directionDiff >= -5 && directionDiff <= 5 ? 256 : 512;
            player.speed = speed;
        }
    }

    if (swos.gameStatePl != GameState::kInProgress) {
        if (swos.gameState == GameState::kFirstHalfEnded || swos.gameState == GameState::kGameEnded) {
            // slow down, and stop players gradually; slowdown in greater steps than accelerate when leaving
            player.speed = std::max(player.speed - swos.stoppageTimerTotal * 32, 0);
        } else if (swos.gameState == GameState::kGoingToHalftime || swos.gameState == GameState::kPlayersGoingToShower) {
            // player speed weighted with time elapsed from game/half end
            int leavePitchSlowdownFactor = std::min(swos.stoppageTimerTotal * 4, 100);
            // make players run gradually faster overt time when they're leaving pitch
            player.speed = player.speed * leavePitchSlowdownFactor / 100;
        }
    }

    // subtract from max, so slow players would have greater animation delay
    constexpr int kMaxSpeed = 1280;
    player.frameDelay = std::max(kMaxSpeed - player.speed, 0) / 128 + 6;
}

// in:
//      A1 -> sprite
//
// Set coordinates of player that has the ball. Make him dodge
// a little, by varying x and y coordinates by 1.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void updatePlayerWithBall()
{
    A0 = 328492;                            // mov A0, offset ballSprite
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    A0 = 516362;                            // mov A0, offset kPlayerWithBallOffsets
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D2 = res;
    }                                       // add word ptr D2, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 32, 2, ax);           // mov word ptr [esi+(Sprite.x+2)], ax
    ax = D2;                                // mov ax, word ptr D2
    writeMemory(esi + 36, 2, ax);           // mov word ptr [esi+(Sprite.y+2)], ax
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = D2;                                // mov ax, word ptr D2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
}

// in:
//      A1 -> sprite (player)
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void updateControllingPlayer()
{
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    A0 = 516178;                            // mov A0, offset kControlledBallJiggleOffsets
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D2 = res;
    }                                       // add word ptr D2, ax
    A0 = 328492;                            // mov A0, offset ballSprite
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 32, 2, ax);           // mov word ptr [esi+(Sprite.x+2)], ax
    ax = D2;                                // mov ax, word ptr D2
    writeMemory(esi + 36, 2, ax);           // mov word ptr [esi+(Sprite.y+2)], ax
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = D2;                                // mov ax, word ptr D2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    writeMemory(esi + 40, 2, 0);            // mov word ptr [esi+(Sprite.z+2)], 0
    eax = readMemory(esi + 54, 4);          // mov eax, [esi+Sprite.deltaZ]
    D0 = eax;                               // mov D0, eax
    {
        int32_t res = *(int32_t *)&D0 >> 1;
        flags.carry = ((dword)*(int32_t *)&D0 >> 31) & 1;
        *(int32_t *)&D0 = res;
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // sar D0, 1
    eax = D0;                               // mov eax, D0
    writeMemory(esi + 54, 4, eax);          // mov [esi+Sprite.deltaZ], eax
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
}

// in:
//      A1 -> sprite of player that holds the ball
//
// Set ball coordinates in regard to player that controls it. Ball is
// always about a pixel in front of player, wherever he may be turned.
// If it's in the air, steady it down.
//
void updateBallWithControllingGoalkeeper(const Sprite& player)
{
    assert(player.direction >= 0 && player.direction <= 7);

    int ballX = player.x.whole() + kControlledBallJiggleOffsets[player.direction * 2];
    int ballY = player.y.whole() + kControlledBallJiggleOffsets[player.direction * 2 + 1];

    auto& ball = getBallSprite();
    ball.speed = 0;
    ball.x.setWhole(ballX);
    ball.y.setWhole(ballY);
    ball.destX = ballX;
    ball.destY = ballY;
    ball.deltaZ >>= 1;
    if (ball.deltaZ.raw() > 0)
        ball.deltaZ.setRaw(-ball.deltaZ.raw());

    resetBothTeamSpinTimers();
}

// in:
//      D0 =  current player direction
//      A6 -> team data
//      A1 -> current player sprite
//
// When running.
// Sets team's direction of player controlling the ball.
// Updates the ball when controlled by the player (makes it "run away" a bit).
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void calculateIfPlayerWinsBall()
{
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 128, 2, 0);           // mov [esi+TeamGeneralInfo.passInProgress], 0
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 138, 2);    // mov ax, [esi+TeamGeneralInfo.wonTheBallTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_set_team_direction;          // jnz @@set_team_direction

    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_set_team_direction;          // jz @@set_team_direction

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_set_team_direction;          // js @@set_team_direction

    push(D0);                               // push D0
    push(D1);                               // push D1
    push(A1);                               // push A1
    push(A6);                               // push A6
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A1 = eax;                               // mov A1, eax
    {
        int32_t dstSigned = A1;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A1, 0
    if (flags.zero)
        goto l_no_opponent_controlled_player; // jz @@no_opponent_controlled_player

    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 72, 1);     // mov al, [esi+PlayerGameHeader.tackling]
    *(byte *)&D1 = al;                      // mov byte ptr D1, al
    al = (byte)readMemory(esi + 73, 1);     // mov al, [esi+PlayerGameHeader.ballControl]
    {
        int8_t dstSigned = *(byte *)&D1;
        int8_t srcSigned = al;
        byte res = dstSigned + srcSigned;
        *(byte *)&D1 = res;
    }                                       // add byte ptr D1, al
    {
        byte res = *(byte *)&D1 >> 1;
        *(byte *)&D1 = res;
    }                                       // shr byte ptr D1, 1
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A6 = eax;                               // mov A6, eax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A1 = eax;                               // mov A1, eax
    {
        int32_t dstSigned = A1;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A1, 0
    if (flags.zero)
        goto l_no_opponent_controlled_player; // jz @@no_opponent_controlled_player

    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 72, 1);     // mov al, [esi+PlayerGameHeader.tackling]
    *(byte *)&D0 = al;                      // mov byte ptr D0, al
    al = (byte)readMemory(esi + 73, 1);     // mov al, [esi+PlayerGameHeader.ballControl]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned + srcSigned;
        *(byte *)&D0 = res;
    }                                       // add byte ptr D0, al
    {
        byte res = *(byte *)&D0 >> 1;
        *(byte *)&D0 = res;
    }                                       // shr byte ptr D0, 1
    al = D0;                                // mov al, byte ptr D0
    {
        int8_t dstSigned = *(byte *)&D1;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        *(byte *)&D1 = res;
    }                                       // sub byte ptr D1, al
    if (!flags.sign)
        goto cseg_7A26D;                    // jns short cseg_7A26D

    flags.carry = *(int8_t *)&D1 != 0;
    flags.overflow = *(int8_t *)&D1 == INT8_MIN;
    *(int8_t *)&D1 = -*(int8_t *)&D1;
    flags.sign = (*(int8_t *)&D1 & 0x80) != 0;
    flags.zero = *(int8_t *)&D1 == 0;       // neg byte ptr D1
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A6 = eax;                               // mov A6, eax

cseg_7A26D:;
    al = D1;                                // mov al, byte ptr D1
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    A0 = 516170;                            // mov A0, offset kPlAvgTacklingBallControlDiffChance
    SWOS::Rand();                           // call Rand
    {
        word res = *(word *)&D0 & 31;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 1Fh
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D1;                     // movzx ebx, word ptr D1
    al = (byte)readMemory(esi + ebx, 1);    // mov al, [esi+ebx]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, al
    if (flags.carry)
        goto l_init_ball_winner_team;       // jb short @@init_ball_winner_team

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A6 = eax;                               // mov A6, eax

l_init_ball_winner_team:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 138, 2, 12);          // mov [esi+TeamGeneralInfo.wonTheBallTimer], 12
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    pop(A6);                                // pop A6
    pop(A1);                                // pop A1
    pop(D1);                                // pop D1
    pop(D0);                                // pop D0
    return;                                 // jmp @@out

l_no_opponent_controlled_player:;
    pop(A6);                                // pop A6
    pop(A1);                                // pop A1
    pop(D1);                                // pop D1
    pop(D0);                                // pop D0

l_set_team_direction:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 56, 2, ax);           // mov [esi+TeamGeneralInfo.controlledPlDirection], ax
    A2 = 328492;                            // mov A2, offset ballSprite
    esi = A2;                               // mov esi, A2
    eax = readMemory(esi + 54, 4);          // mov eax, [esi+Sprite.deltaZ]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (flags.zero || flags.sign != flags.overflow)
        goto cseg_7A362;                    // jle short cseg_7A362

    writeMemory(esi + 54, 4, -1);           // mov [esi+Sprite.deltaZ], -1

cseg_7A362:;
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 0;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 0
    if (!flags.zero)
        goto l_set_player_destination;      // jnz short @@set_player_destination

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D2 = res;
    }                                       // sub word ptr D2, ax
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 4
    if (flags.sign == flags.overflow)
        goto l_set_player_destination;      // jge short @@set_player_destination

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = -4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, -4
    if (flags.zero || flags.sign != flags.overflow)
        goto l_set_player_destination;      // jle short @@set_player_destination

    ax = D2;                                // mov ax, word ptr D2
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_nudge_ball_right;            // jns short @@nudge_ball_right

    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 32, 2, src);
    }                                       // sub word ptr [esi+(Sprite.x+2)], 1
    goto l_set_player_destination;          // jmp short @@set_player_destination

l_nudge_ball_right:;
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 32, 2, src);
    }                                       // add word ptr [esi+(Sprite.x+2)], 1

l_set_player_destination:;
    A0 = 515768;                            // mov A0, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    *(word *)&D1 = 0;                       // mov word ptr D1, 0
    {
        byte src = g_memByte[323628];
        byte res = src & 2;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // test byte ptr currentTick, 2
    if (flags.zero)
        goto l_update_ball_speed;           // jz short @@update_ball_speed

    A0 = 516154;                            // mov A0, offset kBallSpeedDeltaWhenControlled
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 73, 1);     // mov al, [esi+PlayerGameHeader.ballControl]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        word res = *(word *)&D1 << 1;
        *(word *)&D1 = res;
    }                                       // shl word ptr D1, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D1;                     // movzx ebx, word ptr D1
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax

l_update_ball_speed:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 82, 2);     // mov ax, [esi+Sprite.fullDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = -128;
        byte res = dstSigned + srcSigned;
        *(byte *)&D0 = res;
    }                                       // add byte ptr D0, 128
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        word res = *(word *)&D1 << 5;
        *(word *)&D1 = res;
    }                                       // shl word ptr D1, 5
    al = D1;                                // mov al, byte ptr D1
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D0 = res;
    }                                       // sub byte ptr D0, al
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = 64;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, 64
    if (flags.sign == flags.overflow)
        goto cseg_7A517;                    // jge short cseg_7A517

    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = -64;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, 192
    if (!flags.zero && flags.sign == flags.overflow)
        goto cseg_7A523;                    // jg short cseg_7A523

cseg_7A517:;
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 256;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // add [esi+Sprite.speed], 256

cseg_7A523:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 56, 2);     // mov ax, [esi+TeamGeneralInfo.controlledPlDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (flags.zero)
        goto l_reset_ball_spin;             // jz short @@reset_ball_spin

    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 108, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 108, 2, src);
    }                                       // add [esi+TeamGeneralInfo.unkBallTimer], 1
    A0 = 516106;                            // mov A0, offset dseg_17E276
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 73, 1);     // mov al, [esi+PlayerGameHeader.ballControl]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 108, 2);    // mov ax, [esi+TeamGeneralInfo.unkBallTimer]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.carry && !flags.zero)
        goto l_reset_ball_spin;             // ja short @@reset_ball_spin

    writeMemory(esi + 138, 2, 8);           // mov [esi+TeamGeneralInfo.wonTheBallTimer], 8

l_reset_ball_spin:;
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
}

// in:
//      A1 -> player sprite
//      A6 -> team
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void playerKickingBall()
{
    *(word *)&g_memByte[449514] = 0;        // mov stateGoal, 0
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A0 = eax;                               // mov A0, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 56, 2, ax);           // mov [esi+TeamGeneralInfo.controlledPlDirection], ax
    auto table = getBallDestCoordinatesTable();          // call GetBallDestCoordinatesTable
    A2 = 328492;                            // mov A2, offset ballSprite
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = table[ebx].x;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = table[ebx].y;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = *(word *)&g_memByte[325438];       // mov ax, kBallKickingSpeed
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    eax = *(dword *)&g_memByte[325434];     // mov eax, kBallKickingDeltaZ
    writeMemory(esi + 54, 4, eax);          // mov [esi+Sprite.deltaZ], eax
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 100;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameStatePl, ST_GAME_IN_PROGRESS
    if (flags.zero)
        goto l_game_in_progress;            // jz short @@game_in_progress

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 15;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_THROW_IN_FORWARD_RIGHT
    if (flags.carry)
        goto l_game_in_progress;            // jb short @@game_in_progress

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 20;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_THROW_IN_BACK_LEFT
    if (flags.carry || flags.zero)
        return;                             // jbe @@out

l_game_in_progress:;
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (flags.zero)
        goto l_left_team;                   // jz short @@left_team

    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 342;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 342
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_not_a_shot_on_goal;          // jg @@not_a_shot_on_goal

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 56, 2);     // mov ax, [esi+TeamGeneralInfo.controlledPlDirection]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_possible_shot_on_goal;       // jz short @@possible_shot_on_goal

    {
        word src = (word)readMemory(esi + 56, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.controlledPlDirection], 1
    if (flags.zero)
        goto l_possible_shot_on_goal;       // jz short @@possible_shot_on_goal

    {
        word src = (word)readMemory(esi + 56, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 7;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.controlledPlDirection], 7
    if (flags.zero)
        goto l_possible_shot_on_goal;       // jz short @@possible_shot_on_goal

    goto l_not_a_shot_on_goal;              // jmp @@not_a_shot_on_goal

l_left_team:;
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 556;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 556
    if (flags.sign != flags.overflow)
        goto l_not_a_shot_on_goal;          // jl @@not_a_shot_on_goal

    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 56, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.controlledPlDirection], 4
    if (flags.zero)
        goto l_possible_shot_on_goal;       // jz short @@possible_shot_on_goal

    {
        word src = (word)readMemory(esi + 56, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.controlledPlDirection], 3
    if (flags.zero)
        goto l_possible_shot_on_goal;       // jz short @@possible_shot_on_goal

    {
        word src = (word)readMemory(esi + 56, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.controlledPlDirection], 5
    if (!flags.zero)
        goto l_not_a_shot_on_goal;          // jnz @@not_a_shot_on_goal

l_possible_shot_on_goal:;
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 241;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 241
    if (flags.sign != flags.overflow)
        goto l_its_a_long_shot;             // jl @@its_a_long_shot

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 431;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 431
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_its_a_long_shot;             // jg short @@its_a_long_shot

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 204;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 204
    if (flags.sign != flags.overflow)
        goto l_its_a_finishing_shot;        // jl short @@its_a_finishing_shot

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 694;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 694
    if (flags.sign != flags.overflow)
        goto l_its_a_long_shot;             // jl short @@its_a_long_shot

l_its_a_finishing_shot:;
    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    A0 = 516138;                            // mov A0, offset kBallSpeedFinishing
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 75, 1);     // mov al, [esi+PlayerGameHeader.finishing]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // add [esi+Sprite.speed], ax
    goto l_not_a_shot_on_goal;              // jmp short @@not_a_shot_on_goal

l_its_a_long_shot:;
    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    A0 = 516090;                            // mov A0, offset kBallSpeedKicking
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 70, 1);     // mov al, [esi+PlayerGameHeader.shooting]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // add [esi+Sprite.speed], ax

l_not_a_shot_on_goal:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 2, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerOrdinal], 1
    if (!flags.zero)
        goto cseg_7AE0E;                    // jnz short cseg_7AE0E

    {
        word src = (word)readMemory(esi + 42, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.direction], 2
    if (flags.zero)
        goto l_play_kick_sample_and_leave;  // jz short @@play_kick_sample_and_leave

    {
        word src = (word)readMemory(esi + 42, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.direction], 6
    if (flags.zero)
        goto l_play_kick_sample_and_leave;  // jz short @@play_kick_sample_and_leave

cseg_7AE0E:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 76, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_play_kick_sample_and_leave;  // js short @@play_kick_sample_and_leave

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 118, 2, 0);           // mov [esi+TeamGeneralInfo.spinTimer], 0

l_play_kick_sample_and_leave:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 128, 2, 0);           // mov [esi+TeamGeneralInfo.passInProgress], 0
    SWOS::PlayKickSample();                 // call PlayKickSample
}

// in:
//      A1 -> player (sprite)
//      A6 -> team general info
//
// Runs when a player makes contact with the ball while doing static header.
// Applies ball speed and destination. Increases player speed, updates delta z
// and plays kick sample.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void playerHittingStaticHeader()
{
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 128, 2, 0);           // mov [esi+TeamGeneralInfo.passInProgress], 0

    if (A1.as<Sprite&>().animTable == getStaticHeaderHitAnimTable())
        goto l_set_static_header_anim_table;

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    if (flags.zero)
        goto l_set_static_header_anim_table; // jz @@set_static_header_anim_table

    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 4
    if (flags.zero)
        goto l_set_static_header_anim_table; // jz @@set_static_header_anim_table

    if (flags.carry)
        goto l_turn_player_right;           // jb short @@turn_player_right

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 42, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        src = res;
        writeMemory(esi + 42, 2, src);
    }                                       // sub [esi+Sprite.direction], 1
    {
        word src = (word)readMemory(esi + 42, 2);
        word res = src & 7;
        src = res;
        writeMemory(esi + 42, 2, src);
    }                                       // and [esi+Sprite.direction], 7
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (flags.zero)
        goto l_fix_sprite_direction;        // jz short @@fix_sprite_direction

    {
        word src = (word)readMemory(esi + 42, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 42, 2, src);
    }                                       // sub [esi+Sprite.direction], 1
    goto l_fix_sprite_direction;            // jmp short @@fix_sprite_direction

l_turn_player_right:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 42, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 42, 2, src);
    }                                       // add [esi+Sprite.direction], 1
    {
        word src = (word)readMemory(esi + 42, 2);
        word res = src & 7;
        src = res;
        writeMemory(esi + 42, 2, src);
    }                                       // and [esi+Sprite.direction], 7
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (flags.zero)
        goto l_fix_sprite_direction;        // jz short @@fix_sprite_direction

    {
        word src = (word)readMemory(esi + 42, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 42, 2, src);
    }                                       // add [esi+Sprite.direction], 1

l_fix_sprite_direction:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 42, 2);
        word res = src & 7;
        src = res;
        writeMemory(esi + 42, 2, src);
        flags.carry = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and [esi+Sprite.direction], 7

l_set_static_header_anim_table:;
    setPlayerAnimationTableAndPictureIndex(A1.as<Sprite&>(), getStaticHeaderHitAnimTable());
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    if (!flags.sign)
        goto l_got_direction;               // jns short @@got_direction

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax

l_got_direction:;
    A2 = 328492;                            // mov A2, offset ballSprite
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 56, 2, ax);           // mov [esi+TeamGeneralInfo.controlledPlDirection], ax
    A0 = 515768;                            // mov A0, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = *(word *)&g_memByte[516884];       // mov ax, kStaticHeaderBallSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    A0 = 516016;                            // mov A0, offset kPlayerHeaderSpeedIncrease
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 71, 1);     // mov al, [esi+PlayerGameHeader.heading]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // add [esi+Sprite.speed], ax
    eax = readMemory(esi + 54, 4);          // mov eax, [esi+Sprite.deltaZ]
    D0 = eax;                               // mov D0, eax
    *(int32_t *)&D0 = -*(int32_t *)&D0;     // neg D0
    {
        int32_t res = *(int32_t *)&D0 >> 1;
        flags.carry = ((dword)*(int32_t *)&D0 >> 31) & 1;
        *(int32_t *)&D0 = res;
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // sar D0, 1
    eax = D0;                               // mov eax, D0
    writeMemory(esi + 54, 4, eax);          // mov [esi+Sprite.deltaZ], eax
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 98, 2, 1);            // mov [esi+Sprite.heading], 1
    SWOS::PlayKickSample();                 // call PlayKickSample
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
}

// in:
//     A1 -> player
//     A6 -> team
//
// Hitting flying or lob header. Only triggers on ball contact.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void playerHittingJumpHeader()
{
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 128, 2, 0);           // mov [esi+TeamGeneralInfo.passInProgress], 0
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_set_player_z_and_ball_speed; // jns short @@set_player_z_and_ball_speed

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax

l_set_player_z_and_ball_speed:;
    A2 = 328492;                            // mov A2, offset ballSprite
    eax = *(dword *)&g_memByte[325304];     // mov eax, kBallJumpHeaderDeltaZ
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 54, 4, eax);          // mov [esi+Sprite.deltaZ], eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 2;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 2
    ax = D0;                                // mov ax, word ptr D0
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // add [esi+Sprite.speed], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_do_static_header;            // js short @@do_static_header

    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 7
    if (flags.zero)
        goto l_use_allowed_direction;       // jz @@use_allowed_direction

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 4
    if (flags.zero)
        goto l_lob_header;                  // jz short @@lob_header

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 1
    if (flags.zero)
        goto l_aim_left;                    // jz short @@aim_left

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 7;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 7
    if (flags.zero)
        goto l_aim_right;                   // jz short @@aim_right

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 2
    if (flags.zero)
        goto l_left_held;                   // jz short @@left_held

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 6
    if (flags.zero)
        goto l_right_held;                  // jz short @@right_held

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 3
    if (flags.zero)
        goto l_down_left_held;              // jz short @@down_left_held

    doLobHeader();                          // call DoLobHeader
    goto l_aim_right;                       // jmp short @@aim_right

l_down_left_held:;
    doLobHeader();                          // call DoLobHeader
    goto l_aim_left;                        // jmp short @@aim_left

l_right_held:;
    doFlyingHeader();                       // call DoFlyingHeader
    goto l_aim_right;                       // jmp short @@aim_right

l_left_held:;
    doFlyingHeader();                       // call DoFlyingHeader
    goto l_aim_left;                        // jmp short @@aim_left

l_do_static_header:;
    doFlyingHeader();                       // call DoFlyingHeader
    goto l_use_allowed_direction;           // jmp short @@use_allowed_direction

l_lob_header:;
    doLobHeader();                          // call DoLobHeader
    goto l_use_allowed_direction;           // jmp short @@use_allowed_direction

l_aim_right:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 1
    goto l_update_player_direction;         // jmp short @@update_player_direction

l_aim_left:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    goto l_update_player_direction;         // jmp short @@update_player_direction

l_use_allowed_direction:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+TeamGeneralInfo.allowedDirections]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax

l_update_player_direction:;
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 56, 2, ax);           // mov [esi+TeamGeneralInfo.controlledPlDirection], ax
    A0 = 515768;                            // mov A0, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    A0 = 516016;                            // mov A0, offset kPlayerHeaderSpeedIncrease
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 71, 1);     // mov al, [esi+PlayerGameHeader.heading]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // add [esi+Sprite.speed], ax
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        flags.carry = ((word)src >> 15) & 1;
        src = res;
        writeMemory(esi + 44, 2, src);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shr [esi+Sprite.speed], 1
    writeMemory(esi + 98, 2, 1);            // mov [esi+Sprite.heading], 1
    SWOS::PlayKickSample();                 // call PlayKickSample
    playHeaderComment(A6.as<TeamGeneralInfo&>());
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
}

// in:
//      A1 -> sprite (player)
//      A6 -> team (general)
//
// Player is tackling and hitting the ball. Adjust ball direction according to the player direction and
// controls direction (do deflected tackles). Set ball destination coordinates far away in that direction.
// Adjust ball speed after tackle to 125% of player's speed (100% if CPU player). Set player's speed
// afterward to 50%. If opponent's controlled player is more than 9u away from the ball and distance
// between the 2 players is greater than 32u it is considered a good tackle.
//  u = sqr((x1 - x2) ^ 2) + sqr((y1 - y2) ^ 2)
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void playerTackledTheBallStrong()
{
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_current_direction_allowed;   // jns short @@current_direction_allowed

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax

l_current_direction_allowed:;
    A2 = 328492;                            // mov A2, offset ballSprite
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    if (flags.zero)
        goto l_set_controlled_player_direction; // jz short @@set_controlled_player_direction

    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 4
    if (flags.zero)
        goto l_set_controlled_player_direction; // jz short @@set_controlled_player_direction

    if (flags.carry)
        goto l_controls_leaning_leftward;   // jb short @@controls_leaning_leftward

    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 1
    goto l_set_new_direction;               // jmp short @@set_new_direction

l_controls_leaning_leftward:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    goto l_set_new_direction;               // jmp short @@set_new_direction

l_set_controlled_player_direction:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax

l_set_new_direction:;
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 56, 2, ax);           // mov [esi+TeamGeneralInfo.controlledPlDirection], ax
    A0 = 515768;                            // mov A0, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_player_not_cpu;              // jnz short @@player_not_cpu

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    goto l_halve_player_speed;              // jmp short @@halve_player_speed

l_player_not_cpu:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 2;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 2
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 2;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 2
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax

l_halve_player_speed:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // shr [esi+Sprite.speed], 1
    writeMemory(esi + 96, 2, 1);            // mov [esi+Sprite.tackleState], 1
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A0 = eax;                               // mov A0, eax
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, 0
    if (flags.zero)
        goto l_out;                         // jz @@out

    esi = A0;                               // mov esi, A0
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 9;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 9
    if (flags.carry)
        goto l_out;                         // jb @@out

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    bx = D0;                                // mov bx, word ptr D0
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)((byte *)&D0 + 2) = dx;        // mov word ptr D0+2, dx
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    bx = D1;                                // mov bx, word ptr D1
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    *(word *)((byte *)&D1 + 2) = dx;        // mov word ptr D1+2, dx
    eax = D1;                               // mov eax, D1
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D0 = res;
    }                                       // add D0, eax
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 32;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 32
    if (flags.carry || flags.zero)
        goto l_out;                         // jbe short @@out

    SWOS::PlayGoodTackleComment();          // call PlayGoodTackleComment
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 96, 2, 2);            // mov [esi+Sprite.tackleState], TS_GOOD_TACKLE

l_out:;
    SWOS::PlayKickSample();                 // call PlayKickSample
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
}

// in:
//     A1 -> player
//     A6 -> team (general info)
//
// Weak tackle: player speed goes down to 50%, ball speed = 75% of original player speed.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void playerTackledTheBallWeak()
{
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 128, 2, 0);           // mov [esi+TeamGeneralInfo.passInProgress], 0
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_controls_something;          // jns short @@controls_something

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax

l_controls_something:;
    A2 = 328492;                            // mov A2, offset ballSprite
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    if (flags.zero)
        goto l_tackling_in_same_or_oposite_direction; // jz short @@tackling_in_same_or_oposite_direction

    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 4
    if (flags.zero)
        goto l_tackling_in_same_or_oposite_direction; // jz short @@tackling_in_same_or_oposite_direction

    if (flags.carry)
        goto l_strive_left;                 // jb short @@strive_left

    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 1
    goto l_set_new_ball_direction_and_speed; // jmp short @@set_new_ball_direction_and_speed

l_strive_left:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    goto l_set_new_ball_direction_and_speed; // jmp short @@set_new_ball_direction_and_speed

l_tackling_in_same_or_oposite_direction:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax

l_set_new_ball_direction_and_speed:;
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 56, 2, ax);           // mov [esi+TeamGeneralInfo.controlledPlDirection], ax
    A0 = 515768;                            // mov A0, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 1;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 1
    ax = D0;                                // mov ax, word ptr D0
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    {
        word src = (word)readMemory(esi + 44, 2);
        src |= 1;
        writeMemory(esi + 44, 2, src);
    }                                       // or [esi+Sprite.speed], 1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 1;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 96, 2, 1);            // mov [esi+Sprite.tackleState], TS_TACKLING_THE_BALL
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A0 = eax;                               // mov A0, eax
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, 0
    if (flags.zero)
        goto l_out;                         // jz @@out

    esi = A0;                               // mov esi, A0
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 9;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 9
    if (flags.carry)
        goto l_out;                         // jb @@out

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    eax = *(int16_t *)&D0;                  // movsx eax, word ptr D0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        int64_t res = (int64_t)eax * (int32_t)ebx;
        eax = res & 0xffffffff;
        edx = res >> 32;
    }                                       // imul ebx
    D0 = eax;                               // mov D0, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    bx = D1;                                // mov bx, word ptr D1
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    *(word *)((byte *)&D1 + 2) = dx;        // mov word ptr D1+2, dx
    eax = D1;                               // mov eax, D1
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D0 = res;
    }                                       // add D0, eax
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 32;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 32
    if (flags.carry || flags.zero)
        goto l_out;                         // jbe short @@out

    esi = A1;                               // mov esi, A1
    writeMemory(esi + 96, 2, 2);            // mov [esi+Sprite.tackleState], TS_GOOD_TACKLE

l_out:;
    SWOS::PlayKickSample();                 // call PlayKickSample
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
}

// in:
//      A1 -> goalkeeper sprite
//      A2 -> ball sprite
//      A6 -> player team (general)
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void goalkeeperClaimedTheBall()
{
    eax = *(dword *)&g_memByte[515576];     // mov eax, lastPlayerPlayed
    *(dword *)&g_memByte[515236] = eax;     // mov lastPlayerBeforeGoalkeeper, eax
    eax = *(dword *)&g_memByte[515572];     // mov eax, lastTeamPlayed
    *(dword *)&g_memByte[515232] = eax;     // mov lastTeamScored, eax
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto l_right_team;                  // jz short @@right_team

    *(word *)&g_memByte[515608] = 4;        // mov cameraDirection, 4
    g_memByte[515610] = 124;                // mov byte ptr playerTurnFlags, 7Ch
    goto l_camera_direction_set;            // jmp short @@camera_direction_set

l_right_team:;
    *(word *)&g_memByte[515608] = 0;        // mov cameraDirection, 0
    g_memByte[515610] = 199;                // mov byte ptr playerTurnFlags, 0C7h

l_camera_direction_set:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_player_controlled_team;      // jnz short @@player_controlled_team

    {
        byte src = g_memByte[515610];
        byte res = src & 187;
        src = res;
        g_memByte[515610] = src;
    }                                       // and byte ptr playerTurnFlags, 10111011b

l_player_controlled_team:;
    {
        word src = *(word *)&g_memByte[449234];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp forceLeftTeam, 1
    if (!flags.zero)
        goto l_keeper_holds_the_ball;       // jnz short @@keeper_holds_the_ball

    A6 = 515272;                            // mov A6, offset topTeamData

l_keeper_holds_the_ball:;
    *(word *)&g_memByte[515598] = 3;        // mov gameState, ST_KEEPER_HOLDS_BALL
    *(word *)&g_memByte[515600] = -1;       // mov breakCameraMode, -1
    ax = *(word *)&g_memByte[328524];       // mov ax, word ptr ballSprite.x+2
    *(word *)&g_memByte[515604] = ax;       // mov foulXCoordinate, ax
    ax = *(word *)&g_memByte[328528];       // mov ax, word ptr ballSprite.y+2
    *(word *)&g_memByte[515606] = ax;       // mov foulYCoordinate, ax
    *(word *)&g_memByte[515596] = 101;      // mov gameStatePl, ST_STOPPED
    *(word *)&g_memByte[515584] = 0;        // mov gameNotInProgressCounterWriteOnly, 0
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515588] = eax;     // mov lastTeamPlayedBeforeBreak, eax
    *(word *)&g_memByte[515592] = 0;        // mov stoppageTimerTotal, 0
    *(word *)&g_memByte[515594] = 0;        // mov stoppageTimerActive, 0
    stopAllPlayers();                       // call StopAllPlayers
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 80, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperDivingRight]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz short @@out

    ax = (word)readMemory(esi + 82, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperDivingLeft]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz short @@out

    eax = A1;                               // mov eax, A1
    writeMemory(esi + 32, 4, eax);          // mov [esi+TeamGeneralInfo.controlledPlayer], eax
    writeMemory(esi + 84, 1, 1);            // mov byte ptr [esi+TeamGeneralInfo.ballOutOfPlayOrKeeper], 1
    ax = *(word *)&g_memByte[515608];       // mov ax, cameraDirection
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 86, 1, 1);            // mov byte ptr [esi+TeamGeneralInfo.goaliePlayingOrOut], 1
    updatePlayerWithBall();                 // call UpdatePlayerWithBall
    *(word *)&g_memByte[449176] = 0;        // mov cameraXVelocity, 0
    *(word *)&g_memByte[449178] = 0;        // mov cameraYVelocity, 0
}

// in:
//      A2 -> ball sprite
//      A6 -> player team (general)
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void goalkeeperDeflectedBall()
{
    *(word *)&D0 = 0;                       // mov word ptr D0, 0
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto cseg_78DB8;                    // jz short cseg_78DB8

    *(word *)&D0 = 4;                       // mov word ptr D0, 4

cseg_78DB8:;
    A0 = 515768;                            // mov A0, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 31;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 1Fh
    {
        word res = *(word *)&D0 << 5;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 5
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 512;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 512
    ax = D0;                                // mov ax, word ptr D0
    {
        word src = (word)readMemory(esi + 58, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 58, 2, src);
    }                                       // add [esi+Sprite.destX], ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 24, 4);          // mov eax, [esi+TeamGeneralInfo.shotChanceTable]
    A0 = eax;                               // mov A0, eax
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        word res = *(word *)&D1 & 60;
        *(word *)&D1 = res;
    }                                       // and word ptr D1, 3Ch
    {
        word res = *(word *)&D1 >> 2;
        *(word *)&D1 = res;
    }                                       // shr word ptr D1, 2
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 52, 2);     // mov ax, [esi+52]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    if (flags.sign)
        goto l_strong_deflect;              // js short @@strong_deflect

    ax = (word)readMemory(esi + 54, 2);     // mov ax, [esi+54]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    if (flags.sign)
        goto l_medium_deflect;              // js short @@medium_deflect

    ax = kGoalkeeperWeakDeflectBallSpeed;   // mov ax, kGoalkeeperWeakDeflectBallSpeed
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    goto l_set_ball_speed;                  // jmp short @@set_ball_speed

l_medium_deflect:;
    ax = kGoalkeeperMediumDeflectBallSpeed; // mov ax, kGoalkeeperMediumDeflectBallSpeed
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    goto l_set_ball_speed;                  // jmp short @@set_ball_speed

l_strong_deflect:;
    ax = kGoalkeeperStrongDeflectBallSpeed; // mov ax, kGoalkeeperStrongDeflectBallSpeed
    *(word *)&D1 = ax;                      // mov word ptr D1, ax

l_set_ball_speed:;
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 511;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 1FFh
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 256;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 256
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    eax = kGoalkeeperDeflectDeltaZ;         // mov eax, kGoalkeeperDeflectDeltaZ
    D1 = eax;                               // mov D1, eax
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        dword res = D0 & 127;
        D0 = res;
    }                                       // and D0, 7Fh
    {
        dword res = D0 << 8;
        D0 = res;
    }                                       // shl D0, 8
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 16384;
        dword res = dstSigned - srcSigned;
        D0 = res;
    }                                       // sub D0, 16384
    eax = D1;                               // mov eax, D1
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D0 = res;
    }                                       // add D0, eax
    eax = D0;                               // mov eax, D0
    writeMemory(esi + 54, 4, eax);          // mov [esi+Sprite.deltaZ], eax
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
    SWOS::PlayKickSample();                 // call PlayKickSample
}

// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void doFlyingHeader()
{
    eax = *(dword *)&g_memByte[516048];     // mov eax, kHeaderLowJumpHeight
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 54, 4, eax);          // mov [esi+Sprite.deltaZ], eax
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 2;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 2
    ax = D0;                                // mov ax, word ptr D0
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    setPlayerJumpHeaderHitAnimationTable(); // call SetPlayerJumpHeaderHitAnimationTable
}

// in:
//      A1 -> player sprite
//      A6 -> team
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void doPass()
{
    *(word *)&g_memByte[515910] = 0;        // mov goodPassSampleCommand, 0
    *(word *)&g_memByte[449514] = 0;        // mov stateGoal, 0
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A0 = eax;                               // mov A0, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)&D7 = ax;                      // mov word ptr D7, ax
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 56, 2, ax);           // mov [esi+TeamGeneralInfo.controlledPlDirection], ax
    push(D0);                               // push D0
    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    pop(D0);                                // pop D0
    push(D7);                               // push D7
    getClosestNonControlledPlayerInDirection(); // call GetClosestNonControlledPlayerInDirection
    pop(D7);                                // pop D7
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = -1;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, -1
    if (flags.zero)
        goto l_no_closest_player;           // jz @@no_closest_player

    eax = A0;                               // mov eax, A0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 36, 4, eax);          // mov [esi+TeamGeneralInfo.passToPlayerPtr], eax
    writeMemory(esi + 88, 2, 1);            // mov [esi+TeamGeneralInfo.passingBall], 1
    writeMemory(esi + 90, 2, 1);            // mov [esi+TeamGeneralInfo.passingToPlayer], 1
    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 100;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameStatePl, ST_GAME_IN_PROGRESS
    if (!flags.zero)
        goto l_calculate_pass_to_player_delta_x_y; // jnz @@calculate_pass_to_player_delta_x_y

    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_calculate_pass_to_player_delta_x_y; // jnz @@calculate_pass_to_player_delta_x_y

    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 69, 1);     // mov al, [esi+PlayerGameHeader.passing]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    A1 = 516122;                            // mov A1, offset kAIFailedPassChance
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D7 = ax;                      // mov word ptr D7, ax
    {
        word res = *(word *)&D7 & 30;
        *(word *)&D7 = res;
    }                                       // and word ptr D7, 1Eh
    {
        word res = *(word *)&D7 >> 1;
        *(word *)&D7 = res;
    }                                       // shr word ptr D7, 1
    esi = A1;                               // mov esi, A1
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, ax
    if (!flags.carry)
        goto l_calculate_pass_to_player_delta_x_y; // jnb @@calculate_pass_to_player_delta_x_y

    *(word *)&g_memByte[515910] = -2;       // mov goodPassSampleCommand, -2
    A1 = 328492;                            // mov A1, offset ballSprite
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        byte src = g_memByte[323626];
        byte res = src & 32;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // test byte ptr currentGameTick, 20h
    if (!flags.zero)
        goto l_multiply_dx;                 // jnz short @@multiply_dx

    {
        int16_t res = *(int16_t *)&D0 >> 1;
        *(int16_t *)&D0 = res;
    }                                       // sar word ptr D0, 1

l_multiply_dx:;
    {
        word res = *(word *)&D0 << 5;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 5
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        byte src = g_memByte[323626];
        byte res = src & 32;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // test byte ptr currentGameTick, 20h
    if (flags.zero)
        goto l_multiply_dy;                 // jz short @@multiply_dy

    {
        int16_t res = *(int16_t *)&D0 >> 1;
        *(int16_t *)&D0 = res;
    }                                       // sar word ptr D0, 1

l_multiply_dy:;
    {
        word res = *(word *)&D0 << 5;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 5
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto l_determine_ball_speed;            // jmp @@determine_ball_speed

l_calculate_pass_to_player_delta_x_y:;
    A1 = 328492;                            // mov A1, offset ballSprite
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D2 = res;
    }                                       // sub word ptr D2, ax
    ax = D1;                                // mov ax, word ptr D1
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_increase_distances_loop;     // jnz short @@increase_distances_loop

    ax = D2;                                // mov ax, word ptr D2
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_increase_distances_loop;     // jnz short @@increase_distances_loop

    *(word *)&D1 = 1;                       // mov word ptr D1, 1

l_increase_distances_loop:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    if (flags.sign)
        goto l_set_dest_x_y;                // js short @@set_dest_x_y

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 672;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 672
    if (flags.sign == flags.overflow)
        goto l_set_dest_x_y;                // jge short @@set_dest_x_y

    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    if (flags.sign)
        goto l_set_dest_x_y;                // js short @@set_dest_x_y

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 880;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 880
    if (flags.sign == flags.overflow)
        goto l_set_dest_x_y;                // jge short @@set_dest_x_y

    {
        word res = *(word *)&D1 << 1;
        *(word *)&D1 = res;
    }                                       // shl word ptr D1, 1
    {
        word res = *(word *)&D2 << 1;
        flags.carry = ((word)*(word *)&D2 >> 15) & 1;
        flags.overflow = (((*(word *)&D2 >> 15) & 1) ^ ((*(word *)&D2 >> 14) & 1)) != 0;
        *(word *)&D2 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shl word ptr D2, 1
    goto l_increase_distances_loop;         // jmp short @@increase_distances_loop

l_set_dest_x_y:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax

l_determine_ball_speed:;
    ax = *(word *)&g_memByte[516072];       // mov ax, kPassingSpeedCloserThan2500
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    D0 = eax;                               // mov D0, eax
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 2500;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 2500
    if (flags.carry)
        goto l_set_ball_speed;              // jb @@set_ball_speed

    ax = *(word *)&g_memByte[516074];       // mov ax, kPassingSpeed_2500_10000
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 10000;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 10000
    if (flags.carry)
        goto l_set_ball_speed;              // jb @@set_ball_speed

    ax = *(word *)&g_memByte[516076];       // mov ax, kPassingSpeed_10000_22500
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 22500;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 22500
    if (flags.carry)
        goto l_set_ball_speed;              // jb @@set_ball_speed

    ax = *(word *)&g_memByte[516078];       // mov ax, kPassingSpeed_22500_40000
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 40000;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 40000
    if (flags.carry)
        goto l_set_ball_speed;              // jb @@set_ball_speed

    ax = *(word *)&g_memByte[516080];       // mov ax, kPassingSpeed_40000_62500
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 62500;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 62500
    if (flags.carry)
        goto l_set_ball_speed;              // jb short @@set_ball_speed

    ax = *(word *)&g_memByte[516082];       // mov ax, kPassingSpeed_62500_90000
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 90000;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 90000
    if (flags.carry)
        goto l_set_ball_speed;              // jb short @@set_ball_speed

    ax = *(word *)&g_memByte[516084];       // mov ax, kPassingSpeed_90000_122500
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 122500;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 122500
    if (flags.carry)
        goto l_set_ball_speed;              // jb short @@set_ball_speed

    ax = *(word *)&g_memByte[516086];       // mov ax, kPassingSpeedFurtherThan122500
    *(word *)&D1 = ax;                      // mov word ptr D1, ax

l_set_ball_speed:;
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 69, 1);     // mov al, [esi+PlayerGameHeader.passing]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    A0 = 516056;                            // mov A0, offset kBallSpeedPassingIncrease
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    *(word *)&g_memByte[515910] = -1;       // mov goodPassSampleCommand, -1
    goto l_reset_spin_timers;               // jmp @@reset_spin_timers

l_no_closest_player:;
{
    auto table = getBallDestCoordinatesTable();          // call GetBallDestCoordinatesTable
    A1 = 328492;                            // mov A1, offset ballSprite
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D7;                     // movzx ebx, word ptr D7
    ax = table[ebx].x;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = table[ebx].y;
}
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = *(word *)&g_memByte[516088];       // mov ax, kFreePassReleasingBallSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax

l_reset_spin_timers:;
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_player_passing;              // jz short @@player_passing

    writeMemory(esi + 118, 2, 0);           // mov [esi+TeamGeneralInfo.spinTimer], 0

l_player_passing:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 128, 2, 1);           // mov [esi+TeamGeneralInfo.passInProgress], 1
    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 100;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameStatePl, ST_GAME_IN_PROGRESS
    if (flags.zero)
        goto l_play_kick_and_pass_samples;  // jz short @@play_kick_and_pass_samples

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 15;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_THROW_IN_FORWARD_RIGHT
    if (flags.carry)
        goto l_play_kick_and_pass_samples;  // jb short @@play_kick_and_pass_samples

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 20;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_THROW_IN_BACK_LEFT
    if (flags.carry || flags.zero)
        return;                             // jbe short @@out

l_play_kick_and_pass_samples:;
    SWOS::PlayKickSample();                 // call PlayKickSample
    playStopGoodPassSampleIfNeeded();       // call PlayStopGoodPassSampleIfNeeded
}

// in:
//      A1 -> sprite
//      A6 -> team data
//
// Set how much time will player be lying on the ground after tackle.
// Better tackling skill means less time.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void setPlayerDowntimeAfterTackle()
{
    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    A0 = 516250;                            // mov A0, offset kPlayerTacklingDownTime
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 106, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = -1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.tacklingTimer], -1
    if (!flags.zero)
        goto l_ordinary_player;             // jnz short @@ordinary_player

    A0 = 516266;                            // mov A0, offset kComputerTacklingDownTime

l_ordinary_player:;
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 72, 1);     // mov al, [esi+PlayerGameHeader.tackling]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 1;
        flags.carry = ((word)*(word *)&D0 >> 15) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shl word ptr D0, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 13, 1, al);           // mov [esi+Sprite.playerDownTimer], al
}

void setJumpHeaderHitAnimTable(Sprite& player)
{
    // this is an animation switch from previous heading "intro" animation
    if (player.animTable != getJumpHeaderHitAnimTable() && !player.isHeadingBall &&
        player.playerDownTimer == 40 && static_cast<uint16_t>(player.frameSwitchCounter) <= 2 &&
        swos.currentGameTick & 0x0200)
    {
        setPlayerAnimationTableAndPictureIndex(A1.as<Sprite&>(), getJumpHeaderHitAnimTable());
        player.speed >>= 1;
    }
}

const PlayerInfo& getPlayerPointerFromShirtNumber(const TeamGeneralInfo& team, const Sprite& player)
{
    assert(team.inGameTeamPtr);
    assert(player.playerOrdinal - 1 < std::size(team.inGameTeamPtr->players));

    return team.inGameTeamPtr->players[player.playerOrdinal - 1];
}

// in:
//     A1 -> player sprite
//     A2 -> ball sprite
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void doLobHeader()
{
    eax = *(dword *)&g_memByte[516052];     // mov eax, kHeaderHighJumpHeight
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 54, 4, eax);          // mov [esi+Sprite.deltaZ], eax
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 4;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 4
    ax = D0;                                // mov ax, word ptr D0
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    setPlayerJumpHeaderHitAnimationTable(); // call SetPlayerJumpHeaderHitAnimationTable
}

// in:
//      D0 -  direction
//      A6 -> team (general)
// out:
//      A0 -> player sprite
//      D2 -  ball distance
//
// Return pointer to player closest to the ball facing approximately given
// direction. Also return his distance from the ball.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void getClosestNonControlledPlayerInDirection()
{
    eax = A1;                               // mov eax, A1
    A5 = eax;                               // mov A5, eax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 20, 4);          // mov eax, [esi+TeamGeneralInfo.spritesTable]
    A2 = eax;                               // mov A2, eax
    A0 = 328492;                            // mov A0, offset ballSprite
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    {
        word res = *(word *)&D0 << 5;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 5
    A0 = -1;                                // mov A0, -1
    D2 = -1;                                // mov D2, -1
    *(word *)&D7 = 10;                      // mov word ptr D7, 10

l_players_loop:;
    esi = A2;                               // mov esi, A2
    eax = readMemory(esi, 4);               // mov eax, [esi]
    {
        int32_t dstSigned = A2;
        int32_t srcSigned = 4;
        dword res = dstSigned + srcSigned;
        A2 = res;
    }                                       // add A2, 4
    A1 = eax;                               // mov A1, eax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    {
        int32_t dstSigned = A1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A1, eax
    if (flags.zero)
        goto l_next;                        // jz short @@next

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 108, 2);    // mov ax, [esi+Sprite.sentAway]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_next;                        // jnz short @@next

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 0;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_NORMAL
    if (!flags.zero)
        goto l_next;                        // jnz short @@next

    ax = (word)readMemory(esi + 82, 2);     // mov ax, [esi+Sprite.fullDirection]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    al = D0;                                // mov al, byte ptr D0
    {
        int8_t dstSigned = *(byte *)&D1;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D1 = res;
    }                                       // sub byte ptr D1, al
    {
        int8_t dstSigned = *(byte *)&D1;
        int8_t srcSigned = -16;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D1, 240
    if (flags.sign != flags.overflow)
        goto l_next;                        // jl short @@next

    {
        int8_t dstSigned = *(byte *)&D1;
        int8_t srcSigned = 16;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D1, 16
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_next;                        // jg short @@next

    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    D1 = eax;                               // mov D1, eax
    eax = D2;                               // mov eax, D2
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D1, eax
    if (!flags.carry)
        goto l_next;                        // jnb short @@next

    eax = D1;                               // mov eax, D1
    D2 = eax;                               // mov D2, eax
    eax = A1;                               // mov eax, A1
    A0 = eax;                               // mov A0, eax

l_next:;
    (*(int16_t *)&D7)--;
    flags.overflow = (int16_t)(*(int16_t *)&D7) == INT16_MIN;
    flags.sign = (*(int16_t *)&D7 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D7 == 0;      // dec word ptr D7
    if (!flags.sign)
        goto l_players_loop;                // jns @@players_loop
}

// in:
//     A1 -> player sprite
//
// Table for flying and lob headers.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void setPlayerJumpHeaderHitAnimationTable()
{
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 28, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.frameSwitchCounter], 2
    if (!flags.carry && !flags.zero)
        return;                             // ja short @@out

    setPlayerAnimationTableAndPictureIndex(A1.as<Sprite&>(), getJumpHeaderHitAnimTable());
}

// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void playStopGoodPassSampleIfNeeded()
{
    {
        word src = *(word *)&g_memByte[515910];
        int16_t dstSigned = src;
        int16_t srcSigned = -1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp goodPassSampleCommand, -1
    if (flags.zero)
        goto l_enqueue_good_pass_sample;    // jz short @@enqueue_good_pass_sample

    {
        word src = *(word *)&g_memByte[515910];
        int16_t dstSigned = src;
        int16_t srcSigned = -2;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp goodPassSampleCommand, -2
    if (flags.zero)
        goto l_stop_sample;                 // jz short @@stop_sample

    return;                                 // retn

    *(word *)&g_memByte[515910] = 0;        // mov goodPassSampleCommand, 0
    //EnqueueDeletedSample();                 // call EnqueueDeletedSample
    return;                                 // retn

l_stop_sample:;
    *(word *)&g_memByte[515910] = 0;        // mov goodPassSampleCommand, 0
    stopGoodPassSample();                   // call StopGoodPassSample
    return;                                 // retn

l_enqueue_good_pass_sample:;
    *(word *)&g_memByte[515910] = 0;        // mov goodPassSampleCommand, 0
    enqueuePlayingGoodPassSample();          // call EnqueuPlayingGoodPassSample
}

// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void stopGoodPassSample()
{
    *(word *)&g_memByte[477898] = -1;       // mov playingGoodPassTimer, -1
}

// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void enqueuePlayingGoodPassSample()
{
    *(word *)&g_memByte[477898] = -1;       // mov playingGoodPassTimer, -1
    {
        word src = *(word *)&g_memByte[477896];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        *(word *)&g_memByte[477896] = src;
    }                                       // add goodPassTimer, 1
    {
        word src = *(word *)&g_memByte[477896];
        int16_t dstSigned = src;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp goodPassTimer, 5
    if (!flags.zero)
        return;                             // jnz short @@out

    *(word *)&g_memByte[477896] = 0;        // mov goodPassTimer, 0
    *(word *)&g_memByte[477898] = 10;       // mov playingGoodPassTimer, 10
}
