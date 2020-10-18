#pragma once

constexpr int kMaxFilenameLength = 260;

SDL_RWops *openFile(const char *path, const char *mode = "rb");
int loadFile(const char *path, void *buffer, int maxSize = -1, size_t skipBytes = 0, bool required = true);
bool saveFile(const char *path, void *buffer, size_t size);
std::pair<char *, size_t> loadFile(const char *path, size_t bufferOffset = 0, size_t skipBytes = 0);

void setRootDir(const char *dir);
std::string rootDir();
std::string pathInRootDir(const char *filename);

inline char getDirSeparator()
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

std::string joinPaths(const char *path1, const char *path2);
bool dirExists(const char *path);
bool createDir(const char *path);

const char *getFileExtension(const char *path);
const char *getBasename(const char *path);

struct FoundFile {
    std::string name;
    size_t extensionOffset;
    FoundFile(const std::string& name, size_t extensionOffset) : name(name), extensionOffset(extensionOffset) {}
};
using FoundFileList = std::vector<FoundFile>;

FoundFileList findFiles(const char *extension, const char *dirName = nullptr,
    const char **allowedExtensions = nullptr, size_t numAllowedExtensions = 0);
