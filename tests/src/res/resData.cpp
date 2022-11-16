#include "resData.h"
#include "file.h"
#include "unitTest.h"
#include "zlib-ng.h"

static constexpr char kArchiveExtension[] = ".gz";
static constexpr int kArchiveExtensionLen = sizeof(kArchiveExtension) - 1;

struct FilenameCompare {
    bool operator()(const std::string& a, const std::string& b) const {
        return !pathCompare(a.c_str(), b.c_str());
    }
};

using FileSet = std::unordered_set<std::string, std::hash<std::string>, FilenameCompare>;

ResFilenameList findResFiles(const char *extension)
{
    FileSet files;

    traverseDirectory(TEST_DATA_DIR, extension, [&files](const char *filename, auto, auto) {
        files.insert(joinPaths(TEST_DATA_DIR, filename));
        return true;
    });

    int extLen = 0;
    if (extension) {
        if (*extension == '.')
            extension++;
        extLen = strlen(extension);
    }

    traverseDirectory(TEST_DATA_DIR, kArchiveExtension, [&](const char *filename, int len, auto) {
        if (len >= extLen + 4 && !pathNCompare(filename + len - kArchiveExtensionLen - extLen, extension, extLen)) {
            const auto& path = joinPaths(TEST_DATA_DIR, filename);
            if (files.find(path.substr(0, path.size() - kArchiveExtensionLen)) == files.end())
                files.insert(path);
        }
        return true;
    });

    return { files.begin(), files.end() };
}

SDL_RWops *openResFile(const char *path)
{
    int len = strlen(path);
    if (len > kArchiveExtensionLen && !pathCompare(&path[len - kArchiveExtensionLen], kArchiveExtension)) {
        auto context = SDL_AllocRW();
        if (context) {
            auto handle = zng_gzopen(path, "rb");
            assertMessage(handle, "Failed to open GZIP file "s + path);
            context->hidden.unknown.data1 = handle;
            context->size = [](SDL_RWops *) { return -1ll; };
            context->seek = [](SDL_RWops *, Sint64, int) { return -1ll; };
            context->read = [](SDL_RWops *context, void *buf, size_t size, size_t num) {
                auto handle = reinterpret_cast<gzFile>(context->hidden.unknown.data1);
                auto readItems = zng_gzfread(buf, size, num, handle);
                assertMessage(readItems == num, "Failed to read GZIP file "s + std::to_string(reinterpret_cast<intptr_t>(handle)) +
                    ", " + std::to_string(num) + " item(s), size " + std::to_string(size));
                return readItems;
            };
            context->close = [](SDL_RWops *context) {
                auto handle = reinterpret_cast<gzFile>(context->hidden.unknown.data1);
                assertMessage(zng_gzclose(handle) == Z_OK, "Failed to close GZIP file "s +
                    std::to_string(reinterpret_cast<intptr_t>(handle)));
                SDL_FreeRW(context);
                return 0;
            };
            context->write = [](SDL_RWops *, const void *, size_t, size_t) { return 0ull; };
            context->type = 0x45543377;
        }
        return context;
    } else {
        return SDL_RWFromFile(path, "rb");
    }
}

std::pair<std::unique_ptr<char[]>, size_t> loadResFile(const char *name)
{
    const auto& path = joinPaths(TEST_DATA_DIR, name);
    auto failedToMessage = [&](const char *what) {
        return "Failed to "s + what + " resource file " + name + " at path " + path;
    };
    auto handle = openResFile(path.c_str());
    assertMessage(handle, failedToMessage("open"));
    size_t size = SDL_RWsize(handle);
    assertMessage(size > 0, failedToMessage("get size of"));
    auto buffer = std::make_unique<char[]>(size);
    assertMessage(SDL_RWread(handle, buffer.get(), size, 1) == 1, failedToMessage("read"));
    return { std::move(buffer), size };
}
