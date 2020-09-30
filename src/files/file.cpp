#include "file.h"
#include "file.h"
#include "log.h"
#include "util.h"
#include <dirent.h>
#include <direct.h>

static std::string m_rootDir;

// Opens a file with consideration to SWOS root directory.
FILE *openFile(const char *path, const char *mode /* = "rb" */)
{
    auto f = fopen((m_rootDir + path).c_str(), mode);
    if (!f)
        logWarn("Failed to open file %s with mode \"%s\"", path, mode);

    return f;
}

// Returns the size of the given file.
int getFileSize(const char *path, bool required /* = true */)
{
    struct stat st;

    if (stat((m_rootDir + path).c_str(), &st) != 0) {
        if (required)
            errorExit("Failed to stat file %s", path);

        return -1;
    }

    return st.st_size;
}

// Internal routine that does all the work.
static int doLoadFile(const char *path, void *buffer, size_t bufferSize, size_t skipBytes, bool required)
{
    auto f = openFile(path);
    if (!f) {
        if (required)
            errorExit("Could not open %s for reading", path);

        return -1;
    }

    if (fseek(f, skipBytes, SEEK_SET)) {
        fclose(f);
        return -1;
    }

    setbuf(f, nullptr);
    bool plural = bufferSize != 1;
    logInfo("Loading `%s' [%s byte%s]", path, formatNumberWithCommas(bufferSize).c_str(), plural ? "s" : "");

    bool readOk = fread(buffer, bufferSize, 1, f) == 1;
    fclose(f);

    if (!readOk) {
        if (required)
            errorExit("Error reading file %s", path);

        return -1;
    }

    return bufferSize;
}

int loadFile(const char *path, void *buffer, int maxSize /* = -1 */, size_t skipBytes /* = 0 */, bool required /* = true */)
{
    if (maxSize < 0) {
        maxSize = getFileSize(path, required);
        if (maxSize < 0)
            return -1;
    }

    auto volatile savedSelTeamsPtr = swos.selTeamsPtr;  // why should a load file routine do this?!?!
    auto result = doLoadFile(path, buffer, maxSize, skipBytes, required);
    swos.selTeamsPtr = savedSelTeamsPtr;

    return result;
}

// loadFile
//
// in:
//      path         - path of the file to load
//      bufferOffset - number of additional bytes at the start of the buffer that will remain uninitialized
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
    auto size = getFileSize(path, false);
    if (size < 0)
        return {};

    auto buffer = new char[size + bufferOffset];
    auto dest = buffer + bufferOffset;

    if (doLoadFile(path, dest, size, skipBytes, false) != size)
        delete[] buffer;

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
__declspec(naked) int SWOS::LoadFile()
{
#ifdef SWOS_VM
    loadFileImp();
    D0 = 0;
    SwosVM::flags.zero = true;
    return 0;
#else
    __asm {
        call loadFileImp
        xor  eax, eax
        retn
    }
#endif
}

bool saveFile(const char *path, void *buffer, size_t size)
{
    logInfo("Writing `%s' [%s bytes]", path, formatNumberWithCommas(size).c_str());

    auto f = openFile(path, "wb");
    bool ok = f && fwrite(buffer, 1, size, f) == size;

    if (f)
        fclose(f);

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
__declspec(naked) int SWOS::WriteFile()
{
#ifdef SWOS_VM
    auto result = writeFileImp();
    SwosVM::flags.zero = result != 0;
    D0 = !result;
    return 0;
#else
    __asm {
        call writeFileImp
        call setZeroFlagAndD0FromAl
        retn
    }
#endif
}

void setRootDir(const char *dir)
{
    m_rootDir = dir;
    if (m_rootDir.back() != '\\')
        m_rootDir += '\\';
}

std::string rootDir()
{
    return m_rootDir;
}

std::string pathInRootDir(const char *filename)
{
    return m_rootDir.empty() ? filename : m_rootDir + filename;
}

char getDirSeparator()
{
    return '\\';
}

bool isFileSystemCaseInsensitive()
{
    return true;
}

std::string joinPaths(const char *path1, const char *path2)
{
    std::string result = path1;
    auto dirSeparator = getDirSeparator();

    if (!result.empty() && result.back() != dirSeparator)
        result += dirSeparator;

    return result + path2;
}

bool dirExists(const char *path)
{
#ifdef _WIN32
    auto fileAttributes = ::GetFileAttributes(path);
    return fileAttributes != INVALID_FILE_ATTRIBUTES && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
# error Please implement platform specific dirExists()
#endif
}

bool createDir(const char *path)
{
    bool result = _mkdir(path) == 0;

    if (!result) {
        struct stat st;
        result = stat(path, &st) == 0 && st.st_mode & _S_IFDIR;
    }

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

static bool isAllowedExtension(const char *ext, const char **allowedExtensions, size_t numAllowedExtensions)
{
    if (!allowedExtensions)
        return true;

    while (numAllowedExtensions--) {
        if (!_stricmp(ext, *allowedExtensions++))
            return true;
    }

    return false;
}

FoundFileList findFiles(const char *extension, const char *dirName /* = nullptr */,
    const char **allowedExtensions /* = nullptr */, size_t numAllowedExtensions /* = 0 */)
{
    assert(extension && (extension[0] == '\0' || extension[0] == '.'));

    FoundFileList result;

    logInfo("Searching for files, extension: %s", extension && *extension ? extension : "*.*");

    auto dirPath = pathInRootDir(dirName ? dirName : ".");
    auto dir = opendir(dirPath.c_str());
    if (!dir)
        sdlErrorExit("Couldn't open %s directory", dirName ? dirName : "SWOS root");

    bool acceptAll = extension && (extension[0] == '\0' || extension[1] == '*');

    for (dirent *entry; entry = readdir(dir); ) {
        if (!entry->d_namlen)
            continue;

        auto dot = entry->d_name + entry->d_namlen - 1;
        while (dot >= entry->d_name && *dot != '.')
            dot--;

        if (dot < entry->d_name)
            continue;

        if (!acceptAll) {
            if (entry->d_namlen < 4)
                continue;

            bool match = true;

            int i = 0;
            for (; i < 3; i++) {
                if (!dot[i + 1])
                    break;
                if (toupper(extension[i + 1]) != toupper(dot[i + 1])) {
                    match = false;
                    break;
                }
            }

            if (!match || i == 3 && dot[4])
                continue;
        } else {
            // skip "." and ".." entries
            if (entry->d_namlen == 1 && entry->d_name[0] == '.' ||
                entry->d_namlen == 2 && entry->d_name[0] == '.' && entry->d_name[1] == '.')
                continue;
        }

        if (!isAllowedExtension(dot + 1, allowedExtensions, numAllowedExtensions))
            continue;

        // must make it upper case or SWOS will reject it like a cruel mother
        toUpper(entry->d_name);

        result.emplace_back(entry->d_name, dot - entry->d_name + 1);
    }

    closedir(dir);

    logInfo("Found %d files", result.size());

    return result;
}
