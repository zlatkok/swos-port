#pragma once

enum class GoalType {
    kRegular,
    kPenalty,
    kOwnGoal,
};

void resetResult(const char *team1Name, const char *team2Name);
void updateResult();
void hideResult();
void drawResult();
void registerScorer(const Sprite& scorer, int teamNum, GoalType goalType);
