#include "file.h"
#include "log.h"
#include "util.h"
#include "dirent.h"

#ifdef _WIN32
# include <direct.h>
#endif

static std::string m_rootDir;

static bool isAbsolutePath(const char *path);
static void traverseDirectory(DIR *dir, const char *extension, std::function<bool(const char *, int, const char *)> f);
static bool isAnyExtensionAllowed(const char *ext, const char **allowedExtensions, size_t numAllowedExtensions);

// Opens a file with consideration to SWOS root directory.
SDL_RWops *openFile(const char *path, const char *mode /* = "rb" */)
{
#ifdef _WIN32
    auto f = SDL_RWFromFile(isAbsolutePath(path) ? path : (m_rootDir + path).c_str(), mode);
#else
    // convert every path to lower case and replace backslashes with forward slashes
    auto fullPath = isAbsolutePath(path) ? path : m_rootDir + path;
    std::transform(fullPath.begin(), fullPath.end(), fullPath.begin(), [](unsigned char c) {
        return c == '\\' ? '/' : std::tolower(c);
    });
    auto f = SDL_RWFromFile(fullPath.c_str(), mode);
#endif
    if (!f)
        logWarn("Failed to open file \"%s\" with mode \"%s\"", path, mode);

    return f;
}

static std::pair<SDL_RWops *, int> openFileAndGetSize(const char *path, bool required /* = true */)
{
    auto f = openFile(path);
    if (!f) {
        if (required)
            sdlErrorExit("Required file \"%s\" could not be opened", path);
        return { nullptr, -1 };
    }

    auto size = SDL_RWsize(f);
    if (size < 0) {
        if (required)
            sdlErrorExit("Failed to get file size for file \"%s\", received size: %lld", path, size);
        return { nullptr, -1 };
    }

    return { f, static_cast<int>(size) };
}

// Internal routine that does all the work.
static int doLoadFile(SDL_RWops *f, const char *path, void *buffer, size_t bufferSize, size_t skipBytes, bool required)
{
    assert(f);

    if (SDL_RWseek(f, skipBytes, RW_SEEK_SET) < 0) {
        if (required)
            sdlErrorExit("Error during seek operation on file \"%s\"", path);
        SDL_RWclose(f);
        return -1;
    }

#ifndef DEBUG
    bool plural = bufferSize != 1;
    logInfo("Loading \"%s\" [%s byte%s]", path, formatNumberWithCommas(bufferSize).c_str(), plural ? "s" : "");
#endif

    bool readOk = SDL_RWread(f, buffer, bufferSize, 1) == 1;
    SDL_RWclose(f);

    if (!readOk) {
        if (required)
            sdlErrorExit("Error reading file \"%s\"", path);
        return -1;
    }

    return bufferSize;
}

int loadFile(const char *path, void *buffer, int maxSize /* = -1 */, size_t skipBytes /* = 0 */, bool required /* = true */)
{
    SDL_RWops *f;
    int size;

    std::tie(f, size) = openFileAndGetSize(path, required);
    if (maxSize < 0) {
        maxSize = size;
        if (maxSize < 0)
            return -1;
    }

    auto volatile savedSelTeamsPtr = swos.selTeamsPtr;  // why should a load file routine do this?!?!
    auto result = doLoadFile(f, path, buffer, maxSize, skipBytes, required);
    swos.selTeamsPtr = savedSelTeamsPtr;

    return result;
}

// loadFile
//
// in:
//      path         - path of the file to load
//      bufferOffset - number of additional bytes at the start of the destination buffer that will remain uninitialized
//      skipBytes    - number of bytes to skip from the beginning of the file
// out:
//      pair of allocated buffer (which caller must free) and its size; nullptr/0 in case of error
//
// Loads file with given path into memory. Returns file contents in a buffer allocated from the heap together with
// its size. Can optionally extend the buffer by a given size, leaving that much initial bytes uninitialized,
// convenient for writing custom data at the start of the buffer (such as header).
//
std::pair<char *, size_t> loadFile(const char *path, size_t bufferOffset /* = 0 */, size_t skipBytes /* = 0 */)
{
    SDL_RWops *f;
    int size;
    std::tie(f, size) = openFileAndGetSize(path, false);

    if (size < 0)
        return {};

    auto buffer = new char[size + bufferOffset];
    auto dest = buffer + bufferOffset;

    if (doLoadFile(f, path, dest, size, skipBytes, false) != size) {
        delete[] buffer;
        return {};
    }

    return { buffer, size + bufferOffset };
}

static void loadFileImp()
{
    auto filename = A0.as<const char *>();
    auto buffer = A1.as<char *>();

    D0 = 0;
    D1 = loadFile(filename, buffer);
}

// LoadFile
//
// in:
//      A0 -> file name
//      A1 -> buffer
// out:
//      zero flag set - all OK
//        -||-  clear - error   [actually will never return this, so always return with zero flag set]
//      D0 contains result (0 - OK)
//      D1 returns file size
//
// Load file contents into given buffer. If the file can't be found, or some error happens,
// writes the name of the file to the console, and terminates the program.
//
int SWOS::LoadFile()
{
    loadFileImp();
    D0 = 0;
    SwosVM::flags.zero = true;
    return 0;
}

bool saveFile(const char *path, void *buffer, size_t size)
{
    logInfo("Writing `%s' [%s bytes]", path, formatNumberWithCommas(size).c_str());

    auto f = openFile(path, "wb");
    bool ok = f && SDL_RWwrite(f, buffer, 1, size) == size;

    if (f)
        SDL_RWclose(f);

    return ok;
}

static int writeFileImp()
{
    auto filename = A0.as<const char *>();
    auto buffer = A1.as<char *>();
    auto bufferSize = D1;

    return saveFile(filename, buffer, bufferSize);
}

// WriteFile
//
// in:
//      A0 -> filename
//      A1 -> buffer
//      D1 = buffer size
//
// out:
//      D0 = 0 - success
//           1 - failure
//      zero flag set - all OK
//        -||-  clear - error
//
int SWOS::WriteFile()
{
    auto result = writeFileImp();
    SwosVM::flags.zero = result != 0;
    D0 = !result;
    return 0;
}

bool renameFile(const char *oldPath, const char *newPath)
{
#ifdef __ANDROID__
    char oldPathBuf[PATH_MAX];
    char newPathBuf[PATH_MAX];

    if (*oldPath != '/') {
        snprintf(oldPathBuf, sizeof(oldPathBuf), "%s/%s", SDL_AndroidGetInternalStoragePath(), oldPath);
        oldPath = oldPathBuf;
    }
    if (*newPath != '/') {
        snprintf(newPathBuf, sizeof(newPathBuf), "%s/%s", SDL_AndroidGetInternalStoragePath(), newPath);
        newPath = newPathBuf;
    }
#endif
    std::remove(newPath);
    return std::rename(oldPath, newPath) == 0;
}

void setRootDir(const char *dir)
{
    m_rootDir = dir;
    if (m_rootDir.back() != getDirSeparator())
        m_rootDir += getDirSeparator();
}

std::string rootDir()
{
    return m_rootDir;
}

std::string pathInRootDir(const char *filename)
{
    return m_rootDir.empty() ? filename : m_rootDir + filename;
}

int pathCompare(const char *path1, const char *path2)
{
    return isFileSystemCaseSensitive() ? strcmp(path1, path2) : _stricmp(path1, path2);
}

int pathNCompare(const char *path1, const char *path2, size_t count)
{
    return isFileSystemCaseSensitive() ? strncmp(path1, path2, count) : _strnicmp(path1, path2, count);
}

std::string joinPaths(const char *path1, const char *path2)
{
    std::string result = path1;
    auto dirSeparator = getDirSeparator();

    if (!result.empty() && result.back() != dirSeparator)
        result += dirSeparator;

    return result + path2;
}

bool fileExists(const char *path)
{
#ifdef __ANDROID__
    char pathBuf[PATH_MAX];
    if (*path != '/') {
        snprintf(pathBuf, sizeof(pathBuf), "%s/%s", SDL_AndroidGetInternalStoragePath(), path);
        path = pathBuf;
    }
#endif
    struct stat statBuffer;
    return stat(path, &statBuffer) == 0;
}

bool dirExists(const char *path)
{
#ifdef _WIN32
    auto fileAttributes = ::GetFileAttributes(path);
    return fileAttributes != INVALID_FILE_ATTRIBUTES && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#elif __linux__
    struct stat st;
    return stat(path, &st) == 0 && st.st_mode & S_IFDIR;
#else
# error Please implement platform specific dirExists()
#endif
}

bool createDir(const char *path)
{
#ifdef _WIN32
    bool result = _mkdir(path) == 0;
#elif __ANDROID__
    std::string storagePath(SDL_AndroidGetInternalStoragePath());
    path = ((storagePath += getDirSeparator()) += path).c_str();
    bool result = mkdir(path, S_IRWXU) == 0;
#else
# error Please implement platform specific createDir()
#endif

    if (!result)
        return dirExists(path);

    return result;
}

// Returns pointer to last dot in the string, if found, or to the last string character otherwise.
const char *getFileExtension(const char *path)
{
    auto result = strrchr(path, '.');
    return result ? result : path;
}

const char *getBasename(const char *path)
{
    auto result = strrchr(path, getDirSeparator());
    return result ? result + 1 : path;
}

FoundFileList findFiles(const char *extension, const char *dirName /* = nullptr */,
    const char **allowedExtensions /* = nullptr */, size_t numAllowedExtensions /* = 0 */)
{
    assert(extension && (extension[0] == '\0' || extension[0] == '.'));

#ifdef __ANDROID__
    char dirBuf[PATH_MAX];
    if (!dirName || *dirName != '/') {
        snprintf(dirBuf, sizeof(dirBuf), "%s/%s", SDL_AndroidGetInternalStoragePath(), dirName ? dirName : ".");
        dirName = dirBuf;
    }
#endif

    FoundFileList result;

    logInfo("Searching for files, extension: %s", extension && *extension ? extension : "*.*");

    auto dirPath = pathInRootDir(dirName ? dirName : ".");
    auto dir = opendir(dirPath.c_str());
    if (!dir)
        sdlErrorExit("Couldn't open %s directory", dirName ? dirName : "SWOS root");

    auto acceptAll = false;
    if (extension) {
        if (extension[0] == '.')
            extension++;
        acceptAll = extension[0] == '\0' || extension[1] == '*';
    }
    if (acceptAll)
        extension = nullptr;

    traverseDirectory(dir, extension, [&](const char *filename, int, const char *dot) {
        size_t extensionOffset = dot - filename + 1;
        if (isAnyExtensionAllowed(dot + 1, allowedExtensions, numAllowedExtensions))
            result.emplace_back(filename, extensionOffset);
        return true;
    });

    logInfo("Found %d files", result.size());

    return result;
}

void traverseDirectory(const char *directory, const char *extension,
    std::function<bool(const char *, int, const char *)> f)
{
    if (auto dir = opendir(directory))
        traverseDirectory(dir, extension, f);
}

static bool isAbsolutePath(const char *path)
{
#ifdef _WIN32
    return isalpha(path[0]) && path[1] == ':' && path[2] == getDirSeparator();
#else
    return path[0] == getDirSeparator();
#endif
}

// Traverses a given directory and invokes callback function for every encountered file/directory.
// If an extension is given callback will only be invoked for the files with matching extension.
// Extension is not expected to start with a dot, but if it does, it will be ignored.
static void traverseDirectory(DIR *dir, const char *extension,
    std::function<bool(const char *, int, const char *)> f)
{
    assert(dir);

    if (extension && *extension == '.')
        extension++;

    for (dirent *entry; entry = readdir(dir); ) {
#ifdef _WIN32
        auto len = entry->d_namlen;
#else
        auto len = strlen(entry->d_name);
#endif
        // skip '.' and '..' entries
        if (!len || entry->d_name[0] == '.' && (len == 1 || len == 2 && entry->d_name[1] == '.'))
            continue;

        auto dot = entry->d_name + len - 1;
        while (dot >= entry->d_name && *dot != '.')
            dot--;
        // if no extension set up dot to be empty string for comparison (to match empty extension string)
        if (*dot != '.')
            dot = entry->d_name + len - 1;

        if ((!extension || !pathCompare(extension, dot + 1)) && !f(entry->d_name, len, dot))
            break;
    }
    closedir(dir);
}

static bool isAnyExtensionAllowed(const char *ext, const char **allowedExtensions, size_t numAllowedExtensions)
{
    if (!allowedExtensions)
        return true;

    while (numAllowedExtensions--) {
        if (!_stricmp(ext, *allowedExtensions++))
            return true;
    }

    return false;
}
