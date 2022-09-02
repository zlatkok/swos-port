#pragma once

#include <future>

class BaseTest
{
public:
    struct TestOptions {
        std::vector<std::string> testsToRun;
        const char *snapshotsDir;
        bool doSnapshots = false;
        bool passOnExceptions = false;
        bool firstFailExits = false;
        int timeout = 5'000;
    };

    BaseTest();
    static int runTests(const TestOptions& options);

protected:
    using CaseProc = std::function<void()>;
    struct Case {
        const char *name;
        const char *id;
        CaseProc init;
        CaseProc proc;
        size_t numTests = 1;
        bool allowScreenshots = true;
        CaseProc finalize = nullptr;
    };
    using CaseList = std::vector<Case>;

    virtual void init() = 0;                // when test is about to be run
    virtual void finish() = 0;              // when test is done
    virtual void defaultCaseInit() = 0;     // use this to initialize the case if it doesn't provide its own function
    virtual const char *name() const = 0;
    virtual const char *displayName() const = 0;
    virtual CaseList getCases() = 0;

    template <typename T>
    const CaseProc bind(void (T::*f)())
    {
        return std::bind(f, static_cast<T *>(this));
    }

    size_t m_currentDataIndex = 0;

private:
    struct Failure {
        Failure(const std::string& testCaseName, size_t dataIndex, const std::string& error)
            : testCaseName(testCaseName), dataIndex(dataIndex), error(error) {}
        std::string testCaseName;
        size_t dataIndex;
        std::string error;
    };
    struct TestFailures {
        TestFailures(BaseTest *test, const std::string& testCaseName, size_t dataIndex, const std::string& error) : test(test) {
            failures.emplace_back(testCaseName, dataIndex, error);
        }
        BaseTest *test;
        std::vector<Failure> failures;
    };
    using FailureList = std::vector<TestFailures>;
    using TestNamesSet = std::unordered_set<std::string>;
    using TestNamesList = std::vector<std::string>;
    using TestResults = std::pair<int, FailureList>;

    static TestResults runTestsWithTimeoutCheck(const TestOptions& options, const TestNamesSet& testList);
    static TestResults doRunTests(const TestOptions& options, const TestNamesSet& testList);
    static void timeoutCheck(int timeout);
    static TestNamesSet validateTestNames(const TestNamesList& testNames);
    static TestNamesSet includeAllTests();
    static void includeTest(BaseTest *test, TestNamesSet& includedTests);
    static bool tryToIncludeTest(const std::string& testName, TestNamesSet& includedTests);
    static void initTestCase(BaseTest *test, const Case& testCase);
    template <typename F>
    static void runTestCase(BaseTest *test, const Case& testCase, size_t i, const TestOptions& options, F addFailureMessage);
    template <typename Time>
    static void showReport(int numTestsRan, const FailureList& failures, Time startTime);
    static void outputStats(std::chrono::time_point<std::chrono::steady_clock> startTime, int numTestsRan);
    static void initMenuBackgroundDrawHook();
    static int hookRenderCopyF(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcRect, const SDL_FRect *dstRect);
    static std::future<SDL_Surface *> takeSnapshot(const char *snapshotDir, const char *caseId, int caseInstanceIndex);

    static uint32_t getCurrentTicks();
    static std::tuple<uint32_t, size_t, size_t, size_t> unpackCurrentTest();
    static void packCurrentTest(size_t testIndex, size_t testCaseIndex, size_t dataIndex);

    static std::vector<BaseTest *> m_tests;
    static std::condition_variable m_condition;
    static std::mutex m_mutex;
    static std::atomic<int64_t> m_currentTestPacked;
    static bool m_testsDone;

    static std::vector<std::future<SDL_Surface *>> m_snapshotFutures;
};
