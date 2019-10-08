#include "selectFilesMenu.h"
#include "selectFiles.mnu.h"
#include "menuMouse.h"

using namespace SelectFilesMenu;

constexpr int kShortNameWidth = 56;
constexpr int kLongNameWidth = 108;
constexpr int kVerticalMargin = 1;

constexpr int kMarginWidth = 8;
constexpr int kTypeFieldWidth = 39;
constexpr int kTypeFieldMargin = 1;

constexpr int kSingleColumnCutOff = 8;

constexpr int kShortNameFullWidth = kShortNameWidth + kTypeFieldMargin + kTypeFieldWidth;
constexpr int kLongNameFullWidth = kLongNameWidth + kTypeFieldMargin + kTypeFieldWidth;

static_assert(3 * (kShortNameFullWidth + kMarginWidth) + kMarginWidth == kMenuScreenWidth, "");
static_assert(2 * (kLongNameFullWidth + kMarginWidth) + kMarginWidth == kMenuScreenWidth, "");

using EntriesPerColumnList = std::array<int, kMaxColumns>;

static const char *m_menuTitle;
static std::string m_selectedFilename;
static bool m_saving;
static char *m_saveFilename;

static int m_numColumns;
static bool m_longNames;
static EntriesPerColumnList m_numEntriesPerColumn;

static FoundFileList m_filenames;
static int m_scrollOffset;
static int m_maxScrollOffset;

// since entries lack proper user field, we'll have to use an additional array to associate them to belonging files
static std::array<int, kNumFilenameItems> m_entryToFilename;

static int extensionToCode(const std::string& selectedFilename)
{
    int result = 0;
    int extOffset = selectedFilename.size() - 4;

    if (selectedFilename.size() >= 5 && selectedFilename[extOffset] == '.') {
        result = ('.' << 24) |
            (toupper(selectedFilename[extOffset + 1]) << 16) |
            (toupper(selectedFilename[extOffset + 2]) << 8) |
            toupper(selectedFilename[extOffset + 3]);
    }

    return result;
}

static const char *codeToExtension(dword packedExt)
{
    union {
        char ext[5];
        dword d;
    } static u;

    u.d = D0;
    std::swap(u.ext[0], u.ext[3]);
    std::swap(u.ext[1], u.ext[2]);
    u.ext[4] = '\0';

    return u.ext;
}

FoundFileList findFiles(dword packedExtension)
{
    static const char *kAllowedExtensions[] = {
        "DIY", "PRE", "SEA", "SEA", "CAR", "RPL", "TAC", "HIL",
    };

    auto ext = codeToExtension(packedExtension);
    auto numAllowedExtensions = std::size(kAllowedExtensions) - (g_skipNonCompetitionFiles ? 3 : 0);

    return findFiles(ext, kAllowedExtensions, numAllowedExtensions);
}

static char *getFilenameBuffer(const std::string& selectedFilename)
{
    static char filenameBuffer[kMaxPath];
    strncpy_s(filenameBuffer, sizeof(filenameBuffer), selectedFilename.c_str(), _TRUNCATE);

    return filenameBuffer;
}

// SWOS expects 0 for success, 1 for error
static bool getSwosErrorCode(const char *ptr)
{
    return !ptr || !ptr[0];
}

static bool getFilenameAndExtension()
{
    auto savedExtension = D0.asDword();
    auto menuTitle = A0.asPtr();

    auto files = findFiles(savedExtension);

    auto selectedFilename = showSelectFilesMenu(menuTitle, files);

    A0 = getFilenameBuffer(selectedFilename);
    D1 = savedExtension;

    if (!D1)
        D1 = extensionToCode(selectedFilename);

    return getSwosErrorCode(A0);
}

// GetFilenameAndExtension
//
// Creates a menu from files with extension in D1, and returns the name of selected file in A0.
//
// in:
//      D0 - extension to search for, 32-bit char constant, dot is in highest byte (0 = search for all supported)
//      A0 -> menu title
//      A1 ->   -||-
// out:
//      D0 - 1 = success (zero flag set)
//           0 = no files selected (zero flag clear)
//      D1 - extension
//      A0 -> selected filename
//      zero flag - clear = error
//                  set = OK
// globals:
//   read:
//     g_skipNonCompetitionFiles - if -1, .TAC and .HIL files
//                                 are not searched for
//   write:
//     selectedFilename
//     extension
//
__declspec(naked) int SWOS::GetFilenameAndExtension()
{
    __asm {
        call getFilenameAndExtension
        call setZeroFlagAndD0FromAl
        retn
    }
}

static int selectFileToSaveDialog()
{
    auto ext = codeToExtension(D0);
    auto files = findFiles(ext);

    auto menuTitle = A1.asPtr();
    auto saveFilename = A0.asPtr();

    auto selectedFilename = showSelectFilesMenu(menuTitle, files, true, saveFilename);
    if (!selectedFilename.empty())
        selectedFilename += ext;

    A0 = getFilenameBuffer(selectedFilename);
    return getSwosErrorCode(A0);
}

// SelectFileToSaveDialog
//
// in:
//      D0 - file extension
//      A0 - buffer for selected filename
//      A1 - menu header
// out:
//      A0 -> selected file name
//      zero flag - set = canceled or error, clear = OK
//      (D0 also)
//
__declspec(naked) int SWOS::SelectFileToSaveDialog()
{
    __asm {
        call selectFileToSaveDialog
        call setZeroFlagAndD0FromAl
        retn
    }
}

std::string showSelectFilesMenu(const char *menuTitle, const FoundFileList& filenames,
    bool saving /* = false */, char *saveFilename /* = nullptr */)
{
    assert(menuTitle);

    m_menuTitle = menuTitle;
    m_filenames = filenames;
    m_saving = saving;
    m_saveFilename = saveFilename;

    showMenu(selectFilesMenu);

    return m_selectedFilename;
}

static void selectInitialEntry()
{
    if (m_saving)
        setCurrentEntry(m_saveFilename && *m_saveFilename ? saveLabel : inputSaveFilename);
    else if (m_filenames.empty())
        setCurrentEntry(SelectFilesMenu::abort);
}

static void sortFilenames()
{
    // sort by extension first, filename second
    std::sort(m_filenames.begin(), m_filenames.end(), [](const auto& file1, const auto& file2) {
        auto name1 = file1.name.c_str();
        auto name2 = file2.name.c_str();

        auto ext1 = name1 + file1.extensionOffset;
        auto ext2 = name2 + file2.extensionOffset;

        auto result = _stricmp(ext1, ext2);
        if (!result)
            result = _stricmp(name1, name2);

        return result < 0;
    });
}

static void copyStringUntilDot(char *dest, const char *src)
{
    if (src)
        while (*src && *src != '.')
            *dest++ = *src++;

    *dest = '\0';
}

static void setSaveFilename()
{
    if (m_saving) {
        auto saveFilenameEntry = getMenuEntry(inputSaveFilename);

        assert(!m_saveFilename || strlen(m_saveFilename) < kMenuStringLength);

        auto dest = saveFilenameEntry->string();
        copyStringUntilDot(dest, m_saveFilename);

        saveFilenameEntry->show();

        if (m_saveFilename && *m_saveFilename)
            getMenuEntry(saveLabel)->show();
    }
}

static bool isLongFilename(const FoundFile& file)
{
    assert(file.extensionOffset < file.name.size());

    auto extensionLength = file.name.size() - file.extensionOffset;
    auto baseLength = file.extensionOffset - 1;

    return baseLength > 8 || extensionLength > 3;
}

static void splitIntoColumns()
{
    assert(m_numEntriesPerColumn.size() > 0);

    m_scrollOffset = 0;
    m_maxScrollOffset = 0;

    std::fill(m_numEntriesPerColumn.begin(), m_numEntriesPerColumn.end(), 0);

    m_longNames = std::any_of(m_filenames.begin(), m_filenames.end(), isLongFilename);
    m_numColumns = kMaxColumns - (m_longNames != 0);

    if (m_filenames.size() <= kSingleColumnCutOff) {
        m_numEntriesPerColumn[(kMaxColumns - 1) / 2] = m_filenames.size();
        m_numColumns = 1;
        m_maxScrollOffset = 0;
    } else {
        auto quotRem = std::div(m_filenames.size(), m_numColumns);

        auto baseNumberOfEntriesPerColumn = quotRem.quot;
        auto remainder = quotRem.rem;

        auto startingIndex = (m_numColumns - remainder) / 2;

        for (int i = 0; i < m_numColumns; i++) {
            m_numEntriesPerColumn[i] = baseNumberOfEntriesPerColumn;
            // divide remainder equally around columns
            if (i >= startingIndex && remainder > 0) {
                m_numEntriesPerColumn[i]++;
                remainder--;
            }
        }

        auto maxColumn = *std::max_element(m_numEntriesPerColumn.begin(), m_numEntriesPerColumn.end());
        m_maxScrollOffset = std::max(0, maxColumn - kMaxEntriesPerColumn);
    }
}

static void setScrollArrowsNextEntries(MenuEntry *scrollUpArrow, MenuEntry *scrollDownArrow)
{
    int topRightEntry = -1;
    int bottomRightEntry = -1;

    auto getVisibleEntryOrdinalAtCoordinates = [](int i, int j) {
        int entryIndex = kFirstFileEntry + i * kMaxEntriesPerColumn + j;
        auto entry = getMenuEntry(entryIndex);
        return entry->visible() ? entry->ordinal : -1;
    };

    for (int i = kMaxColumns - 1; i >= 0; i--) {
        for (int j = 0; j < kMaxEntriesPerColumn && topRightEntry < 0; j++)
            topRightEntry = getVisibleEntryOrdinalAtCoordinates(i, j);
        for (int j = kMaxEntriesPerColumn - 1; j >= 0 && bottomRightEntry < 0; j--)
            bottomRightEntry = getVisibleEntryOrdinalAtCoordinates(i, j);
    }

    assert(topRightEntry >= 0 && bottomRightEntry >= 0);

    scrollUpArrow->leftEntry = topRightEntry;
    scrollDownArrow->leftEntry = bottomRightEntry;
}

static void setLastFilenameColumnNextEntries()
{
    for (int i = 0; i < kMaxEntriesPerColumn; i++) {
        int entryIndex = kFirstFileEntry + kMaxEntriesPerColumn + i;
        auto entry = getMenuEntry(entryIndex);

        if (m_numColumns == kMaxColumns)
            entry->rightEntry = entryIndex + kMaxEntriesPerColumn;
        else
            entry->rightEntry = entry[kMaxEntriesPerColumn].rightEntry;
    }
}

static int findArrowsX()
{
    int x = kDefaultArrowX + kMenuOffset;

    if (!m_saving)
        return x;

    auto widthPerItem = m_longNames ? kLongNameFullWidth : kShortNameFullWidth;
    auto entry = getMenuEntry(kFirstFileEntry);

    for (int i = 0; i < kNumFilenameItems; i++, entry++) {
        if (entry->visible() && entry->x + widthPerItem >= kDefaultArrowX) {
            x = entry->x + widthPerItem;
            break;
        }
    }

    return x;
}

static std::pair<int, int> getFilenameAreaVerticalBounds()
{
    auto entry = getMenuEntry(kFirstFileEntry);
    auto topY = entry->y;
    auto bottomY = entry->y + entry->height;

    for (int i = 0; i < kNumFilenameItems; i++, entry++) {
        if (entry->visible()) {
            if (entry->y + entry->height > bottomY)
                bottomY = entry->y + entry->height;
            if (entry->y < topY)
                topY = entry->y;
        }
    }

    return { topY, bottomY };
}

static void updateScrollArrows(int startY, int verticalSize, int verticalSlack)
{
    auto showArrows = std::any_of(m_numEntriesPerColumn.begin(), m_numEntriesPerColumn.end(), [](auto numItemsInColumn) {
        return numItemsInColumn > kMaxEntriesPerColumn;
    });

    auto scrollUpArrow = getMenuEntry(arrowUp);
    auto scrollDownArrow = getMenuEntry(arrowDown);

    scrollUpArrow->setVisible(showArrows);
    scrollDownArrow->setVisible(showArrows);

    if (showArrows) {
        scrollUpArrow->x = scrollDownArrow->x = findArrowsX();

        constexpr int kArrowVerticalMargin = 2;
        scrollUpArrow->y = startY - verticalSlack + kArrowVerticalMargin;
        scrollDownArrow->y = startY + verticalSize + kArrowVerticalMargin;

        if (m_saving) {
            auto yBounds = getFilenameAreaVerticalBounds();
            scrollUpArrow->y = yBounds.first - 1;
            scrollDownArrow->y = yBounds.second - scrollDownArrow->height + 2;
        }

        setScrollArrowsNextEntries(scrollUpArrow, scrollDownArrow);
        setLastFilenameColumnNextEntries();
    }
}

static std::tuple<int, int, int> repositionFilenameEntries()
{
    assert(!m_numEntriesPerColumn.empty());

    auto titleEntry = getMenuEntry(title);

    int startY = titleEntry->y;
    if (titleEntry->visible())
        startY += titleEntry->height;

    auto maxEntriesPerColumn = *std::max_element(m_numEntriesPerColumn.begin(), m_numEntriesPerColumn.end());
    maxEntriesPerColumn = std::min(maxEntriesPerColumn - m_scrollOffset, kMaxEntriesPerColumn);

    constexpr int kTotalFilenameHeight = kFilenameHeight + kVerticalMargin;
    auto verticalSize = maxEntriesPerColumn * kTotalFilenameHeight;

    auto lastEntry = getMenuEntry(m_saving ? inputSaveFilename : SelectFilesMenu::abort);
    int verticalSlack = (lastEntry->y - startY - verticalSize) / 2;
    startY += verticalSlack;

    auto filenameEntry = getMenuEntry(fileEntry_00);
    auto totalWidthPerItem = (m_longNames ? kLongNameFullWidth : kShortNameFullWidth) + kMarginWidth;
    int xOffset = m_longNames && m_numColumns == 1 ? -totalWidthPerItem / 2 : 0;

    for (int i = 0; i < kMaxColumns; i++) {
        int x = kMarginWidth + i * totalWidthPerItem;
        int numColumns = std::min(m_numEntriesPerColumn[i] - m_scrollOffset, kMaxEntriesPerColumn);

        for (int j = 0; j < numColumns; j++, filenameEntry++) {
            filenameEntry->width = m_longNames ? kLongNameWidth : kShortNameWidth;
            filenameEntry->x = x + xOffset;
            filenameEntry->y = startY + j * kTotalFilenameHeight;
        }

        filenameEntry += kMaxEntriesPerColumn - numColumns;
    }

    return { startY, verticalSize, verticalSlack };
}

static void assignFilenamesToEntries()
{
    assert(m_numColumns > 0);
    assert(m_scrollOffset >= 0 && m_scrollOffset <= m_maxScrollOffset);
    assert(m_scrollOffset * m_numColumns <= static_cast<int>(m_filenames.size()));

    size_t numShownEntries = 0;

    auto entry = getMenuEntry(fileEntry_00);
    int entryIndex = 0;
    int fileIndex = m_scrollOffset;

    for (int i = 0; i < kMaxColumns; i++) {
        int numColumns = std::min(m_numEntriesPerColumn[i] - m_scrollOffset, kMaxEntriesPerColumn);
        numColumns = std::max(numColumns, 0);

        // not very cache friendly, but what can ya do...
        for (int j = 0; j < numColumns; j++, entry++) {
            entry->show();

            auto entryIndex = entry->ordinal - kFirstFileEntry;
            m_entryToFilename[entryIndex] = fileIndex + j;

            const auto& filename = m_filenames[fileIndex + j];
            auto basenameLength = filename.extensionOffset - 1;
            auto stringLength = std::min<size_t>(basenameLength, kMenuStringLength - 1);
            auto str = entry->string();

            memcpy(str, filename.name.c_str(), stringLength);
            str[stringLength] = '\0';
            if (m_longNames)
                elideString(str, kMenuStringLength, kLongNameWidth - 1);
        }

        for (int j = numColumns; j < kMaxEntriesPerColumn; j++, entry++) {
            entry->hide();

            auto entryIndex = entry->ordinal - kFirstFileEntry;
            m_entryToFilename[entryIndex] = -1;
        }

        fileIndex += m_numEntriesPerColumn[i];
    }
}

static void selectFilesOnReturn()
{
    auto titleString = const_cast<char *>(m_menuTitle);
    getMenuEntry(title)->setString(titleString);

    setSaveFilename();
    splitIntoColumns();

    int startY, verticalSize, verticalSlack;
    std::tie(startY, verticalSize, verticalSlack) = repositionFilenameEntries();

    assignFilenamesToEntries();
    updateScrollArrows(startY, verticalSize, verticalSlack);
}

static void selectFilesOnInit()
{
    setGlobalWheelEntries(arrowUp, arrowDown);

    selectInitialEntry();
    sortFilenames();

    selectFilesOnReturn();
}

static const char *getFileTypeFromExtension(const char *ext)
{
    auto type = "HIGH";

    if (!strcmp(ext, "PRE"))
        type = "PRESET";
    else if (!strcmp(ext, "SEA"))
        type = "SEASON";
    else if (!strcmp(ext, "CAR"))
        type = "CAREER";
    else if (!strcmp(ext, "TAC"))
        type = "TACT";
    else if (!strcmp(ext, "RPL"))
        type = "REPLAY";
    else if (!strcmp(ext, "DIY"))
        type = "DIY";
    else if (strcmp(ext, "HIL"))
        assert(false);

    return type;
}

static int getFileIndex(const MenuEntry *entry)
{
    if (entry->ordinal < kFirstFileEntry || entry->ordinal >= kFirstFileEntry + kNumFilenameItems) {
        assert(false);
        return -1;
    }

    return m_entryToFilename[entry->ordinal - kFirstFileEntry];
}

static void drawDescription()
{
    auto entry = A5.asMenuEntry();

    auto fileIndex = getFileIndex(entry);
    if (fileIndex < 0)
        return;

    if (fileIndex >= 0 && fileIndex < static_cast<int>(m_filenames.size())) {
        const auto& file = m_filenames[fileIndex];
        auto ext = file.name.c_str() + file.extensionOffset;
        auto fileType = getFileTypeFromExtension(ext);

        auto descEntry = getMenuEntry(description);
        descEntry->u2.constString = fileType;

        descEntry->x = entry->x + entry->width + kTypeFieldMargin;
        descEntry->y = entry->y;

        descEntry->show();
        drawMenuItem(descEntry);
        descEntry->hide();
    }
}

static void selectFile()
{
    const auto entry = A5.as<MenuEntry *>();

    auto fileIndex = getFileIndex(entry);
    if (fileIndex >= 0)
        m_selectedFilename = m_filenames[fileIndex].name;
    else
        m_selectedFilename.clear();

    // opportunity for overflow, all places that call have had their buffers widened enough, until we can remove it entirely
    if (m_saving && m_saveFilename)
        strcpy(m_saveFilename, m_selectedFilename.c_str());

    SetExitMenuFlag();
}

static void abortSelectFile()
{
    m_selectedFilename.clear();
    SetExitMenuFlag();
}

static void inputFilenameToSave()
{
    auto entry = A5.asMenuEntry();
    auto entryString = entry->string();

    if (inputText(entryString, kMaxSaveFilenameChars, true)) {
        assert(m_saveFilename && m_saving);

        if (m_saveFilename) {
            copyStringUntilDot(m_saveFilename, entryString);
            m_selectedFilename = m_saveFilename;
        }

        SetExitMenuFlag();
    }
}

static void saveFileSelected()
{
    auto inputFilenameEntry = getMenuEntry(inputSaveFilename);
    m_selectedFilename = inputFilenameEntry->string();

    if (m_saving && m_saveFilename)
        strcpy(m_saveFilename, m_selectedFilename.c_str());

    SetExitMenuFlag();
}

static void scrollUp()
{
    if (m_scrollOffset) {
        m_scrollOffset--;
        assignFilenamesToEntries();
    }
}

static void scrollDown()
{
    if (m_scrollOffset < m_maxScrollOffset) {
        m_scrollOffset++;
        assignFilenamesToEntries();
    }
}
