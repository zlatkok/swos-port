#include "CommentaryTest.h"
#include "audio.h"
#include "comments.h"
#include "sfx.h"
#include "chants.h"
#include "unitTest.h"
#include "mockFile.h"
#include "wavFormat.h"
#include "mockSdlMixer.h"
#include "mockUtil.h"
#include "mockLog.h"

using namespace SWOS;

static CommentaryTest t;

static void disablePenalties();
static void enablePenalties();
static void triggerCornerSample();
static void triggerYellowCardSample();
static void triggerRedCardSample();
static void triggerSubstituteSample();
static void triggerTacticsChangeSample();
static void triggerThrowInSample();
static void triggerGoodPassComment();
static void triggerItsBeenACompleteRoutComment();
static void triggerDrawComment();
static void triggerSensationalGameComment();
static void setupKeeperClaimedComment();
static void setupHeaderComment();

void CommentaryTest::init()
{
    enableFileMocking(true);
}

void CommentaryTest::finish()
{
    enableFileMocking(false);
}

const char *CommentaryTest::name() const
{
    return "commentary";
}

const char *CommentaryTest::displayName() const
{
    return "commentary";
}

auto CommentaryTest::getCases() -> CaseList
{
    return {
        { "test custom samples", "custom-samples",
            bind(&CommentaryTest::setupCustomSamplesLoadingTest), bind(&CommentaryTest::testCustomSamples) },
        { "test loading custom extensions", "custom-sample-extensions",
            bind(&CommentaryTest::setupLoadingCustomExtensionsTest), bind(&CommentaryTest::testLoadingCustomExtensions) },
        { "test corrupted file", "bad-commentary-file",
            bind(&CommentaryTest::setupHandlingBadFileTest), bind(&CommentaryTest::testHandlingBadFile) },
        { "test comment interruption", "comment-interruption",
            bind(&CommentaryTest::setupCommentInterruptionTest), bind(&CommentaryTest::testCommentInterruption) },
        { "test muting comments", "comment-muting",
            bind(&CommentaryTest::setupMutingCommentsTest), bind(&CommentaryTest::testMutingComments) },
        { "test enqueued comments", "enqueued-comments",
            bind(&CommentaryTest::setupEnqueuedCommentsTest), bind(&CommentaryTest::testEnqueuedComments) },
        { "test end game crowd samples", "end-game-crowd-samples",
            bind(&CommentaryTest::setupEndGameCommentsTest), bind(&CommentaryTest::testEndGameComment) },
        { "test end game comments", "end-game-comments",
            bind(&CommentaryTest::setupEndGameCommentsTest), bind(&CommentaryTest::testEndGameChantsAndCrowdSamples) },
    };
}

constexpr int kNumFileDataSlots = 152;
constexpr int kFilenameAreaSize = 13;
static char m_fileDataBuffer[kNumFileDataSlots][kFilenameAreaSize];
static int m_currentFileSlot = 0;

static void addFakeCustomComments()
{
    MockFileList fakeFiles = {
        // sound effects & chants
        { "audio\\fx\\AWAYSUBL.wav" },
        { "audio\\fx\\BGCRD3L.mp3" },
        { "audio\\fx\\BOOWHISL.raw" },
        { "audio\\fx\\BOUNCEX.ogg" },
        { "audio\\fx\\CHANT10L.flac" },
        { "audio\\fx\\CHANT4L.mp3" },
        { "audio\\fx\\CHANT8L.mp3" },
        { "audio\\fx\\CHEER.mp3" },
        { "audio\\fx\\COMBLUE.mp3" },
        { "audio\\fx\\COMBROWN.mp3" },
        { "audio\\fx\\COMGREEN.mp3" },
        { "audio\\fx\\COMRED.mp3" },
        { "audio\\fx\\COMWHITE.mp3" },
        { "audio\\fx\\COMYELO.mp3" },
        { "audio\\fx\\EASY.mp3" },
        { "audio\\fx\\ENDGAMEW.mp3" },
        { "audio\\fx\\EREWEGO.mp3" },
        { "audio\\fx\\FIVENIL.mp3" },
        { "audio\\fx\\FOUL.mp3" },
        { "audio\\fx\\FOURNIL.mp3" },
        { "audio\\fx\\GOALKICK.mp3" },
        // make home goal missing
        //{ "audio\\fx\\HOMEGOAL.mp3" },
        { "audio\\fx\\HOMESUBL.mp3" },
        { "audio\\fx\\HOMEWINL.mp3" },
        { "audio\\fx\\KICKX.mp3" },
        { "audio\\fx\\MISSGOAL.mp3" },
        { "audio\\fx\\NOTSING.mp3" },
        { "audio\\fx\\ONENIL.mp3" },
        { "audio\\fx\\REF_WH.mp3" },
        { "audio\\fx\\THREENIL.mp3" },
        { "audio\\fx\\TWONIL.raw" },
        { "audio\\fx\\WHISTLE.raw" },
        // comments
        { "audio\\commentary\\corner\\corner01.wav" },
        { "audio\\commentary\\corner\\corner02.mp3" },
        { "audio\\commentary\\corner\\corner03.raw" },
        { "audio\\commentary\\corner\\corner04.ogg" },
        { "audio\\commentary\\corner\\corner05.flac" },
        { "audio\\commentary\\good_tackle\\tackled.mp3" },
        { "audio\\commentary\\end_game_so_close\\so_close1.mp3" },
        { "audio\\commentary\\end_game_so_close\\so_close2.mp3" },
        { "audio\\commentary\\end_game_rout\\rout1.mp3" },
        { "audio\\commentary\\end_game_rout\\rout2.mp3" },
        { "audio\\commentary\\end_game_sensational\\sensational1.mp3" },
        { "audio\\commentary\\end_game_sensational\\sensational2.mp3" },
        // two identical comments in a category
        { "audio\\commentary\\penalty\\some_penalty_you_got_there1.mp3", "penalty", 8 },
        { "audio\\commentary\\penalty\\some_penalty_you_got_there2.mp3", "penalty", 8 },
        // three identical comments in a category
        { "audio\\commentary\\header\\use_your_head1.ogg", "header", 7 },
        { "audio\\commentary\\header\\use_your_head2.ogg", "header", 7 },
        { "audio\\commentary\\header\\use_your_head3.ogg", "header", 7 },
        // samples with increased chances of getting played
        { "audio\\commentary\\free_kick\\free_kick_01_x3.mp3" },
        { "audio\\commentary\\free_kick\\free_kick_02_x2.mp3" },
        { "audio\\commentary\\free_kick\\free_kick_03.mp3" },
        // empty dir
        { "audio\\commentary\\hit_bar" },
    };

    for (auto& fakeFile : fakeFiles) {
        if (!fakeFile.data) {
            auto ext = getFileExtension(fakeFile.path);
            if (ext) {
                fakeFile.data = getBasename(fakeFile.path);
                fakeFile.size = strlen(fakeFile.data) + 1;
            }
        }
    }

    resetFakeFiles();
    addFakeFiles(fakeFiles);
}

static void loadFakeCustomSamples()
{
    using namespace SWOS_UnitTest;

    addFakeCustomComments();
    clearCommentsSampleCache();
    clearSfxSamplesCache();

    AssertSilencer assertSilencer;
    LogSilencer logSilencer;

    loadCommentary();
    loadSoundEffects();

    setStrictLogMode(true);
}

void CommentaryTest::setupCustomSamplesLoadingTest()
{
    loadFakeCustomSamples();
    setCrowdChantsEnabled(false);
}

static Mix_Chunk *getAndCheckLastPlayedChunk()
{
    auto chunk = getLastPlayedChunk();
    assertTrue(chunk);
    return chunk;
}

static const char *chunkToStr(const Mix_Chunk *chunk)
{
    return reinterpret_cast<const char *>(chunk->abuf);
}

static std::pair<const char *, const char *> chunkInfo(const Mix_Chunk *chunk)
{
    assert(chunk && chunk->alen > 5);

    auto name = chunkToStr(chunk);
    auto ext = name + chunk->alen - 5;
    assert(*ext == '.');

    auto isRaw = tolower(ext[1]) == 'r' &&
        tolower(ext[2]) == 'a' &&
        tolower(ext[3]) == 'w';

    if (isRaw)
        assert(chunk->alen > kSizeofWaveHeader);

    int offset = isRaw ? kSizeofWaveHeader : 0;
    name += offset;

    return { name, ext };
}

static void verifyBaseChunkNameEqual(const Mix_Chunk *chunk, const char *name)
{
    auto [chunkName, ext] = chunkInfo(chunk);

    auto cmpLen = ext - chunkName;
    assert(cmpLen > 1);

    while (cmpLen--)
        assertEqual(tolower(*name++), tolower(*chunkName++));
}

static void testCustomSfx()
{
    setCrowdChantsEnabled(true);

    struct {
        void (*playFunc)();
        const char *expectedString;
        int offset = 0;
    } static const kSfxTestData[] = {
        { playCrowdNoise, "bgcrd3l.mp3" },
        { PlayRefereeWhistleSample, "whistle.raw", kSizeofWaveHeader },
        { playEndGameWhistleSample, "endgamew.mp3" },
        { PlayFoulWhistleSample, "foul.mp3" },
        { PlayKickSample, "kickx.mp3" },
        { PlayBallBounceSample, "bouncex.ogg" },
    };

    for (const auto& testData : kSfxTestData) {
        testData.playFunc();
        auto chunk = getAndCheckLastPlayedChunk();
        assertStringEqualCaseInsensitive(chunkToStr(chunk) + testData.offset, testData.expectedString);
    }

    // test the missing ones
    using namespace SWOS_UnitTest;

    AssertSilencer assertSilencer;
    LogSilencer logSilencer;

    resetMockSdlMixer();

    PlayHomeGoalSample();
    PlayAwayGoalSample();

    assertEqual(numTimesPlayChunkCalled(), 0);

    setCrowdChantsEnabled(false);
}

static void testCustomIdenticalSamplesCategory(void (*playFunc)(), const char *expectedData, void (*setupFunc)() = nullptr)
{
    for (int i = 0; i < 10; i++) {
        resetMockSdlMixer();
        if (setupFunc)
            setupFunc();

        assert(playFunc);
        playFunc();

        auto chunk = getAndCheckLastPlayedChunk();
        assertStringEqualCaseInsensitive(chunkToStr(chunk), expectedData);
    }
}

static void testCustomCornerSamples()
{
    for (int i = 0; i < 3; i++) {
        struct {
            const char *expectedData;
            int offset = 0;
        } static const kCornerTestData[] = {
            { "corner01.wav" },
            { "corner02.mp3" },
            { "corner03.raw", kSizeofWaveHeader },
            { "corner04.ogg" },
            { "corner05.flac" },
        };

        for (auto data : kCornerTestData) {
            resetMockSdlMixer();

            triggerCornerSample();

            auto chunk = getAndCheckLastPlayedChunk();
            assertStringEqualCaseInsensitive(chunkToStr(chunk) + data.offset, data.expectedData);
        }
    }
}

static void testCustomFreeKickSamples()
{
    for (int i = 0; i < 3; i++) {
        for (size_t index : { 0, 1, 0, 1, 2, 0, 1, 0, 1, 2, 1, 2 }) {
            resetMockSdlMixer();
            PlayFoulComment();

            char expectedData[] = "free_kick_01_x3.mp3";
            static const std::array<const char *, 3> kSuffixes = { "1_x3", "2_x2", "3" };
            assert(index >= 0 && index < kSuffixes.size());
            strcpy(expectedData + 11, kSuffixes[index]);
            strcat(expectedData, ".mp3");

            auto chunk = getAndCheckLastPlayedChunk();
            assertStringEqualCaseInsensitive(chunkToStr(chunk), expectedData);
        }
    }
}

static void testEmptyCategories()
{
    resetFakeFiles();
    clearCommentsSampleCache();

    addFakeFile({ "audio\\fx\\cheer.wav", "123", 4 });

    struct {
        const char *dir;
        void (*playFunc)();
        void (*setupFunc)() = nullptr;
    } static const kEmptyDirsData[] = {
        { "dirty_tackle", PlayDangerousPlayComment },
        { "end_game_rout", triggerItsBeenACompleteRoutComment },
        { "end_game_sensational", triggerSensationalGameComment },
        { "end_game_so_close", triggerDrawComment },
        { "goal", PlayGoalComment, disablePenalties },
        { "good_play", triggerGoodPassComment },
        { "good_tackle", PlayGoodTackleComment },
        { "hit_bar", PlayBarHitComment },
        { "hit_post", PlayPostHitComment },
        { "injury", PlayInjuryComment, setupHeaderComment },
        { "keeper_claimed", PlayKeeperClaimedComment, disablePenalties },
        { "keeper_saved", PlayGoalkeeperSavedComment, disablePenalties },
        { "near_miss", PlayNearMissComment, disablePenalties },
        { "own_goal", PlayOwnGoalComment },
        { "penalty_missed", PlayNearMissComment, enablePenalties },
        { "penalty_saved", PlayGoalkeeperSavedComment },
        { "penalty_scored", PlayGoalComment },
        { "red_card", triggerRedCardSample },
        { "substitution", triggerSubstituteSample },
        { "tactic_changed", triggerTacticsChangeSample },
        { "throw_in", triggerThrowInSample },
        { "yellow_card", triggerYellowCardSample },
    };

    LogSilencer logSilencer;
    const std::vector<int> timerValues(7);

    for (const auto data : kEmptyDirsData) {
        if (data.setupFunc)
            data.setupFunc();

        resetMockSdlMixer();
        assert(data.playFunc);
        data.playFunc();
        setEnqueueTimers(timerValues);

        int expectedPlayChunkTimes = 0;
        if (!strncmp(data.dir, "end", 3))
            expectedPlayChunkTimes = 1;

        assertEqual(numTimesPlayChunkCalled(), expectedPlayChunkTimes);
    }
}

static void testCustomCommentary()
{
    testCustomIdenticalSamplesCategory(PlayHeaderComment, "header", setupHeaderComment);
    testCustomIdenticalSamplesCategory(PlayPenaltyComment, "penalty");
    testCustomCornerSamples();
    testCustomFreeKickSamples();
    testEmptyCategories();
}

void CommentaryTest::testCustomSamples()
{
    LogSilencer logSilencer;

    testCustomSfx();
    testCustomCommentary();
}

struct {
    char header[kSizeofWaveHeader];
    char data[10];
} static kRawFileContents = { {}, "I AM RAW!" };

static_assert(offsetof(decltype(kRawFileContents), data) == kSizeofWaveHeader);

static const MockFileList kFakeCustomFiles = {
    { "audio\\commentary\\goals\\m158_1_.mp3", "I AM MP3!", 10, },
    { "audio\\commentary\\goals\\m158_2_.wav", "I AM WAV!", 10, },
    { "audio\\commentary\\goals\\m158_3_.ogg", "I AM OGG!", 10, },
    { "audio\\commentary\\goals\\m158_4_.flac", "I AM FLAC!", 11, },
    { "audio\\commentary\\goals\\m158_5_.raw", reinterpret_cast<const char *>(&kRawFileContents), sizeof(kRawFileContents), },
    { "audio\\commentary\\goals\\m158_7_.mp3", "I AM MP3_2!", 12, },
};

void CommentaryTest::setupLoadingCustomExtensionsTest()
{
    setupCustomSamplesLoadingTest();

    addFakeFiles(kFakeCustomFiles);

    clearCommentsSampleCache();
    loadCommentary();
}

void CommentaryTest::testLoadingCustomExtensions()
{
    auto getLastPlayedChunkBuffer = []() { return chunkToStr(getLastPlayedChunk()); };

    for (const auto& i : { 0, 1, 2, 3, 4, 5, 0, 1, }) {
        const auto& fakeFile = kFakeCustomFiles[i];
        resetMockSdlMixer();
        disablePenalties();
        PlayGoalComment();

        assertEqual(numTimesPlayChunkCalled(), 1);
        int offset = fakeFile.size > kSizeofWaveHeader ? kSizeofWaveHeader : 0;
        assertEqual(getLastPlayedChunkBuffer() + offset, fakeFile.data);
    }
}

void CommentaryTest::setupHandlingBadFileTest()
{
    MockFileList fakeFiles = {
        { "audio\\commentary\\good_tackle\\extraordinary.mp3", "A", 2 },
        { "audio\\commentary\\good_tackle\\exceptional.mp3", "B", 2 },
        { "audio\\commentary\\good_tackle\\fantastic.mp3", "C", 2 },
        { "audio\\commentary\\good_tackle\\disappointing_x4.mp3", "D", 2 },
    };

    resetFakeFiles();
    addFakeFiles(fakeFiles);
    setFileAsCorrupted("audio\\commentary\\good_tackle\\disappointing_x4.mp3");

    SWOS_UnitTest::AssertSilencer assertSilencer;
    LogSilencer logSilencer;

    clearCommentsSampleCache();
    loadCommentary();
}

void CommentaryTest::testHandlingBadFile()
{
    SWOS_UnitTest::AssertSilencer silencer;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            resetMockSdlMixer();

            PlayGoodTackleComment();

            auto chunk = getAndCheckLastPlayedChunk();
            char expectedData[2] = { static_cast<char>('A' + j), '\0' };
            assertStringEqualCaseInsensitive(chunkToStr(chunk), expectedData);
        }
    }
}

void CommentaryTest::setupCommentInterruptionTest()
{
    loadFakeCustomSamples();
}

void CommentaryTest::testCommentInterruption()
{
    resetMockSdlMixer();
    PlayFoulComment();
    auto chunk = getAndCheckLastPlayedChunk();

    PlayGoodTackleComment();
    auto chunk2 = getAndCheckLastPlayedChunk();
    assertEqual(chunk, chunk2);

    PlayPenaltyComment();
    auto chunk3 = getAndCheckLastPlayedChunk();
    assertNotEqual(chunk, chunk3);
}

void CommentaryTest::setupMutingCommentsTest()
{
    resetFakeFiles();
    clearCommentsSampleCache();
    addFakeFiles({{ "audio\\commentary\\own_goal\\oh_my.mp3", "!", 2 }});

    setSoundEnabled(true);
    setCommentaryEnabled(true);
    loadCommentary();
}

void CommentaryTest::testMutingComments()
{
    resetMockSdlMixer();
    PlayOwnGoalComment();
    assertEqual(numTimesPlayChunkCalled(), 1);

    static const std::vector<void (*)()> kToggleSoundFunctions = {
        [] { setSoundEnabled(!soundEnabled()); },
        [] { setCommentaryEnabled(!commentaryEnabled()); },
        toggleMuteCommentary,
    };

    for (const auto& toggleFunc : kToggleSoundFunctions) {
        for (int i = 1; i >= 0; i--) {
            resetMockSdlMixer();
            PlayOwnGoalComment();
            assertEqual(numTimesPlayChunkCalled(), i);
            toggleFunc();
        }
    }
}

void CommentaryTest::setupEnqueuedCommentsTest()
{
    resetFakeFiles();
    clearCommentsSampleCache();
    addFakeFiles({
        { "audio\\commentary\\yellow_card\\pleahse.mp3", "\x0", 2 },
        { "audio\\commentary\\red_card\\oh_noes.mp3", "\x1", 2 },
        { "audio\\commentary\\good_play\\faster.mp3", "\x2", 2 },
        { "audio\\commentary\\throw_in\\boring.mp3", "\x3", 2 },
        { "audio\\commentary\\corner\\sleeping.mp3", "\x4", 2 },
        { "audio\\commentary\\substitution\\come_on.mp3", "\x5", 2 },
        { "audio\\commentary\\tactic_changed\\sheesh.mp3", "\x6", 2 },
    });

    loadCommentary();
    setCrowdChantsEnabled(true);
    loadIntroChant();
}

void CommentaryTest::testEnqueuedComments()
{
    const CommentaryTest::EnqueuedSamplesData kEnqueuedSamplesData = {
        enqueueYellowCardSample,
        enqueueRedCardSample,
        [] { swos.playingGoodPassTimer = 1; },
        [] { swos.playingThrowInSample = 1; },
        [] { swos.playingCornerSample = 1; },
        enqueueSubstituteSample,
        enqueueTacticsChangedSample,
    };

    // holds 0 or 1 for each enqueued sample timer value
    std::vector<int> values(kEnqueuedSamplesData.size() + 1, 0);
    applyEnqueuedSamplesData(kEnqueuedSamplesData, values);

    resetMockSdlMixer();
    playEnqueuedSamples();

    assertEqual(numTimesPlayChunkCalled(), 1);
    auto lastChunk = getLastPlayedChunk();
    assert(lastChunk);
    auto name = chunkToStr(lastChunk);
    assert(strlen(name) > 5 && !_strnicmp(name, "chant", 5));

    while (createNextPermutation(values)) {
        resetMockSdlMixer();
        applyEnqueuedSamplesData(kEnqueuedSamplesData, values);

        setEnqueueTimers(values);
        playEnqueuedSamples();

        assertEqual(numTimesPlayChunkCalled(), 1);
        const auto& playedChunks = getLastPlayedChunks();
        assertEqual(playedChunks.size(), 1);

        auto it = std::find(values.begin(), values.end(), 1);
        assert(it != values.end());

        int index = it - values.begin();

        assertEqual(chunkToStr(playedChunks.front())[0], index);
    }
}

void CommentaryTest::setupEndGameCommentsTest()
{
    loadFakeCustomSamples();
    clearCommentsSampleCache();
    loadCommentary();
}

void CommentaryTest::testEndGameComment()
{
    struct {
        int team1Goals;
        int team2Goals;
        const char *name;
        bool resetRandom = false;
    } static const kTestEndGameCommentData[] = {
        { 5, 0, "rout1", true },
        { 15, 0, "rout2" },
        { 0, 0, "so_close1", true },
        { 3, 3, "so_close2" },
        { 11, 11, "so_close1" },
        { 10, 8, "sensational1", true },
        { 12, 11, "sensational2" },
        { 3, 1, "sensational1" },
    };

    for (const auto& testData : kTestEndGameCommentData) {
        swos.statsTeam1Goals = testData.team1Goals;
        swos.statsTeam2Goals = testData.team2Goals;

        if (testData.resetRandom)
            resetRandomInRange();

        resetMockSdlMixer();
        playEndGameCrowdSampleAndComment();

        assertEqual(numTimesPlayChunkCalled(), 2);
        const auto& playedChunks = getLastPlayedChunks();
        assertEqual(playedChunks.size(), 2);

        auto chunk = playedChunks.back();
        verifyBaseChunkNameEqual(chunk, testData.name);
    }
}

void CommentaryTest::testEndGameChantsAndCrowdSamples()
{
    testResultChants();
    testEndGameCrowdSample();
}

void CommentaryTest::applyEnqueuedSamplesData(const EnqueuedSamplesData& data, const std::vector<int>& values)
{
    assert(values.size() == data.size() + 1 && values.back() == 0);

    for (size_t i = 0; i < data.size(); i++) {
        if (values[i]) {
            assert(data[i]);
            data[i]();
        }
    }
}

bool CommentaryTest::createNextPermutation(std::vector<int>& values)
{
    assert(values.back() == 0);

    int i = 0;
    while (values[i] != 0)
        i++;

    if (i != values.size() - 1) {
        values[i] = 1;

        while (i > 0)
            values[--i] = 0;

        return true;
    }

    return false;
}

void CommentaryTest::testResultChants()
{
    struct {
        int team1Goals;
        int team2Goals;
        int playedSampleIndices[2];
    } static const kResultSampleData[] = {
        { 0, 0, { -1, -1, } },
        { 7, 0, { -1, -1, } },
        { 0, 7, { -1, -1, } },
        { 1, 0, {  6, 0, } },
        { 2, 0, {  6, 1, } },
        { 3, 0, {  6, 2, } },
        { 4, 0, {  6, 3, } },
        { 5, 0, {  6, 4, } },
        { 6, 0, {  6, 5, } },
        { 0, 1, {  6, 0, } },
        { 0, 2, {  6, 1, } },
        { 0, 3, {  6, 2, } },
        { 0, 4, {  6, 3, } },
        { 0, 5, {  6, 4, } },
        { 0, 6, {  6, 5, } },
        { 1, 1, {  7, 7, } },
        { 2, 2, {  7, 7, } },
        { 3, 3, {  7, 7, } },
        { 5, 5, {  7, 7, } },
        { 25, 25, {  7, 7, } },
    };

    for (const auto& sampleData : kResultSampleData) {
        swos.statsTeam1Goals = sampleData.team1Goals;
        swos.statsTeam2Goals = sampleData.team2Goals;

        resetRandomInRange();

        for (int index : sampleData.playedSampleIndices) {
            resetMockSdlMixer();
            loadCrowdChantSample();

            if (index >= 0) {
                assertEqual(getPlayChants10lFunction(), PlayResultSample);
                PlayResultSample();

                int numSampledPlayed = index >= 0;
                assertEqual(numTimesPlayChunkCalled(), numSampledPlayed);
                const auto& playedChunks = getLastPlayedChunks();
                assertEqual(playedChunks.size(), numSampledPlayed);

                auto expectedSamplePath = swos.resultChantFilenames[index];
                auto expectedBasename = getBasename(expectedSamplePath);

                auto chunk = playedChunks.front();
                verifyBaseChunkNameEqual(chunk, expectedBasename);
            } else {
                assertEqual(getPlayChants10lFunction(), PlayFansChant10lSample);
            }
        }
    }
}

void CommentaryTest::testEndGameCrowdSample()
{
    struct {
        int team1Goals;
        int team2Goals;
        int index;
    } static const kTestEndGameCrowdSample[] = {
        { 0, 0, 2, },
        { 1, 1, 2, },
        { 3, 3, 2, },
        { 3, 0, 0, },
        { 2, 1, 0, },
        { 10, 6, 0, },
        { 6, 7, 1, },
        { 0, 5, 1, },
        { 2, 3, 1, },
    };

    for (const auto& testData : kTestEndGameCrowdSample) {
        swos.team1GoalsDigit2 = testData.team1Goals;
        swos.team2GoalsDigit2 = testData.team2Goals;

        resetMockSdlMixer();
        playEndGameCrowdSampleAndComment();

        assertEqual(numTimesPlayChunkCalled(), 2);
        const auto& playedChunks = getLastPlayedChunks();
        assertEqual(playedChunks.size(), 2);

        auto expectedSamplePath = swos.endGameCrowdSamples[testData.index];
        auto expectedBasename = getBasename(expectedSamplePath);

        auto chunk = playedChunks.front();
        verifyBaseChunkNameEqual(chunk, expectedBasename);
    }
}

static void disablePenalties()
{
    swos.penaltiesState = 0;
}

static void enablePenalties()
{
    swos.penaltiesState = -1;
}

static void triggerCornerSample()
{
    swos.playingCornerSample = 1;
    playEnqueuedSamples();
}

static void triggerYellowCardSample()
{
    enqueueYellowCardSample();
    playEnqueuedSamples();
}

static void triggerRedCardSample()
{
    enqueueRedCardSample();
    playEnqueuedSamples();
}

static void triggerSubstituteSample()
{
    enqueueSubstituteSample();
    playEnqueuedSamples();
}

static void triggerTacticsChangeSample()
{
    enqueueTacticsChangedSample();
    playEnqueuedSamples();
}

static void triggerThrowInSample()
{
    swos.playingThrowInSample = 1;
    playEnqueuedSamples();
}

static void triggerGoodPassComment()
{
    swos.playingGoodPassTimer = 1;
    playEnqueuedSamples();
}

static void triggerItsBeenACompleteRoutComment()
{
    swos.statsTeam1Goals = 5;
    swos.statsTeam2Goals = 1;
    playEndGameCrowdSampleAndComment();
}

static void triggerDrawComment()
{
    swos.statsTeam1Goals = swos.statsTeam2Goals = 0;
    playEndGameCrowdSampleAndComment();
}

static void triggerSensationalGameComment()
{
    swos.statsTeam1Goals = 4;
    swos.statsTeam2Goals = 3;
    playEndGameCrowdSampleAndComment();
}

static void setupKeeperClaimedComment()
{
    // make sure no comments are playing
    Mix_HaltChannel(-1);
}

static void setupHeaderComment()
{
    static auto team = SwosVM::allocateMemory(sizeof(TeamGeneralInfo)).as<TeamGeneralInfo *>();
    team->playerNumber = 1;
    A6 = team;
}
