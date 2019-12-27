#include "Util.h"

static int xfseek(FILE *file, long int offset, int origin)
{
    auto result = fseek(file, offset, origin);

    if (result)
        Util::exit("Error seeking, offset = %lu, origin = %d\n", 1, offset, origin);

    return result;
}

void Util::exit(const char *format, int exitCode /* = EXIT_FAILURE */, ...)
{
    char buffer[32 * 1024];
    va_list args;

    va_start(args, exitCode);
    vsprintf_s(buffer, format, args);
    va_end(args);

    buffer[sizeof(buffer) - 1] = '\0';
    puts(buffer);

    std::exit(exitCode);
}

const char *Util::getFilename(const char *path)
{
    const char *p = path + strlen(path) - 1;

    for (; p >= path; p--) {
        if (*p == '/' || *p == '\\')
            break;
    }

    return p + 1;
}

std::string Util::getBasePath(const char *path)
{
    auto filename = getFilename(path);

    if (filename == path)
        return path;
    else
        return { path, filename };
}

std::string Util::joinPaths(const std::string& path1, const char *path2)
{
    auto result = path1;

    if (result.back() != '/')
        result += '/';

    return result + path2;
}

std::pair<const char *, long> Util::loadFile(const char *path, bool forceLastNewLine)
{
    // load as binary to avoid text conversion (we're gonna parse it anyway)
    FILE *f = fopen(path, "rb");
    if (!f)
        Util::exit("Could not open file: %s\n", 1, path);

    std::setbuf(f, nullptr);

    xfseek(f, 0, SEEK_END);
    auto size = ftell(f);
    if (size < 0)
        Util::exit("Seek operation failed on file: %s\n", 1, path);
    xfseek(f, 0, SEEK_SET);

    auto data = new char[size + 2 + forceLastNewLine];

    if (fread((void *)data, size, 1, f) != 1)
        Util::exit("Error reading file: %s", 1, path);

    fclose(f);

    data[size] = '\0';

    if (forceLastNewLine && data[size - 1] != '\n') {
        data[size] = '\r';
        data[size + 1] = '\n';
        data[size + 2] = '\0';
    }

    return std::make_pair(data, size);
}

bool Util::endsWith(const std::string& base, const std::string& suffix)
{
    return base.size() >= suffix.size() && !base.compare(base.size() - suffix.size(), suffix.size(), suffix);
}

std::string Util::formatNumberWithCommas(int64_t num)
{
    std::string result;

    bool neg = false;
    if (num < 0) {
        neg = true;
        num = -num;
    }

    int i = 0;
    do {
        auto quoteRem = std::div(num, 10ll);
        num = quoteRem.quot;
        if (i++ == 3) {
            result += ',';
            i = 1;
        }
        result += static_cast<char>(quoteRem.rem) + static_cast<int64_t>('0');
    } while (num);

    if (neg)
        result += '-';

    std::reverse(result.begin(), result.end());

    return result;
}
