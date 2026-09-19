#pragma once

constexpr int kEndOfHalfResultTimer = 30'000;

enum class GoalType {
    kRegular,
    kPenalty,
    kOwnGoal,
};

void resetResult(const char *team1Name, const char *team2Name);
void updateResult();
void hideResult();
void drawResult();
int clearResultInterval();
int clearResultHalftimeInterval();
void setClearResultInterval(int interval);
void setClearResultHalftimeInterval(int interval);
void registerScorer(const Sprite& scorer, int teamNum, GoalType goalType);
void goalScored(int teamNum, Sprite& scorer);
