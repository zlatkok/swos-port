#include "unitTest.h"
#include "menu.h"
#include "file.h"
#include "scanCodes.h"
#include "sdlProcs.h"
#include "render.h"

#undef assertEqual

static MenuEntry *getVerifiedEntry(int index, const char *indexStr, const char *file, int line)
{
    if (index < 0 || index > 256)
        throw SWOS_UnitTest::InvalidEntryIndexException(index, indexStr, file, line);
    
    return getMenuEntryAddress(index);
}

static void verifyNumericItem(const MenuEntry *entry, const char *file, int line)
{
    if (entry->type2 != kEntryNumber)
        throw SWOS_UnitTest::InvalidEntryTypeException(entry, "number", file, line);
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

    if (entry->u2.number != value)
        throw InvalidEntryNumericValueException(entry, value, file, line);
}

void SWOS_UnitTest::assertItemIsVisibleImp(int index, const char *indexStr, bool visible, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
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
    if (entry->type2 != kEntryString)
        throw InvalidEntryTypeException(entry, "string", file, line);
    if (strcmp(entry->u2.string, value))
        throw EntryStringMismatch(entry, value, file, line);
}

void SWOS_UnitTest::assertItemIsStringTableImp(int index, const char *indexStr, const char *value, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if (entry->type2 != kEntryStringTable)
        throw InvalidEntryTypeException(entry, "string table", file, line);
    if (strcmp(entry->u2.stringTable->currentString(), value))
        throw EntryStringMismatch(entry, value, file, line);
}

void SWOS_UnitTest::assertItemIsSpriteImp(int index, const char *indexStr, int spriteIndex,
    const char *spriteIndexStr, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if (entry->type2 != kEntrySprite2)
        throw InvalidEntryTypeException(entry, "sprite", file, line);
    if (spriteIndex >= 0)
        assertEqual(static_cast<int>(entry->u2.spriteIndex), spriteIndex, "sprite index", spriteIndexStr, file, line);
}

void SWOS_UnitTest::assertItemHasColorImp(int index, const char *indexStr, int color, const char *colorStr, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if (entry->u1.entryColor != color)
        throw InvalidEntryColorException(entry, color, colorStr, file, line);
}

void SWOS_UnitTest::assertItemHasTextColorImp(int index, const char *indexStr, int color, const char *colorStr, const char *file, int line)
{
    auto entry = getVerifiedEntry(index, indexStr, file, line);
    if ((entry->textColor & 0x0f) != color)
        throw InvalidEntryColorException(entry, color, colorStr, file, line, true);
}

int SWOS_UnitTest::numItems()
{
    return getCurrentMenu()->numEntries;
}

bool SWOS_UnitTest::isItemVisible(int ordinal)
{
    return !getMenuEntryAddress(ordinal)->invisible;
}

bool SWOS_UnitTest::queueKeys(const std::vector<SDL_Scancode>& keys)
{
    assert(!keys.empty());

    size_t i = 0;
    while (keyCount < std::size(keyBuffer) && i < keys.size()) {
        auto pcKey = sdlScanCodeToPc(keys[i++]);
        keyBuffer[keyCount++] = pcKey;
        lastKey = pcKey;
    }

    return i >= keys.size();
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
    if (!entry->onSelect)
        throw EntryOnSelectMissing(entry, file, line);

    assertItemIsVisibleImp(index, indexStr, true, file, line);

    getCurrentMenu()->selectedEntry = entry;
    A5 = entry;
    entry->onSelect();
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
    entry->u2.number = value;
}
