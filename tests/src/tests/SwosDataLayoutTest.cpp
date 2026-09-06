#include "SwosDataLayoutTest.h"
#include "unitTest.h"

static SwosDataLayoutTest t;

const char *SwosDataLayoutTest::name() const
{
    return "swos-data-layout";
}

const char *SwosDataLayoutTest::displayName() const
{
    return "SWOS data layout";
}

auto SwosDataLayoutTest::getCases() -> CaseList
{
    return {
        { "chairman scenes string pool offsets", "chairman-scenes-string-pool", {},
            bind(&SwosDataLayoutTest::verifyChairmanScenesStringPool) },
        { "competition file buffer layout", "competition-file-buffer-layout", {},
            bind(&SwosDataLayoutTest::verifyCompetitionFileBuffer) },
        { "career file buffer layout", "career-file-buffer-layout", {},
            bind(&SwosDataLayoutTest::verifyCareerFileBuffer) },
        { "fixed file buffer sizes", "fixed-file-buffer-sizes", {},
            bind(&SwosDataLayoutTest::verifyFixedFileBuffers) },
        { "original frame table area", "original-frame-table-area", {},
            bind(&SwosDataLayoutTest::verifyFrameTableArea) },
    };
}

void SwosDataLayoutTest::verifyChairmanScenesStringPool()
{
    struct ExpectedString {
        size_t offset;
        const char *text;
    };

    static const ExpectedString expectedStrings[] = {
        { 0x4dc1, "YUGOSLAVIA" },
        { 0x2e5, "THE PRESIDENT" },
        { 0x2f, "RETURN TO GAME" },
        { 0x30c, "EXCELLENT JOB" },
        { 0x58c3, "QUEENSLAND" },
        { 0x331, "APPRECIATE IT" },
        { 0x3d1c, "DISK FULL" },
    };

    for (const auto& expected : expectedStrings)
        assertTrue(!memcmp(swos.aChairmanScenes + expected.offset, expected.text, strlen(expected.text)));
}

void SwosDataLayoutTest::verifyCompetitionFileBuffer()
{
    using Variables = SwosVM::SwosVariables;

    constexpr auto bufferStart = offsetof(Variables, competitionFileBuffer);
    assertEqual(offsetof(Variables, selTeamsPtr) - bufferStart, size_t{ 5'170 });
    assertEqual(offsetof(Variables, trainingGameCopy) - bufferStart, size_t{ 5'187 });
    assertEqual(offsetof(Variables, g_numSelectedTeams) - bufferStart, size_t{ 5'189 });
    assertEqual(offsetof(Variables, g_selectedTeams) - bufferStart, size_t{ 5'191 });
}

void SwosDataLayoutTest::verifyCareerFileBuffer()
{
    using Variables = SwosVM::SwosVariables;

    constexpr auto bufferStart = offsetof(Variables, careerFileBuffer);
    assertEqual(offsetof(Variables, USER_A) - bufferStart, size_t{ 0x16ac8 });
    assertEqual(offsetof(Variables, g_gameLength) - bufferStart, size_t{ 0x17374 });
    assertEqual(offsetof(Variables, g_autoReplays) - bufferStart, size_t{ 0x17376 });
    assertEqual(offsetof(Variables, g_autoSaveHighlights) - bufferStart, size_t{ 0x1737a });
    assertEqual(offsetof(Variables, g_allPlayerTeamsEqual) - bufferStart, size_t{ 0x1737c });
    assertEqual(offsetof(Variables, g_pitchType) - bufferStart, size_t{ 0x1737e });
    assertEqual(offsetof(Variables, g_selectedTeams) - bufferStart, size_t{ 95'153 });
}

void SwosDataLayoutTest::verifyFixedFileBuffers()
{
    assertEqual(sizeof(swos.g_pitchDatBuffer), size_t{ 10'032 });
    assertEqual(sizeof(swos.importTacticsFilename), size_t{ 256 });
    assertEqual(sizeof(swos.g_selectedTeams), size_t{ 68'400 });
    assertEqual(sizeof(swos.kRandomTable), size_t{ 256 });
}

void SwosDataLayoutTest::verifyFrameTableArea()
{
    using Variables = SwosVM::SwosVariables;
    static const int16_t expectedFrameTable[] = { -20, 344, -65, 415, 415, -101 };

    constexpr auto frameTablesStart = offsetof(Variables, frameIndicesTablesStart);
    constexpr auto animationTablesStart = offsetof(Variables, animTablesStart);

    assertEqual(frameTablesStart % alignof(int16_t), size_t{ 0 });
    assertEqual(animationTablesStart % alignof(int16_t), size_t{ 0 });
    assertEqual(animationTablesStart - frameTablesStart, sizeof(expectedFrameTable));
    assertTrue(!memcmp(swos.frameIndicesTablesStart, expectedFrameTable, sizeof(expectedFrameTable)));
}
