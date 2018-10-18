#pragma once

FILE *openFile(const char *path, const char *mode = "rb");
int loadFile(const char *filename, void *buffer, bool required = true);
void setRootDir(const char *dir);
std::string rootDir();
std::string pathInRootDir(const char *filename);
