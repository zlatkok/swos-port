#pragma once

void showStadiumMenu(const TeamGame *team1, const TeamGame *team2, int maxSubstitutes, std::function<void()> callback);
void loadStadiumSprites(const TeamGame *team1, const TeamGame *team2);
