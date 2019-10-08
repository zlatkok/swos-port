#include "SelectFilesMenuTest.h"
#include "selectFilesMenu.h"
#include "unitTest.h"
#include "mockFile.h"
#include "file.h"
#include "menu.h"
#include "controls.h"
#include "sdlProcs.h"
#include "data/canada.diy.h"
#include <initializer_list>

#define SWOS_STUB_MENU_DATA
#include "selectFiles.mnu.h"

using namespace SelectFilesMenu;

static SelectFilesMenuTest t;

static std::string m_swosDir;

static void drawSpriteToBuffer(int spriteIndex, char *buf)
{
    auto savedVsPtr = vsPtr;
    auto savedScreenWidth = screenWidth;
    auto savedDeltaColor = deltaColor;

    vsPtr = buf;
    screenWidth = spritesIndex[spriteIndex]->width;

    D0 = spriteIndex;
    D1 = D2 = 0;
    SAFE_INVOKE(DrawSprite);

    deltaColor = savedDeltaColor;
    screenWidth = savedScreenWidth;
    vsPtr = savedVsPtr;
}

void SelectFilesMenuTest::init()
{
    assert(spritesIndex[kUpArrowSpriteIndex]->width == kArrowWidth && spritesIndex[kUpArrowSpriteIndex]->height == kArrowHeight);

    drawSpriteToBuffer(kUpArrowSpriteIndex, upArrowBuffer);
    drawSpriteToBuffer(kDownArrowSpriteIndex, downArrowBuffer);

    takeOverInput();
}

void SelectFilesMenuTest::finish()
{
    SWOS_UnitTest::setMenuCallback(nullptr);
}

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

struct FakeFile {
    const char *name;
    size_t expectedColumnLength[kMaxColumns];
    size_t expectedColumnCompetitionsOnly[kMaxColumns];
} static const kFakeFiles[] = {
    nullptr, { 0, 0, 0, }, { 0, 0, 0, },
    "one.hil", { 0, 1, 0, }, { 0, 0, 0, },
    "two.car", { 0, 2, 0, }, { 0, 1, 0, },
    "bbn.txt", { 0, 2, 0, }, { 0, 1, 0, },
    "yiu.diy", { 0, 3, 0, }, { 0, 2, 0, },
    "replay.rpl", { 0, 4, 0, }, { 0, 2, 0, },
    "atlantis.sea", { 0, 5, 0, }, { 0, 3, 0, },
    "moNSter.car", { 0, 6, 0, }, { 0, 4, 0, },
    "TEAM1.DAT", { 0, 6, 0, }, { 0, 4, 0, },
    "TACTIC1.TAC", { 0, 7, 0, }, { 0, 4, 0, },
    "long.extension", { 0, 7, 0, }, { 0, 4, 0, },
    "indio.flx", { 0, 7, 0, }, { 0, 4, 0, },
    "lucette.sea", { 0, 8, 0, }, { 0, 5, 0, },
    "anchor.sea", { 3, 3, 3, }, { 0, 6, 0, },
    "dorado.sea", { 3, 4, 3, }, { 0, 7, 0, },
    "stern.sea", { 4, 4, 3, }, { 0, 8, 0, },
    "quepassa.rpl", { 4, 4, 4, }, { 0, 8, 0, },
    "mistery.rpl", { 4, 5, 4, },  { 0, 8, 0, },
    "stronk.car", { 5, 5, 4, }, { 3, 3, 3, },
    "veryLongFilename.hil", { 8, 7, 0, }, { 3, 3, 3, },
    "veryVeryReallyLongScaryFilename.hil", { 8, 8, 0, }, { 3, 3, 3, },
    "noEXT", { 0, 4, 0, }, { 3, 3, 3, },
};

constexpr int kTestEntryTransitionNumFiles = 49;
constexpr int kTestEntryTransitionNumCases = (1 << 2) * kTestEntryTransitionNumFiles;

auto SelectFilesMenuTest::getCases() -> CaseList
{
    return {
        { "test select files menu when loading old competition", "load-competition",
            bind(&SelectFilesMenuTest::setupLoadCompetitionTest), bind(&SelectFilesMenuTest::testLoadCompetition), std::size(kFakeFiles) },
        { "test string elision", "string-elision",
            nullptr, bind(&SelectFilesMenuTest::testStringElision), std::size(kElisionExamples), false },
        { "test entry layout", "select-files-layout", nullptr, bind(&SelectFilesMenuTest::testLayout), },
        { "test entry transitions", "select-files-transitions", bind(&SelectFilesMenuTest::setupEntryTransitionsTest),
            bind(&SelectFilesMenuTest::testEntryTransitions), kTestEntryTransitionNumCases, },
        { "test abort button", "select-files-abort", nullptr, bind(&SelectFilesMenuTest::testAbortButton), 8, false },
        { "test selecting files", "select-files-selection", nullptr, bind(&SelectFilesMenuTest::testSelectingFiles), 1, false },
        { "test scrolling", "select-files-scroll", nullptr, bind(&SelectFilesMenuTest::testScrolling), },
        { "test abort save", "select-files-abort-save", nullptr, bind(&SelectFilesMenuTest::testAbortSave) },
        { "test save button show/hide", "select-files-show-save-button", nullptr, bind(&SelectFilesMenuTest::testSaveButtonShowHide) },
        { "test list with long extensions", "select-files-long-extension-list", nullptr, bind(&SelectFilesMenuTest::testLongExtensions) },
        { "test save competition", "select-files-save-competition", nullptr, bind(&SelectFilesMenuTest::testSaveCompetition) },
    };
}

void SelectFilesMenuTest::setupLoadCompetitionTest()
{
    resetFakeFiles();

    auto root = rootDir();
    addFakeDirectory(root.c_str());

    for (size_t i = 0; i <= m_currentDataIndex; i++)
        if (kFakeFiles[i].name)
            addFakeFile(joinPaths(root.c_str(), kFakeFiles[i].name).c_str());

    D0 = 0;
    A0 = A1 = "LOAD OLD COMPETITION";
    g_skipNonCompetitionFiles = m_currentDataIndex & 1;

    auto result = SWOS::GetFilenameAndExtension();

    assertTrue(result);
}

void SelectFilesMenuTest::testLoadCompetition()
{
    auto entry = getMenuEntry(kFirstFileEntry);

    for (int i = 0; i < kMaxColumns; i++) {
        int numVisibleColumns = 0;

        for (int j = 0; j < kMaxEntriesPerColumn; j++) {
            if (!entry[j].visible())
                break;
            numVisibleColumns++;
        }

        int expectedColumnLength = g_skipNonCompetitionFiles ?
            kFakeFiles[m_currentDataIndex].expectedColumnCompetitionsOnly[i] :
            kFakeFiles[m_currentDataIndex].expectedColumnLength[i];

        assertEqual(numVisibleColumns, expectedColumnLength);

        entry += kMaxEntriesPerColumn;
    }
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

static char *generateFilename(int num, bool useLongNames)
{
    static char filenameBuf[64];
    sprintf(filenameBuf, "FILE%03d%s.CAR", num, useLongNames ? "LOOONG" : "");
    return filenameBuf;
}

static size_t getExtensionOffset(char *filename)
{
    auto ext = strchr(filename, '.');
    assert(ext);

    if (!ext)
        return strlen(filename);

    return ext - filename + 1;
}

static FoundFileList generateFilenames(int numFiles, bool useLongNames)
{
    FoundFileList files;

    for (int i = 0; i < numFiles; i++) {
        auto filename = generateFilename(i, useLongNames);
        auto extOffset = getExtensionOffset(filename);
        files.emplace_back(filename, extOffset);
    }

    return files;
}

static std::string getBasename(const FoundFile& file)
{
    auto extOffset = file.extensionOffset > 0 ? file.extensionOffset - 1 : 0;
    return file.name.substr(0, extOffset);
}

struct ColumnInfo {
    int start[kMaxColumns];
    bool lastRow[kMaxColumns];
    int numColumns;
    int baseColumnLength;
    int lastRowStartItem;

    static int getNumColumns(bool useLongNames) {
        return kMaxColumns - (useLongNames != 0);
    }

    ColumnInfo(int numFiles, bool useLongNames) {
        numColumns = kMaxColumns - (useLongNames != 0);
        auto quoteRem = std::div(numFiles, numColumns);
        baseColumnLength = quoteRem.quot;
        int remainder = quoteRem.rem;
        lastRowStartItem = 0;

        if (remainder) {
            lastRowStartItem = (numColumns - remainder) / 2;
        } else {
            baseColumnLength--;
            remainder = numColumns;
        }

        int currentColumnLength = 0;

        for (int i = 0; i < numColumns; i++) {
            start[i] = currentColumnLength;
            lastRow[i] = i >= lastRowStartItem && i < lastRowStartItem + remainder;
            currentColumnLength += baseColumnLength + lastRow[i];
        }
    }
};

void SelectFilesMenuTest::testLayout()
{
    constexpr int kNumFiles = kNumFilenameItems + 10;
    constexpr int kNumCases = 1 << 2;

    for (int i = 0; i < kNumCases; i++) {
        bool useLongNames = i & 1;
        bool save = (i >> 1) & 1;

        for (int numFiles = 0; numFiles < kNumFiles; numFiles++) {
            auto filenames = generateFilenames(numFiles, useLongNames);

            showSelectFilesMenu("LAYOUT TEST", filenames, save, nullptr);

            ColumnInfo columnInfo(numFiles, useLongNames);
            auto entry = getMenuEntry(kFirstFileEntry);

            if (numFiles <= 8) {
                int middleColumn = columnInfo.numColumns / 2;

                for (int i = 0; i < kMaxColumns; i++)
                    for (int j = 0; j < kMaxEntriesPerColumn; j++, entry++)
                        assertItemVisibility(entry, i == middleColumn && j < numFiles);
            } else {
                int index = 0;

                for (int i = 0; i < columnInfo.numColumns; i++) {
                    int lastColumn = columnInfo.baseColumnLength + columnInfo.lastRow[i];

                    for (int j = 0; j < kMaxEntriesPerColumn; j++, entry++) {
                        bool visible = j < lastColumn;
                        assertItemVisibility(entry, visible);

                        if (visible) {
                            const auto& basename = getBasename(filenames[index + j]);
                            assertItemIsString(entry, basename);
                        }
                    }

                    index += lastColumn;
                }
            }
        }
    }
}

static void simulateKey(SDL_Scancode keyCode)
{
    queueSdlKeyDown(keyCode);
    lastControls = -1;  // prevent CheckControls() from thinking it's a repeat key
    SWOS::MenuProc();
    queueSdlKeyUp(keyCode);
}

int findNextLeftEntry(int i, int j, const MenuEntry *entry)
{
    int nextEntryOrdinal = entry->ordinal;

    if (i > 0) {
        auto leftEntry = entry - kMaxEntriesPerColumn;
        while (j > 0 && !leftEntry->visible()) {
            j--;
            leftEntry--;
        }
        if (leftEntry->visible())
            nextEntryOrdinal =  leftEntry->ordinal;
    }

    return nextEntryOrdinal;
}

int findNextRightEntry(int i, int j, const MenuEntry *entry)
{
    int nextEntryOrdinal = entry->ordinal;

    for (; i < kMaxColumns - 1; i++) {
        auto rightEntry = entry + kMaxEntriesPerColumn;
        for (int k = j; k > 0 && !rightEntry->visible(); k--)
            rightEntry--;

        if (rightEntry->visible())
            nextEntryOrdinal = rightEntry->ordinal;
    }

    if (nextEntryOrdinal == entry->ordinal) {
        auto arrowOrdinal = j >= kMaxEntriesPerColumn / 2 ? arrowDown : arrowUp;
        auto arrowEntry = getMenuEntry(arrowOrdinal);
        if (arrowEntry->visible())
            nextEntryOrdinal = arrowOrdinal;
    }

    return nextEntryOrdinal;
}

int findNextTopEntry(int, int j, const MenuEntry *entry)
{
    int nextEntryOrdinal = entry->ordinal;

    if (j > 0)
        return entry[-1].ordinal;

    if (getMenuEntry(arrowUp)->visible())
        return arrowUp;

    return nextEntryOrdinal;
}

int findNextBottomEntry(int, int j, const MenuEntry *entry)
{
    if (j < kMaxEntriesPerColumn - 1 && entry[1].visible())
        return entry[1].ordinal;

    if (getMenuEntry(inputSaveFilename)->visible())
        return inputSaveFilename;

    return SelectFilesMenu::abort;
}

void SelectFilesMenuTest::setupEntryTransitionsTest()
{
    bool useLongNames = m_currentDataIndex & 1;
    bool save = (m_currentDataIndex >> 1) & 1;
    int numFiles = m_currentDataIndex >> 2;

    auto filenames = generateFilenames(numFiles, useLongNames);

    showSelectFilesMenu("ENTRY TRANSITION TEST", filenames, save, nullptr);
}

// Makes sure arrow renders over background (and not any other menu item).
void SelectFilesMenuTest::testArrowBackground(const MenuEntry *arrowEntry)
{
    assert(arrowEntry && arrowEntry->visible() && (arrowEntry->ordinal == arrowUp || arrowEntry->ordinal == arrowDown));

    int x = arrowEntry->x + arrowEntry->width / 2 - kArrowWidth / 2;
    int y = arrowEntry->y + arrowEntry->height / 2 - kArrowHeight / 2;

    auto pixels = arrowEntry->ordinal == arrowUp ? upArrowBuffer : downArrowBuffer;
    auto screenPtr = linAdr384k + screenWidth * y + x;
    auto backgroundPtr = screenPtr + 2 * kVirtualScreenSize;

    for (int i = 0; i < kArrowHeight; i++) {
        for (int j = 0; j < kArrowWidth; j++) {
            if (!pixels[i * kArrowWidth + j]) {
                auto screenPixel = screenPtr[i * screenWidth + j];
                auto backgroundPixel = backgroundPtr[i * screenWidth + j];
                assertEqual(screenPixel, backgroundPixel);
            }
        }
    }
}

void SelectFilesMenuTest::checkArrowsOverlap()
{
    for (auto arrowIndex : { arrowUp, arrowDown }) {
        auto arrowEntry = getMenuEntry(arrowIndex);
        if (arrowEntry->invisible)
            continue;

        auto xLeft = arrowEntry->x;
        auto xRight = arrowEntry->x + arrowEntry->width;
        auto yTop = arrowEntry->y;
        auto yBottom = arrowEntry->y + arrowEntry->height;

        auto inInterval = [](int p, int p1, int p2) {
            return p >= p1 && p < p2;
        };

        auto entry = getMenuEntry(0);

        for (int i = 0; i < getCurrentMenu()->numEntries; i++, entry++) {
            if (entry->visible() && i != arrowIndex) {
                bool xOverlap = inInterval(entry->x, xLeft, xRight) || inInterval(entry->x + entry->width - 1, xLeft, xRight);
                bool yOverlap = inInterval(entry->y, yTop, yBottom) || inInterval(entry->y + entry->height - 1, yTop, yBottom);
                assertFalse(xOverlap && yOverlap);
            }
        }

        testArrowBackground(arrowEntry);
    }
}

void SelectFilesMenuTest::testEntryTransitions()
{
    // should be a test on its own, but it's much faster if we do it here
    checkArrowsOverlap();

    auto entry = getMenuEntry(kFirstFileEntry);

    for (int i = 0; i < kMaxColumns; i++) {
        for (int j = 0; j < kMaxEntriesPerColumn; j++, entry++) {
            if (entry->visible()) {
                static const auto kKeyFunctionPairs = {
                    std::make_pair(SDL_SCANCODE_LEFT, findNextLeftEntry),
                    std::make_pair(SDL_SCANCODE_RIGHT, findNextRightEntry),
                    std::make_pair(SDL_SCANCODE_UP, findNextTopEntry),
                    std::make_pair(SDL_SCANCODE_DOWN, findNextBottomEntry),
                };

                for (auto [keyCode, findNextEntry] : kKeyFunctionPairs) {
                    highlightEntry(entry);
                    simulateKey(keyCode);
                    auto expectedNextEntry = findNextEntry(i, j, entry);
                    assertEqual(expectedNextEntry, getCurrentEntryOrdinal());
                }
            }
        }
    }
}

void SelectFilesMenuTest::testAbortButton()
{
    SWOS_UnitTest::setMenuCallback([] {
        selectItem(SelectFilesMenu::abort);
        return false;
    });

    constexpr int kNumCases = 1 << 3;

    for (int i = 0; i < kNumCases; i++) {
        bool save = i & 1;
        bool gotFiles = (i >> 1) & 1;
        bool useLongNames = (i >> 2) & 1;
        int numFiles = gotFiles ? 5 : 0;

        auto filenames = generateFilenames(numFiles, useLongNames);

        char saveFilenameBuf[32] = "TUNGA MUNGA";
        auto selectedFilename = showSelectFilesMenu("TEST ABORT BUTTON", filenames, save, saveFilenameBuf);

        assertTrue(selectedFilename.empty());
    }

    SWOS_UnitTest::setMenuCallback(nullptr);
}

static int getNumberOfShownFileItems()
{
    int numShownItems = 0;

    for (auto entry = getMenuEntry(kFirstFileEntry); entry->ordinal < kFirstFileEntry + kNumFilenameItems; entry++)
        numShownItems += entry->visible() == true;

    return numShownItems;
}

void SelectFilesMenuTest::testSelectingFiles()
{
    constexpr int kNumFiles = 52;

    assert(kNumFiles > kNumFilenameItems);

    for (int i = 0; i < 4; i++) {
        bool useLongNames = i & 1;
        bool save = (i >> 1) & 1;

        auto filenames = generateFilenames(kNumFiles, useLongNames);

        constexpr char kInitialSaveFilename[] = "CANADA GRANADA";
        char saveFilenameBuf[32];
        strcpy(saveFilenameBuf, kInitialSaveFilename);

        int column = 0;
        int selectedFileIndex = 0;
        int currentEntryIndex = kFirstFileEntry;

        SWOS_UnitTest::setMenuCallback([&] {
            assertTrue(selectedFileIndex < kNumFiles);

            int numShownItems = getNumberOfShownFileItems();
            int numColumns = ColumnInfo::getNumColumns(useLongNames);
            int numScrollClicks = (kNumFiles + numColumns - 1) / numColumns - kMaxEntriesPerColumn;

            auto entry = getMenuEntry(currentEntryIndex);

            while (entry->invisible) {
                currentEntryIndex++;
                entry++;
                column = 1;
            }

            if (column >= kMaxEntriesPerColumn) {
                if (numScrollClicks > 0 && column < kMaxEntriesPerColumn + numScrollClicks) {
                    column++;
                    for (int i = 0; i < column - kMaxEntriesPerColumn; i++)
                        selectItem(arrowDown);
                } else {
                    column = 1;
                    currentEntryIndex += 2;
                    entry++;
                }
            } else {
                column++;
                if (column < kMaxEntriesPerColumn)
                    currentEntryIndex++;
            }

            if (entry->invisible) {
                while (entry->invisible) {
                    currentEntryIndex++;
                    entry++;
                    column = 1;
                }

                for (int i = 0; i < numScrollClicks; i++)
                    selectItem(arrowUp);

                currentEntryIndex++;
            }

            selectItem(entry);
            return false;
        });

        for (selectedFileIndex = 0; selectedFileIndex < kNumFiles; selectedFileIndex++) {
            auto selectedFilename = showSelectFilesMenu("TEST SELECTING FILES", filenames, save, saveFilenameBuf);
            auto expectedFilename = filenames[selectedFileIndex].name;
            assertEqual(selectedFilename, expectedFilename);
            assertEqual(saveFilenameBuf, save ? selectedFilename : kInitialSaveFilename);
        }
    }

    SWOS_UnitTest::setMenuCallback(nullptr);
}

static void scrollList(int direction, bool useWheel)
{
    assert(direction);

    if (useWheel) {
        queueSdlMouseWheelEvent(direction);
        updateControls();
        SWOS::MenuProc();
    } else {
        auto entryIndex = direction < 0 ? arrowDown : arrowUp;
        selectEntry(entryIndex);
    }
}

static void verifyEntries(const FoundFileList& filenames, const ColumnInfo& columnInfo, int scrollOffset, int maxScrollOffset)
{
    assert(scrollOffset >= 0);

    int columnStart = scrollOffset;
    auto entry = getMenuEntry(kFirstFileEntry);

    for (int i = 0; i < columnInfo.numColumns; i++, entry++) {
        for (int j = 0; j < kMaxEntriesPerColumn - 1; j++, entry++) {
            const auto& basename = getBasename(filenames[columnStart + j]);
            assertItemIsString(entry, basename.c_str());
        }

        bool lastRowItemExpectedVisibility = scrollOffset < maxScrollOffset || columnInfo.lastRow[i];

        if (entry->visible()) {
            assertTrue(lastRowItemExpectedVisibility);
            const auto& basename = getBasename(filenames[columnStart + kMaxEntriesPerColumn - 1]);
            assertItemIsString(entry, basename);
        } else {
            assertFalse(lastRowItemExpectedVisibility);
        }

        columnStart += columnInfo.baseColumnLength + columnInfo.lastRow[i];
    }
};

static void verifyScrolling(const FoundFileList& filenames, const ColumnInfo& columnInfo, int maxScrollOffset, bool useWheel)
{
    int scrollOffset = 0;

    for (int i = 0; i < 2 * maxScrollOffset + 1; i++) {
        int lastScrollOffset = scrollOffset;
        scrollOffset = i > maxScrollOffset ? 2 * maxScrollOffset - i : i;

        if (int direction = lastScrollOffset - scrollOffset)
            scrollList(direction, useWheel);

        verifyEntries(filenames, columnInfo, scrollOffset, maxScrollOffset);
    }
}

void SelectFilesMenuTest::testScrolling()
{
    constexpr int kNumCases = 1 << 6;

    for (int i = 0; i < kNumCases; i++) {
        bool useLongNames = i & 1;
        bool save = (i >> 1) & 1;
        bool empty = (i >> 2) & 1;
        bool useWheel = (i >> 3) & 1;
        int overflowItems = (i >> 4) & 3;

        int numFiles = 0;
        if (!empty)
            numFiles = kNumFilenameItems + ColumnInfo::getNumColumns(useLongNames) + overflowItems;

        auto filenames = generateFilenames(numFiles, useLongNames);
        showSelectFilesMenu("TEST SELECTING FILES", filenames, save);

        ColumnInfo columnInfo(numFiles, useLongNames);
        int maxScrollOffset = columnInfo.baseColumnLength + 1 - kMaxEntriesPerColumn;
        verifyScrolling(filenames, columnInfo, maxScrollOffset, useWheel);
    }
}

static void setupFakeFiles(std::initializer_list<const char *> fileList)
{
    resetFakeFiles();

    auto root = rootDir();
    addFakeDirectory(root.c_str());

    for (auto file : fileList)
        addFakeFile(joinPaths(root.c_str(), file).c_str());
}

void SelectFilesMenuTest::testAbortSave()
{
    SWOS_UnitTest::setMenuCallback([] {
        selectItem(SelectFilesMenu::abort);
        return false;
    });

    setupFakeFiles({ "bomb.hil", "tomb.hil", "romb.hil", "zomb.hil" });

    constexpr char *kSaveFilename = "MALI MIKA";
    char saveFilenameBuf[32];
    strcpy(saveFilenameBuf, kSaveFilename);

    A0 = saveFilenameBuf;
    D0 = *(dword *)"LIH.";
    A1 = "SAVE HIGHLIGHTS";

    SWOS::SelectFileToSaveDialog();

    assertTrue(D0);
    assertTrue(!strcmp(saveFilenameBuf, kSaveFilename));

    SWOS_UnitTest::setMenuCallback(nullptr);
}

void SelectFilesMenuTest::testSaveButtonShowHide()
{
    auto filenames = generateFilenames(5, false);

    char saveFilenameBuf[32] = "TUNGA MUNGA";
    showSelectFilesMenu("TEST ABORT BUTTON", filenames, true, saveFilenameBuf);

    assertItemIsVisible(saveLabel);

    saveFilenameBuf[0] = '\0';
    showSelectFilesMenu("TEST ABORT BUTTON", filenames, true, saveFilenameBuf);

    assertItemIsInvisible(saveLabel);

    showSelectFilesMenu("TEST ABORT BUTTON", filenames, true, nullptr);

    assertItemIsInvisible(saveLabel);
}

void SelectFilesMenuTest::testLongExtensions()
{
    setupFakeFiles({ "grak.diy", "grak.diyu", "grak.di", "grak.diyuuuu", "krak.diy" });

    A0 = "HUMBUG";
    D0 = *(dword *)"YID.";
    A1 = "PROWNING THE SHENNANS";

    SWOS::SelectFileToSaveDialog();

    auto beginEntry = getMenuEntry(kFirstFileEntry);
    auto endEntry = beginEntry + kNumFilenameItems;
    auto firstFileEntry = std::find_if(beginEntry, endEntry, [](const auto& entry) {
        return entry.visible();
    });

    assertTrue(firstFileEntry != endEntry);
    assertItemIsString(firstFileEntry, "GRAK");

    auto secondFileEntry = firstFileEntry + 1;

    assertTrue(secondFileEntry != endEntry);
    assertItemIsString(secondFileEntry, "KRAK");

    auto noMoreFiles = std::all_of(secondFileEntry + 1, endEntry, [](const auto& entry) {
        return entry.invisible;
    });

    assertTrue(noMoreFiles);
}

static MenuEntry *findEntry(const char *text)
{
    auto menu = getCurrentMenu();
    auto entry = getMenuEntry(0);
    auto entrySentinel = entry + menu->numEntries;

    for (; entry < entrySentinel; entry++)
        if (entry->isString() && !strcmp(entry->string(), text))
            return entry;

    assertFalse("Failed to find entry with given text");
    return entry - 1;
}

void SelectFilesMenuTest::testSaveCompetition()
{
    resetFakeFiles();

    auto root = rootDir();
    addFakeDirectory(root.c_str());

    auto path = joinPaths(root.c_str(), "canada.diy");
    MockFile canadaDiy(path.c_str(), kCanadaDiyData, kCanadaDiySize);
    addFakeFile(canadaDiy);

    SWOS::InitMainMenu();
    SWOS::MenuProc();

    SWOS_UnitTest::setMenuCallback([] {
        auto menu = getCurrentMenu();
        auto entry = menu->selectedEntry;

        assertItemIsString(entry, "CANADA");
        selectItem(entry);

        SWOS_UnitTest::setMenuCallback([] {
            auto exitEntry = findEntry("EXIT");
            selectItem(exitEntry);

            SWOS_UnitTest::setMenuCallback();
            return false;
        });

        return false;
    });

    SAFE_INVOKE(LoadOldCompetitionMenu);

    SWOS_UnitTest::setMenuCallback([] {
        auto entry = getMenuEntry(inputSaveFilename);
        assertItemIsString(entry, "CANADA");

        queueSdlKeyDown(SDL_SCANCODE_Z);
        queueSdlKeyDown(SDL_SCANCODE_RETURN);
        menuCycleTimer = 2;
        selectItem(entry);

        return false;
    });

    auto menu = getCurrentMenu();
    auto entry = findEntry("SAVE CANADA ROGERS CUP");
    selectItem(entry);

    auto savedDiyPath = joinPaths(root.c_str(), "CANADAZ.DIY");
    assertTrue(fakeFilesEqualByContent(path.c_str(), savedDiyPath.c_str()));
}
