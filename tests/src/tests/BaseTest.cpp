#include "BaseTest.h"
#include "unitTest.h"
#include "testEnvironment.h"
#include "util.h"
#include "render.h"
#include "menus.h"
#include "sdlProcs.h"
#include "mockRender.h"
#include "mockLog.h"
#include <iomanip>

std::vector<BaseTest *> BaseTest::m_tests;
std::condition_variable BaseTest::m_condition;
std::mutex BaseTest::m_mutex;
std::atomic<int64_t> BaseTest::m_currentTestPacked;
bool BaseTest::m_testsDone;
std::vector<std::future<SDL_Surface *>> BaseTest::m_snapshotFutures;

BaseTest::BaseTest()
{
    assert(m_currentTestPacked.is_lock_free());

    m_tests.push_back(this);
}

int BaseTest::runTests(const TestOptions& options)
{
    auto testsToRun = validateTestNames(options.testsToRun);

    auto startTime = std::chrono::high_resolution_clock::now();

    auto [numTestsRan, failures] = runTestsWithTimeoutCheck(options, testsToRun);

    showReport(numTestsRan, failures, startTime);

    return failures.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}

auto BaseTest::runTestsWithTimeoutCheck(const TestOptions& options, const TestNamesSet& testList) -> TestResults
{
    std::thread timeoutCheckThread(timeoutCheck, options.timeout);

    auto result = doRunTests(options, testList);

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_testsDone = true;
    }

    m_condition.notify_one();
    timeoutCheckThread.join();

    return result;
}

auto BaseTest::doRunTests(const TestOptions &options, const TestNamesSet& testList) -> std::pair<int, FailureList>
{
    FailureList failures;
    int numTestsRan = 0;

    // wait for C++20 ;)
    auto testListContains = [&testList](const char *name) {
        return testList.find(name) != testList.end();
    };

    for (size_t i = 0; i < m_tests.size(); i++) {
        auto test = m_tests[i];

        if (!testListContains(test->name()))
            continue;

        std::cout << "Running " << test->displayName() << " tests\n  ";
        test->init();

        const auto cases = test->getCases();
        assert(!cases.empty());

        for (size_t j = 0; j < cases.size(); j++) {
            const auto& testCase = cases[j];
            if (!testListContains(testCase.id) && !testListContains(testCase.name))
                continue;

            for (auto& k = test->m_currentDataIndex = 0; k < testCase.numTests; k++) {
                auto addFailureMessage = [test, &failures, &testCase, &k](const std::string& errorMessage) {
                    if (!failures.empty() && failures.back().test == test)
                        failures.back().failures.emplace_back(testCase.name, k, errorMessage);
                    else
                        failures.emplace_back(test, testCase.name, k, errorMessage);
                };

                packCurrentTest(i, j, k);
                numTestsRan++;
                runTestCase(test, testCase, k, options, addFailureMessage);

                if (options.firstFailExits && !failures.empty())
                    return { numTestsRan, failures };
            }
        }

        test->finish();
        std::cout << '\n';
    }

    return { numTestsRan, failures };
}

void BaseTest::timeoutCheck(int timeout)
{
    constexpr int kTestTimeoutMs = 4'000;
    constexpr int kCheckIntervalMs = 500;

    auto checkInterval = std::chrono::milliseconds(std::min(timeout, kCheckIntervalMs));

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_condition.wait_for(lock, checkInterval, [] { return m_testsDone; }))
                break;
        }
        auto now = getCurrentTicks();

        auto [testStartTime, testIndex, caseIndex, dataIndex] = unpackCurrentTest();
        auto timeSinceLastTestChanged = now - testStartTime;

        if (!isDebuggerPresent() && timeSinceLastTestChanged > kTestTimeoutMs) {
            const auto test = m_tests[testIndex];
            const auto testCase = test->getCases()[caseIndex];

            std::cerr << "\nTimeout reached, terminating!\n";
            std::cerr << "Offending test: " << test->displayName() << " (" << test->name() << "), case: " <<
                testCase.name << " (" << testCase.id << "), data index: " << dataIndex << std::endl;

            std::terminate();
        }
    }
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
    assert(swos.g_currentMenu);
}

template<typename F>
void BaseTest::runTestCase(BaseTest *test, const Case& testCase, size_t i, const TestOptions &options, F addFailureMessage)
{
    using namespace SWOS_UnitTest;

    try {
        auto mark = SwosVM::markAllMemory();

        resetMenuDrawn();

        initTestCase(test, testCase);
        testCase.proc();
        if (testCase.finalize)
            testCase.finalize();
        std::cout << '.';

        SwosVM::releaseAllMemory(mark);
    } catch (const BaseException& e) {
        auto errorMessage = e.error();
        addFailureMessage(errorMessage);
        std::cout << 'F';
    } catch (const std::exception& e) {
        addFailureMessage("Exception `"s + e.what() + "' caught.");
        std::cout << 'E';

        if (options.passOnExceptions)
            throw;
    } catch (...) {
        addFailureMessage("Unknown exception caught");
        std::cout << 'E';

        if (options.passOnExceptions)
            throw;
    }

    if (options.doSnapshots && testCase.allowScreenshots && wasMenuDrawn()) {
        auto future = takeSnapshot(options.snapshotsDir, testCase.id, i);
        m_snapshotFutures.push_back(std::move(future));
    }

    std::erase_if(m_snapshotFutures, [](auto& future) {
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto surface = future.get();
            SDL_FreeSurface(surface);
            return true;
        } else {
            return false;
        }
    });
}

template <typename Time>
void BaseTest::showReport(int numTestsRan, const FailureList& failures, Time startTime)
{
    if (numTestsRan) {
        std::cout << '\n';

        if (!failures.empty()) {
            auto numFailedTests = std::accumulate(failures.begin(), failures.end(), 0ull, [](auto sum, const auto& f) {
                return sum + f.failures.size();
            });

            std::cout << numFailedTests << " failed test" << (numFailedTests > 1 ? "s" : "") << ":\n";

            std::string lastTestCaseName;

            for (const auto failedTest : failures) {
                std::cout << "  " << failedTest.test->displayName() << ":\n";
                for (const auto& failure : failedTest.failures) {
                    if (failure.testCaseName != lastTestCaseName) {
                        std::cout << "    " << failure.testCaseName << ":\n";
                        lastTestCaseName = failure.testCaseName;
                    }
                    std::cout << "      [" << failure.dataIndex << "] " << failure.error << '\n';
                }
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

static void sanitizePath(std::string& path)
{
    for (auto c : "/:")
        std::replace(path.begin(), path.end(), c, '-');
}

std::future<SDL_Surface *> BaseTest::takeSnapshot(const char *snapshotDir, const char *caseId, int caseInstanceIndex)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%03d", caseInstanceIndex);

    auto filename = new std::string;
    *filename = joinPaths(snapshotDir, caseId);
    filename->append("-").append(buf).append(".bmp");
    sanitizePath(*filename);

    std::replace(filename->begin(), filename->end(), ' ', '-');

    auto surface = getScreenSurface();

    auto future = std::async(std::launch::async, [filename, surface]() {
        SDL_LockSurface(surface);
        if (surface && SDL_SaveBMP(surface, filename->c_str()))
            std::cerr << "Failed to save bitmap " << *filename << '\n';
        SDL_UnlockSurface(surface);
        delete filename;
        return surface;
    });

    return future;
}

uint32_t BaseTest::getCurrentTicks()
{
    auto time = std::chrono::system_clock::now().time_since_epoch();
    auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(time);
    return timeMs.count() & 0xffffffff;
}

std::tuple<uint32_t, size_t, size_t, size_t> BaseTest::unpackCurrentTest()
{
    auto packedTest = m_currentTestPacked.load(std::memory_order_relaxed);
    int dataIndex = packedTest & 0xfff;
    int caseIndex = (packedTest >> 12) & 0x3ff;
    int testIndex = (packedTest >> 22) & 0x3ff;
    uint32_t startTime = (packedTest >> 32) & 0xffffffff;
    return { startTime, testIndex, caseIndex, dataIndex };
}

void BaseTest::packCurrentTest(size_t testIndex, size_t testCaseIndex, size_t dataIndex)
{
    auto currentTestPacked = static_cast<uint64_t>(getCurrentTicks()) << 32;
    currentTestPacked |= (dataIndex & 0xfff) | ((testCaseIndex & 0x3ff) << 12) | ((testIndex & 0x3ff) << 22);
    m_currentTestPacked.store(currentTestPacked);
}
