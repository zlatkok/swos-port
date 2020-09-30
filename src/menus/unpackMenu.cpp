#include "menu.h"
#include "menuCodes.h"

// last menu sent here for unpacking
static const void *m_currentMenu;

static void initMenuEntry(MenuEntry& entry);
static void finalizeMenu(Menu *dstMenu, char *menuEnd, int numEntries);
static void handleSkip(MenuEntry *dstEntry, size_t index, const int16_t *& data);
#ifdef SWOS_VM
static dword fetchAndTranslateProc(const int16_t *& data);
static dword copyStringToVM(const char *str);
static dword translateStringTable(const StringTableNative& strTable);
static dword translateMultiLineTable(const MultiLineTextNative& text);
#endif

const void *getCurrentPackedMenu()
{
    return m_currentMenu;
}

// UnpackMenu
//
// in:
//      A1 -> pointer to destination buffer that will take converted menu
//            (usually g_currentMenu)
//      A0 -> static menu for conversion
//
// Unpacks static menu in A0 to screen menu (held in g_currentMenu).
// Doesn't execute any of the menu functions, only does the unpacking.
//
void SWOS::UnpackMenu()
{
    upackMenu(A0.asPtr());
}

// Doesn't execute any of the menu functions, only does the conversion.
void upackMenu(const void *src, char *dst)
{
    using namespace SWOS_Menu;

    assert(dst >= reinterpret_cast<char *>(swos.g_currentMenu) &&
        dst < reinterpret_cast<char *>(swos.g_currentMenu + sizeof(swos.g_currentMenu)));

    m_currentMenu = src;

    int menuX = 0, menuY = 0;

    MenuEntry templateEntry;
    initMenuEntry(templateEntry);

    auto dstMenu = reinterpret_cast<Menu *>(dst);
    auto headerMark = *reinterpret_cast<const int32_t *>(src);
    const int16_t *data;
    int initialEntry;

    if (headerMark == kMenuHeaderV2Mark) {
        auto header = reinterpret_cast<const MenuHeaderV2 *>(src);

        const auto kFuncs = {
            std::make_tuple(&dstMenu->onInit, header->onInit, header->nativePtr[0]),
            std::make_tuple(&dstMenu->onReturn, header->onReturn, header->nativePtr[1]),
            std::make_tuple(&dstMenu->onDraw, header->onDraw, header->nativePtr[2])
        };

        for (const auto& func : kFuncs) {
            auto dst = std::get<0>(func);
            auto src = std::get<1>(func);
            if (auto isNative = std::get<2>(func))
                *dst = SwosVM::registerProc(src);
            else
                *dst = reinterpret_cast<SwosProcPointer *>(src);
        }

        initialEntry = header->initialEntry;
        data = header->data();
    } else {
        auto header = reinterpret_cast<const PackedMenu *>(src);
        dstMenu->onInit = header->onInit;
        dstMenu->onReturn = header->onReturn;
        dstMenu->onDraw = header->onDraw;
        dstMenu->selectedEntry.setRaw(header->initialEntry);
        data = header->data();
        initialEntry = header->initialEntry;
    }

    auto dstEntry = reinterpret_cast<MenuEntry *>(dst + sizeof(Menu));
    MenuEntry *savedEntry = nullptr;
    int numEntries = 0;

    while (*data != kEndOfMenu) {
        switch (*data) {
        case kMenuXY:
            data++;
            menuX = *data++;
            menuY = *data++;
            continue;

        case kResetTemplate:
            initMenuEntry(templateEntry);
            data++;
            continue;

        case kFillTemplate:
            savedEntry = dstEntry;
            dstEntry = &templateEntry;
            data++;
            break;
        }

        if (dstEntry != &templateEntry)
            *dstEntry = templateEntry;

        dstEntry->x = *data++ + menuX + 8;
        dstEntry->y = *data++ + menuY;
        dstEntry->width = *data++;
        dstEntry->height = *data++;
        dstEntry->drawn = 0;

        while (*data != kEndOfEntry) {
            switch (*data++) {
            case kInvisibilityCloak:
                dstEntry->invisible = 1;
                break;

            case kPositions:
                {
                    auto pos = reinterpret_cast<const char *>(data);
                    dstEntry->leftEntry = pos[0];
                    dstEntry->rightEntry = pos[1];
                    dstEntry->upEntry = pos[2];
                    dstEntry->downEntry = pos[3];
                    data += 2;
                }
                break;

            case kCustomBackgroundFunc:
                dstEntry->type1 = kEntryFunc1;
                dstEntry->u1.entryFunc.loadFrom(data);
                data += 2;
                break;

            case kColor:
                dstEntry->type1 = kEntryFrameAndBackColor;
                dstEntry->u1.entryFunc.clearAligned();
                dstEntry->u1.entryColor = *data++;
                break;

            case kBackgroundSprite:
                dstEntry->type1 = kEntrySprite1;
                dstEntry->u1.entryFunc.clearAligned();
                dstEntry->u1.spriteIndex = *data++;
                break;

            case kInvalid:
                // no idea what this is, seems it was removed from the game
                assert(false);
                dstEntry->type1 = static_cast<MenuEntryBackgroundType>(4);
                dstEntry->u1.entryFunc.clearAligned();
                dstEntry->u1.spriteIndex = *data++;
                break;

            case kCustomForegroundFunc:
                dstEntry->type2 = kEntryFunc2;
                dstEntry->stringFlags = 0;
                dstEntry->u2.entryFunc2.loadFrom(data);
                data += 2;
                break;

            case kString:
                {
                    dstEntry->type2 = kEntryString;
                    dstEntry->stringFlags = *data++;
                    dstEntry->u2.string.loadFrom(data);
                    auto ptrValue = dstEntry->u2.string.asAligned<uintptr_t>();
                    if (ptrValue && ptrValue < 256)
                        dstEntry->u2.string.set(kSentinel);
                    data += 2;
                }
                break;

            case kForegroundSprite:
                dstEntry->type2 = kEntrySprite2;
                dstEntry->stringFlags = 0;
                dstEntry->u2.entryFunc2.clearAligned();
                dstEntry->u2.spriteIndex = *data++;
                break;

            case kStringTable:
                dstEntry->type2 = kEntryStringTable;
                dstEntry->stringFlags = *data++;
                dstEntry->u2.stringTable.loadFrom(data);
                data += 2;
                break;

            case kMultiLineText:
                dstEntry->type2 = kEntryMultiLineText;
                dstEntry->stringFlags = *data++;
                dstEntry->u2.multiLineText.loadFrom(data);
                data += 2;
                break;

            case kInteger:
                dstEntry->type2 = kEntryNumber;
                dstEntry->stringFlags = *data++;
                dstEntry->u2.entryFunc2.clearAligned();
                dstEntry->u2.number = *data++;
                break;

            case kOnSelect:
                dstEntry->controlMask = 0x20;   // fire
                dstEntry->onSelect.loadFrom(data);
                data += 2;
                break;

            case kOnSelectWithMask:
                dstEntry->controlMask = *data++;
                dstEntry->onSelect.loadFrom(data);
                data += 2;
                break;

            case kBeforeDraw:
                dstEntry->beforeDraw.loadFrom(data);
                data += 2;
                break;

            case kOnReturn:
                dstEntry->onReturn.loadFrom(data);
                data += 2;
                break;

            case kLeftSkip:
                handleSkip(dstEntry, 0, data);
                break;

            case kRightSkip:
                handleSkip(dstEntry, 1, data);
                break;

            case kUpSkip:
                handleSkip(dstEntry, 2, data);
                break;

            case kDownSkip:
                handleSkip(dstEntry, 3, data);
                break;

            case kColorConvertedSprite:
                dstEntry->type2 = kEntrySpriteCopy;
                dstEntry->stringFlags = 0;
                dstEntry->u2.spriteCopy.loadFrom(data);
                data += 2;
                break;

#ifdef SWOS_VM
            case kCustomBackgroundFuncNative:
                dstEntry->type1 = kEntryFunc1;
                dstEntry->u1.entryFunc.set(fetchAndTranslateProc(data));
                break;

            case kCustomForegroundFuncNative:
                dstEntry->type2 = kEntryFunc2;
                dstEntry->stringFlags = 0;
                dstEntry->u2.entryFunc2.set(fetchAndTranslateProc(data));
                break;

            case kStringNative:
                {
                    dstEntry->type2 = kEntryString;
                    dstEntry->stringFlags = *data++;

                    const auto str = fetch<const char *>(data);
                    assert(str != kSentinel);
                    if (str && str != kSentinel) {
                        auto ofs = copyStringToVM(str);
                        dstEntry->u2.string.set(ofs);
                    } else {
                        dstEntry->u2.string.set(nullptr);
                    }

                    data += sizeof(char *) / 2;
                }
                break;

            case kStringTableNative:
                {
                    dstEntry->type2 = kEntryStringTable;
                    dstEntry->stringFlags = *data++;
                    const auto strTable = fetch<const StringTableNative *>(data);
                    dstEntry->u2.stringTable = translateStringTable(*strTable);
                    data += sizeof(StringTableNative *) / 2;
                }
                break;

            case kMultiLineTextNative:
                {
                    dstEntry->type2 = kEntryMultiLineText;
                    dstEntry->stringFlags = *data++;
                    auto multiLine = fetch<const MultiLineTextNative *>(data);
                    dstEntry->u2.multiLineText = translateMultiLineTable(*multiLine);
                    data += sizeof(MultiLineTextNative *) / 2;
                }
                break;

            case kOnSelectNative:
                dstEntry->controlMask = 0x20;
                dstEntry->onSelect = fetchAndTranslateProc(data);
                break;

            case kOnSelectWithMaskNative:
                dstEntry->controlMask = *data++;
                dstEntry->onSelect = fetchAndTranslateProc(data);
                break;

            case kBeforeDrawNative:
                dstEntry->beforeDraw = fetchAndTranslateProc(data);
                break;

            case kOnReturnNative:
                dstEntry->onReturn = fetchAndTranslateProc(data);
                break;

            case kColorConvertedSpriteNative:
                {
                    dstEntry->type2 = kEntrySpriteCopy;
                    auto ptr = fetch<void *>(data);
                    dstEntry->u2.spriteCopy = SwosVM::registerPointer(ptr);
                    data += sizeof(void *) / 2;
                }
                break;
#endif

            default:
                assert(false);
            }
        }

        if (dstEntry == &templateEntry) {
            dstEntry = savedEntry;
        } else {
            dstEntry->ordinal = numEntries++;
            dstEntry++;
            assert(reinterpret_cast<char *>(dstEntry) < reinterpret_cast<char *>(swos.g_currentMenu) + sizeof(swos.g_currentMenu));
        }
        data++;
    }

    dstMenu->selectedEntry.setRaw(initialEntry);
    assert(static_cast<int>(initialEntry) < numEntries);

    if (dstEntry == &templateEntry)
        dstEntry = savedEntry;

    finalizeMenu(dstMenu, reinterpret_cast<char *>(dstEntry), numEntries);
}

void restoreMenu(const void *menu, int selectedEntry)
{
    swos.g_scanCode = 0;
    swos.controlWord = 0;
    m_currentMenu = menu;

    upackMenu(menu);

    auto currentMenu = getCurrentMenu();
    assert(selectedEntry < currentMenu->numEntries);
    currentMenu->selectedEntry = &currentMenu->entries()[selectedEntry];

    if (currentMenu->onReturn)
        currentMenu->onReturn();
}

void initMenuEntry(MenuEntry& entry)
{
    entry.type1 = kEntryNoBackground;
    entry.u1.entryFunc.clearAligned();

    entry.type2 = kEntryNoForeground;
    entry.u2.entryFunc2.clearAligned();

    entry.invisible = 0;
    entry.disabled = 0;

    entry.leftEntry = -1;
    entry.rightEntry = -1;
    entry.upEntry = -1;
    entry.downEntry = -1;

    entry.leftDirection = -1;
    entry.rightDirection = -1;
    entry.upDirection = -1;
    entry.downDirection = -1;

    entry.leftEntryDis = -1;
    entry.rightEntryDis = -1;
    entry.upEntryDis = -1;
    entry.downEntryDis = -1;

    entry.onSelect.clearAligned();
    entry.controlMask = 0;
    entry.beforeDraw = nullptr;
    entry.onReturn = nullptr;
}

void finalizeMenu(Menu *dstMenu, char *menuEnd, int numEntries)
{
    dstMenu->numEntries = numEntries;
    MenuEntry *entries = getMenuEntry(0);
    for (int i = 0; i < numEntries; i++) {
        if (entries[i].type2 == kEntryString && entries[i].u2.string == kSentinel) {
            strcpy(menuEnd, "STDMENUTXT");
            entries[i].u2.string = menuEnd;
            menuEnd += kStdMenuTextSize;
            assert(menuEnd < reinterpret_cast<char *>(swos.g_currentMenu) + sizeof(swos.g_currentMenu));
        }
    }
    dstMenu->endOfMenuPtr.set(menuEnd);
}

void handleSkip(MenuEntry *dstEntry, size_t index, const int16_t *& data)
{
    assert(index < 4);

    byte skip = *data & 0xff;
    byte direction = *data++ >> 8;
    (&dstEntry->leftEntryDis)[index] = skip;
    (&dstEntry->leftDirection)[index] = direction;
}

#ifdef SWOS_VM
dword fetchAndTranslateProc(const int16_t *& data)
{
    auto proc = fetch<SwosVM::VoidFunction>(data);
    auto swosProc = SwosVM::registerProc(proc);
    data += sizeof(SwosVM::VoidFunction) / 2;
    return swosProc;
}

dword copyStringToVM(const char *str)
{
    auto len = strlen(str) + 1;
    auto ofs = SwosVM::allocateMemory(len);
    auto ptr = SwosVM::offsetToPtr(ofs);
    memcpy(ptr, str, len);
    return ofs;
}

dword translateStringTable(const StringTableNative& strTable)
{
    auto size = sizeof(StringTable) + strTable.numPointers * 4;
    auto stringOffset = size;

    auto ofs = SwosVM::allocateMemory(size);
    auto swosMem = SwosVM::offsetToPtr(ofs);

    dword indexOfs;
    if (strTable.isIndexPointerNative())
        indexOfs = SwosVM::registerPointer(fetch<int16_t *>(&strTable.nativeIndex));
    else
        indexOfs = fetch(reinterpret_cast<const dword *>(&strTable.index));

    memcpy(swosMem, &indexOfs, 4);
    memcpy(swosMem + 4, &strTable.initialValue, 2);

    for (int i = 0; i < strTable.numPointers; i++) {
        auto str = strTable[i];
        dword strOfs;

        if (strTable.isNativeString(i))
            strOfs = copyStringToVM(str);
        else
            strOfs = reinterpret_cast<uintptr_t>(str);

        memcpy(swosMem + 6 + i * 4, &strOfs, 4);
    }

    return ofs;
}

dword translateMultiLineTable(const MultiLineTextNative& text)
{
    auto size = sizeof(MultiLineTextNative) + text.numLines * 4;
    auto ofs = SwosVM::allocateMemory(size);
    auto swosMem = SwosVM::offsetToPtr(ofs);

    *swosMem = text.numLines;

    for (size_t i = 0; i < text.numLines; i++) {
        auto str = text[i];
        auto ofs = copyStringToVM(str);
        memcpy(swosMem + 1 + i * 4, &ofs, 4);
    }

    return ofs;
}
#endif
