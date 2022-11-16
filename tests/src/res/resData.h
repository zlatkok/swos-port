#pragma once

using ResFilenameList = std::vector<std::string>;
ResFilenameList findResFiles(const char *extension);
SDL_RWops *openResFile(const char *path);
std::pair<std::unique_ptr<char[]>, size_t> loadResFile(const char *name);
