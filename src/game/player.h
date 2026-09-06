#pragma once

void updatePlayerSpeedAndFrameDelay(const TeamGeneralInfo& team, Sprite& player);
void updatePlayerWithBall();
void updateControllingPlayer();
void updateBallWithControllingGoalkeeper(const Sprite& player);
void calculateIfPlayerWinsBall();
void playerKickingBall();
void playerHittingStaticHeader();
void playerHittingJumpHeader();
void playerTackledTheBallStrong();
void playerTackledTheBallWeak();
void goalkeeperClaimedTheBall();
void goalkeeperDeflectedBall();
void doFlyingHeader();
void doPass();
void setPlayerDowntimeAfterTackle();
void setJumpHeaderHitAnimTable(Sprite& player);
const PlayerInfo& getPlayerPointerFromShirtNumber(const TeamGeneralInfo& team, const Sprite& player);
