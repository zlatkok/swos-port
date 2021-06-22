#include "unitTest.h"
#include "menus.h"
#include "file.h"
#include "scanCodes.h"
#include "sdlProcs.h"
#include "windowManager.h"

#undef assertEqual

static SWOS_UnitTest::MenuCallback m_menuCallback;

static MenuEntry *getVerifiedEntry(int index, const char *indexStr, const char *file, int line)
{
    if (index < 0 || index > 256)
        throw SWOS_UnitTest::InvalidEntryIndexException(index, indexStr, file, line);

    return getMenuEntry(index);
}

static void verifyNumericItem(const MenuEntry *entry, const char *file, int line)
{
    if (entry->type != kEntryNumber)
        throw SWOS_UnitTest::InvalidEntryTypeException(entry, "number", file, line);
}

void SWOS_UnitTest::setMenuCallback(MenuCallback callback /* = nullptr */)
{
    m_menuCallback = callback;
}

bool SWOS_UnitTest::exitMenuProc()
{
    if (m_menuCallback) {
        static bool result;
        auto invokeCallback = []() { result = m_menuCallback(); };
        invokeWithSaved68kRegisters(invokeCallback);
        return result;
    }

    return true;
}

void SWOS_UnitTest::assertTrueImp(bool expression, const char *exprStr, const char *file, int line)
{
    if (!expression)
        throw FailedAssertTrueException(exprStr, file, line);
}

void SWOS_UnitTest::assertStringEqualImp(const char *s1, const char *s2, const char *s1Str, const char *s2Str, const char * file, int line)
{
    if (_stricmp(s1, s2) != 0)
        throw FailedEqualAssertException(true, s1, s2, s1Str, s2Str, file, line);
}

void SWOS_UnitTest::assertNumItemsImp(int expectedNumItems, const char *expectedStr, const char *file, int line)
{
    int actualNumItems = getCurrentMenu()->numEntries;
    if (actualNumItems != expectedNumItems)
        throw InvalidNumberOfEntriesInMenu(actualNumItems, expectedNumItems, expectedStr, file, line);
}

void SWOS_UnitTest::assertItemIsNumberImp(int index, const char *indexStr, int value, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    verifyNumericItem(entry, file, line);

    if (entry->fg.number != value)
        throw InvalidEntryNumericValueException(entry, value, file, line);
}

void SWOS_UnitTest::assertItemIsVisibleImp(int index, const char *indexStr, bool visible, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    assertItemIsVisibleImp(entry, indexStr, visible, file, line);
}

void SWOS_UnitTest::assertItemIsVisibleImp(const MenuEntry *entry, const char *, bool visible, const char *file, int line)
{
    if (!entry->invisible ^ visible)
        throw InvalidEntryVisibilityException(entry, file, line);
}

void SWOS_UnitTest::assertItemEnabledImp(int index, const char *indexStr, bool enabled, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if (!entry->disabled ^ enabled)
        throw InvalidEntryStatusException(entry, file, line);
}

void SWOS_UnitTest::assertItemIsStringImp(int index, const char *indexStr, const char *value, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    assertItemIsStringImp(entry, value, value, file, line);
}

void SWOS_UnitTest::assertItemIsStringImpCaseInsensitive(int index, const char *indexStr, const char *value, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    assertItemIsStringImp(entry, value, value, file, line);
}

void SWOS_UnitTest::assertItemIsStringImp(const MenuEntry *entry, const char *, const std::string& value, const char *file, int line)
{
    assertItemIsStringImp(entry, nullptr, value.c_str(), file, line);
}

void SWOS_UnitTest::assertItemIsStringImp(const MenuEntry *entry, const char *, const char *value, const char *file, int line, bool matchCase /* = true */)
{
    assert(entry);

    if (entry->type != kEntryString)
        throw InvalidEntryTypeException(entry, "string", file, line);
    int different = matchCase ? strcmp(entry->string(), value) : _stricmp(entry->string(), value);
    if (different)
        throw EntryStringMismatch(entry, value, file, line);
}

void SWOS_UnitTest::assertItemIsStringTableImp(int index, const char *indexStr, const char *value, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if (entry->type != kEntryStringTable)
        throw InvalidEntryTypeException(entry, "string table", file, line);
    if (strcmp(entry->fg.stringTable->currentString(), value))
        throw EntryStringMismatch(entry, value, file, line);
}

void SWOS_UnitTest::assertItemIsSpriteImp(int index, const char *indexStr, int spriteIndex,
    const char *spriteIndexStr, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if (entry->type != kEntrySprite2)
        throw InvalidEntryTypeException(entry, "sprite", file, line);
    if (spriteIndex >= 0)
        assertEqualImp(true, static_cast<int>(entry->fg.spriteIndex), spriteIndex, "sprite index", spriteIndexStr, file, line);
}

void SWOS_UnitTest::assertItemHasColorImp(int index, const char *indexStr, int color, const char *colorStr, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if (entry->bg.entryColor != color)
        throw InvalidEntryColorException(entry, color, colorStr, file, line);
}

void SWOS_UnitTest::assertItemHasTextColorImp(int index, const char *indexStr, int color, const char *colorStr, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if ((entry->stringFlags & 0x0f) != color)
        throw InvalidEntryColorException(entry, color, colorStr, file, line, true);
}

int SWOS_UnitTest::numItems()
{
    return getCurrentMenu()->numEntries;
}

bool SWOS_UnitTest::isItemVisible(int ordinal)
{
    return !getMenuEntry(ordinal)->invisible;
}

void SWOS_UnitTest::queueKeys(const std::vector<SDL_Scancode>& keys)
{
    assert(!keys.empty());

    for (auto scancode : keys) {
        queueSdlKeyDown(scancode);
        queueSdlKeyUp(scancode);
    }
}

// Translate coordinates since they will be later mapped from window coordinates to game coordinates
static void setMouseState(int x, int y, bool leftClick = false)
{
    auto [width, height] = getWindowSize();
    x = x * width / kVgaWidth;
    y = y * height / kVgaHeight;
    setSdlMouseState(x, y, leftClick);
}

void SWOS_UnitTest::sendMouseWheelEventImp(int index, const char *indexStr, int direction, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);

    int x = entry->x + entry->width / 2;
    int y = entry->y + entry->height / 2;
    setMouseState(x, y);
    queueSdlMouseWheelEvent(direction);
}

void SWOS_UnitTest::selectItemImp(int index, const char *indexStr, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    selectItemImp(entry, nullptr, file, line);
}

void SWOS_UnitTest::selectItemImp(MenuEntry *entry, const char *, const char *file, int line)
{
    if (!entry->onSelect)
        throw EntryOnSelectMissing(entry, file, line);

    assertItemIsVisibleImp(entry, nullptr, true, file, line);

    getCurrentMenu()->selectedEntry = entry;
    selectEntry(entry);
}

void SWOS_UnitTest::clickItemImp(int index, const char *indexStr, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if (!entry->onSelect)
        throw EntryOnSelectMissing(entry, file, line);

    assertItemIsVisibleImp(index, indexStr, true, file, line);

    setMouseState(entry->x + entry->width / 2, entry->y + entry->height / 2, true);
}

void SWOS_UnitTest::setNumericItemValueImp(int index, const char *indexStr, int value, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    verifyNumericItem(entry, file, line);
    entry->fg.number = value;
}
