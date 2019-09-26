#include "SelectFilesMenuTest.h"
#include "selectFilesMenu.h"
#include "unitTest.h"
#include "mockFile.h"
#include "file.h"
#include "menu.h"

static SelectFilesMenuTest t;

static std::string m_swosDir;

void SelectFilesMenuTest::init() {}
void SelectFilesMenuTest::finish() {}
void SelectFilesMenuTest::defaultCaseInit() {}

struct FakeFile {
    const char *name;
    size_t expextedNumFiles;
    bool expectedLongNames;
    size_t expectedColumn1Length;
    size_t expectedColumn2Length;
    size_t expectedColumn3Length;
} static const kFakeFiles[] = {
    nullptr, 0, false, 0, 0, 0,
    "one.hil", 1, false, 0, 1, 0,
    "two.car", 2, false, 0, 2, 0,
    "bbn.txt", 2, false, 0, 2, 0,
    "yiu.diy", 3, false, 0, 3, 0,
    "replay.rpl", 4, false, 0, 4, 0,
    "atlantis.sea", 5, false, 0, 5, 0,
    "moNSter.car", 6, false, 0, 6, 0,
    "TEAM1.DAT", 6, false, 0, 6, 0,
    "TACTIC1.TAC", 6, false, 0, 6, 0,
    "long.extension", 6, false, 0, 6, 0,
    "indio.flx", 6, false, 0, 6, 0,
    "lucette.sea", 7, false, 0, 7, 0,
    "anchor.sea", 8, false, 0, 8, 0,
    "dorado.sea", 9, false, 3, 3, 3,
    "stern.sea", 10, false, 3, 4, 3,
    "quepassa.rpl", 11, false, 4, 4, 3,
    "mistery.rpl", 12, false, 4, 4, 4,
    "stronk.car", 13, false, 4, 5, 4,
    "veryLongFilename.hil", 14, true, 7, 7, 0,
    "veryVeryReallyLongScaryFilename.hil", 15, true, 8, 7, 0,
    "noEXT", 15, false, 0, 4, 0,
//full house to 45, no long names & later with
};

struct {
    const char str[36];
    int maxStrLength;
    int smallLimit;
    int bigLimit;
    const char *expectedStr;
} static const kElisionExamples[] = {
    "HEY YOU", 8, 41, 52, "HEY YOU",
    "HEY YOU", 8, 40, 51, "HEY YOU",
    "HEY YOU", 8, 39, 50, "HEY YOU",
    "HEY YOU", 9, 38, 49, "HEY Y...",
    "HEY YOU", 8, 38, 49, "HEY ...",
    "HEY YOU", 8, 1, 1, "",
    "HEY YOU", 8, 2, 2, "",
    "HEY YOU", 8, 3, 3, "",
    "HEY YOU", 8, 4, 4, "",
    "HEY YOU", 8, 5, 5, "",
    "HEY YOU", 8, 6, -1, "...",
    "HEY YOU", 8, -1, 6, "",
    "HEY YOU", 8, 7, -1, "...",
    "HEY YOU", 8, -1, 7, "",
    "HEY YOU", 8, -1, 8, "",
    "HEY YOU", 8, -1, 9, "...",
    "HEY YOU", 8, 12, -1, "H...",
    "HEY YOU", 8, 13, -1, "H...",
    "HEY YOU", 8, 14, -1, "H...",
    "HEY YOU", 8, 15, -1, "H...",
    "HEY YOU", 8, 16, -1, "H...",
    "HEY YOU", 8, 17, -1, "H...",
    "HEY YOU", 8, 18, -1, "HE...",
    "HEY YOU", 8, -1, 16, "...",
    "HEY YOU", 8, -1, 17, "H...",
    "HEY YOU", 8, -1, 18, "H...",
    "HEY YOU", 8, -1, 19, "H...",
    "HEY YOU", 8, -1, 20, "H...",
    "HEY YOU", 8, -1, 21, "H...",
    "HEY YOU", 8, -1, 22, "H...",
    "HEY YOU", 8, -1, 23, "H...",
    "HEY YOU", 8, -1, 24, "H...",
    "HEY YOU", 8, -1, 25, "HE...",
    "HEY YOU", 8, -1, 26, "HE...",
    "HEY YOU", 8, -1, 27, "HE...",
    "HEY YOU", 8, -1, 28, "HE...",
    "HEY YOU", 8, -1, 29, "HE...",
    "HEY YOU", 8, -1, 30, "HE...",
    "HEY YOU", 8, -1, 31, "HE...",
    "HEY YOU", 8, -1, 32, "HEY...",
    "HEY YOU", 8, -1, 33, "HEY...",
    "HEY YOU", 8, -1, 34, "HEY...",
    "H", 2, 6, -1, "H",
    "H", 2, -1, 8, "H",
    "HI", 3, 7, 10, "",
    "IHI", 5, 9, -1, "I...",
    "IHI", 4, -1, 12, "...",
    "IH", 5, 8, 13, "IH",
    "IH", 5, 7, 10, "...",
    "IH", 3, 7, 10, "",
    "IH", 5, 6, 9, "...",
    "IH", 3, 6, 9, "",
    "IH", 5, 5, 8, "",
    "IH", 3, 5, 8, "",
    "IH", 5, 4, 7, "",
    "IH", 3, 4, 7, "",
    "IH", 5, 3, 6, "",
    "IH", 3, 3, 6, "",
    "IH", 5, 2, 5, "",
    "IH", 3, 2, 5, "",
    "KUMBAYA", 8, 36, 49, "KUMB...",
    "KUMBAYA", 9, 36, 49, "KUMBA...",
    "KUMBAYA", 10, 42, 55, "KUMBAYA",
    // actual competition filename that triggered the assert
    "CANADABUAHAHAHIIIIIIIIIIIIIIIII.DIY", 70, 107, -1, "CANADABUAHAHAHIIIIIIII...",
};

const char *SelectFilesMenuTest::name() const
{
    return "select-files-menu";
}

const char *SelectFilesMenuTest::displayName() const
{
    return "select files menu";
}

auto SelectFilesMenuTest::getCases() -> CaseList
{
    return {
        { "test select files menu when loading old competition", "load-competition",
            bind(&SelectFilesMenuTest::setupLoadingCompetitionTest), bind(&SelectFilesMenuTest::testLoadingCompetition), std::size(kFakeFiles) },
        { "test string elision", "string-elision",
            nullptr, bind(&SelectFilesMenuTest::testStringElision), std::size(kElisionExamples), false },
    };
}

void SelectFilesMenuTest::setupLoadingCompetitionTest()
{
    resetFakeFiles();

    auto root = rootDir();
    addFakeDirectory(root.c_str());

    for (size_t i = 0; i <= m_currentDataIndex; i++)
        if (kFakeFiles[i].name)
            addFakeFile(joinPaths(root.c_str(), kFakeFiles[i].name).c_str());

    D0 = 0;
    A0 = A1 = "LOAD OLD COMPETITION";
    g_skipNonCompetitionFiles = false;

    SWOS::GetFilenameAndExtension();
}

void SelectFilesMenuTest::testLoadingCompetition()
{
    //check return values(s) from GetFilenameAndExtension()
    //iterate through file entries and compare names
    ;
}

void SelectFilesMenuTest::testStringElision()
{
    const auto& data = kElisionExamples[m_currentDataIndex];
    char str[256];

    assert(data.smallLimit >= 0 || data.bigLimit >= 0);

    if (data.smallLimit >= 0) {
        strcpy(str, data.str);
        elideString(str, data.maxStrLength, data.smallLimit, false);
        assertEqual(str, data.expectedStr);
    }

    if (data.bigLimit >= 0) {
        strcpy(str, data.str);
        elideString(str, data.maxStrLength, data.bigLimit, true);
        assertEqual(str, data.expectedStr);
    }
}

//skipTacAndHilInSearch
//<=8 long filenames