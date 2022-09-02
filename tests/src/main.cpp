#include "sdlProcs.h"
#include "BaseTest.h"
#include "testEnvironment.h"
#include "file.h"
#include <iostream>
#undef pop
#include <filesystem>

static const char kSnapshotDir[] = "snapshots";

static BaseTest::TestOptions m_testOptions;

const char kSwosDir[] = "swos-dir=";
const char kCreateSnapshots[] = "create-snapshots";
const char kPassOnExceptions[] = "pass-on-exceptions";
const char kExitOnFirstFail[] = "exit-on-first-fail";
const char kTimeout[] = "timeout=";

static void printHelpAndExit()
{
    std::cout << "SWOS Unit Tester v1.0\n";
    std::cout << "Usage: tests ([<option>]|[<test name>])+\n";
    std::cout << "\nOptions:\n";
    std::cout << "--" << kSwosDir << "<swos dir>    - set SWOS directory location\n";
    std::cout << "--" << kCreateSnapshots << "       - create menu snapshots after each test\n";
    std::cout << "--" << kPassOnExceptions << "     - pass on exceptions to the debugger\n";
    std::cout << "--" << kExitOnFirstFail << "     - do not run all tests, exit after first fail\n";
    std::cout << "--" << kTimeout << "<int>          - set maximum allowed time for test to run (default: 5s)\n";
    std::cout << "\nOnly specified test(s) will be run, in case none are given everything will be tested.\n";
    std::exit(EXIT_SUCCESS);
}

static void parseCommandLine(int argc, char **argv)
{
    while (argv++, --argc) {
        if (strstr(*argv, "--") != *argv) {
            if (strstr(*argv, "/?") == *argv)
                printHelpAndExit();
            m_testOptions.testsToRun.push_back(*argv);
        } else {
            *argv += 2;
            if (strstr(*argv, "help") == *argv)
                printHelpAndExit();
            if (strstr(*argv, kSwosDir) == *argv)
                setRootDir(*argv + sizeof(kSwosDir) - 1);
            else if (strstr(*argv, kCreateSnapshots) == *argv)
                m_testOptions.doSnapshots = true;
            else if (strstr(*argv, kPassOnExceptions) == *argv)
                m_testOptions.passOnExceptions = true;
            else if (strstr(*argv, kExitOnFirstFail) == *argv)
                m_testOptions.firstFailExits = true;
            else if (strstr(*argv, kTimeout) == *argv)
                m_testOptions.timeout = atoi(*argv + sizeof(kTimeout) - 1);
        }
    }

    m_testOptions.snapshotsDir = kSnapshotDir;
}

static void initializeSnapshots()
{
    using namespace std::filesystem;

    if (!exists(kSnapshotDir) || !is_directory(kSnapshotDir))
        create_directory(kSnapshotDir);

    if (!exists(kSnapshotDir) || !is_directory(kSnapshotDir)) {
        std::cerr << "Failed to create snapshots directory!\n";
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv)
{
    if (!initSdlApiTable()) {
        std::cout << "Failed to fetch SDL address table!\n";
        return EXIT_FAILURE;
    }

    parseCommandLine(argc, argv);

    if (m_testOptions.doSnapshots)
        initializeSnapshots();

    initializeTestEnvironment();

    return BaseTest::runTests(m_testOptions);
}
