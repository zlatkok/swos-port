#include "menuProc.h"
#include "menus.h"
#include "menuControls.h"
#include "drawMenu.h"
#include "menuMouse.h"
#include "music.h"
#include "render.h"

static void selectEntryWithControlMask(MenuEntry *entry);
static MenuEntry *handleSwitchingToNextEntry(const MenuEntry *activeEntry, MenuEntry *nextEntry);
static void highlightEntry(Menu *currentMenu, MenuEntry *entry);
static int getControlMask(int entryControlMask);
static void invokeOnSelect(MenuEntry *entry, int controlMask);
static MenuEntry *findNextEntry(byte nextEntryIndex, int nextEntryDirection);

void menuProc()
{
    ReadTimerDelta();
    drawMenu();

    updateMouse();

    menuCheckControls();
    updateSongState();

    auto currentMenu = getCurrentMenu();
    auto activeEntry = currentMenu->selectedEntry;
    MenuEntry *nextEntry = nullptr;

    // we deviate a bit from SWOS behavior here, as it seems like a bug to me: originally only the field in
    // the current menu is assigned but not the activeEntry variable, leading to a potential nullptr access
    if (!activeEntry && swos.menuControlsDirection >= 0 && swos.previousMenuItem)
        activeEntry = currentMenu->selectedEntry = swos.previousMenuItem;

    if (activeEntry) {
        selectEntryWithControlMask(activeEntry);
        nextEntry = handleSwitchingToNextEntry(activeEntry, nextEntry);
    } else if (swos.menuControlsDirection < 0) {
        return;
    } else if (swos.previousMenuItem) {
        currentMenu->selectedEntry = swos.previousMenuItem;
        nextEntry = swos.previousMenuItem;
    }

    highlightEntry(currentMenu, nextEntry);
}

static void selectEntryWithControlMask(MenuEntry *entry)
{
    swos.controlMask = 0;

    if (entry->onSelect) {
        int controlMask = getControlMask(entry->controlMask);

        if (entry->controlMask == -1 || controlMask)
            invokeOnSelect(entry, controlMask);
    }
}

static MenuEntry *handleSwitchingToNextEntry(const MenuEntry *activeEntry, MenuEntry *nextEntry)
{
    // if no fire but there's a movement, try moving to the next entry
    if (!swos.fire && swos.menuControlsDirection >= 0) {
        // map direction values (down, right, left, up) to order in MenuEntry structure
        static const size_t nextDirectionOffsets[MenuEntry::kNumDirections] = { 2, 1, 3, 0, };

        // this will hold the offset of the next entry to move to (direction)
        int nextEntryDirection = nextDirectionOffsets[(swos.menuControlsDirection >> 1) & 3];
        auto nextEntryIndex = (&activeEntry->leftEntry)[nextEntryDirection];

        nextEntry = findNextEntry(nextEntryIndex, nextEntryDirection);
    }

    return nextEntry;
}

static void highlightEntry(Menu *currentMenu, MenuEntry *entry)
{
    assert(currentMenu);

    if (entry) {
        swos.previousMenuItem = currentMenu->selectedEntry;
        currentMenu->selectedEntry = entry;

        if (entry->background == kEntryNoBackground) {
            drawMenu();
        } else {
            A5 = entry;
            SWOS::DrawMenuItem();
        }
    }
}

static int getControlMask(int entryControlMask)
{
    int controlMask = 0;

    // TODO: remove these global variables; the last remaining usage is in some edit tactics menu function
    if (swos.shortFire)
        controlMask |= kShortFireMask;
    if (swos.fire)
        controlMask |= kFireMask;
    if (swos.left)
        controlMask |= kLeftMask;
    if (swos.right)
        controlMask |= kRightMask;
    if (swos.up)
        controlMask |= kUpMask;
    if (swos.down)
        controlMask |= kDownMask;

    if (swos.menuControlsDirection >= 0) {
        switch (swos.menuControlsDirection >> 1) {
        case 0:
            controlMask |= kUpRightMask;
            break;
        case 1:
            controlMask |= kDownRightMask;
            break;
        case 2:
            controlMask |= kDownLeftMask;
            break;
        default:
            controlMask |= kUpLeftMask;
        }
    }

    return controlMask & entryControlMask;
}

static void invokeOnSelect(MenuEntry *entry, int controlMask)
{
    stopTitleSong();
    selectEntry(entry, controlMask);
}

static MenuEntry *findNextEntry(byte nextEntryIndex, int nextEntryDirection)
{
    while (nextEntryIndex != 255) {
        auto nextEntry = getMenuEntry(nextEntryIndex);

        if (!nextEntry->disabled && nextEntry->visible())
            return nextEntry;

        auto newDirection = (&nextEntry->leftDirection)[nextEntryDirection];
        if (newDirection != 255) {
            nextEntryIndex = (&nextEntry->leftEntryDis)[nextEntryDirection];

            assert((nextEntryIndex != nextEntry->ordinal || nextEntryDirection != newDirection) &&
                "Entry referencing itself, infinite loop!");

            if (nextEntryIndex == nextEntry->ordinal && nextEntryDirection == newDirection)
                break;

            nextEntryDirection = newDirection;
            assert(newDirection <= 3);
            assert(nextEntryIndex != 255);
        } else {
            assert(nextEntryIndex != (&nextEntry->leftEntry)[nextEntryDirection]);
            nextEntryIndex = (&nextEntry->leftEntry)[nextEntryDirection];
        }
    }

    return nullptr;
}
