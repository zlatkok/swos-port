#include "menuAlloc.h"
#include "menus.h"
#include "unpackMenu.h"
#include "menuCodes.h"
#include "menuMouse.h"
#include "drawMenu.h"
#include "menuItemRenderer.h"
#include "menuControls.h"

// SWOS introduced limit
constexpr int kMaxMenuEntries = 115;

// last menu sent here for unpacking
static const void *m_currentMenu;

static void (*m_onDestroy)();

struct MenuEntryExtension {
    MenuEntryExtension() {};
    union {
        SwosMenu::EntryBoolOption opt;
    };
};

static std::array<MenuEntryExtension, kMaxMenuEntries> m_entryExtensions;

static void initMenuEntry(MenuEntry& entry);
static void finalizeMenu(Menu *dstMenu, char *menuEnd, int numEntries);
static void handleSkip(MenuEntry *dstEntry, size_t index, const int16_t *& data);
static void switchBoolOption();
static dword fetchAndTranslateProc(const int16_t *& data);
static dword translateStringTable(const SwosMenu::StringTableNative& strTable);
static dword translateMultilineTable(const MultilineText *text);

// Unpacks static menu in src to screen menu at dst.
// Doesn't execute any of the menu functions, only does the conversion.
void unpackMenu(const void *src, char *dst /* = swos.g_currentMenu */)
{
    using namespace SwosMenu;

    assert(dst >= swos.g_currentMenu && dst < swos.g_currentMenu + sizeof(swos.g_currentMenu));

    resetMenuAllocator();

    m_currentMenu = src;

    int menuX = 0, menuY = 0;

    MenuEntry templateEntry;
    initMenuEntry(templateEntry);

    auto dstMenu = reinterpret_cast<Menu *>(dst);
    auto headerMark = *reinterpret_cast<const int32_t *>(src);
    const int16_t *data;
    int initialEntry;

    if (m_onDestroy)
        m_onDestroy();

    clearMenuLocalSprites();

    memset(m_entryExtensions.data(), -1, sizeof(m_entryExtensions));

    if (headerMark == kMenuHeaderV2Mark) {
        auto header = reinterpret_cast<const MenuHeaderV2 *>(src);

        const auto kFuncs = {
            std::make_tuple(&dstMenu->onInit, header->onInit, header->nativePtr[0]),
            std::make_tuple(&dstMenu->onReturn, header->onReturn, header->nativePtr[1]),
            std::make_tuple(&dstMenu->onDraw, header->onDraw, header->nativePtr[2]),
        };

        for (const auto& func : kFuncs) {
            auto dst = std::get<0>(func);
            auto src = std::get<1>(func);
            if (auto isNative = std::get<2>(func))
                *dst = SwosVM::registerProc(src);
            else
                *dst = reinterpret_cast<SwosProcPointer *>(src);
        }

        m_onDestroy = header->onDestroy;

        initialEntry = header->initialEntry;
        data = header->data();
    } else {
        auto header = reinterpret_cast<const PackedMenu *>(src);
        dstMenu->onInit = header->onInit;
        dstMenu->onReturn = header->onReturn;
        dstMenu->onDraw = header->onDraw;
        m_onDestroy = nullptr;
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
                dstEntry->background = kEntryBackgroundFunction;
                dstEntry->bg.entryFunc.loadFrom(data);
                data += 2;
                break;

            case kColor:
                dstEntry->background = kEntryFrameAndBackColor;
                dstEntry->bg.entryFunc.clearAligned();
                dstEntry->bg.entryColor = *data++;
                break;

            case kBackgroundSprite:
                dstEntry->background = kEntrySprite1;
                dstEntry->bg.entryFunc.clearAligned();
                dstEntry->bg.spriteIndex = *data++;
                break;

            case kInvalid:
                // no idea what this is, seems it was removed from the game
                assert(false);
                dstEntry->background = static_cast<MenuEntryBackground>(4);
                dstEntry->bg.entryFunc.clearAligned();
                dstEntry->bg.spriteIndex = *data++;
                break;

            case kCustomForegroundFunc:
                dstEntry->type = kEntryContentFunction;
                dstEntry->stringFlags = 0;
                dstEntry->fg.contentFunction.loadFrom(data);
                data += 2;
                break;

            case kString:
                {
                    dstEntry->type = kEntryString;
                    dstEntry->stringFlags = *data++;
                    dstEntry->fg.string.loadFrom(data);
                    auto ptrValue = dstEntry->fg.string.asAligned<uintptr_t>();
                    if (ptrValue && ptrValue < 256)
                        dstEntry->fg.string.set(kSentinel);
                    data += 2;
                }
                break;

            case kForegroundSprite:
                dstEntry->type = kEntrySprite2;
                dstEntry->stringFlags = 0;
                dstEntry->fg.contentFunction.clearAligned();
                dstEntry->fg.spriteIndex = *data++;
                break;

            case kStringTable:
                dstEntry->type = kEntryStringTable;
                dstEntry->stringFlags = *data++;
                dstEntry->fg.stringTable.loadFrom(data);
                data += 2;
                break;

            case kMultilineText:
                dstEntry->type = kEntryMultilineText;
                dstEntry->stringFlags = *data++;
                dstEntry->fg.multilineText.loadFrom(data);
                data += 2;
                break;

            case kInteger:
                dstEntry->type = kEntryNumber;
                dstEntry->stringFlags = *data++;
                dstEntry->fg.contentFunction.clearAligned();
                dstEntry->fg.number = *data++;
                break;

            case kOnSelect:
                dstEntry->controlMask = kShortFireMask;
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

            case kAfterDraw:
                dstEntry->afterDraw.loadFrom(data);
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
                dstEntry->type = kEntryColorConvertedSprite;
                dstEntry->stringFlags = 0;
                dstEntry->fg.spriteCopy.loadFrom(data);
                data += 2;
                break;

            case kCustomBackgroundFuncNative:
                dstEntry->background = kEntryBackgroundFunction;
                dstEntry->bg.entryFunc.set(fetchAndTranslateProc(data));
                break;

            case kCustomForegroundFuncNative:
                dstEntry->type = kEntryContentFunction;
                dstEntry->stringFlags = 0;
                dstEntry->fg.contentFunction.set(fetchAndTranslateProc(data));
                break;

            case kStringNative:
                {
                    dstEntry->type = kEntryString;
                    dstEntry->stringFlags = *data++;

                    const auto str = fetch<const char *>(data);
                    assert(str != kSentinel);
                    if (str && str != kSentinel) {
                        auto ofs = SwosVM::ptrToOffset(menuAllocString(str));
                        dstEntry->fg.string.setRaw(ofs);
                    } else {
                        dstEntry->fg.string.set(nullptr);
                    }

                    data += sizeof(char *) / 2;
                }
                break;

            case kStringTableNative:
                {
                    dstEntry->type = kEntryStringTable;
                    dstEntry->stringFlags = *data++;
                    const auto strTable = fetch<const StringTableNative *>(data);
                    dstEntry->fg.stringTable = translateStringTable(*strTable);
                    data += sizeof(StringTableNative *) / 2;
                }
                break;

            case kMultilineTextNative:
                {
                    dstEntry->type = kEntryMultilineText;
                    dstEntry->stringFlags = *data++;
                    auto multiLine = fetch<const MultilineText *>(data);
                    dstEntry->fg.multilineText = translateMultilineTable(multiLine);
                    data += sizeof(MultilineText *) / 2;
                }
                break;

            case kOnSelectNative:
                dstEntry->controlMask = kShortFireMask;
                dstEntry->onSelect = fetchAndTranslateProc(data);
                break;

            case kOnSelectWithMaskNative:
                dstEntry->controlMask = *data++;
                dstEntry->onSelect = fetchAndTranslateProc(data);
                break;

            case kBeforeDrawNative:
                dstEntry->beforeDraw = fetchAndTranslateProc(data);
                break;

            case kAfterDrawNative:
                dstEntry->afterDraw = fetchAndTranslateProc(data);
                break;

            case kMenuSpecificSprite:
                dstEntry->type = kEntryMenuSpecificSprite;
                dstEntry->stringFlags = 0;
                dstEntry->fg.spriteIndex = 0;
                break;

            case kBoolOption:
                {
                    dstEntry->type = kEntryBoolOption;
                    dstEntry->controlMask = kShortFireMask;
                    dstEntry->onSelect = SwosVM::registerProc(switchBoolOption);

                    auto& opt = m_entryExtensions[numEntries].opt;
                    opt.get = fetch<EntryBoolOption::GetFn>(data);
                    data += sizeof(EntryBoolOption::GetFn) / 2;
                    opt.set = fetch<EntryBoolOption::SetFn>(data);
                    data += sizeof(EntryBoolOption::SetFn) / 2;
                    opt.description = fetch<const char *>(data);
                    data += sizeof(char *) / 2;

                    assert(opt.get && opt.set && opt.description);
                }
                break;

            default:
                assert(false);
            }
        }

        assert(dstEntry == &templateEntry || reinterpret_cast<char *>(dstEntry + 1) < menuAlloc(0));

        if (dstEntry == &templateEntry) {
            dstEntry = savedEntry;
        } else {
            dstEntry->ordinal = numEntries++;
            dstEntry++;
            assert(reinterpret_cast<char *>(dstEntry) < swos.g_currentMenu + sizeof(swos.g_currentMenu));
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
    if (!menu)
        return;

    m_currentMenu = menu;

    resetControls();

    unpackMenu(menu);

    auto currentMenu = getCurrentMenu();
    assert(selectedEntry < currentMenu->numEntries);
    currentMenu->selectedEntry = &currentMenu->entries()[selectedEntry];

    if (currentMenu->onReturn) {
        SDL_UNUSED auto markAll = SwosVM::getMemoryMark();

        currentMenu->onReturn();

        assert(markAll == SwosVM::getMemoryMark());
    }

    resetMenuMouseData();
    cacheMenuItemBackgrounds();
}

void activateMenu(const void *menu)
{
    unpackMenu(menu);

    auto currentMenu = getCurrentMenu();
    auto currentEntry = currentMenu->selectedEntry.getRaw();

    if (currentEntry <= 0xffff)
        currentMenu->selectedEntry = getMenuEntry(currentEntry);

    if (currentMenu->onInit) {
        SDL_UNUSED auto menuMark = menuAlloc(0);

        A6 = swos.g_currentMenu;
        currentMenu->onInit();

        assert(menuMark == menuAlloc(0));
    }

    assert(currentMenu->numEntries < 256);

    cacheMenuItemBackgrounds();
    resetMenuMouseData();
}

const void *getCurrentPackedMenu()
{
    return m_currentMenu;
}

std::pair<SwosMenu::EntryBoolOption::GetFn, SwosMenu::EntryBoolOption::SetFn> getBoolOptionAccessors(const MenuEntry& entry)
{
    assert(entry.ordinal < kMaxMenuEntries);
    return { m_entryExtensions[entry.ordinal].opt.get, m_entryExtensions[entry.ordinal].opt.set };
}

static void initMenuEntry(MenuEntry& entry)
{
    memset(&entry, 0, sizeof(entry));

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
}

static void finalizeMenu(Menu *dstMenu, char *menuEnd, int numEntries)
{
    dstMenu->numEntries = numEntries;
    MenuEntry *entries = getMenuEntry(0);
    for (int i = 0; i < numEntries; i++) {
        if (entries[i].type == kEntryString && entries[i].fg.string == kSentinel) {
            strcpy(menuEnd, "STDMENUTXT");
            entries[i].fg.string = menuEnd;
            menuEnd += kStdMenuTextSize;
            assert(menuEnd < swos.g_currentMenu + sizeof(swos.g_currentMenu));
        }
    }
    dstMenu->endOfMenuPtr.set(menuEnd);
}

static void handleSkip(MenuEntry *dstEntry, size_t index, const int16_t *& data)
{
    assert(index < 4);

    byte skip = *data & 0xff;
    byte direction = *data++ >> 8;
    (&dstEntry->leftEntryDis)[index] = skip;
    (&dstEntry->leftDirection)[index] = direction;
}

static void switchBoolOption()
{
    auto entry = A5.asMenuEntry();
    const auto& opt = m_entryExtensions[entry->ordinal].opt;

    assert(opt.get && opt.set && opt.description);

    auto oldValue = opt.get();
    logInfo("Setting %s to %s...", opt.description, oldValue ? "OFF" : "ON");
    opt.set(!oldValue);
    logInfo("    %s!", opt.get() != oldValue ? "Success" : "Failed");
}

static dword fetchAndTranslateProc(const int16_t *& data)
{
    auto proc = fetch<SwosVM::VoidFunction>(data);
    auto swosProc = SwosVM::registerProc(proc);
    data += sizeof(SwosVM::VoidFunction) / 2;
    return swosProc;
}

static dword translateStringTable(const SwosMenu::StringTableNative& strTable)
{
    auto size = sizeof(StringTable) + strTable.numPointers * 4;
    auto swosMem = menuAllocStringTable(size);
    assert(reinterpret_cast<uintptr_t>(swosMem) % 4 == 0);

    dword indexOfs;
    if (strTable.isIndexPointerNative())
        indexOfs = SwosVM::registerPointer(strTable.nativeIndex);
    else
        indexOfs = strTable.index.getRaw();

    reinterpret_cast<dword *>(swosMem)[0] = indexOfs;
    reinterpret_cast<int16_t *>(swosMem + 4)[0] = strTable.startIndex;

    for (int i = 0; i < strTable.numPointers; i++) {
        auto str = strTable[i];
        dword strOfs;

        if (strTable.isNativeString(i)) {
            strOfs = SwosVM::ptrToOffset(menuAllocString(str));
        } else {
            assert(reinterpret_cast<uintptr_t>(str) <= std::numeric_limits<dword>::max());
            strOfs = reinterpret_cast<uintptr_t>(str);
        }

        memcpy(swosMem + 6 + i * 4, &strOfs, 4);
    }

    return SwosVM::ptrToOffset(swosMem);
}

dword translateMultilineTable(const MultilineText *text)
{
    if (!text)
        return 0;

    int totalLength = text->totalLenght();

    auto swosMem = menuAlloc(totalLength);
    memcpy(swosMem, &text, totalLength);

    return SwosVM::ptrToOffset(swosMem);
}
