#include "updatePlayers.h"
#include "ball.h"
#include "player.h"
#include "referee.h"
#include "team.h"
#include "amigaMode.h"
#include "comments.h"
#include "animation.h"

static constexpr int kGoalkeeperCatchSpeed = 768;
static constexpr int kGoalkeeperMoveToBallSpeed = 1024;
static constexpr int kGoalkeeperFarJumpSpeed = 2048;
static constexpr int kGoalkeeperFarJumpSlowerSpeed = 1280;
static constexpr int kShotAtGoalMinimumSpeed = 512;

static constexpr int dseg_1105EF = 512;
static constexpr int dseg_110611 = 112;
static constexpr int dseg_110613 = 192;
static constexpr int dseg_110615 = 128;

static int m_clearResultInterval = 660;
static int m_clearResultHalftimeInterval = 385;
static int m_playerDownTacklingInterval = 55;
static int m_playerDownHeadingInterval = 55;


static void shouldGoalkeeperDive();
static void goalkeeperJumping();
static void goalkeeperCaughtTheBall();
static void getFramesNeededToCoverDistance();
static void updateBallVariables();
static void calculateBallNextGroundXYPositions();
static void playerTacklingTestFoul();
static void testFoulForPenaltyAndFreeKick();
static void tryBookingThePlayer();
static void trySendingOffThePlayer();
static void playerTackled();
static void playerBeginTackling();
static void playersTackledTheBallStrong();
static void playerAttemptingJumpHeader();
static void setThrowInPlayerDestinationCoordinates();
static void setPlayerWithNoBallDestination();
static void attemptStaticHeader();
static void setStaticHeaderDirection();
static void AI_SetControlsDirection();
static void AI_Kick();
static void AI_SetDirectionTowardOpponentsGoal();
static void AI_DecideWhetherToTriggerFire();
static void AI_ResumeGameDelay();
static void findClosestPlayerToBallFacing();

using namespace SwosVM;

// in:
//      A6 -> team data
//
// This is practically main SWOS procedure. Everything happens here.
// Binary size: 0x5438/21,560 bytes
//`
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
void updatePlayers(TeamGeneralInfo *team)
{
    A6 = team;
    eax = *(dword *)&g_memByte[515576];     // mov eax, lastPlayerPlayed
    *(dword *)&g_memByte[516888] = eax;     // mov prevLastPlayer, eax
    eax = *(dword *)&g_memByte[515572];     // mov eax, lastTeamPlayed
    *(dword *)&g_memByte[516892] = eax;     // mov prevLastTeamPlayed, eax
    eax = *(dword *)&g_memByte[515568];     // mov eax, lastKeeperPlayed
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (flags.zero)
        goto l_update_goalkeeper_saved_timer; // jz short @@update_goalkeeper_saved_timer

    {
        dword src = *(dword *)&g_memByte[515576];
        int32_t dstSigned = src;
        int32_t srcSigned = 326028;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp lastPlayerPlayed, offset goalie1Sprite
    if (flags.zero)
        goto l_update_goalkeeper_saved_timer; // jz short @@update_goalkeeper_saved_timer

    {
        dword src = *(dword *)&g_memByte[515576];
        int32_t dstSigned = src;
        int32_t srcSigned = 327260;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp lastPlayerPlayed, offset goalie2Sprite
    if (flags.zero)
        goto l_update_goalkeeper_saved_timer; // jz short @@update_goalkeeper_saved_timer

    *(dword *)&g_memByte[515568] = 0;       // mov lastKeeperPlayed, 0

l_update_goalkeeper_saved_timer:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 76, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_pass_kick_timer;      // jz short @@update_pass_kick_timer

    if (flags.sign)
        goto l_decay_goalkeeper_saved_timer; // js short @@decay_goalkeeper_saved_timer

    {
        word src = (word)readMemory(esi + 76, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 76, 2, src);
    }                                       // sub [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer], 1
    goto l_update_pass_kick_timer;          // jmp short @@update_pass_kick_timer

l_decay_goalkeeper_saved_timer:;
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 76, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 76, 2, src);
    }                                       // add [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer], 1

l_update_pass_kick_timer:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 102, 2);    // mov ax, [esi+TeamGeneralInfo.passKickTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_won_the_ball_timer;   // jz short @@update_won_the_ball_timer

    {
        word src = (word)readMemory(esi + 102, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 102, 2, src);
    }                                       // sub [esi+TeamGeneralInfo.passKickTimer], 1
    if (!flags.zero)
        goto l_update_won_the_ball_timer;   // jnz short @@update_won_the_ball_timer

    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0

l_update_won_the_ball_timer:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 138, 2);    // mov ax, [esi+TeamGeneralInfo.wonTheBallTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_player_switch_timer;  // jz short @@update_player_switch_timer

    {
        word src = (word)readMemory(esi + 138, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        src = res;
        writeMemory(esi + 138, 2, src);
    }                                       // sub [esi+TeamGeneralInfo.wonTheBallTimer], 1

l_update_player_switch_timer:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 92, 2);     // mov ax, [esi+TeamGeneralInfo.playerSwitchTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_apply_after_touch_and_set_ball_location_flags; // jz short @@apply_after_touch_and_set_ball_location_flags

    {
        word src = (word)readMemory(esi + 92, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 92, 2, src);
    }                                       // sub [esi+TeamGeneralInfo.playerSwitchTimer], 1

l_apply_after_touch_and_set_ball_location_flags:;
    applyBallAfterTouch();                  // call ApplyBallAfterTouch
    A2 = 328492;                            // mov A2, offset ballSprite
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    *(word *)&g_memByte[516920] = 0;        // mov ballInUpperPenaltyArea, 0
    *(word *)&g_memByte[516922] = 0;        // mov ballInLowerPenaltyArea, 0
    *(word *)&g_memByte[516924] = 0;        // mov ballInGoalkeeperArea, 0
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 193;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 193
    if (flags.sign != flags.overflow)
        goto l_not_in_penalty_area;         // jl @@not_in_penalty_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 478;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 478
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_not_in_penalty_area;         // jg short @@not_in_penalty_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 129
    if (flags.sign != flags.overflow)
        goto l_not_in_penalty_area;         // jl short @@not_in_penalty_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 769;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 769
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_not_in_penalty_area;         // jg short @@not_in_penalty_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 216;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 216
    if (flags.zero || flags.sign != flags.overflow)
        goto l_upper_penalty_area;          // jle short @@upper_penalty_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 682;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 682
    if (flags.sign != flags.overflow)
        goto l_not_in_penalty_area;         // jl short @@not_in_penalty_area

    *(word *)&g_memByte[516922] = 1;        // mov ballInLowerPenaltyArea, 1
    goto l_check_goalkeeper_area;           // jmp short @@check_goalkeeper_area

l_upper_penalty_area:;
    *(word *)&g_memByte[516920] = 1;        // mov ballInUpperPenaltyArea, 1

l_check_goalkeeper_area:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 273;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 273
    if (flags.sign != flags.overflow)
        goto l_not_in_penalty_area;         // jl short @@not_in_penalty_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 398;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 398
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_not_in_penalty_area;         // jg short @@not_in_penalty_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 158;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 158
    if (flags.zero || flags.sign != flags.overflow)
        goto l_goalkeeper_area;             // jle short @@goalkeeper_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 740;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 740
    if (flags.sign != flags.overflow)
        goto l_not_in_penalty_area;         // jl short @@not_in_penalty_area

l_goalkeeper_area:;
    *(word *)&g_memByte[516924] = 1;        // mov ballInGoalkeeperArea, 1

l_not_in_penalty_area:;
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
        goto l_update_player_index;         // jnz short @@update_player_index

    ax = *(word *)&g_memByte[515582];       // mov ax, goalOut
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_player_index;         // jz short @@update_player_index

    eax = *(dword *)&g_memByte[516920];     // mov eax, dword ptr ballInUpperPenaltyArea
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (!flags.zero)
        goto l_update_player_index;         // jnz short @@update_player_index

    *(word *)&g_memByte[515582] = 0;        // mov goalOut, 0

l_update_player_index:;
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 30, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 30, 2, src);
    }                                       // add [esi+TeamGeneralInfo.updatePlayerIndex], 1
    {
        word src = (word)readMemory(esi + 30, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 11;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.updatePlayerIndex], 11
    if (!flags.zero)
        goto l_init_players_loop;           // jnz short @@init_players_loop

    writeMemory(esi + 30, 2, 0);            // mov [esi+TeamGeneralInfo.updatePlayerIndex], 0

l_init_players_loop:;
    *(word *)&D4 = 10;                      // mov word ptr D4, 10
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 20, 4);          // mov eax, [esi+TeamGeneralInfo.spritesTable]
    A3 = eax;                               // mov A3, eax

l_players_loop:;
    push(D4);                               // push small [word ptr D4]
    esi = A3;                               // mov esi, A3
    eax = readMemory(esi, 4);               // mov eax, [esi]
    {
        int32_t dstSigned = A3;
        int32_t srcSigned = 4;
        dword res = dstSigned + srcSigned;
        A3 = res;
    }                                       // add A3, 4
    A1 = eax;                               // mov A1, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    writeMemory(esi + 92, 2, ax);           // mov [esi+Sprite.playerDirection], ax
    eax = readMemory(esi + 46, 4);          // mov eax, [esi+Sprite.deltaX]
    D0 = eax;                               // mov D0, eax
    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    D0 |= eax;
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (D0 & 0x80000000) != 0;
    flags.zero = D0 == 0;                   // or D0, eax
    writeMemory(esi + 94, 1, 0);            // mov byte ptr [esi+Sprite.isMoving], 0
    if (flags.zero)
        goto l_test_if_player_tackled;      // jz short @@test_if_player_tackled

    writeMemory(esi + 94, 1, -1);           // mov byte ptr [esi+Sprite.isMoving], -1

l_test_if_player_tackled:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 3;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_TACKLED
    if (flags.zero)
        goto l_player_tackled;              // jz @@player_tackled

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 13;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_ROLLING_INJURED
    if (flags.zero)
        goto l_player_injured;              // jz @@player_injured

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
        goto l_update_player_ball_distance; // jz short @@update_player_ball_distance

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
    if (flags.zero)
        goto l_update_player_ball_distance; // jz short @@update_player_ball_distance

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
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
        goto l_update_player_ball_distance; // jz short @@update_player_ball_distance

    esi = A1;                               // mov esi, A1
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
        goto l_update_player_ball_distance; // jnz short @@update_player_ball_distance

    ax = *(word *)&g_memByte[515872];       // mov ax, injuriesForever
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_jmp_not_controlled_player;   // jz short @@jmp_not_controlled_player

    {
        word src = (word)readMemory(esi + 104, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = -1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.injuryLevel], -1
    if (flags.zero)
        goto l_injury_forever;              // jz @@injury_forever

l_jmp_not_controlled_player:;
    goto l_not_controlled_player;           // jmp @@not_controlled_player

l_update_player_ball_distance:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    writeMemory(esi + 69, 1, al);           // mov [esi+TeamGeneralInfo.prevPlVeryCloseToBall], al
    writeMemory(esi + 61, 1, 0);            // mov [esi+TeamGeneralInfo.plVeryCloseToBall], 0
    writeMemory(esi + 62, 1, 0);            // mov [esi+TeamGeneralInfo.plCloseToBall], 0
    writeMemory(esi + 63, 1, 0);            // mov [esi+TeamGeneralInfo.plNotFarFromBall], 0
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    D0 = eax;                               // mov D0, eax
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 32;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 32
    if (flags.carry || flags.zero)
        goto l_very_close_to_ball;          // jbe short @@very_close_to_ball

    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 72;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 72
    if (flags.carry || flags.zero)
        goto l_close_to_ball;               // jbe short @@close_to_ball

    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 2450;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 2450
    if (flags.carry || flags.zero)
        goto l_somewhat_close_to_ball;      // jbe short @@somewhat_close_to_ball

    goto l_update_ball_height;              // jmp short @@update_ball_height

l_very_close_to_ball:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 61, 1, 1);            // mov [esi+TeamGeneralInfo.plVeryCloseToBall], 1
    goto l_update_ball_height;              // jmp short @@update_ball_height

l_close_to_ball:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 62, 1, 1);            // mov [esi+TeamGeneralInfo.plCloseToBall], 1
    goto l_update_ball_height;              // jmp short @@update_ball_height

l_somewhat_close_to_ball:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 63, 1, 1);            // mov [esi+TeamGeneralInfo.plNotFarFromBall], 1

l_update_ball_height:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 64, 1, 0);            // mov [esi+TeamGeneralInfo.ballLessEqual4], 0
    writeMemory(esi + 65, 1, 0);            // mov [esi+TeamGeneralInfo.ball4To8], 0
    writeMemory(esi + 66, 1, 0);            // mov [esi+TeamGeneralInfo.ball8To12], 0
    writeMemory(esi + 67, 1, 0);            // mov [esi+TeamGeneralInfo.ball12To17], 0
    writeMemory(esi + 68, 1, 0);            // mov [esi+TeamGeneralInfo.ballAbove17], 0
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 40, 2);     // mov ax, word ptr [esi+(Sprite.z+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 17;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 17
    if (!flags.carry && !flags.zero)
        goto l_ball_too_high;               // ja short @@ball_too_high

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 12
    if (!flags.carry && !flags.zero)
        goto l_ball_high;                   // ja short @@ball_high

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 8;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 8
    if (!flags.carry && !flags.zero)
        goto l_ball_medium_height;          // ja short @@ball_medium_height

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 4
    if (!flags.carry && !flags.zero)
        goto l_ball_low;                    // ja short @@ball_low

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 64, 1, 1);            // mov [esi+TeamGeneralInfo.ballLessEqual4], 1
    goto l_check_player_state;              // jmp short @@check_player_state

l_ball_low:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 65, 1, 1);            // mov [esi+TeamGeneralInfo.ball4To8], 1
    goto l_check_player_state;              // jmp short @@check_player_state

l_ball_medium_height:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 66, 1, 1);            // mov [esi+TeamGeneralInfo.ball8To12], 1
    goto l_check_player_state;              // jmp short @@check_player_state

l_ball_high:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 67, 1, 1);            // mov [esi+TeamGeneralInfo.ball12To17], 1
    goto l_check_player_state;              // jmp short @@check_player_state

l_ball_too_high:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 68, 1, 1);            // mov [esi+TeamGeneralInfo.ballAbove17], 1

l_check_player_state:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_TACKLING
    if (flags.zero)
        goto l_player_tackling;             // jz @@player_tackling

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 9;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_JUMP_HEADING
    if (flags.zero)
        goto l_player_jump_heading;         // jz @@player_jump_heading

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 8;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_STATIC_HEADING
    if (flags.zero)
        goto l_player_doing_static_header;  // jz @@player_doing_static_header

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 5;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_THROW_IN
    if (flags.zero)
        goto l_player_taking_throw_in;      // jz @@player_taking_throw_in

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 6;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_GOALIE_DIVING_HIGH
    if (flags.zero)
        goto l_goalie_diving;               // jz @@goalie_diving

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 7;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_GOALIE_DIVING_LOW
    if (flags.zero)
        goto l_goalie_diving;               // jz @@goalie_diving

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 4;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_GOALIE_CATCHING_BALL
    if (flags.zero)
        goto l_goalie_catching_the_ball;    // jz @@goalie_catching_the_ball

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 10;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_DOWN
    if (flags.zero)
        goto l_player_down_st_10;           // jz @@player_down_st_10

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 12;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_BOOKED
    if (flags.zero)
        goto l_player_booked;               // jz @@player_booked

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 14;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_SAD
    if (flags.zero)
        goto l_player_sad_or_happy;         // jz @@player_sad_or_happy

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 15;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_HAPPY
    if (flags.zero)
        goto l_player_sad_or_happy;         // jz @@player_sad_or_happy

    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 11;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_GOALIE_CLAIMED
    if (flags.zero)
        goto l_goalie_claimed;              // jz @@goalie_claimed

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
    if (flags.zero)
        goto l_player_goalkeeper;           // jz short @@player_goalkeeper

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
        goto l_its_controlled_player;       // jz @@its_controlled_player

    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
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
        goto l_player_expecting_pass;       // jz @@player_expecting_pass

    debugBreak();                           // int 3

l_endless_loop:;
    goto l_endless_loop;                    // jmp short @@endless_loop

l_player_goalkeeper:;
    *(word *)&g_memByte[449500] = 0;        // mov goalTypeScored, GT_REGULAR
    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_shot_chance_table_for_goalie; // jz short @@update_shot_chance_table_for_goalie

    {
        word src = *(word *)&g_memByte[515632];
        int16_t dstSigned = src;
        int16_t srcSigned = 55;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp penaltiesTimer, 55
    if (!flags.carry && !flags.zero)
        goto l_update_player_speed_and_deltas; // ja @@update_player_speed_and_deltas

l_update_shot_chance_table_for_goalie:;
    updatePlayerShotChanceTable(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    ax = *(word *)&g_memByte[449185];       // mov ax, lastPlayerTurnFlags+1
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_this_player_last_played;     // jnz @@this_player_last_played

    *(word *)&g_memByte[516914] = -1;       // mov ballNextGroundX, -1
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 140, 2);    // mov ax, [esi+TeamGeneralInfo.goalkeeperPlaying]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_ball_out_or_keepers;  // jz short @@update_ball_out_or_keepers

    writeMemory(esi + 86, 1, 1);            // mov byte ptr [esi+TeamGeneralInfo.goaliePlayingOrOut], 1
    writeMemory(esi + 84, 1, 0);            // mov byte ptr [esi+TeamGeneralInfo.ballOutOfPlayOrKeeper], 0
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
        goto l_its_controlled_player;       // jz @@its_controlled_player

    writeMemory(esi + 140, 2, 0);           // mov [esi+TeamGeneralInfo.goalkeeperPlaying], 0

l_update_ball_out_or_keepers:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 84, 1, 0);            // mov byte ptr [esi+TeamGeneralInfo.ballOutOfPlayOrKeeper], 0
    writeMemory(esi + 86, 1, 0);            // mov byte ptr [esi+TeamGeneralInfo.goaliePlayingOrOut], 0
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
        goto l_update_goalkeeper_speed;     // jz short @@update_goalkeeper_speed

    writeMemory(esi + 86, 1, -1);           // mov byte ptr [esi+TeamGeneralInfo.goaliePlayingOrOut], -1
    writeMemory(esi + 84, 1, -1);           // mov byte ptr [esi+TeamGeneralInfo.ballOutOfPlayOrKeeper], -1
    ax = *(word *)&g_memByte[325296];       // mov ax, kGoalkeeperSpeedWhenGameStopped
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax

l_update_goalkeeper_speed:;
    ax = *(word *)&g_memByte[325406];       // mov ax, kGoalkeeperGameSpeed
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 449;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 449
    if (!flags.carry && !flags.zero)
        goto l_goalkeeper_in_lower_half;    // ja short @@goalkeeper_in_lower_half

    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto l_this_player_last_played;     // jz @@this_player_last_played

    goto l_goalkeeper_in_upper_half;        // jmp short @@goalkeeper_in_upper_half

l_goalkeeper_in_lower_half:;
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
        goto l_this_player_last_played;     // jz @@this_player_last_played

l_goalkeeper_in_upper_half:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 4;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_GOALIE_CATCHING_BALL
    if (!flags.zero)
        goto l_goalie_not_catching_the_ball; // jnz @@goalie_not_catching_the_ball

    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 100;
        word res = dstSigned - srcSigned;
    }                                       // cmp gameStatePl, ST_GAME_IN_PROGRESS
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (flags.zero)
        goto l_player_got_up;               // jz short @@player_got_up

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto l_increase_y_dest;             // jz short @@increase_y_dest

    {
        word src = (word)readMemory(esi + 60, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 60, 2, src);
    }                                       // sub [esi+Sprite.destY], 1
    goto cseg_7FCD0;                        // jmp cseg_7FCD0

l_increase_y_dest:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 60, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 60, 2, src);
    }                                       // add [esi+Sprite.destY], 1
    goto cseg_7FCD0;                        // jmp cseg_7FCD0

l_player_got_up:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());

l_goalie_not_catching_the_ball:;
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
        goto l_its_controlled_player;       // jz @@its_controlled_player

    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
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
        goto l_player_expecting_pass;       // jz @@player_expecting_pass

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
        goto l_not_controlled_player;       // jnz @@not_controlled_player

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto cseg_7F0DF;                    // jz short cseg_7F0DF

    ax = *(word *)&g_memByte[516920];       // mov ax, ballInUpperPenaltyArea
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_check_pass_to_player;        // jz @@check_pass_to_player

    goto l_ball_in_penalty_area;            // jmp short @@ball_in_penalty_area

cseg_7F0DF:;
    ax = *(word *)&g_memByte[516922];       // mov ax, ballInLowerPenaltyArea
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_check_pass_to_player;        // jz @@check_pass_to_player

l_ball_in_penalty_area:;
    eax = *(dword *)&g_memByte[515576];     // mov eax, lastPlayerPlayed
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
        goto l_this_player_last_played;     // jz @@this_player_last_played

    updateBallVariables();                  // call UpdateBallVariables
    calculateBallNextGroundXYPositions();   // call CalculateBallNextGroundXYPositions
    ax = *(word *)&g_memByte[516924];       // mov ax, ballInGoalkeeperArea
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_ball_in_lower_goalkeeper_area; // jnz short @@ball_in_lower_goalkeeper_area

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 24, 4);          // mov eax, [esi+TeamGeneralInfo.shotChanceTable]
    A0 = eax;                               // mov A0, eax
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 240;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0F0h
    {
        word res = *(word *)&D0 >> 4;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 4
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 58, 2);     // mov ax, [esi+58]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.carry)
        goto l_ball_standing_in_goalkeeper_area; // jnb @@ball_standing_in_goalkeeper_area

l_ball_in_lower_goalkeeper_area:;
    ax = *(word *)&g_memByte[516914];       // mov ax, ballNextGroundX
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_ball_standing_in_goalkeeper_area; // js @@ball_standing_in_goalkeeper_area

    ax = *(word *)&g_memByte[516914];       // mov ax, ballNextGroundX
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = *(word *)&g_memByte[516916];       // mov ax, ballNextYGroundY
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto cseg_7F1C7;                    // jz short cseg_7F1C7

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 137;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 137
    if (flags.sign != flags.overflow)
        goto l_ball_standing_in_goalkeeper_area; // jl @@ball_standing_in_goalkeeper_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 216;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 216
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_ball_standing_in_goalkeeper_area; // jg @@ball_standing_in_goalkeeper_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 193;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 193
    if (flags.sign != flags.overflow)
        goto l_ball_standing_in_goalkeeper_area; // jl @@ball_standing_in_goalkeeper_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 478;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 478
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_ball_standing_in_goalkeeper_area; // jg @@ball_standing_in_goalkeeper_area

    goto l_in_penalty_area;                 // jmp short @@in_penalty_area

cseg_7F1C7:;
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 682;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 682
    if (flags.sign != flags.overflow)
        goto l_ball_standing_in_goalkeeper_area; // jl @@ball_standing_in_goalkeeper_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 761;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 761
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_ball_standing_in_goalkeeper_area; // jg @@ball_standing_in_goalkeeper_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 193;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 193
    if (flags.sign != flags.overflow)
        goto l_ball_standing_in_goalkeeper_area; // jl @@ball_standing_in_goalkeeper_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 478;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 478
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_ball_standing_in_goalkeeper_area; // jg @@ball_standing_in_goalkeeper_area

l_in_penalty_area:;
    ax = *(word *)&g_memByte[516914];       // mov ax, ballNextGroundX
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    ax = *(word *)&g_memByte[516916];       // mov ax, ballNextYGroundY
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    ax = D0;                                // mov ax, word ptr D0
    bx = D0;                                // mov bx, word ptr D0
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)((byte *)&D0 + 2) = dx;        // mov word ptr D0+2, dx
    ax = D1;                                // mov ax, word ptr D1
    bx = D1;                                // mov bx, word ptr D1
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    *(word *)((byte *)&D1 + 2) = dx;        // mov word ptr D1+2, dx
    eax = D0;                               // mov eax, D0
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    ax = *(word *)&g_memByte[516914];       // mov ax, ballNextGroundX
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    ax = *(word *)&g_memByte[516916];       // mov ax, ballNextYGroundY
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D2 = res;
    }                                       // sub word ptr D2, ax
    ax = D0;                                // mov ax, word ptr D0
    bx = D0;                                // mov bx, word ptr D0
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)((byte *)&D0 + 2) = dx;        // mov word ptr D0+2, dx
    ax = D2;                                // mov ax, word ptr D2
    bx = D2;                                // mov bx, word ptr D2
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    *(word *)((byte *)&D2 + 2) = dx;        // mov word ptr D2+2, dx
    eax = D0;                               // mov eax, D0
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D2 = res;
    }                                       // add D2, eax
    {
        dword res = D1 << 2;
        D1 = res;
    }                                       // shl D1, 2
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
    if (!flags.carry && !flags.zero)
        goto l_ball_standing_in_goalkeeper_area; // ja short @@ball_standing_in_goalkeeper_area

    ax = *(word *)&g_memByte[516914];       // mov ax, ballNextGroundX
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = *(word *)&g_memByte[516916];       // mov ax, ballNextYGroundY
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = kGoalkeeperMoveToBallSpeed;        // mov ax, kGoalkeeperMoveToBallSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    goto cseg_7FCD0;                        // jmp cseg_7FCD0

l_ball_standing_in_goalkeeper_area:;
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
        goto cseg_7FCA1;                    // jnz cseg_7FCA1

    {
        word src = *(word *)&g_memByte[516908];
        int16_t dstSigned = src;
        int16_t srcSigned = 295;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp strikeDestX, 295
    if (flags.carry)
        goto l_shot_on_goal_or_close;       // jb short @@shot_on_goal_or_close

    {
        word src = *(word *)&g_memByte[516908];
        int16_t dstSigned = src;
        int16_t srcSigned = 376;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp strikeDestX, 376
    if (flags.carry || flags.zero)
        goto l_goal_attempt;                // jbe @@goal_attempt

l_shot_on_goal_or_close:;
    esi = A1;                               // mov esi, A1
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 512;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 512
    if (!flags.carry)
        goto l_goalkeeper_dont_throw;       // jnb short @@goalkeeper_dont_throw

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_goalkeeper_dont_throw;       // jnz short @@goalkeeper_dont_throw

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto cseg_7FCD0;                        // jmp cseg_7FCD0

l_goalkeeper_dont_throw:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_7F511;                    // jnz cseg_7F511

    ax = *(word *)&g_memByte[516924];       // mov ax, ballInGoalkeeperArea
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_center_goalkeeper_on_ball_x; // jnz short @@center_goalkeeper_on_ball_x

    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 449;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 449
    if (flags.carry)
        goto cseg_7F3F8;                    // jb short cseg_7F3F8

    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (flags.zero || flags.sign != flags.overflow)
        goto cseg_7F511;                    // jle cseg_7F511

    goto cseg_7F409;                        // jmp short cseg_7F409

cseg_7F3F8:;
    esi = A2;                               // mov esi, A2
    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (!flags.sign)
        goto cseg_7F511;                    // jns cseg_7F511

cseg_7F409:;
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 336;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 336
    {
        int16_t res = *(int16_t *)&D0 >> 1;
        *(int16_t *)&D0 = res;
    }                                       // sar word ptr D0, 1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 336;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 336
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    goto cseg_7F458;                        // jmp short cseg_7F458

l_center_goalkeeper_on_ball_x:;
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax

cseg_7F458:;
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 449;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 449
    if (flags.carry)
        goto cseg_7F49A;                    // jb short cseg_7F49A

    *(word *)&D0 = 769;                     // mov word ptr D0, 769
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        word res = *(word *)&D0 >> 1;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 1
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
    goto cseg_7F4C3;                        // jmp short cseg_7F4C3

cseg_7F49A:;
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 129
    {
        word res = *(word *)&D0 >> 1;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 129;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 129

cseg_7F4C3:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 24, 4);          // mov eax, [esi+TeamGeneralInfo.shotChanceTable]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 6, 2);      // mov ax, [esi+6]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 6, 2);      // mov ax, [esi+6]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 1;
        flags.carry = ((word)*(word *)&D0 >> 15) & 1;
        flags.overflow = (((*(word *)&D0 >> 15) & 1)) != 0;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shr word ptr D0, 1
    goto cseg_7FCD0;                        // jmp cseg_7FCD0

cseg_7F511:;
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 40, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 16;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.z+2)], 16
    if (!flags.carry && !flags.zero)
        goto cseg_7F626;                    // ja cseg_7F626

    ax = *(word *)&g_memByte[516896];       // mov ax, ballDefensiveX
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto cseg_7F626;                    // js cseg_7F626

    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    D0 = eax;                               // mov D0, eax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A0 = eax;                               // mov A0, eax
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, 0
    if (flags.zero)
        goto cseg_7F56B;                    // jz short cseg_7F56B

    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, eax
    if (!flags.carry && !flags.zero)
        goto cseg_7F626;                    // ja cseg_7F626

cseg_7F56B:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
    A0 = eax;                               // mov A0, eax
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, 0
    if (flags.zero)
        goto cseg_7F597;                    // jz short cseg_7F597

    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, eax
    if (!flags.carry && !flags.zero)
        goto cseg_7F626;                    // ja cseg_7F626

cseg_7F597:;
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
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, 0
    if (flags.zero)
        goto cseg_7F5CC;                    // jz short cseg_7F5CC

    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, eax
    if (!flags.carry && !flags.zero)
        goto cseg_7F626;                    // ja short cseg_7F626

cseg_7F5CC:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
    A0 = eax;                               // mov A0, eax
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, 0
    if (flags.zero)
        goto cseg_7F601;                    // jz short cseg_7F601

    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, eax
    if (!flags.carry && !flags.zero)
        goto cseg_7F626;                    // ja short cseg_7F626

cseg_7F601:;
    ax = *(word *)&g_memByte[516896];       // mov ax, ballDefensiveX
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = *(word *)&g_memByte[516898];       // mov ax, ballDefensiveY
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto cseg_7FCD0;                        // jmp cseg_7FCD0

cseg_7F626:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_this_player_last_played;     // jnz @@this_player_last_played

    goto l_this_player_last_played;         // jmp @@this_player_last_played

l_goal_attempt:;
    eax = *(dword *)&g_memByte[515572];     // mov eax, lastTeamPlayed
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (!flags.zero)
        goto cseg_7F65A;                    // jnz short cseg_7F65A

    ax = *(word *)&g_memByte[515566];       // mov ax, playerHadBall
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_7FBEF;                    // jz cseg_7FBEF

cseg_7F65A:;
    esi = A1;                               // mov esi, A1
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 512;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 512
    if (!flags.carry)
        goto cseg_7F6A3;                    // jnb short cseg_7F6A3

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_7F6A3;                    // jnz short cseg_7F6A3

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto l_shot_at_goal;                    // jmp @@shot_at_goal

cseg_7F6A3:;
    esi = A1;                               // mov esi, A1
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 2048;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 2048
    if (!flags.carry && !flags.zero)
        goto cseg_7F7BC;                    // ja cseg_7F7BC

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_7F7BC;                    // jnz cseg_7F7BC

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 336;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 336
    {
        int16_t res = *(int16_t *)&D0 >> 1;
        *(int16_t *)&D0 = res;
    }                                       // sar word ptr D0, 1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 336;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 336
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 449;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 449
    if (flags.carry)
        goto cseg_7F745;                    // jb short cseg_7F745

    *(word *)&D0 = 769;                     // mov word ptr D0, 769
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        word res = *(word *)&D0 >> 1;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 1
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
    goto cseg_7F76E;                        // jmp short cseg_7F76E

cseg_7F745:;
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 129
    {
        word res = *(word *)&D0 >> 1;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 129;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 129

cseg_7F76E:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 24, 4);          // mov eax, [esi+TeamGeneralInfo.shotChanceTable]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 6, 2);      // mov ax, [esi+6]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 6, 2);      // mov ax, [esi+6]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 1;
        flags.carry = ((word)*(word *)&D0 >> 15) & 1;
        flags.overflow = (((*(word *)&D0 >> 15) & 1)) != 0;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shr word ptr D0, 1
    goto l_shot_at_goal;                    // jmp @@shot_at_goal

cseg_7F7BC:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_7FC48;                    // jnz cseg_7FC48

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_7FC48;                    // jnz cseg_7FC48

    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_7FC48;                    // jnz cseg_7FC48

    eax = *(dword *)&g_memByte[515572];     // mov eax, lastTeamPlayed
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (!flags.zero)
        goto l_check_shot_at_goal_speed;    // jnz short @@check_shot_at_goal_speed

    ax = *(word *)&g_memByte[515566];       // mov ax, playerHadBall
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_7FBEF;                    // jz cseg_7FBEF

l_check_shot_at_goal_speed:;
    ax = kShotAtGoalMinimumSpeed;
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (flags.carry)
        goto l_shot_at_goal;                // jb short @@shot_at_goal

    esi = A1;                               // mov esi, A1
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 5000;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 5000
    if (!flags.carry && !flags.zero)
        goto cseg_7FC01;                    // ja cseg_7FC01

    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 40, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.z+2)], 12
    if (!flags.carry && !flags.zero)
        goto cseg_7FC01;                    // ja cseg_7FC01

    {
        dword src = readMemory(esi + 54, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 32768;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.deltaZ], 8000h
    if (!flags.zero && flags.sign == flags.overflow)
        goto cseg_7FC01;                    // jg cseg_7FC01

    {
        dword src = readMemory(esi + 54, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 32768;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.deltaZ], 8000h
    if (flags.sign != flags.overflow)
        goto cseg_7FC01;                    // jl cseg_7FC01

    goto cseg_7FC48;                        // jmp cseg_7FC48

l_shot_at_goal:;
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 40, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 16;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.z+2)], 16
    if (!flags.carry && !flags.zero)
        goto cseg_7FC01;                    // ja cseg_7FC01

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 76, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_goalkeeper_saved;            // jg @@goalkeeper_saved

    esi = A1;                               // mov esi, A1
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 128;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 128
    if (!flags.carry && !flags.zero)
        goto l_goalkeeper_saved;            // ja @@goalkeeper_saved

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_goalkeeper_saved;            // jnz @@goalkeeper_saved

    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    al = (byte)readMemory(esi + 50, 1);     // mov al, [esi+TeamGeneralInfo.firePressed]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_goalkeeper_saved;            // jz @@goalkeeper_saved

    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_7F910;                    // jnz short cseg_7F910

    {
        word src = (word)readMemory(esi + 102, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 22;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.passKickTimer], 22
    if (flags.carry)
        goto l_goalkeeper_saved;            // jb @@goalkeeper_saved

cseg_7F910:;
    *(word *)&D1 = 0;                       // mov word ptr D1, 0
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 72, 4);          // mov eax, [esi+TeamGeneralInfo.lastHeadingTacklingPlayer]
    A0 = eax;                               // mov A0, eax
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, 0
    if (flags.zero)
        goto l_get_goalie_skill;            // jz short @@get_goalie_skill

    {
        dword tmp = A1;
        A1 = eax;
        eax = tmp;
    }                                       // xchg eax, A1
    A0 = eax;                               // mov A0, eax
    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    eax = A0;                               // mov eax, A0
    {
        dword tmp = A1;
        A1 = eax;
        eax = tmp;
    }                                       // xchg eax, A1
    A0 = eax;                               // mov A0, eax
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 75, 1);     // mov al, [esi+PlayerGameHeader.finishing]
    *(byte *)&D1 = al;                      // mov byte ptr D1, al

l_get_goalie_skill:;
    A4 = &getPlayerPointerFromShirtNumber(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    A4 -= kTeamGameHeaderSize;
    esi = A4;                               // mov esi, A4
    al = (byte)readMemory(esi + 76, 1);     // mov al, [esi+PlayerGameHeader.goalieSkill]
    {
        int8_t dstSigned = *(byte *)&D1;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D1 = res;
    }                                       // sub byte ptr D1, al
    al = D1;                                // mov al, byte ptr D1
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 7;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 7
    A0 = 519050;                            // mov A0, offset goalScoredChances
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 1;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 1
    {
        word res = *(word *)&D0 & 15;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0Fh
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
        goto l_goal_scored;                 // jb short @@goal_scored

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 76, 2, 5);            // mov [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer], 5
    goto l_goalkeeper_saved;                // jmp @@goalkeeper_saved

l_goal_scored:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 76, 2, -5);           // mov [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer], -5
    *(word *)&D1 = 2;                       // mov word ptr D1, 2
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (flags.carry)
        goto cseg_7FA33;                    // jb short cseg_7FA33

    *(word *)&D0 = 2;                       // mov word ptr D0, 2
    *(word *)&D3 = 2;                       // mov word ptr D3, 2
    goto cseg_7FA45;                        // jmp short cseg_7FA45

cseg_7FA33:;
    *(word *)&D0 = 6;                       // mov word ptr D0, 6
    *(word *)&D3 = 6;                       // mov word ptr D3, 6

cseg_7FA45:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    ax = *(word *)&g_memByte[328536];       // mov ax, ballSprite.speed
    *(word *)&g_memByte[336562] = ax;       // mov ballSpeed, ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 118, 2, -1);          // mov [esi+TeamGeneralInfo.spinTimer], -1
    {
        word src = *(word *)&g_memByte[328536];
        int16_t dstSigned = src;
        int16_t srcSigned = 1536;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp ballSprite.speed, 1536
    if (flags.carry || flags.zero)
        goto cseg_7FA8E;                    // jbe short cseg_7FA8E

    *(word *)&g_memByte[328536] = 1536;     // mov ballSprite.speed, 1536

cseg_7FA8E:;
    push(A1);                               // push A1
    goalkeeperJumping();                    // call GoalkeeperJumping
    pop(A1);                                // pop A1
    ax = *(word *)&g_memByte[328536];       // mov ax, ballSprite.speed
    *(word *)&g_memByte[336564] = ax;       // mov dseg_114EA6, ax
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 0);           // mov [esi+TeamGeneralInfo.passKickTimer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto l_clamp_ball_y_inside_pitch;       // jmp @@clamp_ball_y_inside_pitch

l_goalkeeper_saved:;
    shouldGoalkeeperDive();                 // call ShouldGoalkeeperDive
    ax = D0;                                // mov ax, word ptr D0
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_7FC01;                    // jz cseg_7FC01

    ax = *(word *)&g_memByte[516896];       // mov ax, ballDefensiveX
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (flags.carry)
        goto cseg_7FB57;                    // jb short cseg_7FB57

    *(word *)&D1 = 0;                       // mov word ptr D1, 0
    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_7FB1B;                    // jnz short cseg_7FB1B

    ax = *(word *)&g_memByte[515580];       // mov ax, penalty
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_7FB43;                    // jz short cseg_7FB43

cseg_7FB1B:;
    *(word *)&D1 = 1;                       // mov word ptr D1, 1
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 6;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 6
    if (flags.zero)
        goto cseg_7FB9E;                    // jz short cseg_7FB9E

    *(word *)&D1 = 0;                       // mov word ptr D1, 0

cseg_7FB43:;
    *(word *)&D0 = 2;                       // mov word ptr D0, 2
    *(word *)&D3 = 2;                       // mov word ptr D3, 2
    goto cseg_7FBB0;                        // jmp short cseg_7FBB0

cseg_7FB57:;
    *(word *)&D1 = 0;                       // mov word ptr D1, 0
    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_7FB76;                    // jnz short cseg_7FB76

    ax = *(word *)&g_memByte[515580];       // mov ax, penalty
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_7FB9E;                    // jz short cseg_7FB9E

cseg_7FB76:;
    *(word *)&D1 = 1;                       // mov word ptr D1, 1
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 12;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 0Ch
    if (flags.zero)
        goto cseg_7FB43;                    // jz short cseg_7FB43

    *(word *)&D1 = 0;                       // mov word ptr D1, 0

cseg_7FB9E:;
    *(word *)&D0 = 6;                       // mov word ptr D0, 6
    *(word *)&D3 = 6;                       // mov word ptr D3, 6

cseg_7FBB0:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    push(A1);                               // push A1
    goalkeeperJumping();                    // call GoalkeeperJumping
    pop(A1);                                // pop A1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 0);           // mov [esi+TeamGeneralInfo.passKickTimer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto l_clamp_ball_y_inside_pitch;       // jmp @@clamp_ball_y_inside_pitch

cseg_7FBEF:;
    ax = dseg_1105EF;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    goto cseg_7FC23;                        // jmp short cseg_7FC23

cseg_7FC01:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 24, 4);          // mov eax, [esi+TeamGeneralInfo.shotChanceTable]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 8, 2);      // mov ax, [esi+8]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax

cseg_7FC23:;
    ax = *(word *)&g_memByte[516902];       // mov ax, ballNotHighX
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = *(word *)&g_memByte[516904];       // mov ax, ballNotHighY
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto cseg_7FCD0;                        // jmp cseg_7FCD0

cseg_7FC48:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_7FC01;                    // jnz short cseg_7FC01

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 24, 4);          // mov eax, [esi+TeamGeneralInfo.shotChanceTable]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 6, 2);      // mov ax, [esi+6]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    goto cseg_7FCD0;                        // jmp short cseg_7FCD0

cseg_7FCA1:;
    goto l_this_player_last_played;         // jmp @@this_player_last_played

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax

cseg_7FCD0:;
    eax = *(dword *)&g_memByte[515572];     // mov eax, lastTeamPlayed
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (!flags.zero)
        goto l_opponent_last_played;        // jnz short @@opponent_last_played

    ax = *(word *)&g_memByte[515566];       // mov ax, playerHadBall
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_goalie_cant_catch_ball;      // jz @@goalie_cant_catch_ball

l_opponent_last_played:;
    ax = *(word *)&g_memByte[516924];       // mov ax, ballInGoalkeeperArea
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_7FD39;                    // jnz short cseg_7FD39

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 24, 4);          // mov eax, [esi+TeamGeneralInfo.shotChanceTable]
    A0 = eax;                               // mov A0, eax
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 240;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0F0h
    {
        word res = *(word *)&D0 >> 4;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 4
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 58, 2);     // mov ax, [esi+58]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.carry)
        goto l_goalie_cant_catch_ball;      // jnb @@goalie_cant_catch_ball

cseg_7FD39:;
    ax = *(word *)&g_memByte[516914];       // mov ax, ballNextGroundX
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_goalie_cant_catch_ball;      // js @@goalie_cant_catch_ball

    {
        word src = *(word *)&g_memByte[516900];
        int16_t dstSigned = src;
        int16_t srcSigned = 27;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp ballDefensiveZ, 27
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_goalie_cant_catch_ball;      // jg @@goalie_cant_catch_ball

    {
        word src = *(word *)&g_memByte[516900];
        int16_t dstSigned = src;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp ballDefensiveZ, 12
    if (flags.zero || flags.sign != flags.overflow)
        goto l_goalie_cant_catch_ball;      // jle short @@goalie_cant_catch_ball

    esi = A1;                               // mov esi, A1
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 2116;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 2116
    if (!flags.carry && !flags.zero)
        goto l_goalie_cant_catch_ball;      // ja short @@goalie_cant_catch_ball

    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = *(word *)&g_memByte[516896];       // mov ax, ballDefensiveX
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 12
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_goalie_cant_catch_ball;      // jg short @@goalie_cant_catch_ball

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = -12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, -12
    if (flags.sign != flags.overflow)
        goto l_goalie_cant_catch_ball;      // jl short @@goalie_cant_catch_ball

    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = *(word *)&g_memByte[516898];       // mov ax, ballDefensiveY
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 12
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_goalie_cant_catch_ball;      // jg short @@goalie_cant_catch_ball

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = -12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, -12
    if (flags.sign != flags.overflow)
        goto l_goalie_cant_catch_ball;      // jl short @@goalie_cant_catch_ball

    goalkeeperCaughtTheBall();              // call GoalkeeperCaughtTheBall

l_goalie_cant_catch_ball:;
    eax = *(dword *)&g_memByte[515576];     // mov eax, lastPlayerPlayed
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
        goto l_clamp_ball_y_inside_pitch;   // jz @@clamp_ball_y_inside_pitch

    esi = A1;                               // mov esi, A1
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 72;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 72
    if (!flags.carry && !flags.zero)
        goto l_clamp_ball_y_inside_pitch;   // ja @@clamp_ball_y_inside_pitch

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_clamp_ball_y_inside_pitch;   // jnz @@clamp_ball_y_inside_pitch

    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 4;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_GOALIE_CATCHING_BALL
    if (flags.zero)
        goto cseg_7FE29;                    // jz short cseg_7FE29

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 67, 1);     // mov al, [esi+TeamGeneralInfo.ball12To17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_clamp_ball_y_inside_pitch;   // jnz @@clamp_ball_y_inside_pitch

cseg_7FE29:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    eax = *(dword *)&g_memByte[515572];     // mov eax, lastTeamPlayed
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (!flags.zero)
        goto l_opponent_player_touched_the_ball; // jnz @@opponent_player_touched_the_ball

    ax = *(word *)&g_memByte[515566];       // mov ax, playerHadBall
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_opponent_player_touched_the_ball; // jnz @@opponent_player_touched_the_ball

    updateBallWithControllingGoalkeeper(A1.as<Sprite&>());  // call UpdateBallWithControllingGoalkeeper
    eax = A1;                               // mov eax, A1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 32, 4, eax);          // mov [esi+TeamGeneralInfo.controlledPlayer], eax
    writeMemory(esi + 36, 4, 0);            // mov [esi+TeamGeneralInfo.passToPlayerPtr], 0
    writeMemory(esi + 92, 2, 25);           // mov [esi+TeamGeneralInfo.playerSwitchTimer], 25
    writeMemory(esi + 88, 2, 0);            // mov [esi+TeamGeneralInfo.passingBall], 0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 0;        // mov playerHadBall, 0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 0);           // mov [esi+TeamGeneralInfo.passKickTimer], 0
    writeMemory(esi + 104, 4, 0);           // mov dword ptr [esi+104], 0
    writeMemory(esi + 116, 2, 0);           // mov word ptr [esi+116], 0
    writeMemory(esi + 140, 2, 1);           // mov [esi+TeamGeneralInfo.goalkeeperPlaying], 1
    writeMemory(esi + 86, 1, 1);            // mov byte ptr [esi+TeamGeneralInfo.goaliePlayingOrOut], 1
    writeMemory(esi + 84, 1, 1);            // mov byte ptr [esi+TeamGeneralInfo.ballOutOfPlayOrKeeper], 1
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_opponent_player_touched_the_ball:;
    eax = *(dword *)&g_memByte[515576];     // mov eax, lastPlayerPlayed
    *(dword *)&g_memByte[515568] = eax;     // mov lastKeeperPlayed, eax
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 0;        // mov playerHadBall, 0
    updateBallWithControllingGoalkeeper(A1.as<Sprite&>());  // call UpdateBallWithControllingGoalkeeper
    goalkeeperClaimedTheBall();             // call GoalkeeperClaimedTheBall

l_clamp_ball_y_inside_pitch:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 60, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.destY], 129
    if (flags.sign == flags.overflow)
        goto l_check_ball_y_inside_bottom;  // jge short @@check_ball_y_inside_bottom

    writeMemory(esi + 60, 2, 129);          // mov [esi+Sprite.destY], 129

l_check_ball_y_inside_bottom:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 60, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 769;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.destY], 769
    if (flags.zero || flags.sign != flags.overflow)
        goto l_update_player_speed_and_deltas; // jle @@update_player_speed_and_deltas

    writeMemory(esi + 60, 2, 769);          // mov [esi+Sprite.destY], 769
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_this_player_last_played:;
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
        goto l_not_controlled_player;       // jnz @@not_controlled_player

    ax = *(word *)&g_memByte[323620];       // mov ax, frameCount
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 14;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 0Eh
    if (!flags.zero)
        goto l_clamp_ball_y_inside_pitch;   // jnz short @@clamp_ball_y_inside_pitch

    goto l_this_is_substituted_player;      // jmp @@this_is_substituted_player

l_check_pass_to_player:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 84, 2, 0);            // mov [esi+TeamGeneralInfo.ballOutOfPlayOrKeeper], 0
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
    {
        int32_t dstSigned = A1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A1, eax
    if (!flags.zero)
        goto l_this_player_last_played;     // jnz short @@this_player_last_played

    writeMemory(esi + 36, 4, 0);            // mov [esi+TeamGeneralInfo.passToPlayerPtr], 0
    writeMemory(esi + 88, 2, 0);            // mov [esi+TeamGeneralInfo.passingBall], 0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    goto l_this_player_last_played;         // jmp short @@this_player_last_played

l_player_booked:;
    ax = *(word *)&g_memByte[515888];       // mov ax, whichCard
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

l_go_back_to_normal_state:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_sad_or_happy:;
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 23;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GOING_TO_HALFTIME
    if (flags.zero)
        goto l_go_back_to_normal_state;     // jz short @@go_back_to_normal_state

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 24;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PLAYERS_GOING_TO_SHOWER
    if (flags.zero)
        goto l_go_back_to_normal_state;     // jz short @@go_back_to_normal_state

    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_down_st_10:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_goalie_catching_the_ball:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (!flags.zero)
        goto l_check_diving_side;           // jnz short @@check_diving_side

    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 82, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperDivingLeft]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    writeMemory(esi + 82, 2, 0);            // mov [esi+TeamGeneralInfo.goalkeeperDivingLeft], 0
    updateBallWithControllingGoalkeeper(A1.as<Sprite&>());  // call UpdateBallWithControllingGoalkeeper
    goalkeeperClaimedTheBall();             // call GoalkeeperClaimedTheBall
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 40, 2, 5);            // mov word ptr [esi+(Sprite.z+2)], 5
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_check_diving_side:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 82, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperDivingLeft]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_check_if_goalie_close_to_the_ball; // jz short @@check_if_goalie_close_to_the_ball

    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_check_if_goalie_close_to_the_ball:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    eax = readMemory(esi + 24, 4);          // mov eax, [esi+TeamGeneralInfo.shotChanceTable]
    A0 = eax;                               // mov A0, eax
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        word res = *(word *)&D1 & 240;
        *(word *)&D1 = res;
    }                                       // and word ptr D1, 0F0h
    {
        word res = *(word *)&D1 >> 4;
        *(word *)&D1 = res;
    }                                       // shr word ptr D1, 4
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 48, 2);     // mov ax, [esi+48]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    if (flags.sign)
        goto l_goalie_catches_the_ball;     // js short @@goalie_catches_the_ball

    goalkeeperDeflectedBall();              // call cseg_78D9A
    push(A0);                               // push A0
    SWOS::PlayKeeperClaimedComment();       // call PlayKeeperClaimedComment
    pop(A0);                                // pop A0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_goalie_catches_the_ball:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 82, 2, 1);            // mov [esi+Sprite.fullDirection], 1
    goalkeeperClaimedTheBall();             // call GoalkeeperClaimedTheBall
    push(A0);                               // push A0
    SWOS::PlayKeeperClaimedComment();       // call PlayKeeperClaimedComment
    pop(A0);                                // pop A0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_goalie_claimed:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    push(A0);                               // push A0
    SWOS::PlayKeeperClaimedComment();       // call PlayKeeperClaimedComment
    pop(A0);                                // pop A0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_goalie_diving:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (!flags.zero)
        goto l_goalkeeper_still_diving;     // jnz short @@goalkeeper_still_diving

l_goalkeeper_rise:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_stop_player;                     // jmp @@stop_player

l_goalkeeper_still_diving:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 80, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperDivingRight]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_goalie_diving_left;          // jz short @@goalie_diving_left

    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 42;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerDownTimer], 42
    if (!flags.carry && !flags.zero)
        goto l_goalie_diving_left;          // ja short @@goalie_diving_left

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 80, 2, 0);            // mov [esi+TeamGeneralInfo.goalkeeperDivingRight], 0
    updateBallWithControllingGoalkeeper(A1.as<Sprite&>());  // call UpdateBallWithControllingGoalkeeper
    goalkeeperClaimedTheBall();             // call GoalkeeperClaimedTheBall
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 40, 2, 5);            // mov word ptr [esi+(Sprite.z+2)], 5
    goto l_goalkeeper_rise;                 // jmp short @@goalkeeper_rise

l_goalie_diving_left:;
    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 100;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameStatePl, 100
    if (!flags.zero)
        goto cseg_80282;                    // jnz short cseg_80282

    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 60;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerDownTimer], 60
    if (flags.carry || flags.zero)
        goto l_goalkeeper_rise;             // jbe short @@goalkeeper_rise

cseg_80282:;
    ax = dseg_110611;
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 70, 1);     // mov al, byte ptr [esi+TeamGeneralInfo.field_46]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_802A3;                    // jz short cseg_802A3

    goto l_set_new_goalkeeper_speed;        // jmp @@set_new_goalkeeper_speed

cseg_802A3:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 70, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 976;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.imageIndex], SPR_GOALIE1_CAUGHT_BALL_RIGHT
    if (flags.zero)
        goto l_goalkeeper_on_ground;        // jz @@goalkeeper_on_ground

    {
        word src = (word)readMemory(esi + 70, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 978;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.imageIndex], SPR_GOALIE1_CAUGHT_BALL_LEFT
    if (flags.zero)
        goto l_goalkeeper_on_ground;        // jz @@goalkeeper_on_ground

    {
        word src = (word)readMemory(esi + 70, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1106;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.imageIndex], SPR_GOALIE2_CAUGHT_BALL_RIGHT
    if (flags.zero)
        goto l_goalkeeper_on_ground;        // jz @@goalkeeper_on_ground

    {
        word src = (word)readMemory(esi + 70, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1108;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.imageIndex], SPR_GOALIE2_CAUGHT_BALL_LEFT
    if (flags.zero)
        goto l_goalkeeper_on_ground;        // jz @@goalkeeper_on_ground

    {
        word src = (word)readMemory(esi + 70, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1092;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.imageIndex], 1092
    if (flags.zero)
        goto l_goalkeeper_on_ground;        // jz short @@goalkeeper_on_ground

    {
        word src = (word)readMemory(esi + 70, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1094;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.imageIndex], 1094
    if (flags.zero)
        goto l_goalkeeper_on_ground;        // jz short @@goalkeeper_on_ground

    {
        word src = (word)readMemory(esi + 70, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 990;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.imageIndex], 990
    if (flags.zero)
        goto l_goalkeeper_on_ground;        // jz short @@goalkeeper_on_ground

    {
        word src = (word)readMemory(esi + 70, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 992;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.imageIndex], 992
    if (flags.zero)
        goto l_goalkeeper_on_ground;        // jz short @@goalkeeper_on_ground

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (!flags.zero)
        goto cseg_80360;                    // jnz short cseg_80360

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = -5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, -5
    if (flags.sign != flags.overflow)
        goto cseg_8037A;                    // jl short cseg_8037A

    goto l_set_new_goalkeeper_speed;        // jmp short @@set_new_goalkeeper_speed

cseg_80360:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 5
    if (!flags.zero && flags.sign == flags.overflow)
        goto cseg_8037A;                    // jg short cseg_8037A

    goto l_set_new_goalkeeper_speed;        // jmp short @@set_new_goalkeeper_speed

l_goalkeeper_on_ground:;
    ax = dseg_110613;
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    goto l_set_new_goalkeeper_speed;        // jmp short @@set_new_goalkeeper_speed

cseg_8037A:;
    ax = dseg_110615;
    *(word *)&D0 = ax;                      // mov word ptr D0, ax

l_set_new_goalkeeper_speed:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    if (!flags.sign)
        goto cseg_803A4;                    // jns short cseg_803A4

    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0

cseg_803A4:;
    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 100;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameStatePl, 100
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 70, 1);     // mov al, byte ptr [esi+TeamGeneralInfo.field_46]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_803C4;                    // jz short cseg_803C4

    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_803C4:;
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto cseg_803E8;                    // jz short cseg_803E8

    esi = A2;                               // mov esi, A2
    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    D0 = eax;                               // mov D0, eax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (!flags.sign)
        goto l_update_player_speed_and_deltas; // jns @@update_player_speed_and_deltas

    goto cseg_80404;                        // jmp short cseg_80404

cseg_803E8:;
    esi = A2;                               // mov esi, A2
    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    D0 = eax;                               // mov D0, eax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    if (flags.sign)
        goto l_update_player_speed_and_deltas; // js @@update_player_speed_and_deltas

cseg_80404:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 70, 2);     // mov ax, [esi+Sprite.imageIndex]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_update_player_speed_and_deltas; // js @@update_player_speed_and_deltas

    eax = *(dword *)&g_memByte[323636];     // mov eax, g_spriteGraphicsPtr
    A0 = eax;                               // mov A0, eax
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    eax = readMemory(esi + ebx, 4);         // mov eax, [esi+ebx]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 10, 2);     // mov ax, [esi+SpriteGraphics.pixWidth]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    eax = readMemory(esi + 46, 4);          // mov eax, [esi+Sprite.deltaX]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (!flags.sign)
        goto cseg_804C6;                    // jns short cseg_804C6

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, 2
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 6;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 6
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, 3
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, ax
    if (!flags.carry)
        goto l_update_player_speed_and_deltas; // jnb @@update_player_speed_and_deltas

    ax = D0;                                // mov ax, word ptr D0
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, ax
    if (flags.carry || flags.zero)
        goto l_update_player_speed_and_deltas; // jbe @@update_player_speed_and_deltas

    goto cseg_80519;                        // jmp short cseg_80519

cseg_804C6:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 2;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 2
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 6;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 6
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 3;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 3
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, ax
    if (flags.carry)
        goto l_update_player_speed_and_deltas; // jb @@update_player_speed_and_deltas

    ax = D0;                                // mov ax, word ptr D0
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, ax
    if (!flags.carry && !flags.zero)
        goto l_update_player_speed_and_deltas; // ja @@update_player_speed_and_deltas

cseg_80519:;
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 40, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 20;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.z+2)], 20
    if (!flags.carry && !flags.zero)
        goto l_update_player_speed_and_deltas; // ja @@update_player_speed_and_deltas

    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_80540;                    // jnz short cseg_80540

    ax = *(word *)&g_memByte[515580];       // mov ax, penalty
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_805A7;                    // jz short cseg_805A7

cseg_80540:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 5
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_update_player_speed_and_deltas; // jg @@update_player_speed_and_deltas

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = -5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, -5
    if (flags.sign != flags.overflow)
        goto l_update_player_speed_and_deltas; // jl @@update_player_speed_and_deltas

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
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    goto cseg_80616;                        // jmp short cseg_80616

cseg_805A7:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 7;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 7
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_update_player_speed_and_deltas; // jg @@update_player_speed_and_deltas

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = -7;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, -7
    if (flags.sign != flags.overflow)
        goto l_update_player_speed_and_deltas; // jl @@update_player_speed_and_deltas

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0

cseg_80616:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 70, 1, 1);            // mov byte ptr [esi+70], 1
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        byte src = (byte)readMemory(esi + 12, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 6;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerState], PL_GOALIE_DIVING_HIGH
    if (flags.zero)
        goto l_goalie_jumping_high;         // jz short @@goalie_jumping_high

    {
        int16_t res = *(int16_t *)&D0 >> 1;
        flags.carry = ((word)*(int16_t *)&D0 >> 15) & 1;
        flags.overflow = (((*(int16_t *)&D0 >> 15) & 1)) != 0;
        *(int16_t *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // sar word ptr D0, 1
    goto cseg_8064D;                        // jmp short cseg_8064D

l_goalie_jumping_high:;
    {
        int16_t res = *(int16_t *)&D0 >> 2;
        *(int16_t *)&D0 = res;
    }                                       // sar word ptr D0, 2

cseg_8064D:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 24, 4);          // mov eax, [esi+24]
    A0 = eax;                               // mov A0, eax
    ax = *(word *)&g_memByte[515580];       // mov ax, penalty
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_80681;                    // jnz short cseg_80681

    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_8068B;                    // jz short cseg_8068B

cseg_80681:;
    A0 = 519065;                            // mov A0, offset dseg_17EECC

cseg_8068B:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 0;        // mov playerHadBall, 0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A5 = eax;                               // mov A5, eax
    esi = A5;                               // mov esi, A5
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 76, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto cseg_806EE;                    // js short cseg_806EE

    esi = A5;                               // mov esi, A5
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0

cseg_806EE:;
    ax = *(word *)&g_memByte[516924];       // mov ax, ballInGoalkeeperArea
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_ball_not_in_goalkeeper_area; // jnz short @@ball_not_in_goalkeeper_area

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A5 = eax;                               // mov A5, eax
    esi = A5;                               // mov esi, A5
    eax = readMemory(esi + 14, 4);          // mov eax, [esi+TeamGeneralInfo.teamStatsPtr]
    A5 = eax;                               // mov A5, eax
    esi = A5;                               // mov esi, A5
    {
        word src = (word)readMemory(esi + 10, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 10, 2, src);
    }                                       // add [esi+TeamStatisticsData.goalAttempts], 1

l_ball_not_in_goalkeeper_area:;
    resetBothTeamSpinTimers();              // call ResetBothTeamSpinTimers
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 76, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_8077F;                    // jns short cseg_8077F

    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        word res = *(word *)&D1 & 240;
        *(word *)&D1 = res;
    }                                       // and word ptr D1, 0F0h
    {
        word res = *(word *)&D1 >> 4;
        *(word *)&D1 = res;
    }                                       // shr word ptr D1, 4
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    if (flags.sign)
        goto cseg_807CB;                    // js short cseg_807CB

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    if (flags.sign)
        goto cseg_80A1B;                    // js cseg_80A1B

    goto cseg_808BF;                        // jmp cseg_808BF

cseg_8077F:;
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        word res = *(word *)&D1 & 240;
        *(word *)&D1 = res;
    }                                       // and word ptr D1, 0F0h
    {
        word res = *(word *)&D1 >> 4;
        *(word *)&D1 = res;
    }                                       // shr word ptr D1, 4
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    if (flags.sign)
        goto cseg_807CB;                    // js short cseg_807CB

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    if (flags.sign)
        goto cseg_808BF;                    // js cseg_808BF

    goto cseg_80A1B;                        // jmp cseg_80A1B

cseg_807CB:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 6
    if (!flags.zero && flags.sign == flags.overflow)
        goto cseg_80A1B;                    // jg cseg_80A1B

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = -6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, -6
    if (flags.sign != flags.overflow)
        goto cseg_80A1B;                    // jl cseg_80A1B

    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 32, 2, ax);           // mov word ptr [esi+(Sprite.x+2)], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 36, 2, ax);           // mov word ptr [esi+(Sprite.y+2)], ax
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 80, 2, 1);            // mov [esi+TeamGeneralInfo.goalkeeperDivingRight], 1
    push(D0);                               // push D0
    push(A6);                               // push A6
    goalkeeperClaimedTheBall();             // call GoalkeeperClaimedTheBall
    pop(A6);                                // pop A6
    pop(D0);                                // pop D0
    push(A0);                               // push A0
    SWOS::PlayGoalkeeperSavedComment();     // call PlayGoalkeeperSavedComment
    SWOS::PlayMissGoalSample();             // call PlayMissGoalSample
    pop(A0);                                // pop A0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_808BF:;
    eax = *(dword *)&g_memByte[515576];     // mov eax, lastPlayerPlayed
    *(dword *)&g_memByte[515568] = eax;     // mov lastKeeperPlayed, eax
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    esi = A2;                               // mov esi, A2
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
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    {
        word src = *(word *)&g_memByte[328536];
        int16_t dstSigned = src;
        int16_t srcSigned = 1792;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp ballSprite.speed, 1792
    if (flags.carry || flags.zero)
        goto cseg_80919;                    // jbe short cseg_80919

    *(word *)&g_memByte[328536] = 1792;     // mov ballSprite.speed, 1792

cseg_80919:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 76, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_8095F;                    // jns short cseg_8095F

    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 15;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0Fh
    {
        word res = *(word *)&D0 << 7;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 7
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 960;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 960
    ax = D0;                                // mov ax, word ptr D0
    {
        word src = *(word *)&g_memByte[328550];
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        *(word *)&g_memByte[328550] = src;
    }                                       // add ballSprite.destX, ax
    goto cseg_80B1D;                        // jmp cseg_80B1D

cseg_8095F:;
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 60, 2);     // mov ax, [esi+Sprite.destY]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        word res = *(word *)&D2 & 14;
        *(word *)&D2 = res;
    }                                       // and word ptr D2, 0Eh
    A0 = 325216;                            // mov A0, offset dseg_110BDB
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D2;                     // movzx ebx, word ptr D2
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    cl = D2;                                // mov cl, byte ptr D2
    {
        byte shiftCount = (int8_t)cl & 0x1f;
        if (shiftCount) {
            int16_t res = *(int16_t *)&D0 >> shiftCount;
            *(int16_t *)&D0 = res;
        }
    }                                       // sar word ptr D0, cl
    *(int16_t *)&D0 = -*(int16_t *)&D0;     // neg word ptr D0
    ax = D0;                                // mov ax, word ptr D0
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 58, 2);     // mov ax, [esi+Sprite.destX]
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    goto cseg_80B1D;                        // jmp cseg_80B1D

cseg_80A1B:;
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 60, 2);     // mov ax, [esi+Sprite.destY]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 31;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 1Fh
    {
        word res = *(word *)&D0 << 4;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 4
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 256;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 256
    ax = D0;                                // mov ax, word ptr D0
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 58, 2);     // mov ax, [esi+Sprite.destX]
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
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
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    {
        byte src = g_memByte[323626];
        byte res = src & 16;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // test byte ptr currentGameTick, 10h
    if (flags.zero)
        goto cseg_80B07;                    // jz short cseg_80B07

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

cseg_80B07:;
    {
        word src = *(word *)&g_memByte[328536];
        int16_t dstSigned = src;
        int16_t srcSigned = 1536;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp ballSprite.speed, 1536
    if (flags.carry || flags.zero)
        goto cseg_80B1B;                    // jbe short cseg_80B1B

    *(word *)&g_memByte[328536] = 1536;     // mov ballSprite.speed, 1536

cseg_80B1B:;

cseg_80B1D:;
    *(word *)&D0 = 1;                       // mov word ptr D0, 1
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
        goto cseg_80B39;                    // jz short cseg_80B39

    *(int16_t *)&D0 = -*(int16_t *)&D0;     // neg word ptr D0

cseg_80B39:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 36, 2, ax);           // mov word ptr [esi+(Sprite.y+2)], ax
    ax = D0;                                // mov ax, word ptr D0
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 36, 2, src);
    }                                       // add word ptr [esi+(Sprite.y+2)], ax

cseg_80B5F:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 76, 2);     // mov ax, [esi+TeamGeneralInfo.goalkeeperSavedCommentTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto cseg_80BA0;                    // js short cseg_80BA0

    esi = A0;                               // mov esi, A0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0

cseg_80BA0:;
    push(A0);                               // push A0
    SWOS::PlayGoalkeeperSavedComment();     // call PlayGoalkeeperSavedComment
    SWOS::PlayMissGoalSample();             // call PlayMissGoalSample
    pop(A0);                                // pop A0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_its_controlled_player:;
    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_check_for_cpu_team;          // jz short @@check_for_cpu_team

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 31;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PENALTIES
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

l_check_for_cpu_team:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_80C0C;                    // jnz short cseg_80C0C

    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    AI_SetControlsDirection();              // call AI_SetControlsDirection
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1

cseg_80C0C:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    writeMemory(esi + 42, 2, ax);           // mov [esi+TeamGeneralInfo.allowedDirections], ax
    writeMemory(esi + 40, 2, 0);            // mov [esi+TeamGeneralInfo.playerHasBall], 0
    ax = (word)readMemory(esi + 58, 2);     // mov ax, [esi+TeamGeneralInfo.shooting]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_80C56;                    // jz short cseg_80C56

    ax = (word)readMemory(esi + 50, 2);     // mov ax, word ptr [esi+TeamGeneralInfo.firePressed]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_80C56;                    // jnz short cseg_80C56

    writeMemory(esi + 58, 2, 0);            // mov [esi+TeamGeneralInfo.shooting], 0

cseg_80C56:;
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
        goto l_skip_break_handling;         // jz @@skip_break_handling

    eax = *(dword *)&g_memByte[515588];     // mov eax, lastTeamPlayedBeforeBreak
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (!flags.zero)
        goto cseg_80CB3;                    // jnz short cseg_80CB3

    ax = *(word *)&g_memByte[515608];       // mov ax, cameraDirection
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    {
        word src = *(word *)&g_memByte[449206];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        *(word *)&g_memByte[449206] = src;
    }                                       // add dseg_132806, 1

cseg_80CB3:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (!flags.zero)
        goto l_turn_flags_acceptable;       // jnz short @@turn_flags_acceptable

    *(word *)&D0 = 7;                       // mov word ptr D0, 7

l_find_acceptable_turn_flags_loop:;
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (!flags.zero)
        goto l_created_acceptable_turn_flags; // jnz short @@created_acceptable_turn_flags

    (*(int16_t *)&D0)--;
    flags.overflow = (int16_t)(*(int16_t *)&D0) == INT16_MIN;
    flags.sign = (*(int16_t *)&D0 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D0 == 0;      // dec word ptr D0
    if (!flags.sign)
        goto l_find_acceptable_turn_flags_loop; // jns short @@find_acceptable_turn_flags_loop

    *(word *)&D0 = 0;                       // mov word ptr D0, 0

l_created_acceptable_turn_flags:;
    ax = D0;                                // mov ax, word ptr D0
    *(word *)&g_memByte[515608] = ax;       // mov cameraDirection, ax
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    {
        word src = *(word *)&g_memByte[449210];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        *(word *)&g_memByte[449210] = src;
    }                                       // add deadThrowInDirectionVar, 1

l_turn_flags_acceptable:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_80D49;                    // jnz short cseg_80D49

    {
        word src = *(word *)&g_memByte[515594];
        int16_t dstSigned = src;
        int16_t srcSigned = 55;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp stoppageTimerActive, 55
    if (!flags.carry && !flags.zero)
        goto l_hide_result;                 // ja short @@hide_result

    goto l_test_fire;                       // jmp short @@test_fire

cseg_80D49:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_joy_any_fire_pressed;        // jns short @@joy_any_fire_pressed

l_test_fire:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 50, 1);     // mov al, [esi+TeamGeneralInfo.firePressed]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_joy_any_fire_pressed;        // jnz short @@joy_any_fire_pressed

    al = (byte)readMemory(esi + 48, 1);     // mov al, [esi+TeamGeneralInfo.quickFire]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_joy_any_fire_pressed;        // jnz short @@joy_any_fire_pressed

    al = (byte)readMemory(esi + 49, 1);     // mov al, [esi+TeamGeneralInfo.normalFire]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_test_direction;              // jz short @@test_direction

l_joy_any_fire_pressed:;
    ax = *(word *)&g_memByte[449120];       // mov ax, statsTimer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_hide_result;                 // jz short @@hide_result

    *(word *)&g_memByte[515586] = 1;        // mov fireBlocked, 1

l_hide_result:;
    {
        int16_t src = *(word *)&g_memByte[449124];
        src = -src;
        *(word *)&g_memByte[449124] = src;
    }                                       // neg timeVar
    {
        int16_t src = *(word *)&g_memByte[449122];
        src = -src;
        *(word *)&g_memByte[449122] = src;
    }                                       // neg resultTimer

l_test_direction:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_80DCC;                    // jns short cseg_80DCC

cseg_80DB6:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    goto l_skip_break_handling;             // jmp short @@skip_break_handling

cseg_80DCC:;
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (flags.zero)
        goto cseg_80DB6;                    // jz short cseg_80DB6

    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    updatePlayerWithBall();                 // call UpdatePlayerWithBall

l_skip_break_handling:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_80E41;                    // jns short cseg_80E41

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax

cseg_80E41:;
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
        goto l_test_quick_fire;             // jnz @@test_quick_fire

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 48, 2);     // mov ax, word ptr [esi+TeamGeneralInfo.quickFire]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_test_quick_fire;             // jnz @@test_quick_fire

    ax = (word)readMemory(esi + 58, 2);     // mov ax, [esi+TeamGeneralInfo.shooting]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_test_quick_fire;             // jnz @@test_quick_fire

    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_80E8F;                    // jnz short cseg_80E8F

    al = (byte)readMemory(esi + 62, 1);     // mov al, [esi+TeamGeneralInfo.plCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_80ECF;                    // jz short cseg_80ECF

cseg_80E8F:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+TeamGeneralInfo.allowedDirections]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_80EAA;                    // jnz short cseg_80EAA

    writeMemory(esi + 108, 2, 0);           // mov [esi+TeamGeneralInfo.unkBallTimer], 0

cseg_80EAA:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 40, 2, 1);            // mov [esi+TeamGeneralInfo.playerHasBall], 1
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 118, 2, -1);          // mov [esi+TeamGeneralInfo.spinTimer], -1

cseg_80ECF:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_80FFD;                    // jns cseg_80FFD

    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_jmp_update_player_speed;     // jz @@jmp_update_player_speed

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
    if (flags.zero)
        goto l_jmp_update_player_speed;     // jz @@jmp_update_player_speed

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 51, 1);     // mov al, [esi+TeamGeneralInfo.fireThisFrame]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_jmp_update_player_speed;     // jz @@jmp_update_player_speed

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_jmp_update_player_speed;     // jns @@jmp_update_player_speed

    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_80F61;                    // jnz short cseg_80F61

    al = (byte)readMemory(esi + 62, 1);     // mov al, [esi+TeamGeneralInfo.plCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_80F61;                    // jnz short cseg_80F61

    al = (byte)readMemory(esi + 63, 1);     // mov al, [esi+TeamGeneralInfo.plNotFarFromBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_jmp_update_player_speed;     // jz @@jmp_update_player_speed

cseg_80F61:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 66, 1);     // mov al, [esi+TeamGeneralInfo.ball8To12]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_80F88;                    // jnz short cseg_80F88

    al = (byte)readMemory(esi + 67, 1);     // mov al, [esi+TeamGeneralInfo.ball12To17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_80F88;                    // jnz short cseg_80F88

    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_jmp_update_player_speed;     // jz short @@jmp_update_player_speed

cseg_80F88:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 52, 2, 1);            // mov [esi+TeamGeneralInfo.headerOrTackle], 1
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    attemptStaticHeader();                  // call AttemptStaticHeader
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    eax = A1;                               // mov eax, A1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 72, 4, eax);          // mov [esi+TeamGeneralInfo.lastHeadingTacklingPlayer], eax
    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_jmp_update_player_speed:;
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_80FFD:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_81028;                    // jnz short cseg_81028

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 138, 2, 0);           // mov [esi+TeamGeneralInfo.wonTheBallTimer], 0

cseg_81028:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 138, 2);    // mov ax, [esi+TeamGeneralInfo.wonTheBallTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_81793;                    // jnz cseg_81793

    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_test_quick_fire;             // jz @@test_quick_fire

    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_test_quick_fire;             // jnz @@test_quick_fire

    al = (byte)readMemory(esi + 67, 1);     // mov al, [esi+TeamGeneralInfo.ball12To17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_test_quick_fire;             // jnz @@test_quick_fire

    al = (byte)readMemory(esi + 69, 1);     // mov al, [esi+TeamGeneralInfo.prevPlVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_8108A;                    // jnz short cseg_8108A

    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0

cseg_8108A:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 112, 2);    // mov ax, [esi+TeamGeneralInfo.ballControllingPlayerDirection]
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
        goto l_ball_becomes_free;           // jz short @@ball_becomes_free

    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0

l_ball_becomes_free:;
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 110, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 110, 2, src);
    }                                       // add [esi+TeamGeneralInfo.ballCanBeControlled], 1
    ax = (word)readMemory(esi + 110, 2);    // mov ax, [esi+TeamGeneralInfo.ballCanBeControlled]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 112, 2, ax);          // mov [esi+TeamGeneralInfo.ballControllingPlayerDirection], ax
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 0;        // mov playerHadBall, 0
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_calculate_if_player_wins_ball; // jz short @@calculate_if_player_wins_ball

    *(word *)&g_memByte[515566] = 1;        // mov playerHadBall, 1

l_calculate_if_player_wins_ball:;
    push(D0);                               // push D0
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    calculateIfPlayerWinsBall();            // call CalculateIfPlayerWinsBall
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    pop(D0);                                // pop D0
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 118, 2, -1);          // mov [esi+TeamGeneralInfo.spinTimer], -1
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto cseg_81793;                        // jmp cseg_81793

l_test_quick_fire:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 48, 1);     // mov al, [esi+TeamGeneralInfo.quickFire]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_no_passing;                  // jz @@no_passing

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_no_passing;                  // js @@no_passing

    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_811DF;                    // jnz short cseg_811DF

    al = (byte)readMemory(esi + 62, 1);     // mov al, [esi+TeamGeneralInfo.plCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_no_passing;                  // jz @@no_passing

cseg_811DF:;
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
        goto cseg_81203;                    // jz short cseg_81203

    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (flags.zero)
        goto l_no_passing;                  // jz @@no_passing

cseg_81203:;
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
    if (flags.zero)
        goto cseg_81221;                    // jz short cseg_81221

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 64, 1);     // mov al, [esi+TeamGeneralInfo.ballLessEqual4]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_no_passing;                  // jz @@no_passing

cseg_81221:;
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 0;        // mov playerHadBall, 0
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 40, 2, 0);            // mov word ptr [esi+(Sprite.z+2)], 0
    push(D0);                               // push D0
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    doPass();                               // call DoPass
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    pop(D0);                                // pop D0
    eax = A1;                               // mov eax, A1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 72, 4, eax);          // mov [esi+TeamGeneralInfo.lastHeadingTacklingPlayer], eax
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
        goto cseg_812C6;                    // jz short cseg_812C6

    *(word *)&g_memByte[515584] = 0;        // mov gameNotInProgressCounterWriteOnly, 0

cseg_812C6:;
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 14;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PENALTY
    if (!flags.zero)
        goto l_not_penalty;                 // jnz short @@not_penalty

    *(word *)&g_memByte[515580] = 1;        // mov penalty, 1

l_not_penalty:;
    *(word *)&g_memByte[515596] = 100;      // mov gameStatePl, 100
    *(word *)&g_memByte[515598] = 100;      // mov gameState, ST_GAME_IN_PROGRESS
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 94, 2, 1);            // mov [esi+TeamGeneralInfo.ballInPlay], 1
    writeMemory(esi + 96, 2, 1);            // mov [esi+TeamGeneralInfo.ballOutOfPlay], 1
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 94, 2, 1);            // mov [esi+TeamGeneralInfo.ballInPlay], 1
    writeMemory(esi + 96, 2, 1);            // mov [esi+TeamGeneralInfo.ballOutOfPlay], 1
    writeMemory(esi + 118, 2, -1);          // mov [esi+TeamGeneralInfo.spinTimer], -1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 32, 4, 0);            // mov [esi+TeamGeneralInfo.controlledPlayer], 0
    eax = A1;                               // mov eax, A1
    writeMemory(esi + 104, 4, eax);         // mov [esi+TeamGeneralInfo.passingKickingPlayer], eax
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0
    eax = readMemory(esi, 4);               // mov eax, [esi]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto l_stop_player;                     // jmp @@stop_player

l_no_passing:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 49, 1);     // mov al, [esi+TeamGeneralInfo.normalFire]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_check_if_goalkeeper;         // jz @@check_if_goalkeeper

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_check_if_goalkeeper;         // js @@check_if_goalkeeper

    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_813DA;                    // jnz short cseg_813DA

    al = (byte)readMemory(esi + 62, 1);     // mov al, [esi+TeamGeneralInfo.plCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_check_if_goalkeeper;         // jz @@check_if_goalkeeper

cseg_813DA:;
    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 100;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameStatePl, 100
    if (flags.zero)
        goto cseg_813FE;                    // jz short cseg_813FE

    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (flags.zero)
        goto l_check_if_goalkeeper;         // jz @@check_if_goalkeeper

cseg_813FE:;
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
    if (flags.zero)
        goto cseg_8141C;                    // jz short cseg_8141C

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 64, 1);     // mov al, [esi+TeamGeneralInfo.ballLessEqual4]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_check_if_goalkeeper;         // jz @@check_if_goalkeeper

cseg_8141C:;
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 0;        // mov playerHadBall, 0
    push(D0);                               // push D0
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playerKickingBall();                    // call PlayerKickingBall
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    pop(D0);                                // pop D0
    eax = A1;                               // mov eax, A1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 72, 4, eax);          // mov [esi+TeamGeneralInfo.lastHeadingTacklingPlayer], eax
    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 100;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameStatePl, 100
    if (flags.zero)
        goto cseg_814B5;                    // jz short cseg_814B5

    *(word *)&g_memByte[515584] = 0;        // mov gameNotInProgressCounterWriteOnly, 0

cseg_814B5:;
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 14;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PENALTY
    if (!flags.zero)
        goto l_not_penalty2;                // jnz short @@not_penalty2

    *(word *)&g_memByte[515580] = 1;        // mov penalty, 1

l_not_penalty2:;
    *(word *)&g_memByte[515596] = 100;      // mov gameStatePl, 100
    *(word *)&g_memByte[515598] = 100;      // mov gameState, ST_GAME_IN_PROGRESS
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 94, 2, 1);            // mov [esi+TeamGeneralInfo.ballInPlay], 1
    writeMemory(esi + 96, 2, 1);            // mov [esi+TeamGeneralInfo.ballOutOfPlay], 1
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 94, 2, 1);            // mov [esi+TeamGeneralInfo.ballInPlay], 1
    writeMemory(esi + 96, 2, 1);            // mov [esi+TeamGeneralInfo.ballOutOfPlay], 1
    writeMemory(esi + 118, 2, -1);          // mov [esi+TeamGeneralInfo.spinTimer], -1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 32, 4, 0);            // mov [esi+TeamGeneralInfo.controlledPlayer], 0
    eax = A1;                               // mov eax, A1
    writeMemory(esi + 104, 4, eax);         // mov [esi+TeamGeneralInfo.passingKickingPlayer], eax
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0
    writeMemory(esi + 58, 2, 1);            // mov [esi+TeamGeneralInfo.shooting], 1
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto l_stop_player;                     // jmp @@stop_player

l_check_if_goalkeeper:;
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
    if (flags.zero)
        goto cseg_8167F;                    // jz cseg_8167F

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 51, 1);     // mov al, [esi+TeamGeneralInfo.fireThisFrame]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_8167F;                    // jz cseg_8167F

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto cseg_8167F;                    // js cseg_8167F

    al = (byte)readMemory(esi + 63, 1);     // mov al, [esi+TeamGeneralInfo.plNotFarFromBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_8167F;                    // jz cseg_8167F

    al = (byte)readMemory(esi + 64, 1);     // mov al, [esi+TeamGeneralInfo.ballLessEqual4]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_815F7;                    // jnz short cseg_815F7

    al = (byte)readMemory(esi + 65, 1);     // mov al, [esi+TeamGeneralInfo.ball4To8]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_8167F;                    // jz cseg_8167F

cseg_815F7:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
    A5 = eax;                               // mov A5, eax
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A5, 0
    if (flags.zero)
        goto l_begin_tackling;              // jz short @@begin_tackling

    esi = A5;                               // mov esi, A5
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    D1 = eax;                               // mov D1, eax
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D1, eax
    if (flags.carry)
        goto cseg_8167F;                    // jb short cseg_8167F

l_begin_tackling:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 52, 2, 1);            // mov [esi+TeamGeneralInfo.headerOrTackle], 1
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playerBeginTackling();                  // call PlayerBeginTackling
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_8167F:;
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
    if (flags.zero)
        goto l_not_a_header;                // jz @@not_a_header

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 51, 1);     // mov al, [esi+TeamGeneralInfo.fireThisFrame]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_not_a_header;                // jz @@not_a_header

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_not_a_header;                // js @@not_a_header

    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_816E5;                    // jnz short cseg_816E5

    al = (byte)readMemory(esi + 62, 1);     // mov al, [esi+TeamGeneralInfo.plCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_816E5;                    // jnz short cseg_816E5

    al = (byte)readMemory(esi + 63, 1);     // mov al, [esi+TeamGeneralInfo.plNotFarFromBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_not_a_header;                // jz @@not_a_header

cseg_816E5:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 66, 1);     // mov al, [esi+TeamGeneralInfo.ball8To12]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_its_a_header;                // jnz short @@its_a_header

    al = (byte)readMemory(esi + 67, 1);     // mov al, [esi+TeamGeneralInfo.ball12To17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_its_a_header;                // jnz short @@its_a_header

    al = (byte)readMemory(esi + 68, 1);     // mov al, [esi+TeamGeneralInfo.ballAbove17]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_not_a_header;                // jz short @@not_a_header

l_its_a_header:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 52, 2, 1);            // mov [esi+TeamGeneralInfo.headerOrTackle], 1
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playerAttemptingJumpHeader();           // call PlayerAttemptingJumpHeader
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    eax = A1;                               // mov eax, A1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 72, 4, eax);          // mov [esi+TeamGeneralInfo.lastHeadingTacklingPlayer], eax
    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_not_a_header:;
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
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_update_player_speed_and_deltas; // js @@update_player_speed_and_deltas

cseg_81793:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    *(byte *)&D3 = 0;                       // mov byte ptr D3, 0
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 79;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 79
    if (flags.sign == flags.overflow)
        goto l_inside_pitch_left_x;         // jge short @@inside_pitch_left_x

    *(byte *)&D3 |= 224;                    // or byte ptr D3, 0E0h

l_inside_pitch_left_x:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 592;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 592
    if (flags.zero || flags.sign != flags.overflow)
        goto l_inside_pitch_right_x;        // jle short @@inside_pitch_right_x

    *(byte *)&D3 |= 14;                     // or byte ptr D3, 0Eh

l_inside_pitch_right_x:;
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 127;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 127
    if (flags.sign == flags.overflow)
        goto l_inside_pitch_top_y;          // jge short @@inside_pitch_top_y

    *(byte *)&D3 |= 131;                    // or byte ptr D3, 83h

l_inside_pitch_top_y:;
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 771;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 771
    if (flags.zero || flags.sign != flags.overflow)
        goto l_inside_pitch_bottom_y;       // jle short @@inside_pitch_bottom_y

    *(byte *)&D3 |= 56;                     // or byte ptr D3, 38h

l_inside_pitch_bottom_y:;
    cx = D0;                                // mov cx, word ptr D0
    eax = 1;                                // mov eax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            dword res = eax << shiftCount;
            eax = res;
        }
    }                                       // shl eax, cl
    {
        dword res = D3 & eax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // test D3, eax
    if (flags.zero)
        goto l_update_player_dest_x_y;      // jz short @@update_player_dest_x_y

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_update_player_dest_x_y:;
    A5 = 515768;                            // mov A5, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    eax = A5;                               // mov eax, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = ebx;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, ebx
    A5 = eax;                               // mov A5, eax
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 2, 2);      // mov ax, [esi+2]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D2 = res;
    }                                       // add word ptr D2, ax
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = D2;                                // mov ax, word ptr D2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_taking_throw_in:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_test_allowed_turn_flags;     // jnz short @@test_allowed_turn_flags

    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    AI_SetControlsDirection();              // call AI_SetControlsDirection
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1

l_test_allowed_turn_flags:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        byte src = g_memByte[515610];
        byte res = src & al;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // test byte ptr playerTurnFlags, al
    if (!flags.zero)
        goto l_test_turn_flags_with_camera_direction; // jnz short @@test_turn_flags_with_camera_direction

    ax = *(word *)&g_memByte[515608];       // mov ax, cameraDirection
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    {
        word src = *(word *)&g_memByte[449216];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        *(word *)&g_memByte[449216] = src;
    }                                       // add disallowedTurnFlagsCounter, 1

l_test_turn_flags_with_camera_direction:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        byte src = g_memByte[515610];
        byte res = src & al;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // test byte ptr playerTurnFlags, al
    if (!flags.zero)
        goto l_check_if_throw_in_taker_substituted; // jnz short @@check_if_throw_in_taker_substituted

    *(word *)&D0 = 7;                       // mov word ptr D0, 7

l_next_direction:;
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        byte src = g_memByte[515610];
        byte res = src & al;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // test byte ptr playerTurnFlags, al
    if (!flags.zero)
        goto l_found_direction;             // jnz short @@found_direction

    (*(int16_t *)&D0)--;
    flags.overflow = (int16_t)(*(int16_t *)&D0) == INT16_MIN;
    flags.sign = (*(int16_t *)&D0 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D0 == 0;      // dec word ptr D0
    if (!flags.sign)
        goto l_next_direction;              // jns short @@next_direction

    *(word *)&D0 = 0;                       // mov word ptr D0, 0

l_found_direction:;
    ax = D0;                                // mov ax, word ptr D0
    *(word *)&g_memByte[515608] = ax;       // mov cameraDirection, ax
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    {
        word src = *(word *)&g_memByte[449210];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        *(word *)&g_memByte[449210] = src;
    }                                       // add deadThrowInDirectionVar, 1

l_check_if_throw_in_taker_substituted:;
    ax = *(word *)&g_memByte[478672];       // mov ax, g_substituteInProgress
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_check_throw_in_game_state;   // jz short @@check_throw_in_game_state

    eax = *(dword *)&g_memByte[478676];     // mov eax, substitutedPlSprite
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
        goto l_abort_throw_in;              // jz @@abort_throw_in

l_check_throw_in_game_state:;
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
        goto l_abort_throw_in;              // jb @@abort_throw_in

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
    if (!flags.carry && !flags.zero)
        goto l_abort_throw_in;              // ja @@abort_throw_in

    esi = A1;                               // mov esi, A1
    al = (byte)readMemory(esi + 13, 1);     // mov al, [esi+Sprite.playerDownTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_ready_for_throw_in;          // jz @@ready_for_throw_in

    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (flags.zero)
        goto l_throw_in_over;               // jz @@throw_in_over

    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 18;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerDownTimer], 18
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    ax = *(word *)&g_memByte[478658];       // mov ax, g_inSubstitutesMenu
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_throw_in_done_check_pass_or_kick; // jz short @@throw_in_done_check_pass_or_kick

    setPlayerAnimationTable(A1.as<Sprite&>(), getAboutToThrowInAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 5);            // mov [esi+Sprite.playerState], PL_THROW_IN
    writeMemory(esi + 13, 1, 0);            // mov [esi+Sprite.playerDownTimer], 0
    goto l_ready_for_throw_in;              // jmp short @@ready_for_throw_in

l_throw_in_done_check_pass_or_kick:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 40, 2, 12);           // mov word ptr [esi+(Sprite.z+2)], 12
    *(word *)&g_memByte[449180] = 0;        // mov hideBall, 0
    ax = *(word *)&g_memByte[515620];       // mov ax, throwInPassOrKick
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_do_throw_in_pass;            // jnz @@do_throw_in_pass

    goto l_do_throw_in_kick;                // jmp @@do_throw_in_kick

l_ready_for_throw_in:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_throw_in_check_input_direction; // jnz short @@throw_in_check_input_direction

    {
        word src = *(word *)&g_memByte[515594];
        int16_t dstSigned = src;
        int16_t srcSigned = 55;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp stoppageTimerActive, 55
    if (!flags.carry && !flags.zero)
        goto l_throw_in_hide_result;        // ja short @@throw_in_hide_result

    goto l_throw_in_check_fire;             // jmp short @@throw_in_check_fire

l_throw_in_check_input_direction:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_check_if_stats_showing;      // jns short @@check_if_stats_showing

l_throw_in_check_fire:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 50, 1);     // mov al, [esi+TeamGeneralInfo.firePressed]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_check_if_stats_showing;      // jnz short @@check_if_stats_showing

    al = (byte)readMemory(esi + 48, 1);     // mov al, [esi+TeamGeneralInfo.quickFire]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_check_if_stats_showing;      // jnz short @@check_if_stats_showing

    al = (byte)readMemory(esi + 49, 1);     // mov al, [esi+TeamGeneralInfo.normalFire]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_throw_in_check_direction;    // jz short @@throw_in_check_direction

l_check_if_stats_showing:;
    ax = *(word *)&g_memByte[449120];       // mov ax, statsTimer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_throw_in_hide_result;        // jz short @@throw_in_hide_result

    *(word *)&g_memByte[515586] = 1;        // mov fireBlocked, 1

l_throw_in_hide_result:;
    {
        int16_t src = *(word *)&g_memByte[449124];
        src = -src;
        *(word *)&g_memByte[449124] = src;
    }                                       // neg timeVar
    {
        int16_t src = *(word *)&g_memByte[449122];
        src = -src;
        *(word *)&g_memByte[449122] = src;
    }                                       // neg resultTimer

l_throw_in_check_direction:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_throw_in_got_input_direction; // jns short @@throw_in_got_input_direction

l_throw_in_use_sprite_direction:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    goto l_throw_in_check_quick_fire;       // jmp short @@throw_in_check_quick_fire

l_throw_in_got_input_direction:;
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        byte src = g_memByte[515610];
        byte res = src & al;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // test byte ptr playerTurnFlags, al
    if (flags.zero)
        goto l_throw_in_use_sprite_direction; // jz short @@throw_in_use_sprite_direction

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
        goto l_throw_in_check_quick_fire;   // jz short @@throw_in_check_quick_fire

    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    push(D0);                               // push D0
    setThrowInPlayerDestinationCoordinates(); // call SetThrowInPlayerDestinationCoordinates
    pop(D0);                                // pop D0
    setPlayerAnimationTable(A1.as<Sprite&>(), getAboutToThrowInAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 5);            // mov [esi+Sprite.playerState], PL_THROW_IN
    writeMemory(esi + 13, 1, 0);            // mov [esi+Sprite.playerDownTimer], 0

l_throw_in_check_quick_fire:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 48, 1);     // mov al, [esi+TeamGeneralInfo.quickFire]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_throw_in_check_normal_fire;  // jz short @@throw_in_check_normal_fire

    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (flags.zero)
        goto l_throw_in_check_normal_fire;  // jz short @@throw_in_check_normal_fire

    *(word *)&g_memByte[515620] = 1;        // mov throwInPassOrKick, 1
    setPlayerAnimationTable(A1.as<Sprite&>(), getThrowInShortAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 13, 1, 20);           // mov [esi+Sprite.playerDownTimer], 20
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_throw_in_check_normal_fire:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 49, 1);     // mov al, [esi+TeamGeneralInfo.normalFire]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    *(word *)&g_memByte[515620] = 0;        // mov throwInPassOrKick, 0
    setPlayerAnimationTable(A1.as<Sprite&>(), getThrowInLongAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 13, 1, 25);           // mov [esi+Sprite.playerDownTimer], 25
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_do_throw_in_pass:;
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515566] = 1;        // mov playerHadBall, 1
    push(D0);                               // push D0
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    doPass();                               // call DoPass
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    pop(D0);                                // pop D0
    eax = A1;                               // mov eax, A1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 72, 4, eax);          // mov [esi+TeamGeneralInfo.lastHeadingTacklingPlayer], eax
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 54, 4, 1);            // mov [esi+Sprite.deltaZ], 1
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
        goto l_throw_in_ball_passed;        // jz short @@throw_in_ball_passed

    *(word *)&g_memByte[515584] = 0;        // mov gameNotInProgressCounterWriteOnly, 0

l_throw_in_ball_passed:;
    *(word *)&g_memByte[515596] = 100;      // mov gameStatePl, ST_GAME_IN_PROGRESS
    *(word *)&g_memByte[515598] = 100;      // mov gameState, ST_GAME_IN_PROGRESS
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 94, 2, 1);            // mov [esi+TeamGeneralInfo.ballInPlay], 1
    writeMemory(esi + 96, 2, 1);            // mov [esi+TeamGeneralInfo.ballOutOfPlay], 1
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 94, 2, 1);            // mov [esi+TeamGeneralInfo.ballInPlay], 1
    writeMemory(esi + 96, 2, 1);            // mov [esi+TeamGeneralInfo.ballOutOfPlay], 1
    writeMemory(esi + 118, 2, -1);          // mov [esi+TeamGeneralInfo.spinTimer], -1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 32, 4, 0);            // mov [esi+TeamGeneralInfo.controlledPlayer], 0
    eax = A1;                               // mov eax, A1
    writeMemory(esi + 104, 4, eax);         // mov [esi+TeamGeneralInfo.passingKickingPlayer], eax
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_do_throw_in_kick:;
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515566] = 1;        // mov playerHadBall, 1
    push(D0);                               // push D0
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playerKickingBall();                    // call PlayerKickingBall
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    pop(D0);                                // pop D0
    eax = A1;                               // mov eax, A1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 72, 4, eax);          // mov [esi+TeamGeneralInfo.lastHeadingTacklingPlayer], eax
    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 100;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameStatePl, 100
    if (flags.zero)
        goto l_throw_in_ball_kicked;        // jz short @@throw_in_ball_kicked

    *(word *)&g_memByte[515584] = 0;        // mov gameNotInProgressCounterWriteOnly, 0

l_throw_in_ball_kicked:;
    *(word *)&g_memByte[515596] = 100;      // mov gameStatePl, ST_GAME_IN_PROGRESS
    *(word *)&g_memByte[515598] = 100;      // mov gameState, ST_GAME_IN_PROGRESS
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 94, 2, 1);            // mov [esi+TeamGeneralInfo.ballInPlay], 1
    writeMemory(esi + 96, 2, 1);            // mov [esi+TeamGeneralInfo.ballOutOfPlay], 1
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 94, 2, 1);            // mov [esi+TeamGeneralInfo.ballInPlay], 1
    writeMemory(esi + 96, 2, 1);            // mov [esi+TeamGeneralInfo.ballOutOfPlay], 1
    writeMemory(esi + 118, 2, -1);          // mov [esi+TeamGeneralInfo.spinTimer], -1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 32, 4, 0);            // mov [esi+TeamGeneralInfo.controlledPlayer], 0
    eax = A1;                               // mov eax, A1
    writeMemory(esi + 104, 4, eax);         // mov [esi+TeamGeneralInfo.passingKickingPlayer], eax
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_abort_throw_in:;
    *(word *)&g_memByte[449180] = 0;        // mov hideBall, 0

l_throw_in_over:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_tackling:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 106, 2);    // mov ax, [esi+Sprite.tacklingTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_bump_player_down_timer;      // js short @@bump_player_down_timer

    {
        word src = (word)readMemory(esi + 106, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 106, 2, src);
    }                                       // add [esi+Sprite.tacklingTimer], 1

l_bump_player_down_timer:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (!flags.zero)
        goto l_player_down_tackling;        // jnz short @@player_down_tackling

    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_stop_player;                     // jmp @@stop_player

l_player_down_tackling:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 106, 2);    // mov ax, [esi+Sprite.tacklingTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_computer_tackling;           // js short @@computer_tackling

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_computer_tackling;           // jz short @@computer_tackling

    al = (byte)readMemory(esi + 50, 1);     // mov al, [esi+TeamGeneralInfo.firePressed]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_computer_tackling;           // jnz short @@computer_tackling

    esi = A1;                               // mov esi, A1
    {
        int16_t src = (int16_t)readMemory(esi + 106, 2);
        src = -src;
        writeMemory(esi + 106, 2, src);
    }                                       // neg [esi+Sprite.tacklingTimer]
    {
        word src = (word)readMemory(esi + 106, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = -2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.tacklingTimer], -2
    if (flags.sign != flags.overflow)
        goto l_computer_tackling;           // jl short @@computer_tackling

    writeMemory(esi + 106, 2, -1);          // mov [esi+Sprite.tacklingTimer], -1

l_computer_tackling:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    ax = *(word *)&g_memByte[325300];       // mov ax, kPlayerGroundConstant
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_player_still_tackling_and_moving; // jg short @@player_still_tackling_and_moving

    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    setPlayerDowntimeAfterTackle();         // call SetPlayerDowntimeAfterTackle
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_still_tackling_and_moving:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 73;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 73
    if (flags.sign != flags.overflow)
        goto l_pl_tackling_out_of_pitch;    // jl @@pl_tackling_out_of_pitch

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 598;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 598
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_pl_tackling_out_of_pitch;    // jg @@pl_tackling_out_of_pitch

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 129
    if (flags.sign != flags.overflow)
        goto l_pl_tackling_out_of_pitch_y;  // jl short @@pl_tackling_out_of_pitch_y

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 769;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 769
    if (flags.zero || flags.sign != flags.overflow)
        goto l_pl_tackling_in_pitch;        // jle @@pl_tackling_in_pitch

l_pl_tackling_out_of_pitch_y:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 275;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 275
    if (flags.sign != flags.overflow)
        goto l_pl_tackling_out_of_goalie_area; // jl short @@pl_tackling_out_of_goalie_area

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 396;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 396
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_pl_tackling_out_of_goalie_area; // jg short @@pl_tackling_out_of_goalie_area

    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // shr [esi+Sprite.speed], 1
    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // shr [esi+Sprite.speed], 1
    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // shr [esi+Sprite.speed], 1
    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // shr [esi+Sprite.speed], 1
    {
        word src = (word)readMemory(esi + 44, 2);
        src |= 1;
        writeMemory(esi + 44, 2, src);
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (src & 0x8000) != 0;
        flags.zero = src == 0;
    }                                       // or [esi+Sprite.speed], 1
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_pl_tackling_out_of_goalie_area:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 121;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 121
    if (flags.sign != flags.overflow)
        goto l_pl_tackling_out_of_pitch;    // jl short @@pl_tackling_out_of_pitch

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 777;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 777
    if (flags.zero || flags.sign != flags.overflow)
        goto l_update_player_speed_and_deltas; // jle @@update_player_speed_and_deltas

l_pl_tackling_out_of_pitch:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 >> 3;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 3
    ax = D0;                                // mov ax, word ptr D0
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_pl_tackling_in_pitch:;
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
        goto l_tackling_empty_space;        // jnz @@tackling_empty_space

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 96, 2);     // mov ax, [esi+Sprite.tackleState]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_tackling_empty_space;        // jnz @@tackling_empty_space

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
        goto l_tackling_near_the_ball;      // jnz short @@tackling_near_the_ball

    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 64;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 64
    if (!flags.carry && !flags.zero)
        goto l_tackling_empty_space;        // ja @@tackling_empty_space

l_tackling_near_the_ball:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 64, 1);     // mov al, [esi+TeamGeneralInfo.ballLessEqual4]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_player_tackling_the_ball;    // jnz short @@player_tackling_the_ball

    al = (byte)readMemory(esi + 65, 1);     // mov al, [esi+TeamGeneralInfo.ball4To8]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_tackling_empty_space;        // jz @@tackling_empty_space

l_player_tackling_the_ball:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 0;        // mov playerHadBall, 0
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_check_if_strong_tackle;      // jz short @@check_if_strong_tackle

    *(word *)&g_memByte[515566] = 1;        // mov playerHadBall, 1

l_check_if_strong_tackle:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 106, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = -1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.tacklingTimer], -1
    if (!flags.zero)
        goto l_strong_tackle;               // jnz short @@strong_tackle

    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playerTackledTheBallWeak();             // call PlayerTackledTheBallWeak
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    goto cseg_821F5;                        // jmp short cseg_821F5

l_strong_tackle:;
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playersTackledTheBallStrong();          // call PlayersTackledTheBallStrong
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0

cseg_821F5:;
    eax = A1;                               // mov eax, A1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 72, 4, eax);          // mov [esi+TeamGeneralInfo.lastHeadingTacklingPlayer], eax
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    writeMemory(esi + 118, 2, -1);          // mov [esi+TeamGeneralInfo.spinTimer], -1
    writeMemory(esi + 138, 2, 12);          // mov [esi+TeamGeneralInfo.wonTheBallTimer], 12
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_tackling_empty_space:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 512;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.speed], 512
    if (flags.carry)
        goto l_update_player_speed_and_deltas; // jb @@update_player_speed_and_deltas

    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playerTacklingTestFoul();               // call PlayerTacklingTestFoul
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_doing_static_header:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (!flags.zero)
        goto l_player_down_with_static_header; // jnz short @@player_down_with_static_header

    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_stop_player;                     // jmp @@stop_player

l_player_down_with_static_header:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 16;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], 16
    if (!flags.sign)
        goto cseg_822E4;                    // jns short cseg_822E4

    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0

cseg_822E4:;
    setStaticHeaderDirection();             // call SetStaticHeaderDirection
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
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 98, 2);     // mov ax, [esi+Sprite.heading]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 64;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 64
    if (!flags.carry && !flags.zero)
        goto l_update_player_speed_and_deltas; // ja @@update_player_speed_and_deltas

    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 40, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 8;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.z+2)], 8
    if (flags.carry)
        goto l_update_player_speed_and_deltas; // jb @@update_player_speed_and_deltas

    {
        word src = (word)readMemory(esi + 40, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 15;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.z+2)], 15
    if (!flags.carry && !flags.zero)
        goto l_update_player_speed_and_deltas; // ja @@update_player_speed_and_deltas

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_update_player_speed_and_deltas; // js @@update_player_speed_and_deltas

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 1;        // mov playerHadBall, 1
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playerHittingStaticHeader();            // call PlayerHittingStaticHeader
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_jump_heading:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (!flags.zero)
        goto l_still_jump_heading;          // jnz short @@still_jump_heading

l_set_player_to_normal:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_stop_player;                     // jmp @@stop_player

l_still_jump_heading:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_check_if_jump_header_inside_pitch; // jz short @@check_if_jump_header_inside_pitch

    setJumpHeaderHitAnimTable(A1.as<Sprite&>());
    if (A1.as<Sprite *>()->animTable == getJumpHeaderAttemptAnimTable())
        goto cseg_8245D;                    // jz short cseg_8245D

    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 17;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerDownTimer], 17
    if (!flags.carry && !flags.zero)
        goto l_update_heading_speed;        // ja short @@update_heading_speed

    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        flags.carry = ((word)src >> 15) & 1;
        flags.overflow = (((src >> 15) & 1)) != 0;
        src = res;
        writeMemory(esi + 44, 2, src);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shr [esi+Sprite.speed], 1
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_8245D:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 37;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerDownTimer], 37
    if (!flags.carry && !flags.zero)
        goto l_update_heading_speed;        // ja short @@update_heading_speed

    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        flags.carry = ((word)src >> 15) & 1;
        flags.overflow = (((src >> 15) & 1)) != 0;
        src = res;
        writeMemory(esi + 44, 2, src);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shr [esi+Sprite.speed], 1
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_update_heading_speed:;
    ax = *(word *)&g_memByte[325302];       // mov ax, kPlayerAirConstant
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    if (!flags.sign)
        goto l_check_if_jump_header_inside_pitch; // jns short @@check_if_jump_header_inside_pitch

    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_check_if_jump_header_inside_pitch:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 73;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 73
    if (flags.sign != flags.overflow)
        goto l_slow_down_heading_player_and_leave; // jl @@slow_down_heading_player_and_leave

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 598;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 598
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_slow_down_heading_player_and_leave; // jg @@slow_down_heading_player_and_leave

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 132;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 132
    if (flags.sign != flags.overflow)
        goto l_heading_close_to_goal_lines; // jl short @@heading_close_to_goal_lines

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 766;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 766
    if (flags.zero || flags.sign != flags.overflow)
        goto l_check_if_header_winded_up;   // jle @@check_if_header_winded_up

l_heading_close_to_goal_lines:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 296;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 296
    if (flags.sign != flags.overflow)
        goto cseg_8253D;                    // jl short cseg_8253D

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 375;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 375
    if (!flags.zero && flags.sign == flags.overflow)
        goto cseg_8253D;                    // jg short cseg_8253D

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 305;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 305
    if (flags.sign != flags.overflow)
        goto l_heading_finished;            // jl short @@heading_finished

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 366;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 366
    if (flags.carry || flags.zero)
        goto cseg_8253D;                    // jbe short cseg_8253D

l_heading_finished:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    writeMemory(esi + 13, 1, 0);            // mov [esi+Sprite.playerDownTimer], 0
    goto l_set_player_to_normal;            // jmp @@set_player_to_normal

cseg_8253D:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 129
    if (flags.sign != flags.overflow)
        goto l_heading_into_the_goal;       // jl short @@heading_into_the_goal

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 769;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 769
    if (flags.zero || flags.sign != flags.overflow)
        goto l_check_if_header_winded_up;   // jle short @@check_if_header_winded_up

l_heading_into_the_goal:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 290;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 290
    if (flags.sign != flags.overflow)
        goto l_check_if_heading_inside_a_goal; // jl short @@check_if_heading_inside_a_goal

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 381;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 381
    if (flags.zero || flags.sign != flags.overflow)
        goto l_heading_finished;            // jle short @@heading_finished

l_check_if_heading_inside_a_goal:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 121;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 121
    if (flags.sign != flags.overflow)
        goto l_slow_down_heading_player_and_leave; // jl short @@slow_down_heading_player_and_leave

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 777;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 777
    if (flags.zero || flags.sign != flags.overflow)
        goto l_check_if_header_winded_up;   // jle short @@check_if_header_winded_up

l_slow_down_heading_player_and_leave:;
    esi = A1;                               // mov esi, A1
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
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
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
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_check_if_header_winded_up:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 42;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerDownTimer], 42
    if (flags.carry)
        goto l_update_player_speed_and_deltas; // jb @@update_player_speed_and_deltas

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
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    ax = (word)readMemory(esi + 98, 2);     // mov ax, [esi+Sprite.heading]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 64;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 64
    if (!flags.carry && !flags.zero)
        goto l_update_player_speed_and_deltas; // ja @@update_player_speed_and_deltas

    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 40, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 8;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.z+2)], 8
    if (flags.carry)
        goto l_update_player_speed_and_deltas; // jb @@update_player_speed_and_deltas

    {
        word src = (word)readMemory(esi + 40, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 15;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.z+2)], 15
    if (!flags.carry && !flags.zero)
        goto l_update_player_speed_and_deltas; // ja @@update_player_speed_and_deltas

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 1;        // mov playerHadBall, 1
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playerHittingJumpHeader();              // call PlayerHittingJumpHeader
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 25);          // mov [esi+TeamGeneralInfo.passKickTimer], 25
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_injured:;
    ax = *(word *)&g_memByte[515872];       // mov ax, injuriesForever
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_stop_player;                     // jmp @@stop_player

l_player_tackled:;
    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 1;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 13, 1, src);
    }                                       // sub [esi+Sprite.playerDownTimer], 1
    if (!flags.zero)
        goto l_player_on_the_ground;        // jnz short @@player_on_the_ground

    ax = (word)readMemory(esi + 104, 2);    // mov ax, [esi+Sprite.injuryLevel]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_get_up_normally;             // jz short @@get_up_normally

    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    writeMemory(esi + 12, 1, 13);           // mov [esi+Sprite.playerState], PL_ROLLING_INJURED
    writeMemory(esi + 13, 1, al);           // mov [esi+Sprite.playerDownTimer], al
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerInjuredAnimTable());
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_get_up_normally:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 0);            // mov [esi+Sprite.playerState], PL_NORMAL
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_stop_player;                     // jmp @@stop_player

l_player_on_the_ground:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 159;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 159
    if (flags.sign != flags.overflow)
        goto l_in_goalkeepers_area_by_y;    // jl short @@in_goalkeepers_area_by_y

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 739;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 739
    if (flags.zero || flags.sign != flags.overflow)
        goto l_player_not_in_goalkeepers_area; // jle short @@player_not_in_goalkeepers_area

l_in_goalkeepers_area_by_y:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 265;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 265
    if (flags.sign != flags.overflow)
        goto l_player_not_in_goalkeepers_area; // jl short @@player_not_in_goalkeepers_area

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 406;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 406
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_player_not_in_goalkeepers_area; // jg short @@player_not_in_goalkeepers_area

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
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax

l_player_not_in_goalkeepers_area:;
    ax = *(word *)&g_memByte[325300];       // mov ax, kPlayerGroundConstant
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // sub [esi+Sprite.speed], ax
    if (!flags.sign)
        goto l_update_player_speed_and_deltas; // jns @@update_player_speed_and_deltas

    writeMemory(esi + 44, 2, 0);            // mov [esi+Sprite.speed], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_expecting_pass:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 81;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 81
    if (flags.sign != flags.overflow)
        goto l_player_passed_to_out_of_pitch; // jl short @@player_passed_to_out_of_pitch

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 590;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 590
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_player_passed_to_out_of_pitch; // jg short @@player_passed_to_out_of_pitch

    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 129
    if (flags.sign != flags.overflow)
        goto l_player_passed_to_out_of_pitch; // jl short @@player_passed_to_out_of_pitch

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 769;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 769
    if (flags.zero || flags.sign != flags.overflow)
        goto l_passed_to_player_inside_pitch; // jle short @@passed_to_player_inside_pitch

l_player_passed_to_out_of_pitch:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 88, 2, 0);            // mov [esi+TeamGeneralInfo.passingBall], 0
    goto l_stop_player;                     // jmp @@stop_player

l_passed_to_player_inside_pitch:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_cpu_passing_to;              // jz @@cpu_passing_to

    eax = readMemory(esi + 124, 4);         // mov eax, dword ptr [esi+TeamGeneralInfo.longPass]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (!flags.zero)
        goto l_test_for_long_pass;          // jnz short @@test_for_long_pass

    ax = (word)readMemory(esi + 120, 2);    // mov ax, [esi+TeamGeneralInfo.leftSpin]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_test_for_long_pass;          // jnz short @@test_for_long_pass

    ax = (word)readMemory(esi + 122, 2);    // mov ax, [esi+TeamGeneralInfo.rightSpin]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_cpu_passing_to;              // jz @@cpu_passing_to

l_test_for_long_pass:;
    {
        word src = *(word *)&g_memByte[328536];
        int16_t dstSigned = src;
        int16_t srcSigned = 512;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp ballSprite.speed, 512
    if (flags.carry)
        goto l_cpu_passing_to;              // jb @@cpu_passing_to

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 90, 2);     // mov ax, [esi+TeamGeneralInfo.passingToPlayer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_cpu_passing_to;              // jz @@cpu_passing_to

    ax = *(word *)&g_memByte[328534];       // mov ax, ballSprite.direction
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_ball_got_no_direction;       // js short @@ball_got_no_direction

    ax = *(word *)&g_memByte[328574];       // mov ax, ballSprite.fullDirection
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    al = (byte)readMemory(esi + 82, 1);     // mov al, byte ptr [esi+Sprite.fullDirection]
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
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_player_not_turned_enough_toward_ball; // jg short @@player_not_turned_enough_toward_ball

    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = -64;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, 192
    if (flags.sign == flags.overflow)
        goto cseg_82952;                    // jge short cseg_82952

l_player_not_turned_enough_toward_ball:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 88, 2, 0);            // mov [esi+TeamGeneralInfo.passingBall], 0
    goto l_cpu_passing_to;                  // jmp @@cpu_passing_to

cseg_82952:;
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = 4;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, 4
    if (!flags.zero && flags.sign == flags.overflow)
        goto cseg_82991;                    // jg short cseg_82991

    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = -4;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, 252
    if (flags.sign != flags.overflow)
        goto cseg_82991;                    // jl short cseg_82991

l_ball_got_no_direction:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 98, 2, ax);           // mov [esi+TeamGeneralInfo.ballX], ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 100, 2, ax);          // mov [esi+TeamGeneralInfo.ballY], ax
    goto l_cpu_passing_to;                  // jmp @@cpu_passing_to

cseg_82991:;
    *(byte *)&D1 = 192;                     // mov byte ptr D1, 192
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 124, 4);         // mov eax, dword ptr [esi+TeamGeneralInfo.longPass]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (flags.zero)
        goto cseg_829AC;                    // jz short cseg_829AC

    *(byte *)&D1 = 224;                     // mov byte ptr D1, 224

cseg_829AC:;
    al = D0;                                // mov al, byte ptr D0
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.sign != flags.overflow)
        goto cseg_829BB;                    // jl short cseg_829BB

    *(int8_t *)&D1 = -*(int8_t *)&D1;       // neg byte ptr D1

cseg_829BB:;
    ax = *(word *)&g_memByte[328574];       // mov ax, ballSprite.fullDirection
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    al = D1;                                // mov al, byte ptr D1
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned + srcSigned;
        *(byte *)&D0 = res;
    }                                       // add byte ptr D0, al
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 4;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 4
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    A0 = 515640;                            // mov A0, offset kBallFriction
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 3;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 3
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
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 98, 2, ax);           // mov [esi+TeamGeneralInfo.ballX], ax
    esi = A1;                               // mov esi, A1
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
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 100, 2, ax);          // mov [esi+TeamGeneralInfo.ballY], ax

l_cpu_passing_to:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_82AAF;                    // jnz short cseg_82AAF

    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    AI_Kick();                              // call AI_Kick
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1

cseg_82AAF:;
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
        goto cseg_82C41;                    // jz cseg_82C41

    eax = *(dword *)&g_memByte[515588];     // mov eax, lastTeamPlayedBeforeBreak
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_KEEPER_HOLDS_BALL
    if (!flags.zero)
        goto cseg_82AE5;                    // jnz short cseg_82AE5

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
    if (flags.zero)
        goto cseg_82AF6;                    // jz short cseg_82AF6

cseg_82AE5:;
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (!flags.zero)
        goto cseg_82EC2;                    // jnz cseg_82EC2

cseg_82AF6:;
    {
        word src = *(word *)&g_memByte[515608];
        int16_t dstSigned = src;
        int16_t srcSigned = 8;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp cameraDirection, 8
    if (flags.carry)
        goto l_pass_success;                // jb short @@pass_success

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&g_memByte[515608] = ax;       // mov cameraDirection, ax
    {
        word src = *(word *)&g_memByte[449204];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        *(word *)&g_memByte[449204] = src;
    }                                       // add dseg_132804, 1

l_pass_success:;
    ax = *(word *)&g_memByte[515608];       // mov ax, cameraDirection
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
    writeMemory(esi + 32, 4, eax);          // mov [esi+TeamGeneralInfo.controlledPlayer], eax
    writeMemory(esi + 36, 4, 0);            // mov [esi+TeamGeneralInfo.passToPlayerPtr], 0
    writeMemory(esi + 92, 2, 25);           // mov [esi+TeamGeneralInfo.playerSwitchTimer], 25
    writeMemory(esi + 88, 2, 0);            // mov [esi+TeamGeneralInfo.passingBall], 0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 58, 2, 0);            // mov [esi+TeamGeneralInfo.shooting], 0
    esi = A2;                               // mov esi, A2
    writeMemory(esi + 44, 2, 0);            // mov [esi+TeamGeneralInfo.currentAllowedDirection], 0
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 0;        // mov playerHadBall, 0
    updatePlayerWithBall();                 // call UpdatePlayerWithBall
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 0);           // mov [esi+TeamGeneralInfo.passKickTimer], 0
    writeMemory(esi + 104, 4, 0);           // mov dword ptr [esi+104], 0
    writeMemory(esi + 116, 2, 0);           // mov word ptr [esi+116], 0
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
        goto l_update_player_speed_and_deltas; // jb @@update_player_speed_and_deltas

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
    if (!flags.carry && !flags.zero)
        goto l_update_player_speed_and_deltas; // ja @@update_player_speed_and_deltas

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    push(D0);                               // push D0
    setThrowInPlayerDestinationCoordinates(); // call SetThrowInPlayerDestinationCoordinates
    pop(D0);                                // pop D0
    setPlayerAnimationTable(A1.as<Sprite&>(), getAboutToThrowInAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 5);            // mov [esi+Sprite.playerState], PL_THROW_IN
    writeMemory(esi + 13, 1, 0);            // mov [esi+Sprite.playerDownTimer], 0
    *(word *)&g_memByte[449180] = 1;        // mov hideBall, 1
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_82C41:;
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
    if (flags.zero)
        goto cseg_82D59;                    // jz cseg_82D59

    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 51, 1);     // mov al, [esi+TeamGeneralInfo.fireThisFrame]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_82D59;                    // jz cseg_82D59

    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto cseg_82D59;                    // js cseg_82D59

    al = (byte)readMemory(esi + 63, 1);     // mov al, [esi+TeamGeneralInfo.plNotFarFromBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_82D59;                    // jz cseg_82D59

    al = (byte)readMemory(esi + 64, 1);     // mov al, [esi+TeamGeneralInfo.ballLessEqual4]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_82CAB;                    // jnz short cseg_82CAB

    al = (byte)readMemory(esi + 65, 1);     // mov al, [esi+TeamGeneralInfo.ball4To8]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_82D59;                    // jz cseg_82D59

cseg_82CAB:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A5 = eax;                               // mov A5, eax
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A5, 0
    if (flags.zero)
        goto cseg_82CE1;                    // jz short cseg_82CE1

    esi = A5;                               // mov esi, A5
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    D1 = eax;                               // mov D1, eax
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D1, eax
    if (flags.carry)
        goto cseg_82D59;                    // jb short cseg_82D59

cseg_82CE1:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 52, 2, 1);            // mov [esi+TeamGeneralInfo.headerOrTackle], 1
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    playerBeginTackling();                  // call PlayerBeginTackling
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 110, 2, 0);           // mov [esi+TeamGeneralInfo.ballCanBeControlled], 0
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_82D59:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 64, 1);     // mov al, [esi+TeamGeneralInfo.ballLessEqual4]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_82EC2;                    // jz cseg_82EC2

    {
        word src = *(word *)&g_memByte[328536];
        int16_t dstSigned = src;
        int16_t srcSigned = 1536;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp ballSprite.speed, 1536
    if (flags.carry)
        goto cseg_82D82;                    // jb short cseg_82D82

    al = (byte)readMemory(esi + 62, 1);     // mov al, [esi+TeamGeneralInfo.plCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_passed_to_player_becomes_main; // jnz short @@passed_to_player_becomes_main

cseg_82D82:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_82EC2;                    // jz cseg_82EC2

l_passed_to_player_becomes_main:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
    writeMemory(esi + 32, 4, eax);          // mov [esi+TeamGeneralInfo.controlledPlayer], eax
    writeMemory(esi + 36, 4, 0);            // mov [esi+TeamGeneralInfo.passToPlayerPtr], 0
    writeMemory(esi + 92, 2, 25);           // mov [esi+TeamGeneralInfo.playerSwitchTimer], 25
    writeMemory(esi + 88, 2, 0);            // mov [esi+TeamGeneralInfo.passingBall], 0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    writeMemory(esi + 58, 2, 0);            // mov [esi+TeamGeneralInfo.shooting], 0
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+TeamGeneralInfo.controlledPlayer]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
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
        goto l_update_controlling_player;   // jnz short @@update_controlling_player

    updateBallWithControllingGoalkeeper(A1.as<Sprite&>());  // call UpdateBallWithControllingGoalkeeper
    goto cseg_82E23;                        // jmp short cseg_82E23

l_update_controlling_player:;
    updateControllingPlayer();              // call UpdateControllingPlayer

cseg_82E23:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 102, 2, 0);           // mov [esi+TeamGeneralInfo.passKickTimer], 0
    writeMemory(esi + 104, 4, 0);           // mov [esi+TeamGeneralInfo.passingKickingPlayer], 0
    writeMemory(esi + 116, 2, 0);           // mov [esi+TeamGeneralInfo.field_74], 0
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
        goto cseg_82E97;                    // jnz short cseg_82E97

    eax = *(dword *)&g_memByte[515572];     // mov eax, lastTeamPlayed
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (!flags.zero)
        goto cseg_82E92;                    // jnz short cseg_82E92

    ax = *(word *)&g_memByte[515566];       // mov ax, playerHadBall
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_82E92;                    // jnz short cseg_82E92

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 140, 2, 1);           // mov [esi+TeamGeneralInfo.goalkeeperPlaying], 1
    writeMemory(esi + 86, 1, 1);            // mov byte ptr [esi+TeamGeneralInfo.goaliePlayingOrOut], 1
    writeMemory(esi + 84, 1, 1);            // mov byte ptr [esi+TeamGeneralInfo.ballOutOfPlayOrKeeper], 1
    goto cseg_82E97;                        // jmp short cseg_82E97

cseg_82E92:;
    goalkeeperClaimedTheBall();             // call GoalkeeperClaimedTheBall

cseg_82E97:;
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515572] = eax;     // mov lastTeamPlayed, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515576] = eax;     // mov lastPlayerPlayed, eax
    *(word *)&g_memByte[515580] = 0;        // mov penalty, 0
    *(word *)&g_memByte[515566] = 0;        // mov playerHadBall, 0
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_82EC2:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 88, 2);     // mov ax, [esi+TeamGeneralInfo.passingBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_player_chase_ball;           // jnz @@player_chase_ball

    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A5 = eax;                               // mov A5, eax
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A5, 0
    if (flags.zero)
        goto l_its_a_pass;                  // jz short @@its_a_pass

    esi = A5;                               // mov esi, A5
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 3200;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 3200
    if (!flags.carry && !flags.zero)
        goto l_its_a_pass;                  // ja short @@its_a_pass

    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    D0 = eax;                               // mov D0, eax
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, eax
    if (!flags.carry && !flags.zero)
        goto l_its_a_pass;                  // ja short @@its_a_pass

    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 3200;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 3200
    if (!flags.carry && !flags.zero)
        goto l_player_chase_ball;           // ja short @@player_chase_ball

    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_its_a_pass:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 88, 2, 1);            // mov [esi+TeamGeneralInfo.passingBall], 1

l_player_chase_ball:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 98, 2);     // mov ax, [esi+TeamGeneralInfo.ballX]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 100, 2);    // mov ax, [esi+TeamGeneralInfo.ballY]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = (word)readMemory(esi + 58, 2);     // mov ax, [esi+Sprite.destX]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, ax
    if (!flags.zero)
        goto l_player_still_moving;         // jnz short @@player_still_moving

    ax = (word)readMemory(esi + 60, 2);     // mov ax, [esi+Sprite.destY]
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, ax
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

l_player_still_moving:;
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
        goto cseg_8308D;                    // jnz cseg_8308D

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
        goto cseg_8308D;                    // jnz cseg_8308D

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 140, 2);    // mov ax, [esi+TeamGeneralInfo.goalkeeperPlaying]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_8308D;                    // jnz cseg_8308D

    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto cseg_8303E;                    // jz short cseg_8303E

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 206;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 206
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_cancel_pass;                 // jg short @@cancel_pass

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 203;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 203
    if (flags.sign != flags.overflow)
        goto l_cancel_pass;                 // jl short @@cancel_pass

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 468;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 468
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_cancel_pass;                 // jg short @@cancel_pass

    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_8303E:;
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 692;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 692
    if (flags.sign != flags.overflow)
        goto l_cancel_pass;                 // jl short @@cancel_pass

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 203;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 203
    if (flags.sign != flags.overflow)
        goto l_cancel_pass;                 // jl short @@cancel_pass

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 468;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 468
    if (flags.zero || flags.sign != flags.overflow)
        goto l_update_player_speed_and_deltas; // jle @@update_player_speed_and_deltas

l_cancel_pass:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 36, 4, 0);            // mov [esi+TeamGeneralInfo.passToPlayerPtr], 0
    writeMemory(esi + 88, 2, 0);            // mov [esi+TeamGeneralInfo.passingBall], 0
    writeMemory(esi + 90, 2, 0);            // mov [esi+TeamGeneralInfo.passingToPlayer], 0
    goto l_stop_player;                     // jmp @@stop_player

cseg_8308D:;
    ax = *(word *)&g_memByte[515582];       // mov ax, goalOut
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

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
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 183;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 183
    if (flags.sign != flags.overflow)
        goto l_update_player_speed_and_deltas; // jl @@update_player_speed_and_deltas

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 488;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 488
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_update_player_speed_and_deltas; // jg @@update_player_speed_and_deltas

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 226;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 226
    if (flags.zero || flags.sign != flags.overflow)
        goto l_stop_player;                 // jle @@stop_player

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 672;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 672
    if (flags.sign == flags.overflow)
        goto l_stop_player;                 // jge @@stop_player

    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_injury_forever:;
    ax = *(word *)&g_memByte[325440];       // mov ax, dseg_110D8D
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    {
        word src = (word)readMemory(esi + 60, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 8;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        writeMemory(esi + 60, 2, src);
    }                                       // sub [esi+Sprite.destY], 8
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_not_controlled_player:;
    ax = *(word *)&g_memByte[478672];       // mov ax, g_substituteInProgress
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_test_update_player_index;    // jz short @@test_update_player_index

    eax = *(dword *)&g_memByte[478676];     // mov eax, substitutedPlSprite
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
        goto l_this_is_substituted_player;  // jz @@this_is_substituted_player

l_test_update_player_index:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 30, 2);     // mov ax, [esi+TeamGeneralInfo.updatePlayerIndex]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = (word)stack[stackTop];             // mov ax, [esp]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.zero)
        goto l_next_player;                 // jnz @@next_player

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
        goto l_this_is_substituted_player;  // jz @@this_is_substituted_player

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 30;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GAME_ENDED
    if (!flags.zero)
        goto l_this_is_substituted_player;  // jnz @@this_is_substituted_player

    eax = *(dword *)&g_memByte[336012];     // mov eax, winningTeamPtr
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (flags.zero)
        goto l_this_is_substituted_player;  // jz @@this_is_substituted_player

    esi = A1;                               // mov esi, A1
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
        goto l_this_is_substituted_player;  // jnz @@this_is_substituted_player

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
    if (flags.zero)
        goto l_this_is_substituted_player;  // jz @@this_is_substituted_player

    eax = readMemory(esi + 46, 4);          // mov eax, [esi+Sprite.deltaX]
    D0 = eax;                               // mov D0, eax
    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    D0 |= eax;
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (D0 & 0x80000000) != 0;
    flags.zero = D0 == 0;                   // or D0, eax
    if (!flags.zero)
        goto l_this_is_substituted_player;  // jnz short @@this_is_substituted_player

    SWOS::Rand();                           // call Rand
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 64;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 64
    if (!flags.carry && !flags.zero)
        goto l_this_is_substituted_player;  // ja short @@this_is_substituted_player

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 10, 4);          // mov eax, [esi+TeamGeneralInfo.inGameTeamPtr]
    D0 = eax;                               // mov D0, eax
    eax = *(dword *)&g_memByte[336012];     // mov eax, winningTeamPtr
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, eax
    if (flags.zero)
        goto l_player_in_winning_team;      // jz short @@player_in_winning_team

    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerLosingReactionAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 14);           // mov [esi+Sprite.playerState], PL_SAD
    goto l_this_is_substituted_player;      // jmp short @@this_is_substituted_player

l_player_in_winning_team:;
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerWinningReactionAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 15);           // mov [esi+Sprite.playerState], PL_HAPPY

l_this_is_substituted_player:;
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
        goto l_update_destination_reached_state; // jnz short @@update_destination_reached_state

    {
        word src = *(word *)&g_memByte[515612];
        int16_t dstSigned = src;
        int16_t srcSigned = 25;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp inGameCounter, 25
    if (!flags.carry && !flags.zero)
        goto l_update_destination_reached_state; // ja short @@update_destination_reached_state

    {
        word src = *(word *)&g_memByte[515614];
        int16_t dstSigned = src;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp breakState, ST_CORNER_LEFT
    if (flags.zero)
        goto l_next_player;                 // jz @@next_player

    {
        word src = *(word *)&g_memByte[515614];
        int16_t dstSigned = src;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp breakState, ST_CORNER_RIGHT
    if (flags.zero)
        goto l_next_player;                 // jz @@next_player

    {
        word src = *(word *)&g_memByte[515614];
        int16_t dstSigned = src;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp breakState, ST_FREE_KICK_LEFT1
    if (flags.carry)
        goto l_update_destination_reached_state; // jb short @@update_destination_reached_state

    {
        word src = *(word *)&g_memByte[515614];
        int16_t dstSigned = src;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp breakState, ST_FREE_KICK_RIGHT3
    if (flags.carry || flags.zero)
        goto l_next_player;                 // jbe @@next_player

l_update_destination_reached_state:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 100, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.destReachedState], 1
    if (!flags.zero)
        goto l_check_for_controlled_player; // jnz short @@check_for_controlled_player

    writeMemory(esi + 100, 2, 2);           // mov [esi+Sprite.destReachedState], 2

l_check_for_controlled_player:;
    esi = A6;                               // mov esi, A6
    {
        dword src = readMemory(esi + 32, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.controlledPlayer], 0
    if (!flags.zero)
        goto l_check_if_this_player_getting_booked; // jnz short @@check_if_this_player_getting_booked

    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_check_if_this_player_getting_booked; // jnz short @@check_if_this_player_getting_booked

    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    AI_SetControlsDirection();              // call AI_SetControlsDirection
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1

l_check_if_this_player_getting_booked:;
    eax = *(dword *)&g_memByte[515892];     // mov eax, bookedPlayer
    {
        int32_t dstSigned = A1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A1, eax
    if (!flags.zero)
        goto l_check_for_substituted_player; // jnz @@check_for_substituted_player

    ax = *(word *)&g_memByte[515604];       // mov ax, foulXCoordinate
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = *(word *)&g_memByte[515606];       // mov ax, foulYCoordinate
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 21;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 21
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, ax
    if (!flags.zero)
        goto l_player_not_by_foul_spot;     // jnz @@player_not_by_foul_spot

    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, ax
    if (!flags.zero)
        goto l_player_not_by_foul_spot;     // jnz short @@player_not_by_foul_spot

    {
        word src = *(word *)&g_memByte[515870];
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp refState, REF_WAITING_PLAYER
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    *(word *)&g_memByte[515870] = 3;        // mov refState, REF_ABOUT_TO_GIVE_CARD
    {
        word src = *(word *)&g_memByte[515888];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp whichCard, CARD_YELLOW
    if (flags.zero)
        goto l_player_getting_yellow_card;  // jz short @@player_getting_yellow_card

    {
        word src = *(word *)&g_memByte[515888];
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp whichCard, CARD_RED
    if (flags.zero)
        goto l_player_getting_red_card;     // jz short @@player_getting_red_card

    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerGetting2ndYellowCardAnimTable());
    goto l_set_player_state_booked;         // jmp short @@set_player_state_booked

l_player_getting_red_card:;
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerGettingRedCardAnimTable());
    goto l_set_player_state_booked;         // jmp short @@set_player_state_booked

l_player_getting_yellow_card:;
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerGettingYellowCardAnimTable());

l_set_player_state_booked:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 12);           // mov [esi+Sprite.playerState], PL_BOOKED
    writeMemory(esi + 13, 1, 1);            // mov [esi+Sprite.playerDownTimer], 1
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_player_not_by_foul_spot:;
    ax = D1;                                // mov ax, word ptr D1
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = D2;                                // mov ax, word ptr D2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_check_for_substituted_player:;
    ax = *(word *)&g_memByte[478672];       // mov ax, g_substituteInProgress
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_check_if_sent_away;          // jz @@check_if_sent_away

    if (flags.sign)
        goto l_set_player_going_in_speed;   // js @@set_player_going_in_speed

    eax = *(dword *)&g_memByte[478676];     // mov eax, substitutedPlSprite
    {
        int32_t dstSigned = A1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A1, eax
    if (!flags.zero)
        goto l_check_if_sent_away;          // jnz @@check_if_sent_away

    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 104, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = -2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.injuryLevel], -2
    if (flags.zero)
        goto l_new_player_about_to_go_in;   // jz short @@new_player_about_to_go_in

    ax = *(word *)&g_memByte[515616];       // mov ax, substitutedPlDestX
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.zero)
        goto l_set_substituted_player_destination; // jnz @@set_substituted_player_destination

    ax = *(word *)&g_memByte[515618];       // mov ax, substitutedPlDestY
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.zero)
        goto l_set_substituted_player_destination; // jnz @@set_substituted_player_destination

    {
        word src = *(word *)&g_memByte[478672];
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp g_substituteInProgress, 2
    if (flags.zero)
        goto l_new_player_about_to_go_in;   // jz short @@new_player_about_to_go_in

    *(word *)&g_memByte[478672] = 2;        // mov g_substituteInProgress, 2
    ax = *(word *)&g_memByte[478664];       // mov ax, plComingX
    *(word *)&g_memByte[515616] = ax;       // mov substitutedPlDestX, ax
    ax = *(word *)&g_memByte[478666];       // mov ax, plComingY
    *(word *)&g_memByte[515618] = ax;       // mov substitutedPlDestY, ax
    goto l_set_substituted_player_destination; // jmp short @@set_substituted_player_destination

l_new_player_about_to_go_in:;
    *(word *)&g_memByte[478672] = -1;       // mov g_substituteInProgress, -1
    ax = *(word *)&g_memByte[478668];       // mov ax, plSubstitutedX
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 32, 2, ax);           // mov word ptr [esi+(Sprite.x+2)], ax
    ax = *(word *)&g_memByte[478670];       // mov ax, plSubstitutedY
    writeMemory(esi + 36, 2, ax);           // mov word ptr [esi+(Sprite.y+2)], ax
    ax = *(word *)&g_memByte[325292];       // mov ax, kSubstitutedPlayerSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    writeMemory(esi + 108, 2, 0);           // mov [esi+Sprite.sentAway], 0
    goto l_check_if_sent_away;              // jmp short @@check_if_sent_away

    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_set_substituted_player_destination:;
    ax = *(word *)&g_memByte[515616];       // mov ax, substitutedPlDestX
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = *(word *)&g_memByte[515618];       // mov ax, substitutedPlDestY
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = *(word *)&g_memByte[325292];       // mov ax, kSubstitutedPlayerSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_set_player_going_in_speed:;
    eax = *(dword *)&g_memByte[478676];     // mov eax, substitutedPlSprite
    {
        int32_t dstSigned = A1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A1, eax
    if (!flags.zero)
        goto l_check_if_sent_away;          // jnz short @@check_if_sent_away

    ax = *(word *)&g_memByte[325292];       // mov ax, kSubstitutedPlayerSpeed
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    eax = readMemory(esi + 46, 4);          // mov eax, [esi+Sprite.deltaX]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (!flags.zero)
        goto l_check_if_sent_away;          // jnz short @@check_if_sent_away

    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (!flags.zero)
        goto l_check_if_sent_away;          // jnz short @@check_if_sent_away

    *(word *)&g_memByte[478672] = 0;        // mov g_substituteInProgress, 0

l_check_if_sent_away:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 108, 2);    // mov ax, [esi+Sprite.sentAway]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_update_player_speed_and_deltas; // jnz @@update_player_speed_and_deltas

    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_set_player_positions_if_game_break; // jz short @@set_player_positions_if_game_break

    *(word *)&D0 = 31;                      // mov word ptr D0, ST_PENALTIES
    goto l_get_foul_coordinates;            // jmp short @@get_foul_coordinates

l_set_player_positions_if_game_break:;
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
        goto l_game_interrupted_get_ball_x_y; // jnz short @@game_interrupted_get_ball_x_y

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D6 = ax;                      // mov word ptr D6, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D7 = ax;                      // mov word ptr D7, ax
    goto l_set_player_with_no_ball_destination; // jmp @@set_player_with_no_ball_destination

l_game_interrupted_get_ball_x_y:;
    ax = *(word *)&g_memByte[515598];       // mov ax, gameState
    *(word *)&D0 = ax;                      // mov word ptr D0, ax

l_get_foul_coordinates:;
    ax = *(word *)&g_memByte[515604];       // mov ax, foulXCoordinate
    *(word *)&D6 = ax;                      // mov word ptr D6, ax
    ax = *(word *)&g_memByte[515606];       // mov ax, foulYCoordinate
    *(word *)&D7 = ax;                      // mov word ptr D7, ax
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 29;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FIRST_HALF_ENDED
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 30;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GAME_ENDED
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_KEEPER_HOLDS_BALL
    if (flags.zero)
        goto l_set_player_with_no_ball_destination; // jz @@set_player_with_no_ball_destination

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GOAL_OUT_LEFT
    if (flags.zero)
        goto l_set_player_with_no_ball_destination; // jz @@set_player_with_no_ball_destination

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GOAL_OUT_RIGHT
    if (flags.zero)
        goto l_set_player_with_no_ball_destination; // jz @@set_player_with_no_ball_destination

    {
        word src = *(word *)&g_memByte[449234];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp forceLeftTeam, 1
    if (!flags.zero)
        goto l_check_for_penalty_shootout;  // jnz short @@check_for_penalty_shootout

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_CORNER_LEFT
    if (flags.zero)
        goto l_corner;                      // jz short @@corner

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_CORNER_RIGHT
    if (!flags.zero)
        goto l_check_for_penalty_shootout;  // jnz short @@check_for_penalty_shootout

l_corner:;
    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 449;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 449
    if (flags.sign != flags.overflow)
        goto l_top_break;                   // jl short @@top_break

l_check_for_penalty_shootout:;
    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_check_team_for_penalty_positions; // jz short @@check_team_for_penalty_positions

    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (!flags.zero)
        goto l_top_break;                   // jnz short @@top_break

    goto l_bottom_break;                    // jmp short @@bottom_break

l_check_team_for_penalty_positions:;
    eax = *(dword *)&g_memByte[515588];     // mov eax, lastTeamPlayedBeforeBreak
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (!flags.zero)
        goto l_top_break;                   // jnz short @@top_break

l_bottom_break:;
    A5 = 516942;                            // mov A5, offset bottomBallOutOfPlayPositions
    goto cseg_8364D;                        // jmp short cseg_8364D

l_top_break:;
    A5 = 517070;                            // mov A5, offset topBallOutOfPlayPositions

cseg_8364D:;
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A5;                               // mov esi, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    eax = readMemory(esi + ebx, 4);         // mov eax, [esi+ebx]
    A5 = eax;                               // mov A5, eax
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A5, 0
    if (flags.zero)
        goto l_set_player_with_no_ball_destination; // jz @@set_player_with_no_ball_destination

    *(word *)&g_memByte[516940] = 0;        // mov freeKickDestX, 0
    eax = *(dword *)&g_memByte[515588];     // mov eax, lastTeamPlayedBeforeBreak
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (flags.zero)
        goto l_not_in_free_kick_state;      // jz @@not_in_free_kick_state

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_LEFT1
    if (flags.carry)
        goto l_not_in_free_kick_state;      // jb @@not_in_free_kick_state

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_RIGHT3
    if (!flags.carry && !flags.zero)
        goto l_not_in_free_kick_state;      // ja @@not_in_free_kick_state

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_LEFT1
    if (flags.zero)
        goto l_not_in_free_kick_state;      // jz @@not_in_free_kick_state

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_RIGHT3
    if (flags.zero)
        goto l_not_in_free_kick_state;      // jz @@not_in_free_kick_state

    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 2, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerOrdinal], 2
    if (flags.carry)
        goto l_not_in_free_kick_state;      // jb @@not_in_free_kick_state

    {
        word src = (word)readMemory(esi + 2, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerOrdinal], 5
    if (!flags.carry && !flags.zero)
        goto l_not_in_free_kick_state;      // ja @@not_in_free_kick_state

    *(word *)&D1 = 2;                       // mov word ptr D1, 2
    *(word *)&D0 = 2;                       // mov word ptr D0, 2
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 20, 4);          // mov eax, [esi+TeamGeneralInfo.spritesTable]
    A4 = eax;                               // mov A4, eax
    {
        int32_t dstSigned = A4;
        int32_t srcSigned = 4;
        dword res = dstSigned + srcSigned;
        A4 = res;
    }                                       // add A4, 4

l_pl_loop_start:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 2, 2);      // mov ax, [esi+Sprite.playerOrdinal]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, ax
    if (flags.zero)
        goto cseg_8375A;                    // jz short cseg_8375A

    esi = A4;                               // mov esi, A4
    eax = readMemory(esi, 4);               // mov eax, [esi]
    {
        int32_t dstSigned = A4;
        int32_t srcSigned = 4;
        dword res = dstSigned + srcSigned;
        A4 = res;
    }                                       // add A4, 4
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 102, 2);    // mov ax, [esi+Sprite.cards]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_player_got_red_card_get_next; // js short @@player_got_red_card_get_next

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 1

l_player_got_red_card_get_next:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 1
    goto l_pl_loop_start;                   // jmp short @@pl_loop_start

cseg_8375A:;
    ax = *(word *)&g_memByte[515598];       // mov ax, gameState
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        *(word *)&D2 = res;
    }                                       // sub word ptr D2, 6
    {
        word res = *(word *)&D2 << 1;
        *(word *)&D2 = res;
    }                                       // shl word ptr D2, 1
    A0 = 516926;                            // mov A0, offset freeKickFactorsX
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D2;                     // movzx ebx, word ptr D2
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        word res = *(word *)&D2 << 2;
        *(word *)&D2 = res;
    }                                       // shl word ptr D2, 2
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516940] = ax;       // mov freeKickDestX, ax
    {
        word res = *(word *)&D2 >> 2;
        *(word *)&D2 = res;
    }                                       // shr word ptr D2, 2
    *(word *)&D1 = 3;                       // mov word ptr D1, 3
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 20, 4);          // mov eax, [esi+TeamGeneralInfo.spritesTable]
    A4 = eax;                               // mov A4, eax
    {
        int32_t dstSigned = A4;
        int32_t srcSigned = 4;
        dword res = dstSigned + srcSigned;
        A4 = res;
    }                                       // add A4, 4

l_next_free_kick_taker:;
    esi = A4;                               // mov esi, A4
    eax = readMemory(esi, 4);               // mov eax, [esi]
    {
        int32_t dstSigned = A4;
        int32_t srcSigned = 4;
        dword res = dstSigned + srcSigned;
        A4 = res;
    }                                       // add A4, 4
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 102, 2);    // mov ax, [esi+Sprite.cards]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_free_kick_taker_has_red_card; // js short @@free_kick_taker_has_red_card

    ax = D2;                                // mov ax, word ptr D2
    {
        word src = *(word *)&g_memByte[516940];
        int16_t dstSigned = src;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        src = res;
        *(word *)&g_memByte[516940] = src;
    }                                       // sub freeKickDestX, ax

l_free_kick_taker_has_red_card:;
    (*(int16_t *)&D1)--;
    flags.overflow = (int16_t)(*(int16_t *)&D1) == INT16_MIN;
    flags.sign = (*(int16_t *)&D1 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D1 == 0;      // dec word ptr D1
    if (!flags.sign)
        goto l_next_free_kick_taker;        // jns short @@next_free_kick_taker

    goto cseg_8381B;                        // jmp short cseg_8381B

l_not_in_free_kick_state:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 2, 2);      // mov ax, [esi+Sprite.playerOrdinal]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax

cseg_8381B:;
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A5;                               // mov esi, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        word src = (word)readMemory(esi + ebx, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 22222;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+ebx], 22222
    if (flags.zero)
        goto l_set_player_with_no_ball_destination; // jz @@set_player_with_no_ball_destination

    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (!flags.zero)
        goto cseg_838AA;                    // jnz short cseg_838AA

    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = -1000;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, -1000
    if (flags.zero || flags.sign != flags.overflow)
        goto cseg_83981;                    // jle cseg_83981

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1000;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 1000
    if (flags.sign == flags.overflow)
        goto cseg_83999;                    // jge cseg_83999

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 5;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 5
    goto cseg_8394B;                        // jmp cseg_8394B

cseg_838AA:;
    *(word *)&D3 = -5;                      // mov word ptr D3, -5
    esi = A5;                               // mov esi, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = -1000;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, -1000
    if (flags.zero || flags.sign != flags.overflow)
        goto cseg_839B1;                    // jle cseg_839B1

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1000;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 1000
    if (flags.sign == flags.overflow)
        goto cseg_839D7;                    // jge cseg_839D7

    *(word *)&D1 = 509;                     // mov word ptr D1, 509
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, 5
    *(word *)&D2 = 640;                     // mov word ptr D2, 640
    ax = (word)readMemory(esi + ebx + 2, 2); // mov ax, [esi+ebx+2]
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D2 = res;
    }                                       // sub word ptr D2, ax

cseg_8394B:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 81;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 81
    ax = D1;                                // mov ax, word ptr D1
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 129;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D2 = res;
    }                                       // add word ptr D2, 129
    ax = D2;                                // mov ax, word ptr D2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

cseg_83981:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1000;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 1000
    ax = *(word *)&g_memByte[516940];       // mov ax, freeKickDestX
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
    goto l_update_goalie_dest_x_y;          // jmp short @@update_goalie_dest_x_y

cseg_83999:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1000;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, 1000
    ax = *(word *)&g_memByte[516940];       // mov ax, freeKickDestX
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
    goto l_update_goalie_dest_x_y;          // jmp short @@update_goalie_dest_x_y

cseg_839B1:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1000;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 1000
    *(int16_t *)&D1 = -*(int16_t *)&D1;     // neg word ptr D1
    *(int16_t *)&D2 = -*(int16_t *)&D2;     // neg word ptr D2
    ax = *(word *)&g_memByte[516940];       // mov ax, freeKickDestX
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    goto l_update_goalie_dest_x_y;          // jmp short @@update_goalie_dest_x_y

cseg_839D7:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1000;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, 1000
    *(int16_t *)&D1 = -*(int16_t *)&D1;     // neg word ptr D1
    *(int16_t *)&D2 = -*(int16_t *)&D2;     // neg word ptr D2
    ax = *(word *)&g_memByte[516940];       // mov ax, freeKickDestX
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax

l_update_goalie_dest_x_y:;
    ax = D6;                                // mov ax, word ptr D6
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D7;                                // mov ax, word ptr D7
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D2 = res;
    }                                       // add word ptr D2, ax
    ax = D1;                                // mov ax, word ptr D1
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = D2;                                // mov ax, word ptr D2
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    goto l_update_player_speed_and_deltas;  // jmp @@update_player_speed_and_deltas

l_set_player_with_no_ball_destination:;
    setPlayerWithNoBallDestination();       // call SetPlayerWithNoBallDestination

cseg_83A41:;
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
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

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
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    eax = *(dword *)&g_memByte[515588];     // mov eax, lastTeamPlayedBeforeBreak
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (flags.zero)
        goto l_update_player_speed_and_deltas; // jz @@update_player_speed_and_deltas

    ax = (word)readMemory(esi + 58, 2);     // mov ax, [esi+Sprite.destX]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = *(word *)&g_memByte[515604];       // mov ax, foulXCoordinate
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
    ax = (word)readMemory(esi + 60, 2);     // mov ax, [esi+Sprite.destY]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = *(word *)&g_memByte[515606];       // mov ax, foulYCoordinate
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
        int32_t srcSigned = 4225;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 4225
    if (!flags.carry && !flags.zero)
        goto l_update_player_speed_and_deltas; // ja short @@update_player_speed_and_deltas

    ax = *(word *)&g_memByte[515604];       // mov ax, foulXCoordinate
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 336;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 336
    if (flags.sign != flags.overflow)
        goto cseg_83B20;                    // jl short cseg_83B20

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 70;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 70
    goto cseg_83B28;                        // jmp short cseg_83B28

cseg_83B20:;
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 70;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 70

cseg_83B28:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    goto l_update_player_speed_and_deltas;  // jmp short @@update_player_speed_and_deltas

l_stop_player:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax

l_update_player_speed_and_deltas:;
    updatePlayerSpeedAndFrameDelay(A6.as<TeamGeneralInfo&>(), A1.as<Sprite&>());
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 58, 2);     // mov ax, [esi+Sprite.destX]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 60, 2);     // mov ax, [esi+Sprite.destY]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+Sprite.speed]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    push(A1);                               // push A1
    SWOS::CalculateDeltaXAndY();            // call CalculateDeltaXAndY
    pop(A1);                                // pop A1
    eax = D1;                               // mov eax, D1
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 46, 4, eax);          // mov [esi+Sprite.deltaX], eax
    eax = D2;                               // mov eax, D2
    writeMemory(esi + 50, 4, eax);          // mov [esi+Sprite.deltaY], eax
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
        goto l_not_goalkeeper;              // jnz short @@not_goalkeeper

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
        goto l_not_goalkeeper;              // jnz short @@not_goalkeeper

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 140, 2);    // mov ax, [esi+TeamGeneralInfo.goalkeeperPlaying]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_not_goalkeeper;              // jnz short @@not_goalkeeper

    eax = *(dword *)&g_memByte[516920];     // mov eax, dword ptr ballInUpperPenaltyArea
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (!flags.zero)
        goto l_calc_direction;              // jnz short @@calc_direction

l_not_goalkeeper:;
    ax = D0;                                // mov ax, word ptr D0
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_got_movement;                // jns short @@got_movement

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
        goto l_skip_setting_direction;      // jz short @@skip_setting_direction

    eax = readMemory(esi + 104, 4);         // mov eax, [esi+TeamGeneralInfo.passingKickingPlayer]
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
        goto l_skip_setting_direction;      // jz short @@skip_setting_direction

l_calc_direction:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 82, 2);     // mov ax, [esi+Sprite.fullDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 128;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 128
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh

l_got_movement:;
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 16
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 5;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 5
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax

l_skip_setting_direction:;
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
        goto l_ball_going_to_player;        // jz short @@ball_going_to_player

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
    if (!flags.carry && !flags.zero)
        goto l_ball_going_to_player;        // ja short @@ball_going_to_player

    ax = *(word *)&g_memByte[515604];       // mov ax, foulXCoordinate
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = *(word *)&g_memByte[515606];       // mov ax, foulYCoordinate
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    goto l_calculate_player_ball_direction; // jmp short @@calculate_player_ball_direction

l_ball_going_to_player:;
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D4 = ax;                      // mov word ptr D4, ax

l_calculate_player_ball_direction:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    *(word *)&D0 = 256;                     // mov word ptr D0, 256
    push(A1);                               // push A1
    SWOS::CalculateDeltaXAndY();            // call CalculateDeltaXAndY
    pop(A1);                                // pop A1
    if (!flags.sign)
        goto l_turn_player_toward_ball;     // jns short @@turn_player_toward_ball

    *(word *)&D0 = 0;                       // mov word ptr D0, 0

l_turn_player_toward_ball:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 82, 2, ax);           // mov [esi+Sprite.fullDirection], ax
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
        goto l_next_player;                 // jnz @@next_player

    eax = readMemory(esi + 46, 4);          // mov eax, [esi+Sprite.deltaX]
    D1 = eax;                               // mov D1, eax
    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    D1 |= eax;
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (D1 & 0x80000000) != 0;
    flags.zero = D1 == 0;                   // or D1, eax
    *(byte *)&D1 = 0;                       // mov byte ptr D1, 0
    if (flags.zero)
        goto l_check_player_movement;       // jz short @@check_player_movement

    *(byte *)&D1 = -1;                      // mov byte ptr D1, -1

l_check_player_movement:;
    esi = A1;                               // mov esi, A1
    al = (byte)readMemory(esi + 94, 1);     // mov al, byte ptr [esi+Sprite.isMoving]
    {
        int8_t dstSigned = *(byte *)&D1;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D1, al
    if (flags.zero)
        goto l_no_change_in_movement;       // jz short @@no_change_in_movement

cseg_83D7F:;
    al = D1;                                // mov al, byte ptr D1
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_player_is_running;           // jnz short @@player_is_running

    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerNormalStandingAnimTable());
    goto l_next_player;                     // jmp short @@next_player

l_player_is_running:;
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerRunningAnimTable());
    goto l_next_player;                     // jmp short @@next_player

l_no_change_in_movement:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 92, 2);     // mov ax, [esi+Sprite.playerDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
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
    if (!flags.zero)
        goto cseg_83D7F;                    // jnz short cseg_83D7F

l_next_player:;
    {
        int32_t val = stack[stackTop++];
        *(word *)&D4 = val;
    }                                       // pop small [word ptr D4]
    (*(int16_t *)&D4)--;
    flags.overflow = (int16_t)(*(int16_t *)&D4) == INT16_MIN;
    flags.sign = (*(int16_t *)&D4 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D4 == 0;      // dec word ptr D4
    if (!flags.sign)
        goto l_players_loop;                // jns @@players_loop

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
        goto cseg_83DF5;                    // jz short cseg_83DF5

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_KEEPER_HOLDS_BALL
    if (!flags.zero)
        return;                             // jnz short @@out

cseg_83DF5:;
    {
        dword src = *(dword *)&g_memByte[515576];
        int32_t dstSigned = src;
        int32_t srcSigned = 326028;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp lastPlayerPlayed, offset goalie1Sprite
    if (!flags.zero)
        goto l_check_goalie2;               // jnz short @@check_goalie2

    {
        dword src = *(dword *)&g_memByte[516888];
        int32_t dstSigned = src;
        int32_t srcSigned = 326028;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp prevLastPlayer, offset goalie1Sprite
    if (flags.zero)
        return;                             // jz short @@out

    goto cseg_83E27;                        // jmp short cseg_83E27

l_check_goalie2:;
    {
        dword src = *(dword *)&g_memByte[515576];
        int32_t dstSigned = src;
        int32_t srcSigned = 327260;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp lastPlayerPlayed, offset goalie2Sprite
    if (!flags.zero)
        return;                             // jnz short @@out

    {
        dword src = *(dword *)&g_memByte[516888];
        int32_t dstSigned = src;
        int32_t srcSigned = 327260;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp prevLastPlayer, offset goalie2Sprite
    if (flags.zero)
        return;                             // jz short @@out

cseg_83E27:;
    eax = *(dword *)&g_memByte[516888];     // mov eax, prevLastPlayer
    *(dword *)&g_memByte[515236] = eax;     // mov lastPlayerBeforeGoalkeeper, eax
    eax = *(dword *)&g_memByte[516892];     // mov eax, prevLastTeamPlayed
    *(dword *)&g_memByte[515232] = eax;     // mov lastTeamScored, eax
    *(word *)&g_memByte[450782] = 50;       // mov nobodysBallTimer, 50
}

void setClearResultHalftimeInterval(int interval)
{
    m_clearResultHalftimeInterval = interval;
}

void setClearResultInterval(int interval)
{
    m_clearResultInterval = interval;
}

void setPlayerDownTacklingInterval(int interval)
{
    m_playerDownTacklingInterval = interval;
}

void setPlayerDownHeadingInterval(int interval)
{
    m_playerDownHeadingInterval = interval;
}

// in:
//      A1 -> goalkeeper sprite
//      A2 -> ball sprite
//      A6 -> team data
// out:
//      D0 - return bool, can goalkeeper at least try saving this
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void shouldGoalkeeperDive()
{
    esi = A2;                               // mov esi, A2
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
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (flags.zero)
        goto l_check_goalkeeper_ball_distance; // jz short @@check_goalkeeper_ball_distance

    *(int16_t *)&D0 = -*(int16_t *)&D0;     // neg word ptr D0

l_check_goalkeeper_ball_distance:;
    ax = D0;                                // mov ax, word ptr D0
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_ball_in_front_of_goalkeeper; // jns short @@ball_in_front_of_goalkeeper

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = -10;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, -10
    if (flags.sign != flags.overflow)
        goto l_goalkeeper_wont_dive;        // jl @@goalkeeper_wont_dive

    goto l_try_saving;                      // jmp @@try_saving

l_ball_in_front_of_goalkeeper:;
    ax = D0;                                // mov ax, word ptr D0
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_goalkeeper_wont_dive;        // js @@goalkeeper_wont_dive

    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_penalty_shot;                // jnz short @@penalty_shot

    ax = *(word *)&g_memByte[515580];       // mov ax, penalty
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_normal_shot;                 // jz short @@normal_shot

l_penalty_shot:;
    ax = *(word *)&g_memByte[325234];       // mov ax, kKeeperPenaltySaveDistanceFar
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    push(D0);                               // push small [word ptr D0]
    SWOS::Rand();                           // call Rand
    ax = D0;                                // mov ax, word ptr D0
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        int32_t val = stack[stackTop++];
        *(word *)&D0 = val;
    }                                       // pop small [word ptr D0]
    {
        word res = *(word *)&D2 & 24;
        *(word *)&D2 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D2, 18h
    if (flags.zero)
        goto l_penalty_compare_distance;    // jz short @@penalty_compare_distance

    ax = *(word *)&g_memByte[325236];       // mov ax, kKeeperPenaltySaveDistanceNear
    *(word *)&D1 = ax;                      // mov word ptr D1, ax

l_penalty_compare_distance:;
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_goalkeeper_wont_dive;        // jg @@goalkeeper_wont_dive

    goto l_try_saving;                      // jmp @@try_saving

l_normal_shot:;
    ax = *(word *)&g_memByte[325232];       // mov ax, kKeeperSaveDistance
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_goalkeeper_wont_dive;        // jg @@goalkeeper_wont_dive

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D4;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D4 = res;
    }                                       // sub word ptr D4, ax
    if (!flags.sign)
        goto cseg_78AE6;                    // jns short cseg_78AE6

    flags.carry = *(int16_t *)&D4 != 0;
    flags.overflow = *(int16_t *)&D4 == INT16_MIN;
    *(int16_t *)&D4 = -*(int16_t *)&D4;
    flags.sign = (*(int16_t *)&D4 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D4 == 0;      // neg word ptr D4

cseg_78AE6:;
    esi = A2;                               // mov esi, A2
    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    D0 = eax;                               // mov D0, eax
    getFramesNeededToCoverDistance();       // call GetFramesNeededToCoverDistance
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_goalkeeper_wont_dive;        // jz @@goalkeeper_wont_dive

    ax = *(word *)&g_memByte[516896];       // mov ax, ballDefensiveX
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D4;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D4 = res;
    }                                       // sub word ptr D4, ax
    if (!flags.sign)
        goto cseg_78B34;                    // jns short cseg_78B34

    flags.carry = *(int16_t *)&D4 != 0;
    flags.overflow = *(int16_t *)&D4 == INT16_MIN;
    *(int16_t *)&D4 = -*(int16_t *)&D4;
    flags.sign = (*(int16_t *)&D4 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D4 == 0;      // neg word ptr D4

cseg_78B34:;
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 46, 4);          // mov eax, [esi+Sprite.deltaX]
    D0 = eax;                               // mov D0, eax
    getFramesNeededToCoverDistance();       // call GetFramesNeededToCoverDistance
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_goalkeeper_wont_dive;        // jz @@goalkeeper_wont_dive

    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, ax
    if (flags.carry || flags.zero)
        goto l_goalkeeper_wont_dive;        // jbe @@goalkeeper_wont_dive

    ax = *(word *)&g_memByte[516896];       // mov ax, ballDefensiveX
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    {
        int16_t dstSigned = *(word *)&D4;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D4 = res;
    }                                       // sub word ptr D4, ax
    if (!flags.sign)
        goto cseg_78B95;                    // jns short cseg_78B95

    *(int16_t *)&D4 = -*(int16_t *)&D4;     // neg word ptr D4

cseg_78B95:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 24, 4);          // mov eax, [esi+TeamGeneralInfo.shotChanceTable]
    A0 = eax;                               // mov A0, eax
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 15;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0Fh
    {
        word res = *(word *)&D0 << 1;
        flags.carry = ((word)*(word *)&D0 >> 15) & 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx + 10, 2); // mov ax, [esi+ebx+10]
    (ax)--;
    flags.overflow = (int16_t)(ax) == INT16_MIN;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // dec ax
    if (!flags.sign)
        goto l_calc_frames;                 // jns short @@calc_frames

    ax = 0;                                 // mov ax, 0

l_calc_frames:;
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    A0 = 323744;                            // mov A0, offset kGoalkeeperDiveDeltas
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    eax = readMemory(esi + ebx, 4);         // mov eax, [esi+ebx]
    D0 = eax;                               // mov D0, eax
    {
        word src = *(word *)&g_memByte[336574];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        *(word *)&g_memByte[336574] = src;
    }                                       // add goalkeeperDiveDeadVar, 1
    getFramesNeededToCoverDistance();       // call GetFramesNeededToCoverDistance
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_goalkeeper_wont_dive;        // jz short @@goalkeeper_wont_dive

    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D3;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D3, ax
    if (flags.carry)
        goto l_goalkeeper_wont_dive;        // jb short @@goalkeeper_wont_dive

l_try_saving:;
    *(word *)&D0 = 1;                       // mov word ptr D0, 1
    ax = 1;                                 // mov ax, 1
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    return;                                 // retn

l_goalkeeper_wont_dive:;
    *(word *)&D0 = 0;                       // mov word ptr D0, 0
    ax ^= ax;
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // xor ax, ax
}

// in:
//      D0 -  direction
//      D1 -  goalkeeper far jump speed flag, overrides goalkeeper jump speed if:
//                0 - no influence, use high speed as usual (2048)
//                1 - use lower speed (1280)
//                * - anything else: use low speed (1024) + low byte of currentGameTick
//                    effectively random: 1024..1279
//      D3 -  also some direction
//      A1 -> player sprite
//      A6 -> player team (general)
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void goalkeeperJumping()
{
    esi = A1;                               // mov esi, A1
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 128;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 128
    if (!flags.carry && !flags.zero)
        goto l_ball_far_away;               // ja short @@ball_far_away

    ax = *(word *)&g_memByte[516248];       // mov ax, kGoalkeeperNearJumpSpeed
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    goto l_speed_setting_done;              // jmp short @@speed_setting_done

l_ball_far_away:;
    ax = kGoalkeeperFarJumpSpeed;           // mov ax, kGoalkeeperFarJumpSpeed
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    ax = D1;                                // mov ax, word ptr D1
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_speed_setting_done;          // jz short @@speed_setting_done

    ax = kGoalkeeperFarJumpSlowerSpeed;     // mov ax, kGoalkeeperFarJumpSlowerSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 1
    if (flags.zero)
        goto l_speed_setting_done;          // jz short @@speed_setting_done

    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        word res = *(word *)&D1 & 255;
        *(word *)&D1 = res;
    }                                       // and word ptr D1, 0FFh
    ax = *(word *)&g_memByte[516248];       // mov ax, kGoalkeeperNearJumpSpeed
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax

l_speed_setting_done:;
    esi = A2;                               // mov esi, A2
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
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 70, 1, 0);            // mov byte ptr [esi+TeamGeneralInfo.field_46], 0
    writeMemory(esi + 80, 2, 0);            // mov [esi+TeamGeneralInfo.goalkeeperDivingRight], 0
    {
        word src = *(word *)&g_memByte[516906];
        int16_t dstSigned = src;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp ballNotHighZ, 5
    if (!flags.carry && !flags.zero)
        goto l_goalie_jumping_high;         // ja short @@goalie_jumping_high

    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 7);            // mov [esi+Sprite.playerState], PL_GOALIE_DIVING_LOW
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto l_right_goalie_jumping_low;    // jz short @@right_goalie_jumping_low

    setPlayerAnimationTable(A1.as<Sprite&>(), getTopGoalieDivingLowAnimTable());
    goto l_set_down_timer;                  // jmp short @@set_down_timer

l_right_goalie_jumping_low:;
    setPlayerAnimationTable(A1.as<Sprite&>(), getBottomGoalieDivingLowAnimTable());
    goto l_set_down_timer;                  // jmp short @@set_down_timer

l_goalie_jumping_high:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 6);            // mov [esi+Sprite.playerState], PL_GOALIE_DIVING_HIGH
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto l_right_goalie_jumping_high;   // jz short @@right_goalie_jumping_high

    setPlayerAnimationTable(A1.as<Sprite&>(), getTopGoalieDivingHighAnimTable());
    goto l_set_down_timer;                  // jmp short @@set_down_timer

l_right_goalie_jumping_high:;
    setPlayerAnimationTable(A1.as<Sprite&>(), getBottomGoalieDivingHighAnimTable());

l_set_down_timer:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 13, 1, 75);           // mov [esi+Sprite.playerDownTimer], 75
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 56, 2, ax);           // mov [esi+TeamGeneralInfo.controlledPlDirection], ax
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    A5 = 515768;                            // mov A5, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    eax = A5;                               // mov eax, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = ebx;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, ebx
    A5 = eax;                               // mov A5, eax
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 2;
        dword res = dstSigned + srcSigned;
        A5 = res;
    }                                       // add A5, 2
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
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
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
}

// There was a shot on goal, but goalkeeper managed to throw himself and catch
// the ball (and save the day). (I think)
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void goalkeeperCaughtTheBall()
{
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (!flags.zero)
        goto l_right_team;                  // jnz short @@right_team

    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, 4);            // mov [esi+Sprite.direction], 4
    goto l_set_anim_table;                  // jmp short @@set_anim_table

l_right_team:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, 0);            // mov [esi+Sprite.direction], 0

l_set_anim_table:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 82, 2, 0);            // mov [esi+TeamGeneralInfo.goalkeeperDivingLeft], 0
    setPlayerAnimationTable(A1.as<Sprite&>(), getGoalkeeperJumpUpAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 4);            // mov [esi+Sprite.playerState], PL_GOALIE_CATCHING_BALL
    writeMemory(esi + 13, 1, 15);           // mov [esi+Sprite.playerDownTimer], 15
    ax = kGoalkeeperCatchSpeed;             // mov ax, kGoalkeeperCatchSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    ax = *(word *)&g_memByte[516914];       // mov ax, ballNextGroundX
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = *(word *)&g_memByte[516916];       // mov ax, ballNextYGroundY
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    {
        word src = (word)readMemory(esi + 60, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 137;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.destY], 137
    if (!flags.zero && flags.sign == flags.overflow)
        goto cseg_78D7F;                    // jg short cseg_78D7F

    writeMemory(esi + 60, 2, 137);          // mov [esi+Sprite.destY], 137

cseg_78D7F:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 60, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 761;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.destY], 761
    if (flags.sign != flags.overflow)
        return;                             // jl short @@out

    writeMemory(esi + 60, 2, 761);          // mov [esi+Sprite.destY], 761
}

// in:
//      D0 - movement delta, distance in a single frame (fixed point)
//           (absolute value is taken)
//      D4 - distance to cover (integer)
// out:
//      D7 - frames needed to cover the given distance (integer)
//
// Does division D4/D0 using loops, without direct divide instruction.
// If they're exactly divisible it will return one more, e.g. 9/3 -> 4.
// If D0 is less than 1, scales it up as well as the value which gets
// subtracted from D4 in a loop, to limit the number of loops when D0 < 1.
// It will fail for extremely low D0 values.
// Called to calculate on both x and y directions.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void getFramesNeededToCoverDistance()
{
    eax = D0;                               // mov eax, D0
    flags.carry = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (flags.zero)
        goto l_not_moving;                  // jz short @@not_moving

    if (!flags.sign)
        goto l_positive;                    // jns short @@positive

    *(int32_t *)&D0 = -*(int32_t *)&D0;     // neg D0

l_positive:;
    *(word *)&D5 = 1;                       // mov word ptr D5, 1
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 65536;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 10000h
    if (!flags.carry)
        goto l_goalkeeper_delta_eq_1_or_more; // jnb short @@goalkeeper_delta_eq_1_or_more

l_scale_loop:;
    {
        dword res = D0 << 1;
        D0 = res;
    }                                       // shl D0, 1
    {
        word res = *(word *)&D5 << 1;
        *(word *)&D5 = res;
    }                                       // shl word ptr D5, 1
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 65536;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 10000h
    if (flags.carry)
        goto l_scale_loop;                  // jb short @@scale_loop

l_goalkeeper_delta_eq_1_or_more:;
    ax = D4;                                // mov ax, word ptr D4
    {
        word tmp = *(word *)((byte *)&D4 + 2);
        *(word *)((byte *)&D4 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D4+2
    *(word *)&D4 = 0;                       // mov word ptr D4, 0
    *(word *)&D7 = 0;                       // mov word ptr D7, 0

l_divide_loop:;
    ax = D5;                                // mov ax, word ptr D5
    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D7 = res;
    }                                       // add word ptr D7, ax
    eax = D0;                               // mov eax, D0
    {
        int32_t dstSigned = D4;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D4 = res;
    }                                       // sub D4, eax
    if (!flags.sign)
        goto l_divide_loop;                 // jns short @@divide_loop

    return;                                 // retn

l_not_moving:;
    *(word *)&D7 = 0;                       // mov word ptr D7, 0
    ax ^= ax;
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // xor ax, ax
}

// in:
//      A1 -> player sprite
//      A2 -> ball sprite
//      A6 -> team (general)
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void updateBallVariables()
{
    push(D0);                               // push D0
    push(D4);                               // push D4
    push(D5);                               // push D5
    push(D6);                               // push D6
    push(D7);                               // push D7
    esi = A2;                               // mov esi, A2
    eax = readMemory(esi + 30, 4);          // mov eax, [esi+Sprite.x]
    D1 = eax;                               // mov D1, eax
    eax = readMemory(esi + 34, 4);          // mov eax, [esi+Sprite.y]
    D2 = eax;                               // mov D2, eax
    eax = readMemory(esi + 38, 4);          // mov eax, [esi+Sprite.z]
    D3 = eax;                               // mov D3, eax
    eax = readMemory(esi + 46, 4);          // mov eax, [esi+Sprite.deltaX]
    D4 = eax;                               // mov D4, eax
    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    D5 = eax;                               // mov D5, eax
    eax = readMemory(esi + 54, 4);          // mov eax, [esi+Sprite.deltaZ]
    D6 = eax;                               // mov D6, eax
    eax = (dword)getBallAirFriction();
    D7 = eax;                               // mov D7, eax
    {
        int32_t dstSigned = D5;
        int32_t srcSigned = -65536;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D5, -10000h
    if (flags.sign != flags.overflow)
        goto l_ball_going_up;               // jl short @@ball_going_up

    {
        int32_t dstSigned = D5;
        int32_t srcSigned = 65536;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D5, 10000h
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_ball_going_down;             // jg @@ball_going_down

    {
        int32_t dstSigned = D4;
        int32_t srcSigned = -65536;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D4, -10000h
    if (flags.sign != flags.overflow)
        goto l_ball_going_left;             // jl @@ball_going_left

    {
        int32_t dstSigned = D4;
        int32_t srcSigned = 65536;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D4, 10000h
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_ball_going_right;            // jg @@ball_going_right

    goto l_ball_not_moving;                 // jmp @@ball_not_moving

l_ball_going_up:;
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (!flags.zero)
        goto l_ball_not_moving;             // jnz @@ball_not_moving

    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 34, 4);          // mov eax, [esi+Sprite.y]
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // sub D2, eax
    if (flags.sign == flags.overflow)
        goto l_set_ball_y_to_0;             // jge short @@set_ball_y_to_0

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&g_memByte[516896] = ax;       // mov ballDefensiveX, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&g_memByte[516898] = ax;       // mov ballDefensiveY, ax
    ax = (word)readMemory(esi + 40, 2);     // mov ax, word ptr [esi+(Sprite.z+2)]
    *(word *)&g_memByte[516900] = ax;       // mov ballDefensiveZ, ax
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 34, 4);          // mov eax, [esi+Sprite.y]
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, eax
    goto l_ball_above_player_check;         // jmp @@ball_above_player_check

l_set_ball_y_to_0:;
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D6 = res;
    }                                       // sub D6, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, eax
    if (flags.sign == flags.overflow)
        goto l_set_ball_y_to_0;             // jge short @@set_ball_y_to_0

    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D1 = res;
    }                                       // sub D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D2 = res;
    }                                       // sub D2, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D3 = res;
    }                                       // sub D3, eax
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 34, 4);          // mov eax, [esi+Sprite.y]
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D2 = res;
    }                                       // add D2, eax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[516896] = ax;       // mov ballDefensiveX, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516898] = ax;       // mov ballDefensiveY, ax
    ax = D3;                                // mov ax, word ptr D3
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_77879;                    // jns short cseg_77879

    *(word *)&D3 = 0;                       // mov word ptr D3, 0

cseg_77879:;
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&g_memByte[516900] = ax;       // mov ballDefensiveZ, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax

l_ball_above_player_check:;
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = 8847360;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // sub D2, 870000h
    if (flags.sign == flags.overflow)
        goto l_ball_above_135;              // jge short @@ball_above_135

    ax = *(word *)&g_memByte[516896];       // mov ax, ballDefensiveX
    *(word *)&g_memByte[516902] = ax;       // mov ballNotHighX, ax
    ax = *(word *)&g_memByte[516898];       // mov ax, ballDefensiveY
    *(word *)&g_memByte[516904] = ax;       // mov ballNotHighY, ax
    ax = *(word *)&g_memByte[516900];       // mov ax, ballDefensiveZ
    *(word *)&g_memByte[516906] = ax;       // mov ballNotHighZ, ax
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = 8847360;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, 870000h
    goto cseg_779F5;                        // jmp cseg_779F5

l_ball_above_135:;
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D6 = res;
    }                                       // sub D6, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, eax
    if (flags.sign == flags.overflow)
        goto l_ball_above_135;              // jge short @@ball_above_135

    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D1 = res;
    }                                       // sub D1, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D3 = res;
    }                                       // sub D3, eax
    D2 = 8847360;                           // mov D2, 870000h
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[516902] = ax;       // mov ballNotHighX, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516904] = ax;       // mov ballNotHighY, ax
    ax = D3;                                // mov ax, word ptr D3
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_779B0;                    // jns short cseg_779B0

    *(word *)&D3 = 0;                       // mov word ptr D3, 0

cseg_779B0:;
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&g_memByte[516906] = ax;       // mov ballNotHighZ, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax

cseg_779F5:;
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = 8454144;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // sub D2, 810000h
    if (flags.sign == flags.overflow)
        goto l_ball_between_129_135;        // jge short @@ball_between_129_135

    *(word *)&g_memByte[516908] = 0;        // mov strikeDestX, 0
    *(word *)&g_memByte[516910] = 0;        // mov dseg_17E661, 0
    *(word *)&g_memByte[516912] = 0;        // mov dseg_17E663, 0
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = 8454144;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, 810000h
    goto l_out;                             // jmp @@out

l_ball_between_129_135:;
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D6 = res;
    }                                       // sub D6, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, eax
    if (flags.sign == flags.overflow)
        goto l_ball_between_129_135;        // jge short @@ball_between_129_135

    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D1 = res;
    }                                       // sub D1, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D3 = res;
    }                                       // sub D3, eax
    D2 = 8454144;                           // mov D2, 810000h
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[516908] = ax;       // mov strikeDestX, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516910] = ax;       // mov dseg_17E661, ax
    ax = D3;                                // mov ax, word ptr D3
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_77ADE;                    // jns short cseg_77ADE

    *(word *)&D3 = 0;                       // mov word ptr D3, 0

cseg_77ADE:;
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&g_memByte[516912] = ax;       // mov dseg_17E663, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    goto l_out;                             // jmp @@out

l_ball_going_down:;
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (!flags.zero)
        goto l_ball_not_moving;             // jnz @@ball_not_moving

    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 34, 4);          // mov eax, [esi+Sprite.y]
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // sub D2, eax
    if (flags.sign != flags.overflow)
        goto l_ball_above_player;           // jl short @@ball_above_player

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&g_memByte[516896] = ax;       // mov ballDefensiveX, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&g_memByte[516898] = ax;       // mov ballDefensiveY, ax
    ax = (word)readMemory(esi + 40, 2);     // mov ax, word ptr [esi+(Sprite.z+2)]
    *(word *)&g_memByte[516900] = ax;       // mov ballDefensiveZ, ax
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 34, 4);          // mov eax, [esi+Sprite.y]
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, eax
    goto cseg_77C95;                        // jmp cseg_77C95

l_ball_above_player:;
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D6 = res;
    }                                       // sub D6, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, eax
    if (flags.sign != flags.overflow)
        goto l_ball_above_player;           // jl short @@ball_above_player

    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D1 = res;
    }                                       // sub D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D2 = res;
    }                                       // sub D2, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D3 = res;
    }                                       // sub D3, eax
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 34, 4);          // mov eax, [esi+Sprite.y]
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D2 = res;
    }                                       // add D2, eax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[516896] = ax;       // mov ballDefensiveX, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516898] = ax;       // mov ballDefensiveY, ax
    ax = D3;                                // mov ax, word ptr D3
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_77C50;                    // jns short cseg_77C50

    *(word *)&D3 = 0;                       // mov word ptr D3, 0

cseg_77C50:;
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&g_memByte[516900] = ax;       // mov ballDefensiveZ, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax

cseg_77C95:;
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = 50003968;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // sub D2, 2FB0000h
    if (flags.sign != flags.overflow)
        goto cseg_77CDD;                    // jl short cseg_77CDD

    ax = *(word *)&g_memByte[516896];       // mov ax, ballDefensiveX
    *(word *)&g_memByte[516902] = ax;       // mov ballNotHighX, ax
    ax = *(word *)&g_memByte[516898];       // mov ax, ballDefensiveY
    *(word *)&g_memByte[516904] = ax;       // mov ballNotHighY, ax
    ax = *(word *)&g_memByte[516900];       // mov ax, ballDefensiveZ
    *(word *)&g_memByte[516906] = ax;       // mov ballNotHighZ, ax
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = 50003968;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, 2FB0000h
    goto cseg_77DD5;                        // jmp cseg_77DD5

cseg_77CDD:;
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D6 = res;
    }                                       // sub D6, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, eax
    if (flags.sign != flags.overflow)
        goto cseg_77CDD;                    // jl short cseg_77CDD

    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D1 = res;
    }                                       // sub D1, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D3 = res;
    }                                       // sub D3, eax
    D2 = 50003968;                          // mov D2, 2FB0000h
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[516902] = ax;       // mov ballNotHighX, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516904] = ax;       // mov ballNotHighY, ax
    ax = D3;                                // mov ax, word ptr D3
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_77D90;                    // jns short cseg_77D90

    *(word *)&D3 = 0;                       // mov word ptr D3, 0

cseg_77D90:;
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&g_memByte[516906] = ax;       // mov ballNotHighZ, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax

cseg_77DD5:;
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = 50397184;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // sub D2, 3010000h
    if (flags.sign != flags.overflow)
        goto cseg_77E0B;                    // jl short cseg_77E0B

    *(word *)&g_memByte[516908] = 0;        // mov strikeDestX, 0
    *(word *)&g_memByte[516910] = 0;        // mov dseg_17E661, 0
    *(word *)&g_memByte[516912] = 0;        // mov dseg_17E663, 0
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = 50397184;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, 3010000h
    goto l_out;                             // jmp @@out

cseg_77E0B:;
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D6 = res;
    }                                       // sub D6, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D2 = res;
    }                                       // add D2, eax
    if (flags.sign != flags.overflow)
        goto cseg_77E0B;                    // jl short cseg_77E0B

    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D1 = res;
    }                                       // sub D1, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D3 = res;
    }                                       // sub D3, eax
    D2 = 50397184;                          // mov D2, 3010000h
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[516908] = ax;       // mov strikeDestX, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516910] = ax;       // mov dseg_17E661, ax
    ax = D3;                                // mov ax, word ptr D3
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_77EBE;                    // jns short cseg_77EBE

    *(word *)&D3 = 0;                       // mov word ptr D3, 0

cseg_77EBE:;
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&g_memByte[516912] = ax;       // mov dseg_17E663, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    goto l_out;                             // jmp @@out

l_ball_going_left:;
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 30, 4);          // mov eax, [esi+Sprite.x]
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D1 = res;
    }                                       // sub D1, eax
    if (flags.sign == flags.overflow)
        goto l_ball_right_of_player;        // jge short @@ball_right_of_player

    eax = readMemory(esi + 30, 4);          // mov eax, [esi+Sprite.x]
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D1 = res;
    }                                       // add D1, eax
    goto l_ball_not_moving;                 // jmp @@ball_not_moving

l_ball_right_of_player:;
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D6 = res;
    }                                       // sub D6, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D2 = res;
    }                                       // add D2, eax
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D1 = res;
    }                                       // add D1, eax
    if (flags.sign == flags.overflow)
        goto l_ball_right_of_player;        // jge short @@ball_right_of_player

    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D1 = res;
    }                                       // sub D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D2 = res;
    }                                       // sub D2, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D3 = res;
    }                                       // sub D3, eax
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 30, 4);          // mov eax, [esi+Sprite.x]
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[516896] = ax;       // mov ballDefensiveX, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516898] = ax;       // mov ballDefensiveY, ax
    ax = D3;                                // mov ax, word ptr D3
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_77FF6;                    // jns short cseg_77FF6

    *(word *)&D3 = 0;                       // mov word ptr D3, 0

cseg_77FF6:;
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&g_memByte[516900] = ax;       // mov ballDefensiveZ, ax
    goto cseg_78136;                        // jmp cseg_78136

l_ball_going_right:;
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 30, 4);          // mov eax, [esi+Sprite.x]
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D1 = res;
    }                                       // sub D1, eax
    if (flags.sign != flags.overflow)
        goto l_ball_left_of_player;         // jl short @@ball_left_of_player

    eax = readMemory(esi + 30, 4);          // mov eax, [esi+Sprite.x]
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D1 = res;
    }                                       // add D1, eax
    goto l_ball_not_moving;                 // jmp @@ball_not_moving

l_ball_left_of_player:;
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D6 = res;
    }                                       // sub D6, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D2 = res;
    }                                       // add D2, eax
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D1 = res;
    }                                       // add D1, eax
    if (flags.sign != flags.overflow)
        goto l_ball_left_of_player;         // jl short @@ball_left_of_player

    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D1 = res;
    }                                       // sub D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D2 = res;
    }                                       // sub D2, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D3 = res;
    }                                       // sub D3, eax
    esi = A1;                               // mov esi, A1
    eax = readMemory(esi + 30, 4);          // mov eax, [esi+Sprite.x]
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[516896] = ax;       // mov ballDefensiveX, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516898] = ax;       // mov ballDefensiveY, ax
    ax = D3;                                // mov ax, word ptr D3
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_780F5;                    // jns short cseg_780F5

    *(word *)&D3 = 0;                       // mov word ptr D3, 0

cseg_780F5:;
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&g_memByte[516900] = ax;       // mov ballDefensiveZ, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    goto cseg_78136;                        // jmp short cseg_78136

l_ball_not_moving:;
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&g_memByte[516896] = ax;       // mov ballDefensiveX, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&g_memByte[516898] = ax;       // mov ballDefensiveY, ax
    ax = (word)readMemory(esi + 40, 2);     // mov ax, word ptr [esi+(Sprite.z+2)]
    *(word *)&g_memByte[516900] = ax;       // mov ballDefensiveZ, ax

cseg_78136:;
    ax = *(word *)&g_memByte[516896];       // mov ax, ballDefensiveX
    *(word *)&g_memByte[516902] = ax;       // mov ballNotHighX, ax
    ax = *(word *)&g_memByte[516898];       // mov ax, ballDefensiveY
    *(word *)&g_memByte[516904] = ax;       // mov ballNotHighY, ax
    ax = *(word *)&g_memByte[516900];       // mov ax, ballDefensiveZ
    *(word *)&g_memByte[516906] = ax;       // mov ballNotHighZ, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    *(word *)&g_memByte[516908] = 0;        // mov strikeDestX, 0
    *(word *)&g_memByte[516910] = 0;        // mov dseg_17E661, 0
    *(word *)&g_memByte[516912] = 0;        // mov dseg_17E663, 0

l_out:;
    pop(D7);                                // pop D7
    pop(D6);                                // pop D6
    pop(D5);                                // pop D5
    pop(D4);                                // pop D4
    pop(D0);                                // pop D0
}

// in:
//      A2 -> ball sprite
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void calculateBallNextGroundXYPositions()
{
    push(D0);                               // push D0
    push(D4);                               // push D4
    push(D5);                               // push D5
    push(D6);                               // push D6
    push(D7);                               // push D7
    esi = A2;                               // mov esi, A2
    eax = readMemory(esi + 30, 4);          // mov eax, [esi+Sprite.x]
    D1 = eax;                               // mov D1, eax
    eax = readMemory(esi + 34, 4);          // mov eax, [esi+Sprite.y]
    D2 = eax;                               // mov D2, eax
    eax = readMemory(esi + 38, 4);          // mov eax, [esi+Sprite.z]
    D3 = eax;                               // mov D3, eax
    eax = readMemory(esi + 46, 4);          // mov eax, [esi+Sprite.deltaX]
    D4 = eax;                               // mov D4, eax
    eax = readMemory(esi + 50, 4);          // mov eax, [esi+Sprite.deltaY]
    D5 = eax;                               // mov D5, eax
    {
        int32_t dstSigned = D5;
        int32_t srcSigned = -65536;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D5, -10000h
    if (flags.sign != flags.overflow)
        goto l_ball_moving;                 // jl short @@ball_moving

    {
        int32_t dstSigned = D5;
        int32_t srcSigned = 65536;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D5, 10000h
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_ball_moving;                 // jg short @@ball_moving

    {
        int32_t dstSigned = D4;
        int32_t srcSigned = -65536;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D4, -10000h
    if (flags.sign != flags.overflow)
        goto l_ball_moving;                 // jl short @@ball_moving

    {
        int32_t dstSigned = D4;
        int32_t srcSigned = 65536;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D4, 10000h
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_ball_moving;                 // jg short @@ball_moving

    goto l_ball_standing;                   // jmp @@ball_standing

l_ball_moving:;
    esi = A2;                               // mov esi, A2
    eax = readMemory(esi + 54, 4);          // mov eax, [esi+Sprite.deltaZ]
    D6 = eax;                               // mov D6, eax
    eax = (dword)getBallAirFriction();
    D7 = eax;                               // mov D7, eax
    D0 = 1310720;                           // mov D0, 140000h

l_subtract_z:;
    eax = D0;                               // mov eax, D0
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D3 = res;
    }                                       // sub D3, eax
    if (!flags.sign)
        goto l_z_in_range;                  // jns short @@z_in_range

    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 851968;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 0D0000h
    if (flags.zero)
        goto l_ball_standing;               // jz @@ball_standing

    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 65536;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D0 = res;
    }                                       // sub D0, 10000h
    goto l_subtract_z;                      // jmp short @@subtract_z

l_z_in_range:;
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D1;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D1 = res;
    }                                       // add D1, eax
    eax = D5;                               // mov eax, D5
    {
        int32_t dstSigned = D2;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D2 = res;
    }                                       // add D2, eax
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D6 = res;
    }                                       // sub D6, eax
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D3 = res;
    }                                       // add D3, eax
    if (!flags.sign)
        goto l_z_in_range;                  // jns short @@z_in_range

    eax = D0;                               // mov eax, D0
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D3 = res;
    }                                       // add D3, eax
    ax = D1;                                // mov ax, word ptr D1
    {
        word tmp = *(word *)((byte *)&D1 + 2);
        *(word *)((byte *)&D1 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D1+2
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        word tmp = *(word *)((byte *)&D2 + 2);
        *(word *)((byte *)&D2 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D2+2
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    ax = D3;                                // mov ax, word ptr D3
    {
        word tmp = *(word *)((byte *)&D3 + 2);
        *(word *)((byte *)&D3 + 2) = ax;
        ax = tmp;
    }                                       // xchg ax, word ptr D3+2
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[516914] = ax;       // mov ballNextGroundX, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[516916] = ax;       // mov ballNextYGroundY, ax
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&g_memByte[516918] = 0;        // mov ballNextZDeadVar, 0
    goto l_out;                             // jmp short @@out

l_ball_standing:;
    *(word *)&g_memByte[516914] = -1;       // mov ballNextGroundX, -1

l_out:;
    pop(D7);                                // pop D7
    pop(D6);                                // pop D6
    pop(D5);                                // pop D5
    pop(D4);                                // pop D4
    pop(D0);                                // pop D0
}

// in:
//      A1 -> sprite (player)
//      A2 -> sprite (ball)
//      A6 -> team data
//
// Check if the tackle ends up in faul, there must be a few conditions satisfied:
// - opponent has a controlled player
// - distance between them is more than 32 units
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void playerTacklingTestFoul()
{
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A2 = eax;                               // mov A2, eax
    esi = A2;                               // mov esi, A2
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A2 = eax;                               // mov A2, eax
    {
        int32_t dstSigned = A2;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A2, 0
    if (flags.zero)
        return;                             // jz @@out

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
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
    esi = A2;                               // mov esi, A2
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
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 32
    if (flags.carry || flags.zero)
        goto l_players_very_close;          // jbe short @@players_very_close

l_out:;
    return;                                 // retn

l_play_foul_comment_and_return:;
    SWOS::PlayDangerousPlayComment();       // call PlayDangerousPlayComment
    return;                                 // retn

l_opponents_player_goalkeeper:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // shr [esi+Sprite.speed], 1
    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // shr [esi+Sprite.speed], 1
    {
        word src = (word)readMemory(esi + 44, 2);
        src |= 1;
        writeMemory(esi + 44, 2, src);
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (src & 0x8000) != 0;
        flags.zero = src == 0;
    }                                       // or [esi+Sprite.speed], 1
    return;                                 // retn

l_players_very_close:;
    esi = A2;                               // mov esi, A2
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
    if (flags.zero)
        goto l_opponents_player_goalkeeper; // jz short @@opponents_player_goalkeeper

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
    if (flags.zero)
        return;                             // jz short @@out

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 81;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 81
    if (flags.sign != flags.overflow)
        return;                             // jl short @@out

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 129
    if (flags.sign != flags.overflow)
        return;                             // jl short @@out

    {
        word src = (word)readMemory(esi + 32, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 590;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.x+2)], 590
    if (!flags.zero && flags.sign == flags.overflow)
        return;                             // jg short @@out

    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 769;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 769
    if (!flags.zero && flags.sign == flags.overflow)
        return;                             // jg short @@out

    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // shr [esi+Sprite.speed], 1
    {
        word src = (word)readMemory(esi + 44, 2);
        word res = src >> 1;
        src = res;
        writeMemory(esi + 44, 2, src);
    }                                       // shr [esi+Sprite.speed], 1
    {
        word src = (word)readMemory(esi + 44, 2);
        src |= 1;
        writeMemory(esi + 44, 2, src);
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (src & 0x8000) != 0;
        flags.zero = src == 0;
    }                                       // or [esi+Sprite.speed], 1
    push(A1);                               // push A1
    push(A6);                               // push A6
    eax = A2;                               // mov eax, A2
    A1 = eax;                               // mov A1, eax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A6 = eax;                               // mov A6, eax
    playerTackled();                        // call PlayerTackled
    pop(A6);                                // pop A6
    pop(A1);                                // pop A1
    esi = A2;                               // mov esi, A2
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 800
    if (!flags.carry && !flags.zero)
        return;                             // ja @@out

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 96, 2);     // mov ax, [esi+Sprite.tackleState]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_foul_concedeed;              // jz short @@foul_concedeed

    {
        word src = (word)readMemory(esi + 96, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.tackleState], TS_GOOD_TACKLE
    if (flags.zero)
        goto l_play_foul_comment_and_return; // jz @@play_foul_comment_and_return

    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = -1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, -1
    if (flags.sign != flags.overflow)
        return;                             // jl @@out

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 1
    if (!flags.zero && flags.sign == flags.overflow)
        return;                             // jg @@out

l_foul_concedeed:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 14, 4);          // mov eax, [esi+TeamGeneralInfo.teamStatsPtr]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    {
        word src = (word)readMemory(esi + 4, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 4, 2, src);
    }                                       // add [esi+TeamStatisticsData.foulsConceded], 1
    ax = *(word *)&g_memByte[515886];       // mov ax, cardsDisallowed
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_no_cards_given;              // jnz @@no_cards_given

    ax = *(word *)&g_memByte[459200];       // mov ax, g_trainingGame
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_no_cards_given;              // jnz @@no_cards_given

    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 193;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 193
    if (flags.sign != flags.overflow)
        goto l_not_in_penalty_area;         // jl short @@not_in_penalty_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 478;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 478
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_not_in_penalty_area;         // jg short @@not_in_penalty_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 129
    if (flags.sign != flags.overflow)
        goto l_not_in_penalty_area;         // jl short @@not_in_penalty_area

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 769;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 769
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_not_in_penalty_area;         // jg short @@not_in_penalty_area

    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (!flags.zero)
        goto l_right_team;                  // jnz short @@right_team

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 216;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 216
    if (flags.zero || flags.sign != flags.overflow)
        goto l_in_penalty_area;             // jle @@in_penalty_area

    goto l_not_in_penalty_area;             // jmp short @@not_in_penalty_area

l_right_team:;
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 682;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 682
    if (flags.sign == flags.overflow)
        goto l_in_penalty_area;             // jge @@in_penalty_area

l_not_in_penalty_area:;
    *(word *)&D1 = 336;                     // mov word ptr D1, 336
    *(word *)&D2 = 129;                     // mov word ptr D2, 129
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

    *(word *)&D2 = 769;                     // mov word ptr D2, 769

l_left_team:;
    eax = A2;                               // mov eax, A2
    A5 = eax;                               // mov A5, eax
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D6 = ax;                      // mov word ptr D6, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D6;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D6 = res;
    }                                       // sub word ptr D6, ax
    ax = D6;                                // mov ax, word ptr D6
    bx = D6;                                // mov bx, word ptr D6
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D6 = ax;                      // mov word ptr D6, ax
    *(word *)((byte *)&D6 + 2) = dx;        // mov word ptr D6+2, dx
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        int16_t dstSigned = *(word *)&D3;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D3 = res;
    }                                       // sub word ptr D3, ax
    ax = D3;                                // mov ax, word ptr D3
    bx = D3;                                // mov bx, word ptr D3
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    *(word *)((byte *)&D3 + 2) = dx;        // mov word ptr D3+2, dx
    eax = D6;                               // mov eax, D6
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D3 = res;
    }                                       // add D3, eax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 20, 4);          // mov eax, [esi+TeamGeneralInfo.spritesTable]
    A0 = eax;                               // mov A0, eax
    *(word *)&D0 = 10;                      // mov word ptr D0, 10

l_players_loop:;
    esi = A0;                               // mov esi, A0
    eax = readMemory(esi, 4);               // mov eax, [esi]
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = 4;
        dword res = dstSigned + srcSigned;
        A0 = res;
    }                                       // add A0, 4
    A4 = eax;                               // mov A4, eax
    esi = A4;                               // mov esi, A4
    ax = (word)readMemory(esi + 108, 2);    // mov ax, [esi+Sprite.sentAway]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_next_player;                 // jnz @@next_player

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
    if (flags.zero)
        goto l_next_player;                 // jz @@next_player

    eax = A1;                               // mov eax, A1
    {
        int32_t dstSigned = A4;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A4, eax
    if (flags.zero)
        goto l_next_player;                 // jz @@next_player

    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D6 = ax;                      // mov word ptr D6, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D6;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D6 = res;
    }                                       // sub word ptr D6, ax
    ax = D6;                                // mov ax, word ptr D6
    bx = D6;                                // mov bx, word ptr D6
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D6 = ax;                      // mov word ptr D6, ax
    *(word *)((byte *)&D6 + 2) = dx;        // mov word ptr D6+2, dx
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D7 = ax;                      // mov word ptr D7, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D7 = res;
    }                                       // sub word ptr D7, ax
    ax = D7;                                // mov ax, word ptr D7
    bx = D7;                                // mov bx, word ptr D7
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D7 = ax;                      // mov word ptr D7, ax
    *(word *)((byte *)&D7 + 2) = dx;        // mov word ptr D7+2, dx
    eax = D7;                               // mov eax, D7
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        D6 = res;
    }                                       // add D6, eax
    eax = D3;                               // mov eax, D3
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, eax
    if (!flags.carry && !flags.zero)
        goto l_next_player;                 // ja short @@next_player

    eax = D6;                               // mov eax, D6
    D3 = eax;                               // mov D3, eax
    eax = A4;                               // mov eax, A4
    A5 = eax;                               // mov A5, eax

l_next_player:;
    (*(int16_t *)&D0)--;
    flags.overflow = (int16_t)(*(int16_t *)&D0) == INT16_MIN;
    flags.sign = (*(int16_t *)&D0 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D0 == 0;      // dec word ptr D0
    if (!flags.sign)
        goto l_players_loop;                // jns @@players_loop

    eax = A5;                               // mov eax, A5
    *(dword *)&g_memByte[336592] = eax;     // mov dseg_114EC2, eax
    {
        int32_t dstSigned = A2;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A2, eax
    if (flags.zero)
        goto cseg_79444;                    // jz short cseg_79444

l_in_penalty_area:;
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 30;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 1Eh
    {
        word res = *(word *)&D0 >> 1;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 1
    ax = *(word *)&g_memByte[515902];       // mov ax, playerCardChance
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.carry)
        goto l_no_cards_given;              // jnb short @@no_cards_given

    SWOS::Rand();                           // call Rand
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 32;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 32
    if (flags.carry)
        goto l_direct_red_card;             // jb short @@direct_red_card

l_yellow_card:;
    tryBookingThePlayer();                  // call TryBookingThePlayer
    if (!flags.zero)
        goto l_no_cards_given;              // jnz short @@no_cards_given

    testFoulForPenaltyAndFreeKick();        // call TestFoulForPenaltyAndFreeKick
    push(A1);                               // push A1
    activateReferee();                      // call ActivateReferee
    pop(A1);                                // pop A1
    return;                                 // retn

cseg_79444:;
    SWOS::Rand();                           // call Rand
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 32;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 32
    if (flags.carry)
        goto l_yellow_card;                 // jb short @@yellow_card

l_direct_red_card:;
    trySendingOffThePlayer();               // call TrySendingOffThePlayer
    if (!flags.zero)
        goto l_no_cards_given;              // jnz short @@no_cards_given

    testFoulForPenaltyAndFreeKick();        // call TestFoulForPenaltyAndFreeKick
    push(A1);                               // push A1
    activateReferee();                      // call ActivateReferee
    pop(A1);                                // pop A1
    return;                                 // retn

l_no_cards_given:;
    testFoulForPenaltyAndFreeKick();        // jmp TestFoulForPenaltyAndFreeKick
}

// in:
//      A2 -> fouled player (sprite)
//      A6 -> team that fouled (general info)
//
// Foul has been made. See where, and make it a penalty or
// free kick, depending on location. If not in appropriate
// areas, it's an ordinary foul.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void testFoulForPenaltyAndFreeKick()
{
    {
        word src = *(word *)&g_memByte[515596];
        int16_t dstSigned = src;
        int16_t srcSigned = 101;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameStatePl, 101
    if (flags.zero)
        return;                             // jz @@out

    SWOS::PlayFoulWhistleSample();          // call PlayFoulWhistleSample
    esi = A2;                               // mov esi, A2
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
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
        goto l_left_team;                   // jz @@left_team

    *(word *)&g_memByte[515608] = 4;        // mov cameraDirection, 4
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 682;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 682
    if (flags.sign != flags.overflow)
        goto l_not_in_lower_penalty_area;   // jl @@not_in_lower_penalty_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 193;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 193
    if (flags.sign != flags.overflow)
        goto l_not_in_lower_penalty_area;   // jl @@not_in_lower_penalty_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 478;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 478
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_not_in_lower_penalty_area;   // jg @@not_in_lower_penalty_area

    *(word *)&g_memByte[515598] = 14;       // mov gameState, ST_PENALTY
    *(word *)&g_memByte[515600] = -1;       // mov breakCameraMode, -1
    g_memByte[515610] = 56;                 // mov byte ptr playerTurnFlags, 38h
    *(word *)&g_memByte[515604] = 336;      // mov foulXCoordinate, 336
    *(word *)&g_memByte[515606] = 711;      // mov foulYCoordinate, 711
    push(D0);                               // push D0
    push(D1);                               // push D1
    push(D2);                               // push D2
    push(D3);                               // push D3
    push(D4);                               // push D4
    push(D5);                               // push D5
    push(D6);                               // push D6
    push(D7);                               // push D7
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    push(A4);                               // push A4
    push(A5);                               // push A5
    push(A6);                               // push A6
    SWOS::PlayPenaltyComment();             // call PlayPenaltyComment
    pop(A6);                                // pop A6
    pop(A5);                                // pop A5
    pop(A4);                                // pop A4
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    pop(D7);                                // pop D7
    pop(D6);                                // pop D6
    pop(D5);                                // pop D5
    pop(D4);                                // pop D4
    pop(D3);                                // pop D3
    pop(D2);                                // pop D2
    pop(D1);                                // pop D1
    pop(D0);                                // pop D0
    goto l_continue_after_penalty;          // jmp @@continue_after_penalty

l_left_team:;
    *(word *)&g_memByte[515608] = 0;        // mov cameraDirection, 0
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 216;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 216
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_not_in_upper_penalty_area;   // jg @@not_in_upper_penalty_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 193;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 193
    if (flags.sign != flags.overflow)
        goto l_not_in_upper_penalty_area;   // jl @@not_in_upper_penalty_area

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 478;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 478
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_not_in_upper_penalty_area;   // jg @@not_in_upper_penalty_area

    *(word *)&g_memByte[515598] = 14;       // mov gameState, ST_PENALTY
    *(word *)&g_memByte[515600] = -1;       // mov breakCameraMode, -1
    g_memByte[515610] = 131;                // mov byte ptr playerTurnFlags, 83h
    *(word *)&g_memByte[515604] = 336;      // mov foulXCoordinate, 336
    *(word *)&g_memByte[515606] = 187;      // mov foulYCoordinate, 187
    push(D0);                               // push D0
    push(D1);                               // push D1
    push(D2);                               // push D2
    push(D3);                               // push D3
    push(D4);                               // push D4
    push(D5);                               // push D5
    push(D6);                               // push D6
    push(D7);                               // push D7
    push(A0);                               // push A0
    push(A1);                               // push A1
    push(A2);                               // push A2
    push(A3);                               // push A3
    push(A4);                               // push A4
    push(A5);                               // push A5
    push(A6);                               // push A6
    SWOS::PlayPenaltyComment();             // call PlayPenaltyComment
    pop(A6);                                // pop A6
    pop(A5);                                // pop A5
    pop(A4);                                // pop A4
    pop(A3);                                // pop A3
    pop(A2);                                // pop A2
    pop(A1);                                // pop A1
    pop(A0);                                // pop A0
    pop(D7);                                // pop D7
    pop(D6);                                // pop D6
    pop(D5);                                // pop D5
    pop(D4);                                // pop D4
    pop(D3);                                // pop D3
    pop(D2);                                // pop D2
    pop(D1);                                // pop D1
    pop(D0);                                // pop D0
    goto l_continue_after_penalty;          // jmp @@continue_after_penalty

l_not_in_upper_penalty_area:;
    SWOS::PlayFoulComment();                // call PlayFoulComment
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 216;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 216
    if (flags.sign != flags.overflow)
        goto l_ordinary_foul;               // jl @@ordinary_foul

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 331;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 331
    if (flags.sign != flags.overflow)
        goto l_its_a_free_kick;             // jl short @@its_a_free_kick

    goto l_ordinary_foul;                   // jmp @@ordinary_foul

l_not_in_lower_penalty_area:;
    SWOS::PlayFoulComment();                // call PlayFoulComment
    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 567;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 567
    if (flags.sign != flags.overflow)
        goto l_ordinary_foul;               // jl @@ordinary_foul

    {
        int16_t dstSigned = *(word *)&D2;
        int16_t srcSigned = 682;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D2, 682
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_ordinary_foul;               // jg @@ordinary_foul

l_its_a_free_kick:;
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto l_right_team_made_foul;        // jz short @@right_team_made_foul

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 153;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 153
    if (flags.sign != flags.overflow)
        goto l_free_kick_left_1;            // jl @@free_kick_left_1

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 261;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 261
    if (flags.sign != flags.overflow)
        goto l_free_kick_left_2;            // jl @@free_kick_left_2

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 309;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 309
    if (flags.sign != flags.overflow)
        goto l_free_kick_left_3;            // jl @@free_kick_left_3

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 362;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 362
    if (flags.sign != flags.overflow)
        goto l_free_kick_center;            // jl @@free_kick_center

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 410;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 410
    if (flags.sign != flags.overflow)
        goto l_free_kick_right_1;           // jl @@free_kick_right_1

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 518;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 518
    if (flags.sign != flags.overflow)
        goto l_free_kick_right_2;           // jl @@free_kick_right_2

    goto l_free_kick_right_3;               // jmp @@free_kick_right_3

l_right_team_made_foul:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 153;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 153
    if (flags.sign != flags.overflow)
        goto l_free_kick_right_3;           // jl @@free_kick_right_3

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 261;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 261
    if (flags.sign != flags.overflow)
        goto l_free_kick_right_2;           // jl short @@free_kick_right_2

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 309;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 309
    if (flags.sign != flags.overflow)
        goto l_free_kick_right_1;           // jl short @@free_kick_right_1

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 362;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 362
    if (flags.sign != flags.overflow)
        goto l_free_kick_center;            // jl short @@free_kick_center

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 410;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 410
    if (flags.sign != flags.overflow)
        goto l_free_kick_left_3;            // jl short @@free_kick_left_3

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 518;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 518
    if (flags.sign != flags.overflow)
        goto l_free_kick_left_2;            // jl short @@free_kick_left_2

l_free_kick_left_1:;
    *(word *)&g_memByte[515598] = 6;        // mov gameState, ST_FREE_KICK_LEFT1
    goto l_save_foul_coordinates;           // jmp short @@save_foul_coordinates

l_free_kick_left_2:;
    *(word *)&g_memByte[515598] = 7;        // mov gameState, ST_FREE_KICK_LEFT2
    goto l_save_foul_coordinates;           // jmp short @@save_foul_coordinates

l_free_kick_left_3:;
    *(word *)&g_memByte[515598] = 8;        // mov gameState, ST_FREE_KICK_LEFT3
    goto l_save_foul_coordinates;           // jmp short @@save_foul_coordinates

l_free_kick_center:;
    *(word *)&g_memByte[515598] = 9;        // mov gameState, ST_FREE_KICK_CENTER
    goto l_save_foul_coordinates;           // jmp short @@save_foul_coordinates

l_free_kick_right_1:;
    *(word *)&g_memByte[515598] = 10;       // mov gameState, ST_FREE_KICK_RIGHT1
    goto l_save_foul_coordinates;           // jmp short @@save_foul_coordinates

l_free_kick_right_2:;
    *(word *)&g_memByte[515598] = 11;       // mov gameState, ST_FREE_KICK_RIGHT2
    goto l_save_foul_coordinates;           // jmp short @@save_foul_coordinates

l_free_kick_right_3:;
    *(word *)&g_memByte[515598] = 12;       // mov gameState, ST_FREE_KICK_RIGHT3
    goto l_save_foul_coordinates;           // jmp short @@save_foul_coordinates

l_ordinary_foul:;
    *(word *)&g_memByte[515598] = 13;       // mov gameState, ST_FOUL

l_save_foul_coordinates:;
    *(word *)&g_memByte[515600] = -1;       // mov breakCameraMode, -1
    g_memByte[515610] = -1;                 // mov byte ptr playerTurnFlags, -1
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[515604] = ax;       // mov foulXCoordinate, ax
    ax = D2;                                // mov ax, word ptr D2
    *(word *)&g_memByte[515606] = ax;       // mov foulYCoordinate, ax

l_continue_after_penalty:;
    {
        word src = *(word *)&g_memByte[449234];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp forceLeftTeam, 1
    if (!flags.zero)
        goto l_jump_here;                   // jnz short @@jump_here

    A6 = 515420;                            // mov A6, offset bottomTeamData

l_jump_here:;
    *(word *)&g_memByte[515596] = 101;      // mov gameStatePl, 101
    *(word *)&g_memByte[515584] = 0;        // mov gameNotInProgressCounterWriteOnly, 0
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    *(dword *)&g_memByte[515588] = eax;     // mov lastTeamPlayedBeforeBreak, eax
    *(word *)&g_memByte[515592] = 0;        // mov stoppageTimerTotal, 0
    *(word *)&g_memByte[515594] = 0;        // mov stoppageTimerActive, 0
    stopAllPlayers();                       // call StopAllPlayers
    *(word *)&g_memByte[449176] = 0;        // mov cameraXVelocity, 0
    *(word *)&g_memByte[449178] = 0;        // mov cameraYVelocity, 0
}

// in:
//      A1 -> player sprite
//      A6 -> team (general)
// out:
//      zero flag = 1, D0 = 0, card given
//      zero flag = 0, D0 = 1, card not given
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void tryBookingThePlayer()
{
    ax = *(word *)&g_memByte[448704];       // mov ax, plg_D3_param
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_794A5;                    // jnz short cseg_794A5

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_player_controls_team;        // jnz short @@player_controls_team

    ax = (word)readMemory(esi + 6, 2);      // mov ax, [esi+TeamGeneralInfo.playerCoachNumber]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_player_controls_team;        // jnz short @@player_controls_team

    goto l_no_card_out;                     // jmp @@no_card_out

cseg_794A5:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_player_controls_team;        // jnz short @@player_controls_team

    ax = (word)readMemory(esi + 6, 2);      // mov ax, [esi+TeamGeneralInfo.playerCoachNumber]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_player_controls_team;        // jnz short @@player_controls_team

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 102, 2);    // mov ax, [esi+Sprite.cards]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_no_card_out;                 // jnz @@no_card_out

l_player_controls_team:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 102, 2);    // mov ax, [esi+Sprite.cards]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_player_has_no_cards;         // jz short @@player_has_no_cards

    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 18, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.teamNumber], 1
    if (!flags.zero)
        goto l_second_team;                 // jnz short @@second_team

    ax = *(word *)&g_memByte[448706];       // mov ax, team1NumAllowedInjuries
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_no_card_out;                 // jz @@no_card_out

    goto l_player_has_no_cards;             // jmp short @@player_has_no_cards

l_second_team:;
    ax = *(word *)&g_memByte[448708];       // mov ax, team2NumAllowedInjuries
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_no_card_out;                 // jz @@no_card_out

l_player_has_no_cards:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 10, 4);          // mov eax, [esi+TeamGeneralInfo.inGameTeamPtr]
    A5 = eax;                               // mov A5, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 2, 2);      // mov ax, [esi+Sprite.playerOrdinal]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    A0 = 331902;                            // mov A0, offset inGameTeamPlayerOffsets
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    eax = A5;                               // mov eax, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = ebx;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, ebx
    A5 = eax;                               // mov A5, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 102, 2);    // mov ax, [esi+Sprite.cards]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 1
    ax = *(word *)&g_memByte[448704];       // mov ax, plg_D3_param
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_795E0;                    // jnz short cseg_795E0

    A0 = 516394;                            // mov A0, offset dseg_17E3EE
    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + 51, 1);     // mov al, [esi+PlayerGameHeader.previousCards]
    flags.carry = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_795B7;                    // jz short cseg_795B7

    A0 = 516399;                            // mov A0, offset dseg_17E3F3

cseg_795B7:;
    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + 52, 1);     // mov al, [esi+PlayerGameHeader.cards]
    *(byte *)&D1 = al;                      // mov byte ptr D1, al
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    al = (byte)readMemory(esi + ebx, 1);    // mov al, [esi+ebx]
    esi = A5;                               // mov esi, A5
    writeMemory(esi + 52, 1, al);           // mov [esi+PlayerGameHeader.cards], al
    goto l_jmp_give_yellow_card;            // jmp short @@jmp_give_yellow_card

cseg_795E0:;
    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + 52, 1);     // mov al, [esi+PlayerGameHeader.cards]
    *(byte *)&D1 = al;                      // mov byte ptr D1, al
    flags.carry = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto cseg_79603;                    // jnz short cseg_79603

    writeMemory(esi + 52, 1, 1);            // mov [esi+PlayerGameHeader.cards], 1
    goto l_give_yellow_card_to_player;      // jmp short @@give_yellow_card_to_player

cseg_79603:;
    esi = A5;                               // mov esi, A5
    writeMemory(esi + 52, 1, 3);            // mov [esi+PlayerGameHeader.cards], 3

l_jmp_give_yellow_card:;
    goto l_give_yellow_card_to_player;      // jmp short @@give_yellow_card_to_player

l_no_card_out:;
    *(word *)&D0 = 1;                       // mov word ptr D0, 1
    ax = 1;                                 // mov ax, 1
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    return;                                 // retn

l_give_yellow_card_to_player:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 102, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 102, 2, src);
    }                                       // add [esi+Sprite.cards], 1
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515896] = eax;     // mov lastTeamBooked, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515892] = eax;     // mov bookedPlayer, eax
    *(word *)&g_memByte[515900] = 0;        // mov refTimer, 0
    {
        word src = (word)readMemory(esi + 102, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.cards], 2
    if (flags.zero)
        goto l_second_yellow_card;          // jz short @@second_yellow_card

    *(word *)&g_memByte[515888] = 1;        // mov whichCard, CARD_YELLOW
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 14, 4);          // mov eax, [esi+TeamGeneralInfo.teamStatsPtr]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    {
        word src = (word)readMemory(esi + 6, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 6, 2, src);
    }                                       // add [esi+TeamStatisticsData.bookings], 1
    *(word *)&D0 = 0;                       // mov word ptr D0, 0
    ax ^= ax;
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // xor ax, ax
    return;                                 // retn

l_second_yellow_card:;
    *(word *)&g_memByte[515888] = 3;        // mov whichCard, CARD_SECOND_YELLOW
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 14, 4);          // mov eax, [esi+TeamGeneralInfo.teamStatsPtr]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    {
        word src = (word)readMemory(esi + 6, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        src = res;
        writeMemory(esi + 6, 2, src);
    }                                       // sub [esi+TeamStatisticsData.bookings], 1
    {
        word src = (word)readMemory(esi + 8, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 8, 2, src);
    }                                       // add [esi+TeamStatisticsData.sendingsOff], 1
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 18, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.teamNumber], 1
    if (!flags.zero)
        goto l_team2_red_card;              // jnz short @@team2_red_card

    {
        word src = *(word *)&g_memByte[448706];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        *(word *)&g_memByte[448706] = src;
    }                                       // sub team1NumAllowedInjuries, 1
    goto l_given_card_ok;                   // jmp short @@given_card_ok

l_team2_red_card:;
    {
        word src = *(word *)&g_memByte[448708];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        src = res;
        *(word *)&g_memByte[448708] = src;
    }                                       // sub team2NumAllowedInjuries, 1

l_given_card_ok:;
    *(word *)&D0 = 0;                       // mov word ptr D0, 0
    ax ^= ax;
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // xor ax, ax
}

// in:
//      A1 -> player (sprite)
//      A6 -> team (general)
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void trySendingOffThePlayer()
{
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 4, 2);      // mov ax, [esi+TeamGeneralInfo.playerNumber]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_player;                      // jnz short @@player

    ax = (word)readMemory(esi + 6, 2);      // mov ax, [esi+TeamGeneralInfo.playerCoachNumber]
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_computer_team_no_red_card;   // jz @@computer_team_no_red_card

l_player:;
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 18, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.teamNumber], 1
    if (!flags.zero)
        goto l_team2;                       // jnz short @@team2

    ax = *(word *)&g_memByte[448706];       // mov ax, team1NumAllowedInjuries
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_computer_team_no_red_card;   // jz @@computer_team_no_red_card

    goto cseg_7972E;                        // jmp short cseg_7972E

l_team2:;
    ax = *(word *)&g_memByte[448708];       // mov ax, team2NumAllowedInjuries
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_computer_team_no_red_card;   // jz @@computer_team_no_red_card

cseg_7972E:;
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 10, 4);          // mov eax, [esi+TeamGeneralInfo.inGameTeamPtr]
    A5 = eax;                               // mov A5, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 2, 2);      // mov ax, [esi+Sprite.playerOrdinal]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    A0 = 331902;                            // mov A0, offset inGameTeamPlayerOffsets
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    eax = A5;                               // mov eax, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = ebx;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, ebx
    A5 = eax;                               // mov A5, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 102, 2);    // mov ax, [esi+Sprite.cards]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)&D0 ^= 1;                      // xor word ptr D0, 1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 3;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 3
    ax = *(word *)&g_memByte[448704];       // mov ax, plg_D3_param
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_79804;                    // jnz short cseg_79804

    A0 = 516394;                            // mov A0, offset dseg_17E3EE
    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + 51, 1);     // mov al, [esi+PlayerGameHeader.previousCards]
    flags.carry = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_797DB;                    // jz short cseg_797DB

    A0 = 516399;                            // mov A0, offset dseg_17E3F3

cseg_797DB:;
    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + 52, 1);     // mov al, [esi+PlayerGameHeader.cards]
    *(byte *)&D1 = al;                      // mov byte ptr D1, al
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    al = (byte)readMemory(esi + ebx, 1);    // mov al, [esi+ebx]
    esi = A5;                               // mov esi, A5
    writeMemory(esi + 52, 1, al);           // mov [esi+PlayerGameHeader.cards], al
    goto l_jmp_to_update;                   // jmp short @@jmp_to_update

cseg_79804:;
    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + 52, 1);     // mov al, [esi+PlayerGameHeader.cards]
    *(byte *)&D1 = al;                      // mov byte ptr D1, al
    writeMemory(esi + 52, 1, 3);            // mov [esi+PlayerGameHeader.cards], 3

l_jmp_to_update:;
    goto l_update_statistics_with_red_card; // jmp short @@update_statistics_with_red_card

l_computer_team_no_red_card:;
    *(word *)&D0 = 1;                       // mov word ptr D0, 1
    ax = 1;                                 // mov ax, 1
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    return;                                 // retn

l_update_statistics_with_red_card:;
    eax = A6;                               // mov eax, A6
    *(dword *)&g_memByte[515896] = eax;     // mov lastTeamBooked, eax
    eax = A1;                               // mov eax, A1
    *(dword *)&g_memByte[515892] = eax;     // mov bookedPlayer, eax
    *(word *)&g_memByte[515900] = 0;        // mov refTimer, 0
    *(word *)&g_memByte[515888] = 2;        // mov whichCard, CARD_RED
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 14, 4);          // mov eax, [esi+TeamGeneralInfo.teamStatsPtr]
    A0 = eax;                               // mov A0, eax
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 102, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.cards], 1
    if (!flags.zero)
        goto l_no_yellow_card;              // jnz short @@no_yellow_card

    esi = A0;                               // mov esi, A0
    {
        word src = (word)readMemory(esi + 6, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        src = res;
        writeMemory(esi + 6, 2, src);
    }                                       // sub [esi+TeamStatisticsData.bookings], 1

l_no_yellow_card:;
    esi = A0;                               // mov esi, A0
    {
        word src = (word)readMemory(esi + 8, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 8, 2, src);
    }                                       // add [esi+TeamStatisticsData.sendingsOff], 1
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 18, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.teamNumber], 1
    if (!flags.zero)
        goto l_second_team_player;          // jnz short @@second_team_player

    {
        word src = *(word *)&g_memByte[448706];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        *(word *)&g_memByte[448706] = src;
    }                                       // sub team1NumAllowedInjuries, 1
    goto l_red_card_given_out;              // jmp short @@red_card_given_out

l_second_team_player:;
    {
        word src = *(word *)&g_memByte[448708];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        src = res;
        *(word *)&g_memByte[448708] = src;
    }                                       // sub team2NumAllowedInjuries, 1

l_red_card_given_out:;
    *(word *)&D0 = 0;                       // mov word ptr D0, 0
    ax ^= ax;
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // xor ax, ax
}

// in:
//      A1 -> player sprite
//      A6 -> team (general)
//
// This function is called when this player was tackled.
// It sets player's state to tackled and sets animation
// table. Also randomly sets injury and injury level.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void playerTackled()
{
    ax = *(word *)&g_memByte[459200];       // mov ax, g_trainingGame
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_not_a_training_game;         // jz short @@not_a_training_game

    SWOS::Rand();                           // call Rand
    {
        word res = *(word *)&D0 & 3;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 3
    if (!flags.zero)
        goto l_set_tackled_anim_table;      // jnz @@set_tackled_anim_table

l_not_a_training_game:;
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 18, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.teamNumber], 1
    if (!flags.zero)
        goto l_second_team;                 // jnz short @@second_team

    ax = *(word *)&g_memByte[448706];       // mov ax, team1NumAllowedInjuries
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_set_tackled_anim_table;      // jz @@set_tackled_anim_table

    goto l_injury_allowed;                  // jmp short @@injury_allowed

l_second_team:;
    ax = *(word *)&g_memByte[448708];       // mov ax, team2NumAllowedInjuries
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_set_tackled_anim_table;      // jz @@set_tackled_anim_table

l_injury_allowed:;
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 2, 2);      // mov ax, [esi+Sprite.playerOrdinal]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    A0 = 331902;                            // mov A0, offset inGameTeamPlayerOffsets
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 10, 4);          // mov eax, [esi+TeamGeneralInfo.inGameTeamPtr]
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = ebx;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, ebx
    A0 = eax;                               // mov A0, eax
    ax = *(word *)&g_memByte[448690];       // mov ax, gameLengthInGame
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    A5 = 516240;                            // mov A5, offset kTackleInjuryProbability
    esi = A0;                               // mov esi, A0
    al = (byte)readMemory(esi + 77, 1);     // mov al, [esi+PlayerGameHeader.injuriesBitfield]
    *(byte *)&D0 = al;                      // mov byte ptr D0, al
    {
        word res = *(word *)&D0 & 224;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0E0h
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 32;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 20h
    if (!flags.zero)
        goto l_not_injured;                 // jnz short @@not_injured

    A5 = 516244;                            // mov A5, offset kTackleInjuryProbabilityAlreadyInjured

l_not_injured:;
    SWOS::Rand();                           // call Rand
    esi = A5;                               // mov esi, A5
    ebx = *(word *)&D1;                     // movzx ebx, word ptr D1
    al = (byte)readMemory(esi + ebx, 1);    // mov al, [esi+ebx]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, al
    if (!flags.carry)
        goto l_set_tackled_anim_table;      // jnb @@set_tackled_anim_table

    playInjuryComment(A6.as<TeamGeneralInfo&>());
    A5 = 516210;                            // mov A5, offset kInjuryLevels
    esi = A0;                               // mov esi, A0
    al = (byte)readMemory(esi + 77, 1);     // mov al, [esi+PlayerGameHeader.injuriesBitfield]
    *(byte *)&D0 = al;                      // mov byte ptr D0, al
    {
        word res = *(word *)&D0 & 224;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0E0h
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 32;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 20h
    if (!flags.zero)
        goto cseg_79E4D;                    // jnz short cseg_79E4D

    A5 = 516217;                            // mov A5, offset kInjuryLevelAlreadyInjured

cseg_79E4D:;
    SWOS::Rand();                           // call Rand
    {
        word res = *(word *)&D0 & 63;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 3Fh
    *(word *)&D1 = 32;                      // mov word ptr D1, 32
    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi, 1);          // mov al, [esi]
    (*(int32_t *)&A5)++;                    // inc A5
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, al
    if (flags.carry)
        goto l_set_injury_level;            // jb @@set_injury_level

    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + -1, 1);     // mov al, [esi-1]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D0 = res;
    }                                       // sub byte ptr D0, al
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 32;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 32
    al = (byte)readMemory(esi, 1);          // mov al, [esi]
    (*(int32_t *)&A5)++;                    // inc A5
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, al
    if (flags.carry)
        goto l_set_injury_level;            // jb @@set_injury_level

    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + -1, 1);     // mov al, [esi-1]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D0 = res;
    }                                       // sub byte ptr D0, al
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 32;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 32
    al = (byte)readMemory(esi, 1);          // mov al, [esi]
    (*(int32_t *)&A5)++;                    // inc A5
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, al
    if (flags.carry)
        goto l_set_injury_level;            // jb @@set_injury_level

    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + -1, 1);     // mov al, [esi-1]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D0 = res;
    }                                       // sub byte ptr D0, al
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 32;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 32
    al = (byte)readMemory(esi, 1);          // mov al, [esi]
    (*(int32_t *)&A5)++;                    // inc A5
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, al
    if (flags.carry)
        goto l_set_injury_level;            // jb @@set_injury_level

    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + -1, 1);     // mov al, [esi-1]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D0 = res;
    }                                       // sub byte ptr D0, al
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 32;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 32
    al = (byte)readMemory(esi, 1);          // mov al, [esi]
    (*(int32_t *)&A5)++;                    // inc A5
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, al
    if (flags.carry)
        goto l_set_injury_level;            // jb short @@set_injury_level

    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + -1, 1);     // mov al, [esi-1]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D0 = res;
    }                                       // sub byte ptr D0, al
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 32;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 32
    al = (byte)readMemory(esi, 1);          // mov al, [esi]
    (*(int32_t *)&A5)++;                    // inc A5
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, al
    if (flags.carry)
        goto l_set_injury_level;            // jb short @@set_injury_level

    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + -1, 1);     // mov al, [esi-1]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D0 = res;
    }                                       // sub byte ptr D0, al
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 32;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 32
    al = (byte)readMemory(esi, 1);          // mov al, [esi]
    (*(int32_t *)&A5)++;                    // inc A5
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, al
    if (flags.carry)
        goto l_set_injury_level;            // jb short @@set_injury_level

    esi = A5;                               // mov esi, A5
    al = (byte)readMemory(esi + -1, 1);     // mov al, [esi-1]
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D0 = res;
    }                                       // sub byte ptr D0, al
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 32;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 32

l_set_injury_level:;
    esi = A0;                               // mov esi, A0
    writeMemory(esi + 48, 1, 1);            // mov [esi+PlayerGameHeader.isInjured], 1
    al = (byte)readMemory(esi + 77, 1);     // mov al, [esi+PlayerGameHeader.injuriesBitfield]
    {
        int8_t dstSigned = *(byte *)&D1;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D1, al
    if (flags.carry || flags.zero)
        goto cseg_79FD7;                    // jbe short cseg_79FD7

    al = D1;                                // mov al, byte ptr D1
    writeMemory(esi + 77, 1, al);           // mov [esi+PlayerGameHeader.injuriesBitfield], al

cseg_79FD7:;
    {
        word res = *(word *)&D1 & 224;
        *(word *)&D1 = res;
    }                                       // and word ptr D1, 0E0h
    {
        word res = *(word *)&D1 >> 5;
        *(word *)&D1 = res;
    }                                       // shr word ptr D1, 5
    A0 = 516224;                            // mov A0, offset dseg_17E2EC
    {
        word res = *(word *)&D1 << 1;
        *(word *)&D1 = res;
    }                                       // shl word ptr D1, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D1;                     // movzx ebx, word ptr D1
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 104, 2, ax);          // mov [esi+Sprite.injuryLevel], ax
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 18, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.teamNumber], 1
    if (!flags.zero)
        goto l_team2_injury;                // jnz short @@team2_injury

    {
        word src = *(word *)&g_memByte[448706];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        *(word *)&g_memByte[448706] = src;
    }                                       // sub team1NumAllowedInjuries, 1
    goto l_set_tackled_anim_table;          // jmp short @@set_tackled_anim_table

l_team2_injury:;
    {
        word src = *(word *)&g_memByte[448708];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        *(word *)&g_memByte[448708] = src;
    }                                       // sub team2NumAllowedInjuries, 1

l_set_tackled_anim_table:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 3);            // mov [esi+Sprite.playerState], PL_TACKLED
    writeMemory(esi + 13, 1, 50);           // mov [esi+Sprite.playerDownTimer], 50
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerTackledAnimTable());
}

// in:
//     D0 -  currently pressed direction
//     A1 -> player sprite that's tackling
//
// Set player state to tackling. Set his dest x and y with a table, and set speed to taclking speed.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void playerBeginTackling()
{
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 96, 2, 0);            // mov [esi+Sprite.tackleState], 0
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 56, 2, ax);           // mov [esi+TeamGeneralInfo.controlledPlDirection], ax
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    setPlayerAnimationTable(A1.as<Sprite&>(), getPlayerTacklingAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 1);            // mov [esi+Sprite.playerState], PL_TACKLING
    writeMemory(esi + 13, 1, m_playerDownTacklingInterval); // mov [esi+Sprite.playerDownTimer], playerDownTacklingInterval
    ax = (word)readMemory(esi + 2, 2);      // mov ax, [esi+Sprite.playerOrdinal]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, 1
    A0 = 331902;                            // mov A0, offset inGameTeamPlayerOffsets
    {
        word res = *(word *)&D1 << 1;
        *(word *)&D1 = res;
    }                                       // shl word ptr D1, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D1;                     // movzx ebx, word ptr D1
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 10, 4);          // mov eax, [esi+TeamGeneralInfo.inGameTeamPtr]
    ebx = *(word *)&D1;                     // movzx ebx, word ptr D1
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = ebx;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, ebx
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    al = (byte)readMemory(esi + 50, 1);     // mov al, [esi+PlayerGameHeader.fasterTackle]
    flags.carry = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto l_no_faster_tackle;            // jz short @@no_faster_tackle

    esi = A1;                               // mov esi, A1
    writeMemory(esi + 13, 1, 25);           // mov [esi+Sprite.playerDownTimer], 25

l_no_faster_tackle:;
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 13, 1, -1);           // mov [esi+Sprite.playerDownTimer], -1
    A5 = 515768;                            // mov A5, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    eax = A5;                               // mov eax, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = ebx;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, ebx
    A5 = eax;                               // mov A5, eax
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 2;
        dword res = dstSigned + srcSigned;
        A5 = res;
    }                                       // add A5, 2
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
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
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = *(word *)&g_memByte[325408];       // mov ax, kPlayerTacklingSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    writeMemory(esi + 106, 2, 0);           // mov [esi+Sprite.tacklingTimer], 0
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
static void playersTackledTheBallStrong()
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
//      D0 -  direction
//      A1 -> player sprite
//
// Player starts jumping in attempt to hit the ball with his head. Set his state
// and render him 'inoperable' for the next 55 cycles. Sets his animation table,
// dest x and y, and his speed. Only triggers for flying and lob header attempts,
// not for static ones.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void playerAttemptingJumpHeader()
{
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 98, 2, 0);            // mov [esi+Sprite.heading], 0
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    setPlayerAnimationTable(A1.as<Sprite&>(), getJumpHeaderAttemptAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 13, 1, m_playerDownHeadingInterval); // mov [esi+Sprite.playerDownTimer], m_playerDownHeadingInterval
    writeMemory(esi + 12, 1, 9);            // mov [esi+Sprite.playerState], PL_JUMP_HEADING
    A5 = 515768;                            // mov A5, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    eax = A5;                               // mov eax, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = ebx;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, ebx
    A5 = eax;                               // mov A5, eax
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 2;
        dword res = dstSigned + srcSigned;
        A5 = res;
    }                                       // add A5, 2
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
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
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = *(word *)&g_memByte[325410];       // mov ax, kJumpHeaderSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
}

// in:
//      A1 -> player sprite that's taking the throw-in
//
// Sets the coordinates of a player that takes the throw in.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void setThrowInPlayerDestinationCoordinates()
{
    A0 = 328492;                            // mov A0, offset ballSprite
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 336;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 336
    if (flags.carry)
        goto l_left_half;                   // jb short @@left_half

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 3;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 3
    goto l_set_player_x;                    // jmp short @@set_player_x

l_left_half:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, 3

l_set_player_x:;
    ax = D1;                                // mov ax, word ptr D1
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 32, 2, ax);           // mov word ptr [esi+(Sprite.x+2)], ax
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 36, 2, ax);           // mov word ptr [esi+(Sprite.y+2)], ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
}

// in:
//      A1 -> current player sprite
//      A6 -> team data
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void setPlayerWithNoBallDestination()
{
    A0 = 372152;                            // mov A0, offset g_tacticsTable
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 28, 2);     // mov ax, [esi+TeamGeneralInfo.tactics]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    eax = readMemory(esi + ebx, 4);         // mov eax, [esi+ebx]
    A0 = eax;                               // mov A0, eax
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_KEEPER_HOLDS_BALL
    if (flags.zero)
        goto l_keepers_ball_or_goalout;     // jz short @@keepers_ball_or_goalout

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GOAL_OUT_LEFT
    if (flags.zero)
        goto l_keepers_ball_or_goalout;     // jz short @@keepers_ball_or_goalout

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GOAL_OUT_RIGHT
    if (!flags.zero)
        goto l_game_in_progress;            // jnz short @@game_in_progress

l_keepers_ball_or_goalout:;
    esi = A0;                               // mov esi, A0
    al = (byte)readMemory(esi + 369, 1);    // mov al, [esi+Tactics.ballOutOfPlayTactics]
    ah = (int8_t)al < 0 ? -1 : 0;           // cbw
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    A0 = 372152;                            // mov A0, offset g_tacticsTable
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    eax = readMemory(esi + ebx, 4);         // mov eax, [esi+ebx]
    A0 = eax;                               // mov A0, eax

l_game_in_progress:;
    eax = A0;                               // mov eax, A0
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = 9;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, Tactics.playerPos
    A0 = eax;                               // mov A0, eax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 2, 2);      // mov ax, [esi+Sprite.playerOrdinal]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 2
    if (flags.sign)
        goto l_goalkeeper;                  // js @@goalkeeper

    ax = D0;                                // mov ax, word ptr D0
    bx = 35;                                // mov bx, 35
    tmp = ax * bx;
    ax = tmp.lo16;
    dx = tmp.hi16;                          // mul bx
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)((byte *)&D0 + 2) = dx;        // mov word ptr D0+2, dx
    ax = *(word *)&g_memByte[516012];       // mov ax, playerXQuadrantOffset
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = *(word *)&g_memByte[516014];       // mov ax, playerYQuadrantOffset
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (!flags.zero)
        goto l_right_team_invert;           // jnz short @@right_team_invert

    ax = *(word *)&g_memByte[516006];       // mov ax, ballQuadrantIndex
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    al = (byte)readMemory(esi + ebx, 1);    // mov al, [esi+ebx]
    *(byte *)&D1 = al;                      // mov byte ptr D1, al
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    goto l_send_player_to_his_quadrant;     // jmp short @@send_player_to_his_quadrant

l_right_team_invert:;
    *(word *)&D1 = 34;                      // mov word ptr D1, 34
    ax = *(word *)&g_memByte[516006];       // mov ax, ballQuadrantIndex
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    *(byte *)&D1 = 239;                     // mov byte ptr D1, 0EFh
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    al = (byte)readMemory(esi + ebx, 1);    // mov al, [esi+ebx]
    {
        int8_t dstSigned = *(byte *)&D1;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D1 = res;
    }                                       // sub byte ptr D1, al

l_send_player_to_his_quadrant:;
    al = D1;                                // mov al, byte ptr D1
    *(byte *)&D2 = al;                      // mov byte ptr D2, al
    {
        word res = *(word *)&D1 >> 4;
        *(word *)&D1 = res;
    }                                       // shr word ptr D1, 4
    {
        word res = *(word *)&D1 & 15;
        *(word *)&D1 = res;
    }                                       // and word ptr D1, 0Fh
    {
        word res = *(word *)&D2 & 15;
        *(word *)&D2 = res;
    }                                       // and word ptr D2, 0Fh
    A0 = 515944;                            // mov A0, offset playerXQuadrantsCoordinates
    {
        word res = *(word *)&D1 << 1;
        *(word *)&D1 = res;
    }                                       // shl word ptr D1, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D1;                     // movzx ebx, word ptr D1
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D3;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D3 = res;
    }                                       // add word ptr D3, ax
    A0 = 515974;                            // mov A0, offset playerYQuadrantCoordinates
    {
        word res = *(word *)&D2 << 1;
        *(word *)&D2 = res;
    }                                       // shl word ptr D2, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D2;                     // movzx ebx, word ptr D2
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D4;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D4 = res;
    }                                       // add word ptr D4, ax
    {
        int16_t dstSigned = *(word *)&D3;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        *(word *)&D3 = res;
    }                                       // sub word ptr D3, 4
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
        goto l_clip_x_dest;                 // jz short @@clip_x_dest

    {
        int16_t dstSigned = *(word *)&D3;
        int16_t srcSigned = 8;
        word res = dstSigned + srcSigned;
        *(word *)&D3 = res;
    }                                       // add word ptr D3, 8

l_clip_x_dest:;
    {
        int16_t dstSigned = *(word *)&D3;
        int16_t srcSigned = 81;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D3, 81
    if (flags.sign == flags.overflow)
        goto l_x_inside_left_pitch_border;  // jge short @@x_inside_left_pitch_border

    *(word *)&D3 = 81;                      // mov word ptr D3, 81

l_x_inside_left_pitch_border:;
    {
        int16_t dstSigned = *(word *)&D3;
        int16_t srcSigned = 590;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D3, 590
    if (flags.zero || flags.sign != flags.overflow)
        goto l_clip_y_dest;                 // jle short @@clip_y_dest

    *(word *)&D3 = 590;                     // mov word ptr D3, 590

l_clip_y_dest:;
    {
        int16_t dstSigned = *(word *)&D4;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D4, 129
    if (flags.sign == flags.overflow)
        goto l_y_inside_upper_pitch_border; // jge short @@y_inside_upper_pitch_border

    *(word *)&D4 = 129;                     // mov word ptr D4, 129

l_y_inside_upper_pitch_border:;
    {
        int16_t dstSigned = *(word *)&D4;
        int16_t srcSigned = 769;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D4, 769
    if (flags.zero || flags.sign != flags.overflow)
        goto l_set_player_dest_coordinates; // jle short @@set_player_dest_coordinates

    *(word *)&D4 = 769;                     // mov word ptr D4, 769

l_set_player_dest_coordinates:;
    ax = D3;                                // mov ax, word ptr D3
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    ax = D4;                                // mov ax, word ptr D4
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    return;                                 // retn

l_goalkeeper:;
    *(word *)&D2 = 285;                     // mov word ptr D2, 285
    *(word *)&D3 = 387;                     // mov word ptr D3, 387
    ax = D6;                                // mov ax, word ptr D6
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 81;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 81
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 1
    ax = D0;                                // mov ax, word ptr D0
    tmp = ax * *(word *)&D1;
    ax = tmp.lo16;
    dx = tmp.hi16;                          // mul word ptr D1
    bx = 510;                               // mov bx, 510
    {
        dword dividend = (dx << 16) | ax;
        word quot = (word)(dividend / bx);
        word rem = (word)(dividend % bx);
        ax = quot;
        dx = rem;
    }                                       // div bx
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)((byte *)&D0 + 2) = dx;        // mov word ptr D0+2, dx
    ax = D2;                                // mov ax, word ptr D2
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 58, 2, ax);           // mov [esi+Sprite.destX], ax
    *(word *)&D2 = 135;                     // mov word ptr D2, 135
    *(word *)&D3 = 161;                     // mov word ptr D3, 161
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
        goto l_calc_y_goalie_dest;          // jz short @@calc_y_goalie_dest

    *(word *)&D2 = 737;                     // mov word ptr D2, 737
    *(word *)&D3 = 763;                     // mov word ptr D3, 763

l_calc_y_goalie_dest:;
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 129;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 129
    ax = D3;                                // mov ax, word ptr D3
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, 1
    ax = D0;                                // mov ax, word ptr D0
    tmp = ax * *(word *)&D1;
    ax = tmp.lo16;
    dx = tmp.hi16;                          // mul word ptr D1
    bx = 641;                               // mov bx, 641
    {
        dword dividend = (dx << 16) | ax;
        word quot = (word)(dividend / bx);
        word rem = (word)(dividend % bx);
        ax = quot;
        dx = rem;
    }                                       // div bx
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    *(word *)((byte *)&D0 + 2) = dx;        // mov word ptr D0+2, dx
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
    ax = D0;                                // mov ax, word ptr D0
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
}

// in:
//      D0 - some direction
//      A1 -> player sprite
//
// Players jumps in place trying to hit static header.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void attemptStaticHeader()
{
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 98, 2, 0);            // mov [esi+Sprite.heading], 0
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 42, 2, ax);           // mov [esi+Sprite.direction], ax
    A5 = 515768;                            // mov A5, offset kDefaultDestinations
    {
        word res = *(word *)&D0 << 2;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 2
    eax = A5;                               // mov eax, A5
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    {
        int32_t dstSigned = eax;
        int32_t srcSigned = ebx;
        dword res = dstSigned + srcSigned;
        eax = res;
    }                                       // add eax, ebx
    A5 = eax;                               // mov A5, eax
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 2;
        dword res = dstSigned + srcSigned;
        A5 = res;
    }                                       // add A5, 2
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
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
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        flags.carry = res < static_cast<uint16_t>(dstSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    ax = D0;                                // mov ax, word ptr D0
    writeMemory(esi + 60, 2, ax);           // mov [esi+Sprite.destY], ax
    ax = *(word *)&g_memByte[516886];       // mov ax, kStaticHeaderPlayerSpeed
    writeMemory(esi + 44, 2, ax);           // mov [esi+Sprite.speed], ax
    setPlayerAnimationTable(A1.as<Sprite&>(), getStaticHeaderAttemptAnimTable());
    esi = A1;                               // mov esi, A1
    writeMemory(esi + 12, 1, 8);            // mov [esi+Sprite.playerState], PL_STATIC_HEADING
    writeMemory(esi + 13, 1, 20);           // mov [esi+Sprite.playerDownTimer], 20
}

// in:
//      A1 -> player (sprite)
//      A6 -> team general data
//
// Static header was activated now we need to aim it somewhere.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void setStaticHeaderDirection()
{
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 44, 2);     // mov ax, [esi+TeamGeneralInfo.currentAllowedDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        return;                             // js short @@out

    esi = A1;                               // mov esi, A1
    {
        byte src = (byte)readMemory(esi + 13, 1);
        int8_t dstSigned = src;
        int8_t srcSigned = 18;
        byte res = dstSigned - srcSigned;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerDownTimer], 18
    if (!flags.carry && !flags.zero)
        return;                             // ja short @@out

    if (esi.as<Sprite&>().animTable != getStaticHeaderAttemptAnimTable())
        return;

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
        return;                             // jz short @@out

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
        return;                             // jz short @@out

    if (flags.carry)
        goto l_turn_right;                  // jb short @@turn_right

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
    goto l_normalize_direction;             // jmp short @@normalize_direction

l_turn_right:;
    esi = A1;                               // mov esi, A1
    {
        word src = (word)readMemory(esi + 42, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 42, 2, src);
    }                                       // add [esi+Sprite.direction], 1

l_normalize_direction:;
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
}

// in:
//      A6 -> team data
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void AI_SetControlsDirection()
{
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 142, 2);    // mov ax, [esi+TeamGeneralInfo.resetControls]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz return

    ax = *(word *)&g_memByte[519156];       // mov ax, AI_counter
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_bump_resume_play_ai_timer;   // jz short @@bump_resume_play_ai_timer

    {
        word src = *(word *)&g_memByte[519156];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        src = res;
        *(word *)&g_memByte[519156] = src;
    }                                       // sub AI_counter, 1

l_bump_resume_play_ai_timer:;
    ax = *(word *)&g_memByte[519128];       // mov ax, AI_resumePlayTimer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_generate_rand;               // jz short @@generate_rand

    {
        word src = *(word *)&g_memByte[519128];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        src = res;
        *(word *)&g_memByte[519128] = src;
    }                                       // sub AI_resumePlayTimer, 1

l_generate_rand:;
    SWOS::Rand();                           // call Rand
    ax = D0;                                // mov ax, word ptr D0
    *(word *)&g_memByte[519130] = ax;       // mov AI_rand, ax
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 130, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        writeMemory(esi + 130, 2, src);
    }                                       // add [esi+TeamGeneralInfo.AI_timer], 1
    writeMemory(esi + 44, 2, -1);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], -1
    writeMemory(esi + 50, 1, 0);            // mov [esi+TeamGeneralInfo.firePressed], 0
    writeMemory(esi + 51, 1, 0);            // mov [esi+TeamGeneralInfo.fireThisFrame], 0
    writeMemory(esi + 48, 1, 0);            // mov [esi+TeamGeneralInfo.quickFire], 0
    writeMemory(esi + 49, 1, 0);            // mov [esi+TeamGeneralInfo.normalFire], 0
    ax = *(word *)&g_memByte[478658];       // mov ax, g_inSubstitutesMenu
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz return

    *(word *)&D7 = -1;                      // mov word ptr D7, -1
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A5 = eax;                               // mov A5, eax
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
    A4 = eax;                               // mov A4, eax
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A5, 0
    if (flags.zero)
        goto l_player_direction_set;        // jz short @@player_direction_set

    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D7 = ax;                      // mov word ptr D7, ax

l_player_direction_set:;
    A0 = 328492;                            // mov A0, offset ballSprite
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto l_bottom_team;                 // jz short @@bottom_team

    *(word *)&D2 = 769;                     // mov word ptr D2, 769
    goto l_calc_distance;                   // jmp short @@calc_distance

l_bottom_team:;
    *(word *)&D2 = 129;                     // mov word ptr D2, 129

l_calc_distance:;
    *(word *)&D1 = 336;                     // mov word ptr D1, 336
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    ax = D1;                                // mov ax, word ptr D1
    {
        int16_t dstSigned = *(word *)&D3;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D3 = res;
    }                                       // sub word ptr D3, ax
    ax = D2;                                // mov ax, word ptr D2
    {
        int16_t dstSigned = *(word *)&D4;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D4 = res;
    }                                       // sub word ptr D4, ax
    ax = D3;                                // mov ax, word ptr D3
    bx = D3;                                // mov bx, word ptr D3
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    *(word *)((byte *)&D3 + 2) = dx;        // mov word ptr D3+2, dx
    ax = D4;                                // mov ax, word ptr D4
    bx = D4;                                // mov bx, word ptr D4
    {
        int32_t res = (int16_t)ax * (int16_t)bx;
        ax = res & 0xffff;
        dx = res >> 16;
    }                                       // imul bx
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    *(word *)((byte *)&D4 + 2) = dx;        // mov word ptr D4+2, dx
    eax = D4;                               // mov eax, D4
    {
        int32_t dstSigned = D3;
        int32_t srcSigned = eax;
        dword res = dstSigned + srcSigned;
        flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = res < static_cast<uint32_t>(dstSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
        D3 = res;
    }                                       // add D3, eax
    eax = D3;                               // mov eax, D3
    D6 = eax;                               // mov D6, eax
    ax = (word)readMemory(esi + 32, 2);     // mov ax, word ptr [esi+(Sprite.x+2)]
    *(word *)&D3 = ax;                      // mov word ptr D3, ax
    ax = (word)readMemory(esi + 36, 2);     // mov ax, word ptr [esi+(Sprite.y+2)]
    *(word *)&D4 = ax;                      // mov word ptr D4, ax
    *(word *)&D0 = 256;                     // mov word ptr D0, 256
    push(D5);                               // push D5
    SWOS::CalculateDeltaXAndY();            // call CalculateDeltaXAndY
    pop(D5);                                // pop D5
    if (!flags.sign)
        goto l_save_angle;                  // jns short @@save_angle

    *(word *)&D0 = 0;                       // mov word ptr D0, 0

l_save_angle:;
    ax = D0;                                // mov ax, word ptr D0
    *(word *)&D5 = ax;                      // mov word ptr D5, ax
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
        goto l_game_in_progress;            // jz @@game_in_progress

    eax = *(dword *)&g_memByte[515588];     // mov eax, lastTeamPlayedBeforeBreak
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (!flags.zero)
        return;                             // jnz return

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 21;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_STARTING_GAME
    if (flags.carry)
        goto l_game_not_over;               // jb @@game_not_over

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 30;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GAME_ENDED
    if (!flags.carry && !flags.zero)
        goto l_game_not_over;               // ja @@game_not_over

    ax = *(word *)&g_memByte[448686];       // mov ax, team1Computer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_game_not_over;               // jz @@game_not_over

    ax = *(word *)&g_memByte[448688];       // mov ax, team2Computer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_game_not_over;               // jz @@game_not_over

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 25;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_RESULT_ON_HALFTIME
    if (flags.zero)
        goto l_showing_result_on_halftime;  // jz short @@showing_result_on_halftime

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 26;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_RESULT_AFTER_THE_GAME
    if (!flags.zero)
        goto l_not_showing_final_result;    // jnz short @@not_showing_final_result

    {
        word src = *(word *)&g_memByte[515592];
        int16_t dstSigned = src;
        int16_t srcSigned = *(word *)&m_clearResultInterval;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp stoppageTimerTotal, clearResultInterval
    if (flags.carry)
        return;                             // jb return

    goto l_interval_expired_fire;           // jmp short @@interval_expired_fire

l_showing_result_on_halftime:;
    {
        word src = *(word *)&g_memByte[515592];
        int16_t dstSigned = src;
        int16_t srcSigned = *(word *)&m_clearResultInterval;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp stoppageTimerTotal, clearResultInterval
    if (flags.carry)
        return;                             // jb return

    goto l_interval_expired_fire;           // jmp short @@interval_expired_fire

l_not_showing_final_result:;
    {
        word src = *(word *)&g_memByte[515592];
        int16_t dstSigned = src;
        int16_t srcSigned = *(word *)&m_clearResultHalftimeInterval;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp stoppageTimerTotal, clearResultHalftimeInterval
    if (flags.carry)
        return;                             // jb return

l_interval_expired_fire:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 50, 1, 1);            // mov [esi+TeamGeneralInfo.firePressed], 1
    return;                                 // retn

l_set_direction:;

l_check_if_result_shown:;
    ax = *(word *)&g_memByte[449122];       // mov ax, resultTimer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz return

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GOAL_OUT_LEFT
    if (flags.zero)
        goto l_update_turn_direction;       // jz @@update_turn_direction

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GOAL_OUT_RIGHT
    if (flags.zero)
        goto l_update_turn_direction;       // jz @@update_turn_direction

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_KEEPER_HOLDS_BALL
    if (flags.zero)
        goto l_update_turn_direction;       // jz @@update_turn_direction

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
        goto l_no_throw_in;                 // jb short @@no_throw_in

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
        goto l_update_turn_direction;       // jbe @@update_turn_direction

l_no_throw_in:;
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 13;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FOUL
    if (flags.zero)
        goto l_foul_or_free_kick;           // jz short @@foul_or_free_kick

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_LEFT1
    if (flags.carry)
        return;                             // jb return

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_RIGHT3
    if (!flags.carry && !flags.zero)
        return;                             // ja return

l_foul_or_free_kick:;
    ax = D5;                                // mov ax, word ptr D5
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 16
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 5;
        flags.carry = ((word)*(word *)&D0 >> 11) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shr word ptr D0, 5
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // jmp return

l_game_not_over:;
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 63;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 3Fh
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 50;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 50
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 50;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 50
    ax = *(word *)&g_memByte[515594];       // mov ax, stoppageTimerActive
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
        goto l_set_direction;               // ja @@set_direction

    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_doing_penalties;             // jnz @@doing_penalties

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 14;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PENALTY
    if (flags.zero)
        goto l_doing_penalties;             // jz @@doing_penalties

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_KEEPER_HOLDS_BALL
    if (flags.zero)
        goto l_keepers_ball;                // jz @@keepers_ball

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GOAL_OUT_LEFT
    if (flags.zero)
        goto l_keepers_ball;                // jz @@keepers_ball

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 2;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_GOAL_OUT_RIGHT
    if (flags.zero)
        goto l_keepers_ball;                // jz @@keepers_ball

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 0;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PLAYERS_TO_INITIAL_POSITIONS
    if (flags.zero)
        goto l_goal_scored;                 // jz @@goal_scored

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_LEFT1
    if (flags.carry)
        goto l_test_throw_in;               // jb short @@test_throw_in

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_RIGHT3
    if (flags.carry || flags.zero)
        goto l_free_kick;                   // jbe @@free_kick

l_test_throw_in:;
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
        goto l_test_foul;                   // jb short @@test_foul

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
    if (!flags.carry && !flags.zero)
        goto l_test_foul;                   // ja short @@test_foul

    ax = *(word *)&g_memByte[515598];       // mov ax, gameState
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 15;
        word res = dstSigned - srcSigned;
        *(word *)&D1 = res;
    }                                       // sub word ptr D1, ST_THROW_IN_FORWARD_RIGHT
    A0 = 519162;                            // mov A0, offset AI_throwInDirections
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D1;                     // movzx ebx, word ptr D1
    al = (byte)readMemory(esi + ebx, 1);    // mov al, [esi+ebx]
    *(byte *)&D1 = al;                      // mov byte ptr D1, al
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (flags.zero)
        goto cseg_84671;                    // jz cseg_84671

    {
        int newCarry = (*(byte *)&D1 >> 3) & 1;
        *(byte *)&D1 = ((byte)*(byte *)&D1 >> 4) | (*(byte *)&D1 << 4);
        flags.carry = newCarry != 0;
    }                                       // ror byte ptr D1, 4
    goto cseg_84671;                        // jmp cseg_84671

l_test_foul:;
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 13;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FOUL
    if (flags.zero)
        goto l_free_kick;                   // jz @@free_kick

    goto l_apply_after_touch;               // jmp @@apply_after_touch

l_keepers_ball:;
    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 1;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 1
    if (flags.zero)
        goto cseg_84775;                    // jz cseg_84775

    ax = *(word *)&g_memByte[515608];       // mov ax, cameraDirection
    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, ax
    if (flags.zero)
        goto l_apply_after_touch;           // jz @@apply_after_touch

    {
        word src = *(word *)&g_memByte[328524];
        int16_t dstSigned = src;
        int16_t srcSigned = 336;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.x+2, 336
    if (!flags.carry && !flags.zero)
        goto cseg_845CC;                    // ja short cseg_845CC

    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 1
    if (flags.zero)
        goto l_apply_after_touch;           // jz @@apply_after_touch

    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 3
    if (flags.zero)
        goto l_apply_after_touch;           // jz @@apply_after_touch

    goto cseg_84775;                        // jmp cseg_84775

cseg_845CC:;
    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 5
    if (flags.zero)
        goto l_apply_after_touch;           // jz @@apply_after_touch

    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 7;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 7
    if (flags.zero)
        goto l_apply_after_touch;           // jz @@apply_after_touch

    goto cseg_84775;                        // jmp cseg_84775

l_goal_scored:;
    {
        word src = *(word *)&g_memByte[515594];
        int16_t dstSigned = src;
        int16_t srcSigned = 150;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp stoppageTimerActive, 150
    if (flags.carry)
        goto cseg_84775;                    // jb cseg_84775

    ax = *(word *)&g_memByte[515608];       // mov ax, cameraDirection
    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, ax
    if (flags.zero)
        goto l_apply_after_touch;           // jz @@apply_after_touch

    goto cseg_84775;                        // jmp cseg_84775

    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 15;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 0Fh
    if (!flags.zero)
        goto l_apply_after_touch;           // jnz @@apply_after_touch

    goto cseg_84775;                        // jmp cseg_84775

    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 15;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 0Fh
    if (!flags.zero)
        goto cseg_84775;                    // jnz cseg_84775

    goto l_apply_after_touch;               // jmp @@apply_after_touch

l_free_kick:;
    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 15;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 0Fh
    if (!flags.zero)
        goto cseg_8470F;                    // jnz cseg_8470F

    goto cseg_84775;                        // jmp cseg_84775

cseg_84671:;
    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 15;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 0Fh
    if (!flags.zero)
        goto cseg_84775;                    // jnz cseg_84775

    cx = D7;                                // mov cx, word ptr D7
    eax = 1;                                // mov eax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            dword res = eax << shiftCount;
            eax = res;
        }
    }                                       // shl eax, cl
    {
        dword res = D1 & eax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // test D1, eax
    if (!flags.zero)
        goto l_apply_after_touch;           // jnz @@apply_after_touch

    goto cseg_84775;                        // jmp cseg_84775

l_doing_penalties:;
    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (flags.zero)
        goto l_penalty_random_direction_disallowed; // jz short @@penalty_random_direction_disallowed

    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // jmp return

l_penalty_random_direction_disallowed:;
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        return;                             // jz return

    {
        word src = (word)readMemory(esi + 42, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.direction], 4
    if (flags.zero)
        return;                             // jz return

    goto l_apply_after_touch;               // jmp short @@apply_after_touch

cseg_8470F:;
    ax = D5;                                // mov ax, word ptr D5
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 16
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 5;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 5
    ax = D7;                                // mov ax, word ptr D7
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
        goto l_apply_after_touch;           // jz short @@apply_after_touch

    goto cseg_84815;                        // jmp cseg_84815

l_apply_after_touch:;
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        return;                             // js return

    {
        word res = *(word *)&D2 << 5;
        *(word *)&D2 = res;
    }                                       // shl word ptr D2, 5
    al = D5;                                // mov al, byte ptr D5
    {
        int8_t dstSigned = *(byte *)&D2;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
        *(byte *)&D2 = res;
    }                                       // sub byte ptr D2, al
    goto l_update_max_stoppage_time;        // jmp @@update_max_stoppage_time

cseg_84775:;
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 5;
        flags.carry = ((word)*(word *)&D0 >> 11) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shl word ptr D0, 5
    findClosestPlayerToBallFacing();        // call FindClosestPlayerToBallFacing
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
        goto l_update_turn_direction;       // jz @@update_turn_direction

    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi+Sprite.teamNumber]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi, 2);          // mov ax, [esi+Sprite.teamNumber]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.zero)
        goto l_update_turn_direction;       // jnz @@update_turn_direction

    goto l_our_player_closest;              // jmp @@our_player_closest

    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 5;
        flags.carry = ((word)*(word *)&D0 >> 11) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shl word ptr D0, 5
    findClosestPlayerToBallFacing();        // call FindClosestPlayerToBallFacing
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
        goto l_apply_after_touch;           // jz @@apply_after_touch

    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi+Sprite.teamNumber]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi, 2);          // mov ax, [esi+Sprite.teamNumber]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.zero)
        goto l_apply_after_touch;           // jnz @@apply_after_touch

    goto l_our_player_closest;              // jmp @@our_player_closest

cseg_84815:;
    ax = D5;                                // mov ax, word ptr D5
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 16
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 5;
        flags.carry = ((word)*(word *)&D0 >> 11) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shr word ptr D0, 5
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // retn

l_update_turn_direction:;
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 14;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 0Eh
    if (!flags.zero)
        return;                             // jnz return

    ax = *(word *)&g_memByte[519126];       // mov ax, AI_turnDirection
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto cseg_8489B;                    // jns short cseg_8489B

    {
        word src = *(word *)&g_memByte[519126];
        int16_t dstSigned = src;
        int16_t srcSigned = -1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp AI_turnDirection, -1
    if (flags.zero)
        goto cseg_84890;                    // jz short cseg_84890

    {
        word src = *(word *)&g_memByte[519126];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        src = res;
        *(word *)&g_memByte[519126] = src;
    }                                       // add AI_turnDirection, 1
    {
        word src = *(word *)&g_memByte[519126];
        int16_t dstSigned = src;
        int16_t srcSigned = -1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp AI_turnDirection, -1
    if (!flags.zero)
        return;                             // jnz return

cseg_84890:;
    *(word *)&D1 = 1;                       // mov word ptr D1, 1
    goto l_apply_turn_direction;            // jmp short @@apply_turn_direction

cseg_8489B:;
    {
        word src = *(word *)&g_memByte[519126];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp AI_turnDirection, 1
    if (flags.zero)
        goto cseg_848BB;                    // jz short cseg_848BB

    {
        word src = *(word *)&g_memByte[519126];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        src = res;
        *(word *)&g_memByte[519126] = src;
    }                                       // sub AI_turnDirection, 1
    {
        word src = *(word *)&g_memByte[519126];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp AI_turnDirection, 1
    if (!flags.zero)
        return;                             // jnz return

cseg_848BB:;
    *(word *)&D1 = -1;                      // mov word ptr D1, -1

l_apply_turn_direction:;
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = *(word *)&g_memByte[519126];       // mov ax, AI_turnDirection
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (flags.zero)
        goto l_save_for_next_frame;         // jz short @@save_for_next_frame

    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // retn

l_save_for_next_frame:;
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&g_memByte[519126] = ax;       // mov AI_turnDirection, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    return;                                 // retn

l_game_in_progress:;
    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_penalty;                     // jnz short @@penalty

    ax = *(word *)&g_memByte[515580];       // mov ax, penalty
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_no_penalty;                  // jz short @@no_penalty

l_penalty:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 118, 2);    // mov ax, [esi+TeamGeneralInfo.spinTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_ball_after_touch_allowed;    // jns @@ball_after_touch_allowed

l_no_penalty:;
    ax = *(word *)&g_memByte[519156];       // mov ax, AI_counter
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_direction_updated;           // jz short @@direction_updated

    AI_SetDirectionTowardOpponentsGoal();   // call AI_SetDirectionTowardOpponentsGoal

l_direction_updated:;
    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A5, 0
    if (flags.zero)
        return;                             // jz return

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 118, 2);    // mov ax, [esi+TeamGeneralInfo.spinTimer]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.sign)
        goto l_ball_after_touch_allowed;    // jns @@ball_after_touch_allowed

    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_theres_a_player_near;        // jnz short @@theres_a_player_near

    al = (byte)readMemory(esi + 62, 1);     // mov al, [esi+TeamGeneralInfo.plCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.zero)
        goto l_theres_a_player_near;        // jnz short @@theres_a_player_near

    goto l_noone_near;                      // jmp @@noone_near

    eax = *(dword *)&g_memByte[515572];     // mov eax, lastTeamPlayed
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (flags.zero)
        goto l_noone_near;                  // jz @@noone_near

    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto l_noone_near;                  // jz @@noone_near

    goto cseg_855B1;                        // jmp cseg_855B1

l_theres_a_player_near:;
    AI_DecideWhetherToTriggerFire();        // call AI_DecideWhetherToTriggerFire
    if (flags.zero)
        return;                             // jz return

    ax = *(word *)&g_memByte[448851];       // mov ax, deadVarAlways0
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_84A0D;                    // jz short cseg_84A0D

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    eax = *(dword *)&g_memByte[448877];     // mov eax, dseg_1309C1
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, eax
    if (!flags.zero)
        goto cseg_84A0D;                    // jnz short cseg_84A0D

    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz return

cseg_84A0D:;
    esi = A5;                               // mov esi, A5
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
        goto l_player_facing_left_or_right; // jz short @@player_facing_left_or_right

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
    if (!flags.zero)
        goto cseg_84A53;                    // jnz short cseg_84A53

l_player_facing_left_or_right:;
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
        goto l_top_team_test;               // jz short @@top_team_test

    {
        word src = *(word *)&g_memByte[328528];
        int16_t dstSigned = src;
        int16_t srcSigned = 158;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.y+2, 158
    if (flags.zero || flags.sign != flags.overflow)
        goto cseg_84AEB;                    // jle cseg_84AEB

    goto cseg_84A53;                        // jmp short cseg_84A53

l_top_team_test:;
    {
        word src = *(word *)&g_memByte[328528];
        int16_t dstSigned = src;
        int16_t srcSigned = 740;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.y+2, 740
    if (flags.sign == flags.overflow)
        goto cseg_84AEB;                    // jge cseg_84AEB

cseg_84A53:;
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = 28800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, 28800
    if (!flags.carry && !flags.zero)
        goto cseg_84AEB;                    // ja cseg_84AEB

    {
        int32_t dstSigned = D6;
        int32_t srcSigned = 12800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, 12800
    if (flags.carry)
        goto cseg_84A85;                    // jb short cseg_84A85

    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 3;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 3
    if (!flags.zero)
        goto cseg_84AEB;                    // jnz short cseg_84AEB

cseg_84A85:;
    *(byte *)&D1 = 15;                      // mov byte ptr D1, 0Fh
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = 3200;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, 3200
    if (!flags.carry && !flags.zero)
        goto cseg_84A9F;                    // ja short cseg_84A9F

    *(byte *)&D1 = 50;                      // mov byte ptr D1, 32h

cseg_84A9F:;
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+42]
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto cseg_84AEB;                    // js short cseg_84AEB

    {
        word res = *(word *)&D2 << 5;
        *(word *)&D2 = res;
    }                                       // shl word ptr D2, 5
    al = D5;                                // mov al, byte ptr D5
    {
        int8_t dstSigned = *(byte *)&D2;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D2 = res;
    }                                       // sub byte ptr D2, al
    al = D1;                                // mov al, byte ptr D1
    {
        int8_t dstSigned = *(byte *)&D2;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D2, al
    if (!flags.zero && flags.sign == flags.overflow)
        goto cseg_84AEB;                    // jg short cseg_84AEB

    *(int8_t *)&D1 = -*(int8_t *)&D1;       // neg byte ptr D1
    al = D1;                                // mov al, byte ptr D1
    {
        int8_t dstSigned = *(byte *)&D2;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D2, al
    if (!flags.zero && flags.sign == flags.overflow)
        goto cseg_850F9;                    // jg cseg_850F9

cseg_84AEB:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 132, 2);    // mov ax, [esi+TeamGeneralInfo.field_84]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_84B5B;                    // jz short cseg_84B5B

    {
        word src = (word)readMemory(esi + 132, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        src = res;
        writeMemory(esi + 132, 2, src);
    }                                       // sub [esi+TeamGeneralInfo.field_84], 1
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 5;
        flags.carry = ((word)*(word *)&D0 >> 11) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shl word ptr D0, 5
    findClosestPlayerToBallFacing();        // call FindClosestPlayerToBallFacing
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
        goto cseg_84D57;                    // jz cseg_84D57

    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi+Sprite.teamNumber]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
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
        goto l_our_player_closest;          // jz @@our_player_closest

    goto cseg_84D57;                        // jmp cseg_84D57

cseg_84B5B:;
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = 9800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, 9800
    if (flags.carry)
        goto cseg_84DD3;                    // jb cseg_84DD3

    {
        int32_t dstSigned = A5;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A5, 0
    if (flags.zero)
        goto cseg_84DD3;                    // jz cseg_84DD3

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A2 = eax;                               // mov A2, eax
    {
        int32_t dstSigned = A2;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A2, 0
    if (flags.zero)
        goto cseg_84BBE;                    // jz short cseg_84BBE

    esi = A2;                               // mov esi, A2
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp dword ptr [esi+74], 800
    if (flags.carry)
        goto cseg_84C00;                    // jb short cseg_84C00

    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 5000;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp dword ptr [esi+74], 5000
    if (flags.carry)
        goto cseg_84C93;                    // jb cseg_84C93

cseg_84BBE:;
    esi = A0;                               // mov esi, A0
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
    A2 = eax;                               // mov A2, eax
    {
        int32_t dstSigned = A2;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A2, 0
    if (flags.zero)
        goto cseg_84DD3;                    // jz cseg_84DD3

    esi = A2;                               // mov esi, A2
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 800
    if (flags.carry)
        goto cseg_84C00;                    // jb short cseg_84C00

    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 5000;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 5000
    if (flags.carry)
        goto cseg_84C93;                    // jb cseg_84C93

    goto cseg_84DD3;                        // jmp cseg_84DD3

cseg_84C00:;
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = 180000;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, 180000
    if (!flags.carry && !flags.zero)
        goto cseg_84F4B;                    // ja cseg_84F4B

    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 5;
        flags.carry = ((word)*(word *)&D0 >> 11) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shl word ptr D0, 5
    findClosestPlayerToBallFacing();        // call FindClosestPlayerToBallFacing
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
        goto cseg_84D10;                    // jz cseg_84D10

    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi+Sprite.teamNumber]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
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
        goto l_our_player_closest;          // jz @@our_player_closest

    goto cseg_84D10;                        // jmp cseg_84D10

    {
        int32_t dstSigned = A0;
        int32_t srcSigned = -1;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, -1
    if (!flags.zero)
        goto l_our_player_closest;          // jnz @@our_player_closest

    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 130, 2);    // mov ax, [esi+130]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 3;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 3
    if (!flags.zero)
        goto cseg_84DD3;                    // jnz cseg_84DD3

    goto l_our_player_closest;              // jmp @@our_player_closest

cseg_84C93:;
    {
        word src = *(word *)&g_memByte[519130];
        int16_t dstSigned = src;
        int16_t srcSigned = 8;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp AI_rand, 8
    if (!flags.carry && !flags.zero)
        goto cseg_84CAD;                    // ja short cseg_84CAD

    {
        int32_t dstSigned = D6;
        int32_t srcSigned = 48400;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, 48400
    if (!flags.carry && !flags.zero)
        goto cseg_84F4B;                    // ja cseg_84F4B

cseg_84CAD:;
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 12;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 0Ch
    if (!flags.zero)
        goto cseg_84DD3;                    // jnz cseg_84DD3

    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 5;
        flags.carry = ((word)*(word *)&D0 >> 11) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shl word ptr D0, 5
    findClosestPlayerToBallFacing();        // call FindClosestPlayerToBallFacing
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
        goto cseg_84D10;                    // jz short cseg_84D10

    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi, 2);          // mov ax, [esi]
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
        goto l_our_player_closest;          // jz @@our_player_closest

cseg_84D10:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 132, 2);    // mov ax, [esi+TeamGeneralInfo.field_84]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_84DD3;                    // jnz cseg_84DD3

    ax = *(word *)&g_memByte[323620];       // mov ax, frameCount
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 127;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7Fh
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 32;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 20h
    if (!flags.carry)
        goto cseg_84DD3;                    // jnb cseg_84DD3

    writeMemory(esi + 132, 2, 4);           // mov [esi+TeamGeneralInfo.field_84], 4

cseg_84D57:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_84E16;                    // jz cseg_84E16

    {
        byte src = g_memByte[323626];
        byte res = src & 128;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // test byte ptr currentGameTick, 80h
    if (flags.zero)
        goto l_turn_left;                   // jz short @@turn_left

    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 1
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 7
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // retn

l_turn_left:;
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 7
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // retn

cseg_84DD3:;
    esi = A6;                               // mov esi, A6
    al = (byte)readMemory(esi + 61, 1);     // mov al, [esi+TeamGeneralInfo.plVeryCloseToBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (flags.zero)
        goto cseg_84E16;                    // jz short cseg_84E16

cseg_84DE0:;
    ax = D5;                                // mov ax, word ptr D5
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 16
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 5;
        flags.carry = ((word)*(word *)&D0 >> 11) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shr word ptr D0, 5
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // retn

cseg_84E16:;
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // retn

l_decide_if_flipping_direction:;
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 82, 2);     // mov ax, [esi+Sprite.fullDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = -128;
        byte res = dstSigned + srcSigned;
        *(byte *)&D0 = res;
    }                                       // add byte ptr D0, 128
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 16
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 5;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 5
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
        goto l_set_opposite_direction;      // jz short @@set_opposite_direction

    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 800
    if (flags.carry)
        goto l_set_opposite_direction;      // jb short @@set_opposite_direction

    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        word res = *(word *)&D1 & 14;
        *(word *)&D1 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D1, 0Eh
    if (flags.zero)
        goto l_set_opposite_direction;      // jz short @@set_opposite_direction

    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // retn

l_set_opposite_direction:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    return;                                 // retn

l_use_current_player_direction:;
    ax = D7;                                // mov ax, word ptr D7
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    writeAmigaModeDirectionFlip(A6.as<TeamGeneralInfo *>());
    return;                                 // retn

l_our_player_closest:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 132, 2, 0);           // mov [esi+TeamGeneralInfo.field_84], 0
    ax = *(word *)&g_memByte[519128];       // mov ax, AI_resumePlayTimer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz return

    AI_ResumeGameDelay();                   // call AI_ResumeGameDelay
    if (!flags.carry)
        return;                             // jnb return

    *(word *)&g_memByte[519128] = 15;       // mov AI_resumePlayTimer, 15
    ax = D7;                                // mov ax, word ptr D7
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    writeMemory(esi + 48, 1, 1);            // mov [esi+TeamGeneralInfo.quickFire], 1
    ax = *(word *)&g_memByte[449238];       // mov ax, AI_maxStoppageTime
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = *(word *)&g_memByte[515594];       // mov ax, stoppageTimerActive
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
        goto l_jmp_return;                  // ja short @@jmp_return

    ax = *(word *)&g_memByte[515594];       // mov ax, stoppageTimerActive
    *(word *)&g_memByte[449238] = ax;       // mov AI_maxStoppageTime, ax

l_jmp_return:;
    return;                                 // jmp return

cseg_84F4B:;
    ax = D5;                                // mov ax, word ptr D5
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 16
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 5;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 5
    ax = D7;                                // mov ax, word ptr D7
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, ax
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 7
    if (flags.zero)
        goto cseg_84FA0;                    // jz short cseg_84FA0

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 1
    if (flags.zero)
        goto cseg_84FA0;                    // jz short cseg_84FA0

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = -1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, -1
    if (flags.zero)
        goto cseg_84FA0;                    // jz short cseg_84FA0

    goto cseg_84D10;                        // jmp cseg_84D10

cseg_84FA0:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 134, 2, 2);           // mov [esi+TeamGeneralInfo.AI_afterTouchStrength], 2
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = 115600;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, 115600
    if (!flags.carry && !flags.zero)
        goto cseg_84FCA;                    // ja short cseg_84FCA

    writeMemory(esi + 134, 2, 1);           // mov [esi+TeamGeneralInfo.AI_afterTouchStrength], 1

cseg_84FCA:;
    ax = D7;                                // mov ax, word ptr D7
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    writeMemory(esi + 49, 1, 1);            // mov [esi+TeamGeneralInfo.normalFire], 1
    *(word *)&D2 = ax;                      // mov word ptr D2, ax
    {
        word res = *(word *)&D2 << 5;
        *(word *)&D2 = res;
    }                                       // shl word ptr D2, 5
    al = D5;                                // mov al, byte ptr D5
    {
        int8_t dstSigned = *(byte *)&D2;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D2 = res;
    }                                       // sub byte ptr D2, al
    *(word *)&g_memByte[519128] = 15;       // mov AI_resumePlayTimer, 15
    *(word *)&D0 = -1;                      // mov word ptr D0, -1
    al = D2;                                // mov al, byte ptr D2
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.sign)
        goto cseg_85025;                    // jns short cseg_85025

    *(int16_t *)&D0 = -*(int16_t *)&D0;     // neg word ptr D0

cseg_85025:;
    eax = *(dword *)&g_memByte[328542];     // mov eax, ballSprite.deltaY
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (flags.sign)
        goto cseg_8503F;                    // js short cseg_8503F

    {
        word src = *(word *)&g_memByte[328528];
        int16_t dstSigned = src;
        int16_t srcSigned = 555;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.y+2, 555
    if (!flags.carry && !flags.zero)
        goto cseg_850E1;                    // ja cseg_850E1

    goto cseg_8504E;                        // jmp short cseg_8504E

cseg_8503F:;
    {
        word src = *(word *)&g_memByte[328528];
        int16_t dstSigned = src;
        int16_t srcSigned = 342;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.y+2, 342
    if (flags.carry)
        goto cseg_850E1;                    // jb cseg_850E1

cseg_8504E:;
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    {
        word res = *(word *)&D1 & 28;
        *(word *)&D1 = res;
    }                                       // and word ptr D1, 1Ch
    {
        word res = *(word *)&D1 >> 2;
        *(word *)&D1 = res;
    }                                       // shr word ptr D1, 2
    {
        word src = *(word *)&g_memByte[328524];
        int16_t dstSigned = src;
        int16_t srcSigned = 193;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.x+2, 193
    if (flags.carry)
        goto cseg_85080;                    // jb short cseg_85080

    {
        word src = *(word *)&g_memByte[328524];
        int16_t dstSigned = src;
        int16_t srcSigned = 478;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.x+2, 478
    if (flags.carry)
        goto cseg_85097;                    // jb short cseg_85097

cseg_85080:;
    {
        word src = *(word *)&g_memByte[328524];
        int16_t dstSigned = src;
        int16_t srcSigned = 118;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.x+2, 118
    if (flags.carry)
        goto cseg_850C5;                    // jb short cseg_850C5

    {
        word src = *(word *)&g_memByte[328524];
        int16_t dstSigned = src;
        int16_t srcSigned = 553;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.x+2, 553
    if (!flags.carry && !flags.zero)
        goto cseg_850C5;                    // ja short cseg_850C5

    goto cseg_850AE;                        // jmp short cseg_850AE

cseg_85097:;
    ax = D1;                                // mov ax, word ptr D1
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_850CF;                    // jz short cseg_850CF

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 5
    if (flags.carry)
        goto cseg_850E1;                    // jb short cseg_850E1

    goto cseg_850DA;                        // jmp short cseg_850DA

cseg_850AE:;
    ax = D1;                                // mov ax, word ptr D1
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_850DA;                    // jz short cseg_850DA

    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 4
    if (flags.carry)
        goto cseg_850CF;                    // jb short cseg_850CF

    goto cseg_850E1;                        // jmp short cseg_850E1

cseg_850C5:;
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D1, 4
    if (flags.carry)
        goto cseg_850E1;                    // jb short cseg_850E1

cseg_850CF:;
    *(word *)&D0 = 0;                       // mov word ptr D0, 0
    goto cseg_850E1;                        // jmp short cseg_850E1

cseg_850DA:;
    flags.carry = *(int16_t *)&D0 != 0;
    flags.overflow = *(int16_t *)&D0 == INT16_MIN;
    *(int16_t *)&D0 = -*(int16_t *)&D0;
    flags.sign = (*(int16_t *)&D0 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D0 == 0;      // neg word ptr D0

cseg_850E1:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 136, 2, ax);          // mov [esi+TeamGeneralInfo.AI_ballSpinDirection], ax
    return;                                 // jmp return

cseg_850F9:;
    ax = *(word *)&g_memByte[519128];       // mov ax, AI_resumePlayTimer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz return

    AI_ResumeGameDelay();                   // call AI_ResumeGameDelay
    if (!flags.carry)
        return;                             // jnb return

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 134, 2, 0);           // mov [esi+TeamGeneralInfo.AI_afterTouchStrength], 0
    goto cseg_85178;                        // jmp short cseg_85178

    ax = *(word *)&g_memByte[519128];       // mov ax, AI_resumePlayTimer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz return

    AI_ResumeGameDelay();                   // call AI_ResumeGameDelay
    if (!flags.carry)
        return;                             // jnb return

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 134, 2, 1);           // mov [esi+TeamGeneralInfo.AI_afterTouchStrength], 1
    goto cseg_85178;                        // jmp short cseg_85178

    ax = *(word *)&g_memByte[519128];       // mov ax, AI_resumePlayTimer
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        return;                             // jnz return

    AI_ResumeGameDelay();                   // call AI_ResumeGameDelay
    if (!flags.carry)
        return;                             // jnb return

    esi = A6;                               // mov esi, A6
    writeMemory(esi + 134, 2, 2);           // mov [esi+TeamGeneralInfo.AI_afterTouchStrength], 2

cseg_85178:;
    *(word *)&g_memByte[519128] = 15;       // mov AI_resumePlayTimer, 15
    ax = D7;                                // mov ax, word ptr D7
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    writeMemory(esi + 49, 1, 1);            // mov [esi+TeamGeneralInfo.normalFire], 1
    *(word *)&D0 = -1;                      // mov word ptr D0, -1
    al = D2;                                // mov al, byte ptr D2
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.sign)
        goto cseg_851B4;                    // jns short cseg_851B4

    flags.carry = *(int16_t *)&D0 != 0;
    flags.overflow = *(int16_t *)&D0 == INT16_MIN;
    *(int16_t *)&D0 = -*(int16_t *)&D0;
    flags.sign = (*(int16_t *)&D0 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D0 == 0;      // neg word ptr D0

cseg_851B4:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 136, 2, ax);          // mov [esi+TeamGeneralInfo.AI_ballSpinDirection], ax
    return;                                 // jmp return

l_update_max_stoppage_time:;
    ax = *(word *)&g_memByte[449238];       // mov ax, AI_maxStoppageTime
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    ax = *(word *)&g_memByte[515594];       // mov ax, stoppageTimerActive
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
        goto l_decide_after_touch_strength; // ja short @@decide_after_touch_strength

    ax = *(word *)&g_memByte[515594];       // mov ax, stoppageTimerActive
    *(word *)&g_memByte[449238] = ax;       // mov AI_maxStoppageTime, ax

l_decide_after_touch_strength:;
    *(word *)&g_memByte[519128] = 15;       // mov AI_resumePlayTimer, 15
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 14;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PENALTY
    if (flags.zero)
        goto l_weak_after_touch;            // jz @@weak_after_touch

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 31;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PENALTIES
    if (flags.zero)
        goto l_weak_after_touch;            // jz @@weak_after_touch

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_LEFT1
    if (flags.carry)
        goto l_test_corner;                 // jb short @@test_corner

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FREE_KICK_RIGHT3
    if (flags.carry || flags.zero)
        goto l_check_distance_from_the_goal; // jbe short @@check_distance_from_the_goal

l_test_corner:;
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_CORNER_LEFT
    if (flags.zero)
        goto l_medium_after_touch;          // jz short @@medium_after_touch

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_CORNER_RIGHT
    if (flags.zero)
        goto l_medium_after_touch;          // jz short @@medium_after_touch

    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 24;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 18h
    if (!flags.zero)
        goto l_check_distance_from_the_goal; // jnz short @@check_distance_from_the_goal

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 10h
    if (flags.zero)
        goto l_weak_after_touch;            // jz short @@weak_after_touch

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 8;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 8
    if (flags.zero)
        goto l_medium_after_touch;          // jz short @@medium_after_touch

    goto l_strong_after_touch;              // jmp short @@strong_after_touch

l_check_distance_from_the_goal:;
    {
        int32_t dstSigned = D6;
        int32_t srcSigned = 28800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, 28800
    if (flags.carry)
        goto l_weak_after_touch;            // jb short @@weak_after_touch

    {
        int32_t dstSigned = D6;
        int32_t srcSigned = 57800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D6, 57800
    if (flags.carry)
        goto l_medium_after_touch;          // jb short @@medium_after_touch

l_strong_after_touch:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 134, 2, 2);           // mov [esi+TeamGeneralInfo.AI_afterTouchStrength], 2
    goto l_activate_normal_fire;            // jmp short @@activate_normal_fire

l_medium_after_touch:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 134, 2, 1);           // mov [esi+TeamGeneralInfo.AI_afterTouchStrength], 1
    goto l_activate_normal_fire;            // jmp short @@activate_normal_fire

l_weak_after_touch:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 134, 2, 0);           // mov [esi+TeamGeneralInfo.AI_afterTouchStrength], 0

l_activate_normal_fire:;
    ax = D7;                                // mov ax, word ptr D7
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    writeMemory(esi + 49, 1, 1);            // mov [esi+TeamGeneralInfo.normalFire], 1
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 14;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PENALTY
    if (flags.zero)
        goto l_no_after_touch;              // jz @@no_after_touch

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 31;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_PENALTIES
    if (flags.zero)
        goto l_no_after_touch;              // jz @@no_after_touch

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_CORNER_LEFT
    if (flags.zero)
        goto l_corner;                      // jz @@corner

    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_CORNER_RIGHT
    if (flags.zero)
        goto l_corner;                      // jz @@corner

    *(word *)&g_memByte[519128] = 15;       // mov AI_resumePlayTimer, 15
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    writeMemory(esi + 49, 1, 1);            // mov [esi+TeamGeneralInfo.normalFire], 1
    *(word *)&D0 = 0;                       // mov word ptr D0, 0
    {
        word src = *(word *)&g_memByte[515598];
        int16_t dstSigned = src;
        int16_t srcSigned = 13;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp gameState, ST_FOUL
    if (!flags.zero)
        goto cseg_8536B;                    // jnz short cseg_8536B

    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (!flags.zero)
        goto cseg_8535D;                    // jnz short cseg_8535D

    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 682;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 682
    if (flags.sign == flags.overflow)
        goto cseg_85384;                    // jge short cseg_85384

    goto cseg_8536B;                        // jmp short cseg_8536B

cseg_8535D:;
    esi = A2;                               // mov esi, A2
    {
        word src = (word)readMemory(esi + 36, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 216;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr [esi+(Sprite.y+2)], 216
    if (flags.zero || flags.sign != flags.overflow)
        goto cseg_85384;                    // jle short cseg_85384

cseg_8536B:;
    *(word *)&D0 = -1;                      // mov word ptr D0, -1
    al = D2;                                // mov al, byte ptr D2
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (al & 0x80) != 0;
    flags.zero = al == 0;                   // or al, al
    if (!flags.sign)
        goto cseg_85384;                    // jns short cseg_85384

    flags.carry = *(int16_t *)&D0 != 0;
    flags.overflow = *(int16_t *)&D0 == INT16_MIN;
    *(int16_t *)&D0 = -*(int16_t *)&D0;
    flags.sign = (*(int16_t *)&D0 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D0 == 0;      // neg word ptr D0

cseg_85384:;
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 136, 2, ax);          // mov [esi+TeamGeneralInfo.AI_ballSpinDirection], ax
    return;                                 // jmp return

l_corner:;
    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 3
    if (flags.carry)
        goto cseg_853FA;                    // jb short cseg_853FA

    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 6;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 6
    if (flags.carry)
        goto cseg_853DB;                    // jb short cseg_853DB

l_no_after_touch:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 136, 2, 0);           // mov [esi+TeamGeneralInfo.AI_ballSpinDirection], 0
    return;                                 // jmp return

cseg_853DB:;
    *(word *)&D1 = -1;                      // mov word ptr D1, -1
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    goto cseg_85417;                        // jmp short cseg_85417

cseg_853FA:;
    *(word *)&D1 = 1;                       // mov word ptr D1, 1
    ax = D7;                                // mov ax, word ptr D7
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 1

cseg_85417:;
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    cl = D0;                                // mov cl, byte ptr D0
    ax = 1;                                 // mov ax, 1
    {
        byte shiftCount = cl & 0x1f;
        if (shiftCount) {
            word res = ax << shiftCount;
            ax = res;
        }
    }                                       // shl ax, cl
    {
        word src = *(word *)&g_memByte[515610];
        word res = src & ax;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // test playerTurnFlags, ax
    if (flags.zero)
        goto l_no_after_touch;              // jz short @@no_after_touch

    ax = D1;                                // mov ax, word ptr D1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 136, 2, ax);          // mov [esi+TeamGeneralInfo.AI_ballSpinDirection], ax
    return;                                 // jmp return

l_noone_near:;
    AI_DecideWhetherToTriggerFire();        // call AI_DecideWhetherToTriggerFire
    if (flags.zero)
        return;                             // jz return

    {
        int32_t dstSigned = A4;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A4, 0
    if (flags.zero)
        goto l_pass_to_player_too_far_or_null; // jz short @@pass_to_player_too_far_or_null

    esi = A4;                               // mov esi, A4
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    D0 = eax;                               // mov D0, eax
    esi = A5;                               // mov esi, A5
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D0 = res;
    }                                       // sub D0, eax
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 50;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 50
    if (flags.sign != flags.overflow)
        goto l_pass_to_player_too_far_or_null; // jl short @@pass_to_player_too_far_or_null

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi + 36, 4);          // mov eax, [esi+TeamGeneralInfo.passToPlayerPtr]
    D0 = eax;                               // mov D0, eax
    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    writeMemory(esi + 36, 4, eax);          // mov [esi+TeamGeneralInfo.passToPlayerPtr], eax
    eax = D0;                               // mov eax, D0
    writeMemory(esi + 32, 4, eax);          // mov [esi+TeamGeneralInfo.controlledPlayer], eax
    return;                                 // jmp return

l_pass_to_player_too_far_or_null:;
    {
        int32_t dstSigned = A4;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A4, 0
    if (flags.zero)
        goto l_decide_if_flipping_direction; // jz @@decide_if_flipping_direction

    ax = *(word *)&g_memByte[448851];       // mov ax, deadVarAlways0
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto cseg_84DE0;                    // jnz cseg_84DE0

    ax = *(word *)&g_memByte[515276];       // mov ax, topTeamData.playerNumber
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_jmp_decide_if_flipping_direction; // jnz short @@jmp_decide_if_flipping_direction

    ax = *(word *)&g_memByte[515424];       // mov ax, bottomTeamData.playerNumber
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_jmp_decide_if_flipping_direction; // jnz short @@jmp_decide_if_flipping_direction

    goto l_randomly_flip_or_continue_direction; // jmp short @@randomly_flip_or_continue_direction

l_jmp_decide_if_flipping_direction:;
    goto l_decide_if_flipping_direction;    // jmp @@decide_if_flipping_direction

    esi = A4;                               // mov esi, A4
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    D0 = eax;                               // mov D0, eax
    esi = A5;                               // mov esi, A5
    eax = readMemory(esi + 74, 4);          // mov eax, [esi+Sprite.ballDistance]
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        D0 = res;
    }                                       // sub D0, eax
    {
        int32_t dstSigned = D0;
        int32_t srcSigned = 800;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp D0, 800
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_decide_if_flipping_direction; // jg @@decide_if_flipping_direction

l_randomly_flip_or_continue_direction:;
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 24;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 18h
    if (!flags.zero)
        goto l_use_current_player_direction; // jnz @@use_current_player_direction

    checkForAmigaModeDirectionFlipBan(A5.as<Sprite *>());
    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 2;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 2
    A0 = 519134;                            // mov A0, offset AI_randomRotateTable
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 82, 2);     // mov ax, [esi+Sprite.fullDirection]
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, ax
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = -128;
        byte res = dstSigned + srcSigned;
        *(byte *)&D0 = res;
    }                                       // add byte ptr D0, 128
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 16
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 5;
        flags.carry = ((word)*(word *)&D0 >> 11) & 1;
        *(word *)&D0 = res;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // shr word ptr D0, 5
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // retn

cseg_855B1:;
    goto cseg_84DD3;                        // jmp cseg_84DD3

    {
        int32_t dstSigned = A4;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A4, 0
    if (flags.zero)
        goto l_decide_if_flipping_direction; // jz @@decide_if_flipping_direction

    esi = A5;                               // mov esi, A5
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 2450;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 2450
    if (!flags.carry && !flags.zero)
        goto l_decide_if_flipping_direction; // ja @@decide_if_flipping_direction

    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+Sprite.direction]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_decide_if_flipping_direction; // js @@decide_if_flipping_direction

    {
        word res = *(word *)&D0 << 5;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 5
    al = D5;                                // mov al, byte ptr D5
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = al;
        byte res = dstSigned - srcSigned;
        *(byte *)&D0 = res;
    }                                       // sub byte ptr D0, al
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = 15;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, 15
    if (!flags.zero && flags.sign == flags.overflow)
        goto l_decide_if_flipping_direction; // jg @@decide_if_flipping_direction

    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = -15;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, 241
    if (flags.sign != flags.overflow)
        goto l_decide_if_flipping_direction; // jl @@decide_if_flipping_direction

    ax = D7;                                // mov ax, word ptr D7
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    writeMemory(esi + 51, 1, 1);            // mov [esi+TeamGeneralInfo.fireThisFrame], 1
    return;                                 // jmp return

l_ball_after_touch_allowed:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 56, 2);     // mov ax, [esi+TeamGeneralInfo.controlledPlDirection]
    *(word *)&D1 = ax;                      // mov word ptr D1, ax
    ax = *(word *)&g_memByte[515628];       // mov ax, playingPenalties
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_no_ball_after_touch;         // jnz short @@no_ball_after_touch

    ax = *(word *)&g_memByte[515580];       // mov ax, penalty
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_no_ball_after_touch;         // jnz short @@no_ball_after_touch

    ax = *(word *)&g_memByte[519130];       // mov ax, AI_rand
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 1;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 1
    if (flags.zero)
        goto l_no_ball_after_touch;         // jz short @@no_ball_after_touch

    ax = (word)readMemory(esi + 136, 2);    // mov ax, [esi+TeamGeneralInfo.AI_ballSpinDirection]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.sign)
        goto l_apply_left_spin;             // js short @@apply_left_spin

    if (!flags.zero)
        goto l_apply_right_spin;            // jnz short @@apply_right_spin

l_no_ball_after_touch:;
    esi = A6;                               // mov esi, A6
    {
        word src = (word)readMemory(esi + 134, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.AI_afterTouchStrength], 1
    if (!flags.zero)
        goto l_do_long_kick;                // jnz short @@do_long_kick

    writeMemory(esi + 44, 2, -1);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], -1
    return;                                 // retn

    ax = (word)readMemory(esi + 56, 2);     // mov ax, [esi+TeamGeneralInfo.controlledPlDirection]
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // retn

l_do_long_kick:;
    A0 = 519150;                            // mov A0, offset AI_longKickTable
    goto l_set_new_direction_for_kick_pass; // jmp short @@set_new_direction_for_kick_pass

l_apply_left_spin:;
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        *(word *)&D0 = res;
    }                                       // sub word ptr D0, 1
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D0, 7
    A0 = 519138;                            // mov A0, offset AI_leftSpinTable
    goto l_set_new_direction_for_kick_pass; // jmp short @@set_new_direction_for_kick_pass

l_apply_right_spin:;
    ax = D1;                                // mov ax, word ptr D1
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 1;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 1
    {
        word res = *(word *)&D0 & 7;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 7
    A0 = 519144;                            // mov A0, offset AI_rotateRightTable

l_set_new_direction_for_kick_pass:;
    esi = A6;                               // mov esi, A6
    ax = (word)readMemory(esi + 134, 2);    // mov ax, [esi+TeamGeneralInfo.AI_afterTouchStrength]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 1;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 1
    esi = A0;                               // mov esi, A0
    ebx = *(word *)&D0;                     // movzx ebx, word ptr D0
    ax = (word)readMemory(esi + ebx, 2);    // mov ax, [esi+ebx]
    {
        int16_t dstSigned = *(word *)&D1;
        int16_t srcSigned = ax;
        word res = dstSigned + srcSigned;
        *(word *)&D1 = res;
    }                                       // add word ptr D1, ax
    {
        word res = *(word *)&D1 & 7;
        *(word *)&D1 = res;
        flags.carry = false;
        flags.overflow = false;
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // and word ptr D1, 7
    ax = D1;                                // mov ax, word ptr D1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
}

// in:
//      A1 -> player controlling the ball
//      A6 -> team general info
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void AI_Kick()
{
    ax = *(word *)&g_memByte[448851];       // mov ax, deadVarAlways0
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        goto cseg_85B62;                    // jz short cseg_85B62

    eax = *(dword *)&g_memByte[448877];     // mov eax, dseg_1309C1
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, eax
    if (flags.zero)
        return;                             // jz @@out

    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    eax = *(dword *)&g_memByte[448877];     // mov eax, dseg_1309C1
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = eax;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, eax
    if (flags.zero)
        return;                             // jz @@out

cseg_85B62:;
    ax = *(word *)&g_memByte[323626];       // mov ax, currentGameTick
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 & 6;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 6
    esi = A1;                               // mov esi, A1
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 200;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 200
    if (!flags.carry && !flags.zero)
        return;                             // ja @@out

    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+TeamGeneralInfo.allowedDirections]
    *(word *)&D7 = ax;                      // mov word ptr D7, ax
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 40, 2);     // mov ax, [esi+TeamGeneralInfo.playerHasBall]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        return;                             // jz @@out

    eax = readMemory(esi + 32, 4);          // mov eax, [esi+TeamGeneralInfo.controlledPlayer]
    A0 = eax;                               // mov A0, eax
    {
        int32_t dstSigned = A0;
        int32_t srcSigned = 0;
        dword res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A0, 0
    if (flags.zero)
        return;                             // jz short @@out

    esi = A0;                               // mov esi, A0
    ax = (word)readMemory(esi + 42, 2);     // mov ax, [esi+TeamGeneralInfo.allowedDirections]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word res = *(word *)&D0 << 5;
        *(word *)&D0 = res;
    }                                       // shl word ptr D0, 5
    esi = A1;                               // mov esi, A1
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
        int8_t srcSigned = -32;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, 224
    if (flags.sign != flags.overflow)
        goto l_kick;                        // jl short @@kick

    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = 32;
        byte res = dstSigned - srcSigned;
        flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0;
        flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned);
        flags.sign = (res & 0x80) != 0;
        flags.zero = res == 0;
    }                                       // cmp byte ptr D0, 32
    if (flags.zero || flags.sign != flags.overflow)
        return;                             // jle short @@out

l_kick:;
    ax = D7;                                // mov ax, word ptr D7
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    writeMemory(esi + 50, 1, 1);            // mov [esi+TeamGeneralInfo.firePressed], 1
    writeMemory(esi + 51, 1, 1);            // mov [esi+TeamGeneralInfo.fireThisFrame], 1
    writeMemory(esi + 49, 1, 1);            // mov [esi+TeamGeneralInfo.normalFire], 1
}

// in:
//     A6 -> team data
//
// Set the direction toward the opposing goal and the ball.
// Limits for ball x are less than 300, 300...371 and greater than 371.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void AI_SetDirectionTowardOpponentsGoal()
{
    ax = *(word *)&g_memByte[519156];       // mov ax, AI_counter
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (flags.zero)
        return;                             // jz @@out

    ax = *(word *)&g_memByte[328524];       // mov ax, word ptr ballSprite.x+2
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        word src = *(word *)&g_memByte[519158];
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp AI_attackHalf, 1
    if (flags.zero)
        goto l_attacking_top;               // jz short @@attacking_top

    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (!flags.zero)
        return;                             // jnz @@out

    *(word *)&D1 = 3;                       // mov word ptr D1, 3
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 300;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 300
    if (flags.carry)
        goto l_set_allowed_direction;       // jb short @@set_allowed_direction

    *(word *)&D1 = 5;                       // mov word ptr D1, 5
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 371;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 371
    if (!flags.carry && !flags.zero)
        goto l_set_allowed_direction;       // ja short @@set_allowed_direction

    *(word *)&D1 = 4;                       // mov word ptr D1, 4
    goto l_set_allowed_direction;           // jmp short @@set_allowed_direction

l_attacking_top:;
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515420;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset bottomTeamData
    if (!flags.zero)
        return;                             // jnz short @@out

    *(word *)&D1 = 1;                       // mov word ptr D1, 1
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 300;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 300
    if (flags.carry)
        goto l_set_allowed_direction;       // jb short @@set_allowed_direction

    *(word *)&D1 = 7;                       // mov word ptr D1, 7
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 371;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, 371
    if (!flags.carry && !flags.zero)
        goto l_set_allowed_direction;       // ja short @@set_allowed_direction

    *(word *)&D1 = 0;                       // mov word ptr D1, 0

l_set_allowed_direction:;
    ax = D1;                                // mov ax, word ptr D1
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    return;                                 // jmp short return

l_out:;
    return;                                 // retn
}

// in:
//      D7 - controlled player direction
//      A5 -> controlled player sprite
//      A6 -> team general data
// out:
//      D0, zero flag - 0/set: firing, !0/clear: not gonna fire
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void AI_DecideWhetherToTriggerFire()
{
    esi = A5;                               // mov esi, A5
    {
        word src = (word)readMemory(esi + 2, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.playerOrdinal], 1
    if (flags.zero)
        goto l_no_fire;                     // jz @@no_fire

    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (!flags.zero)
        goto l_we_are_top;                  // jnz short @@we_are_top

    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 3;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 3
    if (flags.zero)
        goto l_facing_toward_opponents_goal; // jz short @@facing_toward_opponents_goal

    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 4;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 4
    if (flags.zero)
        goto l_facing_toward_opponents_goal; // jz short @@facing_toward_opponents_goal

    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 5;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 5
    if (flags.zero)
        goto l_facing_toward_opponents_goal; // jz short @@facing_toward_opponents_goal

    goto l_no_fire;                         // jmp @@no_fire

l_we_are_top:;
    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 7;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 7
    if (flags.zero)
        goto l_facing_toward_opponents_goal; // jz short @@facing_toward_opponents_goal

    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 0;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 0
    if (flags.zero)
        goto l_facing_toward_opponents_goal; // jz short @@facing_toward_opponents_goal

    {
        int16_t dstSigned = *(word *)&D7;
        int16_t srcSigned = 1;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D7, 1
    if (!flags.zero)
        goto l_no_fire;                     // jnz @@no_fire

l_facing_toward_opponents_goal:;
    esi = A5;                               // mov esi, A5
    {
        dword src = readMemory(esi + 74, 4);
        int32_t dstSigned = src;
        int32_t srcSigned = 648;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+Sprite.ballDistance], 648
    if (!flags.carry && !flags.zero)
        goto l_no_fire;                     // ja @@no_fire

    eax = *(dword *)&g_memByte[328546];     // mov eax, ballSprite.deltaZ
    flags.carry = false;
    flags.sign = (eax & 0x80000000) != 0;
    flags.zero = eax == 0;                  // or eax, eax
    if (flags.sign)
        goto l_ball_falling;                // js short @@ball_falling

    {
        word src = *(word *)&g_memByte[328532];
        int16_t dstSigned = src;
        int16_t srcSigned = 8;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.z+2, 8
    if (flags.carry)
        goto l_no_fire;                     // jb @@no_fire

    {
        word src = *(word *)&g_memByte[328532];
        int16_t dstSigned = src;
        int16_t srcSigned = 14;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.z+2, 14
    if (!flags.carry && !flags.zero)
        goto l_no_fire;                     // ja @@no_fire

    goto l_trigger_joypad;                  // jmp short @@trigger_joypad

l_ball_falling:;
    {
        word src = *(word *)&g_memByte[328532];
        int16_t dstSigned = src;
        int16_t srcSigned = 12;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.z+2, 12
    if (flags.carry)
        goto l_no_fire;                     // jb @@no_fire

    {
        word src = *(word *)&g_memByte[328532];
        int16_t dstSigned = src;
        int16_t srcSigned = 20;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr ballSprite.z+2, 20
    if (!flags.carry && !flags.zero)
        goto l_no_fire;                     // ja @@no_fire

l_trigger_joypad:;
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 51, 1, 1);            // mov [esi+TeamGeneralInfo.fireThisFrame], 1
    esi = A5;                               // mov esi, A5
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
    {
        word res = *(word *)&D0 >> 5;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 5
    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    esi = A5;                               // mov esi, A5
    ax = (word)readMemory(esi + 82, 2);     // mov ax, [esi+Sprite.fullDirection]
    *(word *)&D0 = ax;                      // mov word ptr D0, ax
    {
        int8_t dstSigned = *(byte *)&D0;
        int8_t srcSigned = -128;
        byte res = dstSigned + srcSigned;
        *(byte *)&D0 = res;
    }                                       // add byte ptr D0, 128
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = 16;
        word res = dstSigned + srcSigned;
        *(word *)&D0 = res;
    }                                       // add word ptr D0, 16
    {
        word res = *(word *)&D0 & 255;
        *(word *)&D0 = res;
    }                                       // and word ptr D0, 0FFh
    {
        word res = *(word *)&D0 >> 5;
        *(word *)&D0 = res;
    }                                       // shr word ptr D0, 5
    ax = D7;                                // mov ax, word ptr D7
    {
        int16_t dstSigned = *(word *)&D0;
        int16_t srcSigned = ax;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp word ptr D0, ax
    if (!flags.zero)
        goto l_no_fire;                     // jnz short @@no_fire

    ax = D0;                                // mov ax, word ptr D0
    esi = A6;                               // mov esi, A6
    writeMemory(esi + 44, 2, ax);           // mov [esi+TeamGeneralInfo.currentAllowedDirection], ax
    *(word *)&g_memByte[519156] = 15;       // mov AI_counter, 15
    *(word *)&g_memByte[519160] = ax;       // mov AI_counterWriteOnly, ax
    *(word *)&g_memByte[519158] = 2;        // mov AI_attackHalf, 2
    {
        int32_t dstSigned = A6;
        int32_t srcSigned = 515272;
        dword res = dstSigned - srcSigned;
        flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned);
        flags.sign = (res & 0x80000000) != 0;
        flags.zero = res == 0;
    }                                       // cmp A6, offset topTeamData
    if (flags.zero)
        goto l_out_fire;                    // jz short @@out_fire

    *(word *)&g_memByte[519158] = 1;        // mov AI_attackHalf, 1

l_out_fire:;
    *(word *)&D0 = 0;                       // mov word ptr D0, 0
    ax ^= ax;
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // xor ax, ax
    return;                                 // retn

l_no_fire:;
    *(word *)&D0 = 1;                       // mov word ptr D0, 1
    ax = 1;                                 // mov ax, 1
    flags.carry = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
}

// in:
//      A6 -> team (general)
// out:
//      A0 -> opponents team
//      carry flag = pass/kick time == 13
//
// Only used by AI.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void AI_ResumeGameDelay()
{
    esi = A6;                               // mov esi, A6
    eax = readMemory(esi, 4);               // mov eax, [esi+TeamGeneralInfo.opponentsTeam]
    A0 = eax;                               // mov A0, eax
    {
        word src = (word)readMemory(esi + 102, 2);
        int16_t dstSigned = src;
        int16_t srcSigned = 13;
        word res = dstSigned - srcSigned;
        flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned);
        flags.sign = (res & 0x8000) != 0;
        flags.zero = res == 0;
    }                                       // cmp [esi+TeamGeneralInfo.passKickTimer], 13
}

// in:
//      D0 -  full direction player must be facing (+/- 16)
//      A6 -> team (general)
// out:
//      D2 - ball distance
//      A0 -> player
//
// Find player closest to ball facing specified direction. Search both teams.
//
// MECHANICALLY CONVERTED: This function was generated from SWOS assembly by ida2asm.
// It is only a temporary implementation and should eventually be replaced with idiomatic C++.
static void findClosestPlayerToBallFacing()
{
    A0 = -1;                                // mov A0, -1
    D2 = -1;                                // mov D2, -1
    *(word *)&D3 = 1;                       // mov word ptr D3, 1
    *(word *)&D4 = 10;                      // mov word ptr D4, 10
    A3 = 515272;                            // mov A3, offset topTeamData
    esi = A3;                               // mov esi, A3
    eax = readMemory(esi + 20, 4);          // mov eax, [esi+TeamGeneralInfo.spritesTable]
    A2 = eax;                               // mov A2, eax

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
        goto l_next_player;                 // jz short @@next_player

    esi = A1;                               // mov esi, A1
    ax = (word)readMemory(esi + 108, 2);    // mov ax, [esi+Sprite.sentAway]
    flags.carry = false;
    flags.overflow = false;
    flags.sign = (ax & 0x8000) != 0;
    flags.zero = ax == 0;                   // or ax, ax
    if (!flags.zero)
        goto l_next_player;                 // jnz short @@next_player

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
        goto l_next_player;                 // jnz short @@next_player

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
        goto l_next_player;                 // jl short @@next_player

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
        goto l_next_player;                 // jg short @@next_player

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
        goto l_next_player;                 // jnb short @@next_player

    eax = D1;                               // mov eax, D1
    D2 = eax;                               // mov D2, eax
    eax = A1;                               // mov eax, A1
    A0 = eax;                               // mov A0, eax

l_next_player:;
    (*(int16_t *)&D4)--;
    flags.overflow = (int16_t)(*(int16_t *)&D4) == INT16_MIN;
    flags.sign = (*(int16_t *)&D4 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D4 == 0;      // dec word ptr D4
    if (!flags.sign)
        goto l_players_loop;                // jns @@players_loop

    *(word *)&D4 = 10;                      // mov word ptr D4, 10
    A3 = 515420;                            // mov A3, offset bottomTeamData
    esi = A3;                               // mov esi, A3
    eax = readMemory(esi + 20, 4);          // mov eax, [esi+TeamGeneralInfo.spritesTable]
    A2 = eax;                               // mov A2, eax
    (*(int16_t *)&D3)--;
    flags.overflow = (int16_t)(*(int16_t *)&D3) == INT16_MIN;
    flags.sign = (*(int16_t *)&D3 & 0x8000) != 0;
    flags.zero = *(int16_t *)&D3 == 0;      // dec word ptr D3
    if (!flags.sign)
        goto l_players_loop;                // jns @@players_loop
}
