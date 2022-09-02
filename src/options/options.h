#pragma once

void loadOptions();
void saveOptions();
std::vector<LogItem> parseCommandLine(int argc, char **argv);
void normalizeOptions();

bool showPreMatchMenus();
void setPreMatchMenus(bool enabled);
int getGameStyle();
void setGameStyle(int gameStyle);
