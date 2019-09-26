#include "BaseTest.h"
#include "unitTest.h"
#include "util.h"
#include "render.h"
#include "menu.h"
#include "bmpWriter.h"
#include <iomanip>
#include <future>

std::vector<BaseTest *> BaseTest::m_tests;
std::condition_variable BaseTest::m_condition;
std::mutex BaseTest::m_mutex;
std::atomic<int32_t> BaseTest::m_currentTestPacked;
bool BaseTest::m_testsDone;

BaseTest::BaseTest()
{
    m_tests.push_back(this);
}

int BaseTest::runTests(const TestOptions& options)
{
    auto testsToRun = validateTestNames(options.testsToRun);

    // initialize SWOS stuff first
    SWOS::SWOS();
    SWOS::InitMainMenu();

    auto startTime = std::chrono::high_resolution_clock::now();

    auto [numTestsRan, failures] = runTestsWithTimeoutCheck(options, testsToRun);

    showReport(numTestsRan, failures, startTime);

    return failures.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}

auto BaseTest::runTestsWithTimeoutCheck(const TestOptions& options, const TestNamesSet& testList) -> TestResults
{
    auto lastTest = m_currentTestPacked = 0;
    m_testsDone = false;

    std::promise<TestResults> resultsPromise;
    auto resultsFuture = resultsPromise.get_future();

    auto runTestsWrapper = [](const TestOptions& options, const TestNamesSet& testList, std::promise<TestResults> promise) {
        auto result = doRunTests(options, testList);
        promise.set_value(result);
        m_testsDone = true;
        m_condition.notify_one();
    };

    std::thread testerThread(runTestsWrapper, options, testList, std::move(resultsPromise));

    auto lastCheckTime = std::chrono::high_resolution_clock::now();
    auto checkInterval = std::chrono::milliseconds(std::min(options.timeout, 1000));

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_condition.wait_for(lock, checkInterval, [] { return m_testsDone; }))
                break;
        }

        auto currentTest = m_currentTestPacked.load();
        auto now = std::chrono::high_resolution_clock::now();

        if (!isDebuggerPresent() && currentTest == lastTest) {
            auto msSinceLastTestChanged = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCheckTime);
            if (msSinceLastTestChanged.count() >= options.timeout) {
                auto [testIndex, caseIndex, dataIndex] = unpackCurrentTest();
                const auto test = m_tests[testIndex];
                const auto testCase = test->getCases()[caseIndex];

                std::cerr << "\nTimeout reached, terminating!\n";
                std::cerr << "Offending test: " << test->name() << ", case: " << testCase.id <<
                    ", data index: " << dataIndex << std::endl;

                std::terminate();
            }
        } else {
            lastCheckTime = now;
            lastTest = currentTest;
        }
    }

    auto result = resultsFuture.get();
    testerThread.join();

    return result;
}

auto BaseTest::doRunTests(const TestOptions &options, const TestNamesSet& testList) -> std::pair<int, FailureList>
{
    FailureList failures;
    int numTestsRan = 0;

    for (size_t i = 0; i < m_tests.size(); i++) {
        auto test = m_tests[i];

        if (!testList.contains(test->name()))
            continue;

        std::cout << "Running " << test->displayName() << " tests\n  ";
        test->init();

        const auto cases = test->getCases();
        assert(!cases.empty());

        for (size_t j = 0; j < cases.size(); j++) {
            const auto& testCase = cases[j];
            if (!testList.contains(testCase.id) && !testList.contains(testCase.name))
                continue;

            for (auto& k = test->m_currentDataIndex = 0; k < testCase.numTests; k++) {
                auto addFailureMessage = [test, &failures, &testCase, &k](const std::string& errorMessage) {
                    if (!failures.empty() && failures.back().test == test)
                        failures.back().failures.emplace_back(k, errorMessage);
                    else
                        failures.emplace_back(test, k, errorMessage);
                };

                packCurrentTest(i, j, k);
                numTestsRan++;
                runTestCase(test, testCase, k, options, addFailureMessage);

                if (options.firstFailExits && !failures.empty())
                    return { numTestsRan, failures };
            }
        }
        std::cout << '\n';
    }

    return { numTestsRan, failures };
}

auto BaseTest::validateTestNames(const TestNamesList& testNames) -> TestNamesSet
{
    std::unordered_set<std::string> result;

    if (testNames.empty())
        return includeAllTests();

    for (const auto& testName : testNames) {
        if (!tryToIncludeTest(testName, result)) {
            std::cerr << "Test " << testName << " not found.\n";
            std::exit(EXIT_FAILURE);
        }
    }

    return result;
}

auto BaseTest::includeAllTests() -> TestNamesSet
{
    TestNamesSet result;

    for (const auto test : m_tests)
        includeTest(test, result);

    return result;
}

void BaseTest::includeTest(BaseTest *test, TestNamesSet& includedTests)
{
    includedTests.insert(test->name());
    for (auto const testCase : test->getCases()) {
        assert(testCase.id);
        includedTests.insert(testCase.id);
    }
}

bool BaseTest::tryToIncludeTest(const std::string& potentialTestName, TestNamesSet& includedTests)
{
    for (const auto test : m_tests) {
        if (!strcmp(test->name(), potentialTestName.c_str())) {
            includeTest(test, includedTests);
            return true;
        } else {
            for (const auto testCase : test->getCases()) {
                if (!strcmp(testCase.id, potentialTestName.c_str()) || !strcmp(testCase.name, potentialTestName.c_str())) {
                    includedTests.insert(test->name());
                    includedTests.insert(testCase.id);
                    return true;
                }
            }
        }
    }

    return false;
}

void BaseTest::initTestCase(BaseTest *test, const Case& testCase)
{
    if (testCase.init)
        testCase.init();
    else
        test->defaultCaseInit();

    // somebody better set that darn menu!
    assert(g_currentMenu);
}

template<typename F>
void BaseTest::runTestCase(BaseTest *test, const Case& testCase, size_t i, const TestOptions &options, F addFailureMessage)
{
    using namespace SWOS_UnitTest;

    try {
        initTestCase(test, testCase);
        testCase.proc();
        std::cout << '.';
    } catch (const BaseException& e) {
        auto errorMessage = e.error();
        addFailureMessage(errorMessage);
        std::cout << 'F';
    } catch (const std::exception& e) {
        addFailureMessage(std::string("Exception `") + e.what() + "' caught.");
        std::cout << 'E';

        if (options.passOnExceptions)
            throw;
    } catch (...) {
        addFailureMessage("Unknown exception caught");
        std::cout << 'E';

        if (options.passOnExceptions)
            throw;
    }

    if (options.doSnapshots && testCase.allowScreenshots)
        takeSnapshot(options.snapshotsDir, test->name(), testCase.name, i);
}

template <typename Time>
void BaseTest::showReport(int numTestsRan, const FailureList &failures, Time startTime)
{
    if (numTestsRan) {
        std::cout << '\n';

        if (!failures.empty()) {
            auto numFailedTests = std::accumulate(failures.begin(), failures.end(), 0, [](auto sum, const auto& f) {
                return sum + f.failures.size();
            });

            std::cout << numFailedTests << " failed test" << (numFailedTests > 1 ? "s" : "") << ":\n";

            for (const auto failedTest : failures) {
                std::cout << "  " << failedTest.test->displayName() << ":\n";
                for (const auto& failure : failedTest.failures)
                    std::cout << "    [" << failure.caseIndex << "] " << failure.error << '\n';
            }
        } else {
            std::cout << "All tests passed!\n";
        }
        outputStats(startTime, numTestsRan);
    } else {
        std::cout << "No tests ran!\n";
    }
}

void BaseTest::outputStats(std::chrono::time_point<std::chrono::steady_clock> startTime, int numTestsRan)
{
    assert(numTestsRan > 0);

    auto endTime = std::chrono::high_resolution_clock::now();
    int64_t timeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    std::cout << "\nRan " << numTestsRan << " test" << (numTestsRan == 1 ? "" : "s") << " in " <<
        formatNumberWithCommas(timeElapsed) << "ms.\n";
}

static RgbQuad *getMenuPalette()
{
    auto pal = linAdr384k + 129'536;

    static RgbQuad s_menuPalette[256];

    if (!s_menuPalette[0].red) {
        for (int i = 0; i < 256; i++) {
            s_menuPalette[i].red = pal[3 * i] * 4;
            s_menuPalette[i].green = pal[3 * i + 1] * 4;
            s_menuPalette[i].blue = pal[3 * i + 2] * 4;
        }
    }

    return s_menuPalette;
}

void BaseTest::takeSnapshot(const char *snapshotDir, const char *testName, const char *caseName, int caseInstanceIndex)
{
    static std::vector<std::future<void>> forgottenFutures; // it's not stupid if it works ;)

    // skip tests that don't show menus, currently no better way to check since main menu always gets shown at start
    if (std::count(linAdr384k, linAdr384k + kVgaScreenSize, 0) == kVgaScreenSize)
        return;

    auto screen = new char[kVgaScreenSize];
    memcpy(screen, linAdr384k, kVgaScreenSize);

    char buf[16];
    snprintf(buf, sizeof(buf), "%03d", caseInstanceIndex);

    auto filename = new std::string;
    *filename = joinPaths(snapshotDir, testName);
    filename->append("-").append(caseName).append("-").append(buf).append(".bmp");

    std::replace(filename->begin(), filename->end(), ' ', '-');

    auto future = std::async(std::launch::async, [filename, screen]() {
        if (!saveBmp8Bit(filename->c_str(), kVgaWidth, kVgaHeight, screen, getMenuPalette(), 256))
            std::cerr << "Failed to save bitmap " << *filename  << '\n';
        delete[] screen;
        delete filename;
    });

    forgottenFutures.push_back(std::move(future));
}

std::tuple<size_t, size_t, size_t> BaseTest::unpackCurrentTest()
{
    auto packedTest = m_currentTestPacked.load();
    auto dataIndex = packedTest & 0xfff;
    auto caseIndex = (packedTest >> 12) & 0x3ff;
    auto testIndex = (packedTest >> 22) & 0x3ff;
    return { testIndex, caseIndex, dataIndex };
}

void BaseTest::packCurrentTest(size_t testIndex, size_t testCaseIndex, size_t dataIndex)
{
    m_currentTestPacked = (dataIndex & 0xfff) | ((testCaseIndex & 0x3ff) << 12) | ((testIndex & 0x3ff) << 22);
}
