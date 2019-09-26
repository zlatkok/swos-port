#pragma once

#include "file.h"

std::string showSelectFilesMenu(const char *menuTitle, const FoundFileList& filenames,
    bool saving = false, char *saveFilename = nullptr);
