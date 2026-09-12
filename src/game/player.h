#pragma once

void updatePlayerSpeedAndFrameDelay(const TeamGeneralInfo& team, Sprite& player);
void updatePlayerWithBall(Sprite& player);
void updateBallWithControllingPlayer(const Sprite& player);
void updateBallWithControllingGoalkeeper(const Sprite& player);
void calculateIfPlayerWinsBall(TeamGeneralInfo& team, Sprite& player, Direction direction);
void playerKickingBall(TeamGeneralInfo& team, const Sprite& player);
void playerHittingStaticHeader(TeamGeneralInfo& team, Sprite& player);
void playerHittingJumpHeader(TeamGeneralInfo& team, Sprite& player);
void playerTackledTheBallStrong(TeamGeneralInfo& team, Sprite& player);
void playerTackledTheBallWeak(TeamGeneralInfo& team, Sprite& player);
void goalkeeperClaimedTheBall(TeamGeneralInfo& team, Sprite& goalKeeper, Sprite& ballSprite);
void goalkeeperDeflectedBall(const TeamGeneralInfo& team, Sprite& ballSprite);
void doPass(TeamGeneralInfo& team, const Sprite& passingPlayer);
void setPlayerDowntimeAfterTackle(const TeamGeneralInfo& team, Sprite& player);
void setJumpHeaderHitAnimTable(Sprite& player);
const PlayerInfo& getPlayerPointerFromShirtNumber(const TeamGeneralInfo& team, const Sprite& player);
