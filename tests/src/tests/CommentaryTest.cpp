#include "CommentaryTest.h"
#include "comments.h"
#include "unitTest.h"
#include "mockFile.h"
#include "wavFormat.h"
#include "mockSdlMixer.h"
#include "mockUtil.h"
#include "mockLog.h"

using namespace SWOS;

static CommentaryTest t;

static void disablePenalties()
{
    performingPenalty = false;
    penaltiesState = 0;
}

static void setupKeeperClaimedComment()
{
    performingPenalty = false;
    // make sure no comments are playing
    Mix_HaltChannel(-1);
}

static void setupHeaderComment()
{
    static TeamGeneralInfo team;
    team.playerNumber = 1;
    A6 = &team;
}

struct SampleGroupTestData {
    std::vector<const char *> filenames;
    void (*playCommentFunc)();
    void (*setup)();
} static const kOriginalSamplesTestData[] = {
    {
        {
            "m158_1_.raw",
            "m158_2_.raw",
            "m158_3_.raw",
            "m158_2_.raw",
            "m158_3_.raw",
            "m158_4_.raw",
            "m158_5_.raw",
            "m158_7_.raw",
        }, PlayGoalComment, disablePenalties
    },
    {
        {
            "m313_7_.raw",
            "m313_a_.raw",
            "m313_b_.raw",
            "m196_w_.raw",
            "m196_x_.raw",
            "m196_z_.raw",
            "m233_1_.raw",
            "m313_7_.raw",
            "m313_a_.raw",
            "m313_7_.raw",
            "m313_a_.raw",
            "m313_b_.raw",
            "m196_w_.raw",
            "m196_x_.raw",
            "m196_z_.raw",
            "m233_1_.raw",
        }, PlayGoalkeeperSavedComment, disablePenalties,
    },
    {
        {
            "m158_p_.raw",
            "m158_q_.raw",
            "m158_s_.raw",
            "m158_t_.raw",
            "m158_x_.raw",
            "m158_t_.raw",
            "m158_x_.raw",
            "m158_y_.raw",
        }, PlayOwnGoalComment,
    },
    {
        {
            "m10_j_.raw",
            "m10_k_.raw",
            "m10_d_.raw",
            "m10_e_.raw",
        }, PlayNearMissComment, disablePenalties
    },
    {
        {
            "m10_d_.raw",
            "m10_e_.raw",
            "m10_f_.raw",
            "m10_g_.raw",
            "m10_h_.raw",
            "m10_r_.raw",
            "m10_s_.raw",
            "m10_t_.raw",
        }, PlayPostHitComment,
    },
    {
        {
            "m10_i_.raw",
            "m10_u_.raw",
        }, PlayBarHitComment,
    },
    {
        {
            "m313_6_.raw",
            "m313_8_.raw",
            "m313_9_.raw",
            "m313_c_.raw",
            "m313_d_.raw",
            "m313_g_.raw",
            "m313_h_.raw",
            nullptr,
        }, PlayKeeperClaimedComment, setupKeeperClaimedComment
    },
    {
        {
            "m349_7_.raw",
            "m349_8_.raw",
            "m349_e_.raw",
            "m365_1_.raw",
        }, PlayGoodTackleComment,
    },
    {
        {
            "m313_i_.raw",
            "m313_j_.raw",
            "m349_4_.raw",
            "m313_o_.raw",
        }, PlayGoodPassComment,
    },
    {
        {
            "m158_z_.raw",
            "m195.raw",
            "m196_1_.raw",
            "m196_2_.raw",
            "m196_4_.raw",
            "m196_6_.raw",
            "m196_7_.raw",
            "m158_z_.raw",
            "m195.raw",
            "m158_z_.raw",
            "m195.raw",
            "m196_1_.raw",
            "m196_2_.raw",
            "m196_4_.raw",
            "m196_6_.raw",
            "m196_7_.raw",
        }, PlayFoulComment,
    },
    {
        {
            "m158_z_.raw",
            "m195.raw",
            "m196_1_.raw",
            "m158_z_.raw",
            "m195.raw",
            "m158_z_.raw",
            "m195.raw",
            "m196_1_.raw",
        }, PlayDangerousPlayComment,
    },
    {
        {
            "m196_k_.raw",
            "m196_l_.raw",
            "m196_m_.raw",
            "m196_n_.raw",
        }, PlayPenaltyComment,
    },
    {
        {
            "m196_t_.raw",
            "m196_u_.raw",
            "m196_v_.raw",
            "m313_7_.raw",
            "m313_a_.raw",
            "m196_w_.raw",
            "m196_z_.raw",
            "m233_1_.raw",
        }, PlayPenaltySavedComment,
    },
    {
        {
            "m233_2_.raw",
            "m233_4_.raw",
            "m233_5_.raw",
            "m233_6_.raw",
        }, PlayPenaltyMissComment,
    },
    {
        {
            "m233_7_.raw",
            "m233_8_.raw",
            "m233_9_.raw",
            "m233_c_.raw",
        }, PlayPenaltyGoalComment,
    },
    {
        {
            "m33_2_.raw",
            "m33_3_.raw",
            "m33_4_.raw",
            "m33_5_.raw",
            "m33_6_.raw",
            "m33_8_.raw",
            "m33_a_.raw",
            "m33_b_.raw",
        }, PlayerDoingHeader_157, setupHeaderComment
    },
    {
        {
            "m10_v_.raw",
            "m10_w_.raw",
            "m10_y_.raw",
            "m313_1_.raw",
            "m10_y_.raw",
            "m313_1_.raw",
            "m313_2_.raw",
            "m313_3_.raw",
        }, PlayCornerSample,
    },
    {
        {
            "m10_5_.raw",
            "m10_7_.raw",
            "m10_8_.raw",
            "m10_7_.raw",
            "m10_8_.raw",
            "m10_9_.raw",
            "m10_a_.raw",
            "m10_b_.raw",
        }, PlayTacticsChangeSample,
    },
    {
        {
            "m233_j_.raw",
            "m233_k_.raw",
            "m233_l_.raw",
            "m233_k_.raw",
            "m233_l_.raw",
            "m233_m_.raw",
            "m10_3_.raw",
            "m10_4_.raw",
        }, PlaySubstituteSample,
    },
    {
        {
            "m196_8_.raw",
            "m196_9_.raw",
            "m196_a_.raw",
            "m196_9_.raw",
            "m196_a_.raw",
            "m196_b_.raw",
            "m196_c_.raw",
            "m196_b_.raw",
            "m196_c_.raw",
            "m196_d_.raw",
            "m196_e_.raw",
            "m196_f_.raw",
            "m196_g_.raw",
            "m196_h_.raw",
            "m196_i_.raw",
            "m196_j_.raw",
        }, PlayRedCardSample,
    },
    {
        {
            "m443_7_.raw",
            "m443_8_.raw",
            "m443_9_.raw",
            "m443_8_.raw",
            "m443_9_.raw",
            "m443_a_.raw",
            "m443_b_.raw",
            "m443_c_.raw",
            "m443_d_.raw",
            "m443_e_.raw",
            "m443_f_.raw",
            "m443_g_.raw",
            "m443_h_.raw",
            "m443_i_.raw",
            "m443_j_.raw",
            "m443_7_.raw",
            "m443_8_.raw",
            "m443_7_.raw",
            "m443_8_.raw",
            "m443_9_.raw",
            "m443_a_.raw",
            "m443_9_.raw",
            "m443_a_.raw",
            "m443_b_.raw",
            "m443_c_.raw",
            "m443_d_.raw",
            "m443_e_.raw",
            "m443_f_.raw",
            "m443_g_.raw",
            "m443_h_.raw",
            "m443_i_.raw",
            "m443_j_.raw",
        }, PlayYellowCardSample,
    },
    {
        {
            "m406_3_.raw",
            "m406_8_.raw",
            "m406_7_.raw",
            "m406_9_.raw",
        }, PlayThrowInSample,
    },
    {
        {
            "m406_h_.raw",
        }, PlayItsBeenACompleteRoutComment,
    },
    {
        {
            "m406_i_.raw",
            "m406_j_.raw",
        }, PlaySensationalGameComment,
    },
    {
        {
            "m406_f_.raw",
            "m406_g_.raw",
        }, PlayDrawComment,
    },
};

const CommentaryTest::EnqueuedSamplesData CommentaryTest::kEnqueuedSamplesData = {
    { &playingYellowCardTimer, PlayYellowCardSample, },
    { &playingRedCardTimer, PlayRedCardSample, },
    { &playingGoodPassTimer, PlayGoodPassComment, },
    { &playingThrowInSample, PlayThrowInSample, },
    { &playingCornerSample, PlayCornerSample, },
    { &substituteSampleTimer, PlaySubstituteSample, },
    { &tacticsChangedSampleTimer, PlayTacticsChangeSample, },
};

const std::array<char *, 5> kEndGameComments = {
    "hard\\M406_f_.RAW",
    "hard\\M406_g_.RAW",
    "hard\\M406_h_.RAW",
    "hard\\M406_i_.RAW",
    "hard\\M406_j_.RAW",
};

void CommentaryTest::init()
{
    g_soundOff = false;
    g_commentary = true;
    g_muteCommentary = false;

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
        { "test original samples", "original-samples",
            bind(&CommentaryTest::setupOriginalSamplesLoadingTest), bind(&CommentaryTest::testOriginalSamples) },
        { "test custom samples", "custom-samples",
            bind(&CommentaryTest::setupCustomSamplesLoadingTest), bind(&CommentaryTest::testCustomSamples) },
        { "test loading custom extensions", "custom-sample-extensions",
            bind(&CommentaryTest::setupLoadingCustomExtensionsTest), bind(&CommentaryTest::testLoadingCustomExtensions) },
        { "test corrupted file", "bad-file",
            bind(&CommentaryTest::setupHandlingBadFileTest), bind(&CommentaryTest::testHandlingBadFile) },
        { "test comment interruption", "comment-interruption",
            bind(&CommentaryTest::setupCommentInterruptionTest), bind(&CommentaryTest::testCommentInterruption) },
        { "test muting comments", "comment-muting",
            bind(&CommentaryTest::setupMutingCommentsTest), bind(&CommentaryTest::testMutingComments) },
        { "test that custom comments override original", "custom-samples-override-original",
            bind(&CommentaryTest::setupAudioOverridingOriginalCommentsTest), bind(&CommentaryTest::testAudioOverridingOriginalComments) },
    };
}

constexpr int kNumFileDataSlots = 141;
constexpr int kFilenameAreaSize = 13;
static char m_fileDataBuffer[kNumFileDataSlots][kFilenameAreaSize];
static int m_currentFileSlot = 0;

static void addFakeSampleFile(MockFileList& fakeFiles, const char *path)
{
    auto baseName = getBasename(path);
    assert(strlen(baseName) < kFilenameAreaSize);
    assert(m_currentFileSlot < kNumFileDataSlots);

    auto& data = m_fileDataBuffer[m_currentFileSlot];
    strcpy(data, baseName);

    fakeFiles.emplace_back(path, data, strlen(baseName) + 1);
    m_currentFileSlot++;
}

static void gatherFakeOriginalComments(MockFileList& fakeFiles)
{
    m_currentFileSlot = 0;

    for (auto table : { commentaryTable, getOnDemandSampleTable() })
        for (auto ptr = table; *ptr != kSentinel; ptr++)
            addFakeSampleFile(fakeFiles, *ptr);

    static const std::pair<const char **, int> kResultSampleTables[] = {
        { resultChantFilenames, 8 },
        { endGameCrowdSamples, 3 },
    };

    for (const auto& table : kResultSampleTables)
        for (int i = 0; i < table.second; i++)
            addFakeSampleFile(fakeFiles, table.first[i]);
}

static void addFakeOriginalComments()
{
    MockFileList fakeFiles;

    gatherFakeOriginalComments(fakeFiles);
    assert(m_currentFileSlot <= kNumFileDataSlots);

    resetFakeFiles();
    addFakeFiles(fakeFiles);
}

void CommentaryTest::setupOriginalSamplesLoadingTest()
{
    addFakeOriginalComments();
    clearSampleCache();
    LoadCommentary();
}

void CommentaryTest::testOriginalSamples()
{
    testIfOriginalSamplesLoaded();
    testOriginalEnqueuedSamples();
    testResultChants();
    testEndGameComment();       // run it first so the last played comment for end game comments doesn't get cobbled
    testEndGameCrowdSample();
}

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
    clearSampleCache();

    AssertSilencer assertSilencer;
    LogSilencer logSilencer;

    LoadCommentary();
    LoadSoundEffects();

    setStrictLogMode(true);
}

void CommentaryTest::setupCustomSamplesLoadingTest()
{
    resetFakeFiles();
    loadFakeCustomSamples();
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

static void testCustomSfx()
{
    struct {
        void (*playFunc)();
        const char *expectedString;
        int offset = 0;
    } static const kSfxTestData[] = {
        { PlayCrowdNoiseSample, "bgcrd3l.mp3" },
        { PlayRefereeWhistleSample, "whistle.raw", kSizeofWaveHeader },
        { PlayEndGameWhistleSample, "endgamew.mp3" },
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
            PlayCornerSample();

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
    struct {
        const char *dir;
        void (*playFunc)();
        void (*setupFunc)() = nullptr;
    } static const kEmptyDirsData[] = {
        { "dirty_tackle", PlayDangerousPlayComment },
        { "end_game_rout", PlayItsBeenACompleteRoutComment },
        { "end_game_sensational", PlaySensationalGameComment },
        { "end_game_so_close", PlayDrawComment },
        { "goal", PlayGoalComment, disablePenalties },
        { "good_play", PlayGoodPassComment },
        { "good_tackle", PlayGoodTackleComment },
        { "hit_bar", PlayBarHitComment },
        { "hit_post", PlayPostHitComment },
        { "injury", PlayerTackled_59, setupHeaderComment },
        { "keeper_claimed", PlayKeeperClaimedComment, disablePenalties },
        { "keeper_saved", PlayGoalkeeperSavedComment, disablePenalties },
        { "near_miss", PlayNearMissComment, disablePenalties },
        { "own_goal", PlayOwnGoalComment },
        { "penalty_missed", PlayPenaltyMissComment },
        { "penalty_saved", PlayPenaltySavedComment },
        { "penalty_scored", PlayPenaltyGoalComment },
        { "red_card", PlayRedCardSample },
        { "substitution", PlaySubstituteSample },
        { "tactic_changed", PlayTacticsChangeSample },
        { "throw_in", PlayThrowInSample },
        { "yellow_card", PlayYellowCardSample },
    };

    for (const auto data : kEmptyDirsData) {
        LogSilencer logSilencer;

        if (data.setupFunc)
            data.setupFunc();

        resetMockSdlMixer();
        assert(data.playFunc);
        data.playFunc();

        assertEqual(numTimesPlayChunkCalled(), 0);
    }
}

static void testCustomCommentary()
{
    testCustomIdenticalSamplesCategory(PlayerDoingHeader_157, "header", setupHeaderComment);
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
    { "sfx\\pierce\\m158_1_.mp3", "I AM MP3!", 10, },
    { "sfx\\pierce\\m158_2_.wav", "I AM WAV!", 10, },
    { "sfx\\pierce\\m158_3_.ogg", "I AM OGG!", 10, },
    { "sfx\\pierce\\m158_4_.flac", "I AM FLAC!", 11, },
    { "sfx\\pierce\\m158_5_.raw", reinterpret_cast<const char *>(&kRawFileContents), sizeof(kRawFileContents), },
    { "sfx\\pierce\\m158_7_.mp3", "I AM MP3_2!", 12, },
};

void CommentaryTest::setupLoadingCustomExtensionsTest()
{
    addFakeOriginalComments();
    deleteFakeFile("sfx\\pierce\\m158_1_.raw");
    deleteFakeFile("sfx\\pierce\\m158_2_.raw");
    deleteFakeFile("sfx\\pierce\\m158_3_.raw");
    deleteFakeFile("sfx\\pierce\\m158_4_.raw");
    deleteFakeFile("sfx\\pierce\\m158_7_.raw");

    addFakeFiles(kFakeCustomFiles);

    clearSampleCache();
    LoadCommentary();
}

void CommentaryTest::testLoadingCustomExtensions()
{
    auto getLastPlayedChunkBuffer = []() { return chunkToStr(getLastPlayedChunk()); };

    for (const auto& i : { 0, 1, 2, 1, 2, 3, 4, 5 }) {
        const auto& fakeFile = kFakeCustomFiles[i];
        resetMockSdlMixer();
        disablePenalties();
        PlayGoalComment();

        assertEqual(numTimesPlayChunkCalled(), 1);
        int offset = fakeFile.size > kSizeofWaveHeader ? kSizeofWaveHeader : 0;
        assertEqual(getLastPlayedChunkBuffer() + offset, fakeFile.data);
    }
}

void CommentaryTest::setupAudioOverridingOriginalCommentsTest()
{
    addFakeOriginalComments();
    loadFakeCustomSamples();
}

void CommentaryTest::testAudioOverridingOriginalComments()
{
    testCustomCommentary();
}

void CommentaryTest::setupHandlingBadFileTest()
{
    MockFileList fakeFiles = {
        { "audio\\commentary\\good_play\\extraordinary.mp3", "A", 2 },
        { "audio\\commentary\\good_play\\exceptional.mp3", "B", 2 },
        { "audio\\commentary\\good_play\\fantastic.mp3", "C", 2 },
        { "audio\\commentary\\good_play\\disappointing_x4.mp3", "D", 2 },
    };

    resetFakeFiles();
    addFakeFiles(fakeFiles);
    setFileAsCorrupted("audio\\commentary\\good_play\\disappointing_x4.mp3");

    SWOS_UnitTest::AssertSilencer assertSilencer;
    LogSilencer logSilencer;

    clearSampleCache();
    LoadCommentary();
}

void CommentaryTest::testHandlingBadFile()
{
    SWOS_UnitTest::AssertSilencer silencer;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            resetMockSdlMixer();
            PlayGoodPassComment();

            auto chunk = getAndCheckLastPlayedChunk();
            char expectedData[2] = { static_cast<char>('A' + j), '\0' };
            assertStringEqualCaseInsensitive(chunkToStr(chunk), expectedData);
        }
    }
}

void CommentaryTest::setupCommentInterruptionTest()
{
    setupOriginalSamplesLoadingTest();
}

void CommentaryTest::testCommentInterruption()
{
    resetMockSdlMixer();
    PlayFoulComment();
    auto chunk = getAndCheckLastPlayedChunk();

    PlayGoodPassComment();
    auto chunk2 = getAndCheckLastPlayedChunk();
    assertEqual(chunk, chunk2);

    PlayPenaltyComment();
    auto chunk3 = getAndCheckLastPlayedChunk();
    assertNotEqual(chunk, chunk3);
}

void CommentaryTest::setupMutingCommentsTest()
{
    resetFakeFiles();
    clearSampleCache();
    addFakeFiles({{ "audio\\commentary\\own_goal\\oh_my.mp3", "!", 2 }});
    LoadCommentary();
}

void CommentaryTest::testMutingComments()
{
    resetMockSdlMixer();
    PlayOwnGoalComment();
    assertEqual(numTimesPlayChunkCalled(), 1);

    static const std::vector<std::pair<int16_t *, int>> kSoundControlVariables = {
        { &g_soundOff, 1 },
        { &g_commentary, 0 },
        { reinterpret_cast<int16_t *>(&g_muteCommentary), 1 },
    };

    g_soundOff = 0;
    g_commentary = 1;
    g_muteCommentary = 0;

    for (const auto& var : kSoundControlVariables) {
        for (int i = 0; i < 2; i++) {
            resetMockSdlMixer();
            *var.first = i;
            PlayOwnGoalComment();
            assertEqual(numTimesPlayChunkCalled(), i ^ var.second);
        }
        *var.first = !var.second;
    }
}

void CommentaryTest::testIfOriginalSamplesLoaded()
{
    for (const auto& testData : kOriginalSamplesTestData) {
        resetRandomInRange();
        if (testData.setup)
            testData.setup();

        for (size_t i = 0; i < 2; i++) {
            for (size_t j = 0; j < testData.filenames.size(); j++) {
                resetMockSdlMixer();
                assert(testData.playCommentFunc);
                testData.playCommentFunc();

                assertEqual(numTimesPlayChunkCalled(), 1);

                const auto& chunks = getLastPlayedChunks();
                assertEqual(chunks.size(), 1);
                auto chunk = chunks.back();

                if (chunk->alen == kSizeofWaveHeader)
                    assertTrue(!testData.filenames[j] || !testData.filenames[j][0]);
                else
                    assertStringEqualCaseInsensitive(chunkToStr(chunk) + kSizeofWaveHeader, testData.filenames[j]);
            }
        }
    }
}

void CommentaryTest::testOriginalEnqueuedSamples()
{
    // holds 0 or 1 for each enqueued sample timer value
    std::vector<int> values(kEnqueuedSamplesData.size() + 1, 0);
    applyEnqueuedSamplesData(kEnqueuedSamplesData, values);

    resetMockSdlMixer();
    PlayEnqueuedSamples();
    assertEqual(numTimesPlayChunkCalled(), 0);

    applyEnqueuedSamplesData(kEnqueuedSamplesData, values);

    while (createNextPermutation(values)) {
        resetMockSdlMixer();
        applyEnqueuedSamplesData(kEnqueuedSamplesData, values);
        PlayEnqueuedSamples();

        assertEqual(numTimesPlayChunkCalled(), 1);
        const auto& playedChunks = getLastPlayedChunks();
        assertEqual(playedChunks.size(), 1);

        auto it = std::find(values.begin(), values.end(), 1);
        assert(it != values.end());

        int index = it - values.begin();

        auto expectedInvokedCommentFunc = kEnqueuedSamplesData[index].second;
        auto actualInvokedCommentFunc = getPlaybackFunctionFromChunk(playedChunks.front());
        assertEqual(expectedInvokedCommentFunc, actualInvokedCommentFunc);
    }
}

void CommentaryTest::applyEnqueuedSamplesData(const EnqueuedSamplesData& data, const std::vector<int>& values)
{
    assert(values.size() == data.size() + 1 && values.back() == 0);
    for (size_t i = 0; i < data.size(); i++)
        *data[i].first = values[i];
}

bool CommentaryTest::createNextPermutation(std::vector<int>& values)
{
    assert(values.back() == 0 && values.size() == kEnqueuedSamplesData.size() + 1);

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

void *CommentaryTest::getPlaybackFunctionFromChunk(const Mix_Chunk *chunk)
{
    assert(chunk->alen > kSizeofWaveHeader);

    for (const auto& testData : kOriginalSamplesTestData)
        for (const auto filename : testData.filenames)
            if (filename && _stricmp(filename, chunkToStr(chunk) + kSizeofWaveHeader) == 0)
                return testData.playCommentFunc;

    return nullptr;
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
        statsTeam1Goals = sampleData.team1Goals;
        statsTeam2Goals = sampleData.team2Goals;

        for (int index : sampleData.playedSampleIndices) {
            resetMockSdlMixer();
            LoadCrowdChantSample();

            if (index >= 0) {
                assertEqual(playFansChant10lOffset, PlayResultSample);
                PlayResultSample();

                int numSampledPlayed = index >= 0;
                assertEqual(numTimesPlayChunkCalled(), numSampledPlayed);
                const auto& playedChunks = getLastPlayedChunks();
                assertEqual(playedChunks.size(), numSampledPlayed);

                auto expectedSamplePath = resultChantFilenames[index];
                auto expectedBasename = getBasename(expectedSamplePath);

                auto chunk = playedChunks.front();
                assert(chunk->alen > kSizeofWaveHeader);

                assertStringEqualCaseInsensitive(expectedBasename, chunkToStr(chunk) + kSizeofWaveHeader);
            } else {
                assertEqual(playFansChant10lOffset, PlayFansChant10lSample);
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
        team1GoalsDigit2 = testData.team1Goals;
        team2GoalsDigit2 = testData.team2Goals;

        resetMockSdlMixer();
        LoadAndPlayEndGameComment();

        assertEqual(numTimesPlayChunkCalled(), 2);
        const auto& playedChunks = getLastPlayedChunks();
        assertEqual(playedChunks.size(), 2);

        auto expectedSamplePath = endGameCrowdSamples[testData.index];
        auto expectedBasename = getBasename(expectedSamplePath);

        auto chunk = playedChunks.front();
        assert(chunk->alen > kSizeofWaveHeader);

        assertStringEqualCaseInsensitive(expectedBasename, chunkToStr(chunk) + kSizeofWaveHeader);
    }
}

void CommentaryTest::testEndGameComment()
{
    struct {
        int team1Goals;
        int team2Goals;
        int indices[2];
    } static const kTestEndGameCommentData[] = {
        { 5, 0, { 2, 2, } },
        { 15, 0, { 2, 2, } },
        { 0, 0, { 0, 1, } },
        { 3, 3, { 0, 1, } },
        { 11, 11, { 0, 1, } },
        { 10, 8, { 3, 4, } },
        { 12, 11, { 3, 4, } },
        { 3, 1, { 3, 4, } },
    };

    for (const auto& testData : kTestEndGameCommentData) {
        statsTeam1Goals = testData.team1Goals;
        statsTeam2Goals = testData.team2Goals;

        for (auto index : testData.indices) {
            resetMockSdlMixer();
            LoadAndPlayEndGameComment();

            assertEqual(numTimesPlayChunkCalled(), 2);
            const auto& playedChunks = getLastPlayedChunks();
            assertEqual(playedChunks.size(), 2);

            auto expectedSamplePath = kEndGameComments[index];
            auto expectedBasename = getBasename(expectedSamplePath);

            auto chunk = playedChunks.back();
            assert(chunk->alen > kSizeofWaveHeader);

            assertStringEqualCaseInsensitive(expectedBasename, chunkToStr(chunk) + kSizeofWaveHeader);
        }
    }
}
