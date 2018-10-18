#include <vector>
#include <iostream>
#include <utility>
#include <iostream>
#include <chrono>
#include <cassert>

template<typename F, typename... Args>
static int64_t measure(F func, Args&&... args)
{
    using namespace std::chrono;

    auto t1 = high_resolution_clock::now();
    func(std::forward<Args>(args)...);
    auto t2 = high_resolution_clock::now();
    return duration_cast<milliseconds>(t2 - t1).count();
}

std::pair<const char *, long> loadFile(const char *path, bool forceLastNewLine)
{
    // load as binary to avoid text conversion (we're gonna parse it anyway)
    FILE *f = fopen(path, "rb");

    fseek(f, 0, SEEK_END);
    auto size = ftell(f);
    fseek(f, 0, SEEK_SET);

    auto data = new char[size + 1 + forceLastNewLine];

    fread((void *)data, size, 1, f);

    fclose(f);

    data[size] = '\0';

    if (forceLastNewLine && data[size - 1] != '\n') {
        data[size] = '\n';
        data[size + 1] = '\0';
    }

    return std::make_pair(data, size);
}

enum Type { T_ID, T_HEX, T_DEC, T_BIN };

static Type types1[1'000'000];
static Type types2[1'000'000];
static int tIndex1;
static int tIndex2;

void testIf(const char *p)
{
    tIndex1 = 0;

    while (true) {
        while (*p > 0 && isspace(*p))
            p++;

        if (!*p)
            break;

        bool isHex = true;
        bool isDec = true;
        bool isBin = true;

        while (true) {
            if (p[1] >= 0 && isspace(p[1]))
                break;
            if (*p != '0' && *p != '1') {
                isBin = false;
                if (*p < '2' || *p > '9') {
                    isDec = false;
                    if ((*p < 'a' || *p > 'f') && (*p < 'A' || *p > 'F')) {
                        isHex = false;
                    }
                }
            }
            p++;
        }

        if (isHex && (*p == 'h' || *p == 'H'))
            types1[tIndex1] = T_HEX;
        else if (isDec && *p >= '0' && *p <= '9')
            types1[tIndex1] = T_DEC;
        else if (isBin && (*p == 'b' || *p == 'B'))
            types1[tIndex1] = T_BIN;
        else
            types1[tIndex1] = T_ID;

        tIndex1++;
        p++;
    }
}

void testSwitch(const char *p)
{
    tIndex2 = 0;

    while (true) {
        while (*p > 0 && isspace(*p))
            p++;

        if (!*p)
            break;

        int len = 0;
        for (auto z = p; *z < 0 || !isspace(*z); z++)
            len++;

        assert(len);

        bool isHex = true;
        bool isDec = true;
        bool isBin = true;

        for (int i = 0; i < len - 1; i++) {
            switch (p[i]) {
            case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13:
            case 14: case 15: case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25: case 26:
            case 27: case 28: case 29: case 30: case 31: case 32:
                goto done;
            case '0':
            case '1':
                break;
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                isBin = false;
                break;
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                isBin = false;
                isDec = false;
                break;
            default:
                isBin = isDec = isHex = false;
            }
        }
done:

        switch (p[len - 1]) {
        case 'h':
        case 'H':
            types2[tIndex2] = isHex ? T_HEX : T_ID;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            types2[tIndex2] = isDec ? T_DEC : T_ID;
            break;
        case 'b':
        case 'B':
            types2[tIndex2] = isBin ? T_BIN : T_ID;
            break;
        default:
            types2[tIndex2] = T_ID;
        }

        assert(types1[tIndex2] == types2[tIndex2]);
        tIndex2++;
        p += len;
    }
}

constexpr int kNumLoops = 5;

void loopTestIf(const char *p)
{
    for (int i = 0; i < kNumLoops; i++)
        testIf(p);
}

void loopTestSwitch(const char *p)
{
    for (int i = 0; i < kNumLoops; i++)
        testSwitch(p);
}

int spaces1, nonSpaces1;
int spaces2, nonSpaces2;

void testIsspace(const char *p)
{
    for (int i = 0; i < kNumLoops * 2; i++) {
        while (*p) {
            if (*p > 0 && isspace(*p))
                spaces1++;
            else
                nonSpaces1++;
            p++;
        }
    }
}

bool myisspace(char p)
{
    // slightly faster without these 2 tests
    return p == ' ' || /*p == '\f' ||*/ p == '\n' || p == '\r' || p == '\t'/* || p == '\v'*/;
//    return p == ' ' || p == '\f' || p == '\n' || p == '\r' || p == '\t' || p == '\v';
}

void testMyIsspace(const char *p)
{
    for (int i = 0; i < kNumLoops * 2; i++) {
        while (*p) {
            if (myisspace(*p))
                spaces2++;
            else
                nonSpaces2++;
            p++;
        }
    }
}

int main()
{
    auto x = loadFile(R"(C:\swos-port\swos\swos.asm)", true);

    std::cout.imbue(std::locale(""));

    std::cout << "IF time: " << measure(loopTestIf, x.first) << "ms\n";
    std::cout << "SWITCH time: " << measure(loopTestSwitch, x.first) << "ms\n";
    assert(tIndex1 == tIndex2 && !memcmp(types1, types2, tIndex1));

    std::cout << "isspace() time: " << measure(testIsspace, x.first) << "ms\n";
    std::cout << "myisspace() time: " << measure(testMyIsspace, x.first) << "ms\n";
    assert(spaces1 == spaces2 && nonSpaces1 == nonSpaces2);

    return 0;
}
