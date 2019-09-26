#pragma once

constexpr int kMaxFilenameLength = 260;

struct FoundFile {
    std::string name;
    size_t extensionOffset;
    FoundFile(const std::string& name, size_t extensionOffset) : name(name), extensionOffset(extensionOffset) {}
};
using FoundFileList = std::vector<FoundFile>;

FILE *openFile(const char *path, const char *mode = "rb");
int getFileSize(const char *path, bool required = true);
int loadFile(const char *path, void *buffer, bool required = true);
std::pair<char *, size_t> loadFile(const char *path, size_t bufferOffset = 0);
void setRootDir(const char *dir);
std::string rootDir();
std::string pathInRootDir(const char *filename);
char getDirSeparator();
bool isFileSystemCaseInsensitive();
std::string joinPaths(const char *path1, const char *path2);
bool dirExists(const char *path);
const char *getFileExtension(const char *path);
const char *getBaseName(const char *path);
FoundFileList findFiles(const char *extension, const char **allowedExtensions = nullptr, size_t numAllowedExtensions = 0);
