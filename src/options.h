#pragma once

void loadOptions();
void saveOptions();
std::vector<LogItem> parseCommandLine(int argc, char **argv);
void normalizeOptions();
bool disableIntro();
bool disableImageReels();
int midiBankNumber();
