#include "menu.h"
#include "menuMouse.h"
#include "log.h"
#include "util.h"
#include "render.h"
#include "sprites.h"
#include "music.h"
#include "options.h"
#include "controls.h"
#include "mainMenu.h"

#ifdef SWOS_TEST
# include "unitTest.h"
#endif

void SWOS::FlipInMenu()
{
    frameDelay(2);
    updateScreen();
}

static void menuDelay()
{
#ifdef DEBUG
# ifdef _WIN32
    assert(_CrtCheckMemory());
# endif
    extern void checkMemory();
    checkMemory();
#endif

    if (!swos.menuCycleTimer) {
        timerProc();
        timerProc();    // imitate int 8 ;)
    }
}

static void menuProcCycle()
{
    menuDelay();
    SWOS::MenuProc();
}

void SWOS::WaitRetrace()
{
    menuDelay();
}

void SWOS::MainMenu()
{
    logInfo("Initializing main menu");
    InitMainMenu();

    while (true)
        menuProcCycle();
}

static const char *entryText(int entryOrdinal)
{
    auto entry = getMenuEntry(entryOrdinal);

    static char buf[16];
    if (entry) {
        switch (entry->type2) {
        case kEntryString: return entry->string();
        case kEntryMultiLineText: return entry->u2.multiLineText.asCharPtr() + 1;
        case kEntrySprite2:
        case kEntryNumber: return _itoa(entry->u2.number, buf, 10);
        }
    }

    return "/";
}

// These are macros instead of functions because we need to save these variables for each level of nesting;
// being functions they can't create local variables on callers stack, and then we'd need explicit stacks.
#define saveCurrentMenu() \
    auto currentMenu = getCurrentMenu(); \
    auto savedEntry = currentMenu->selectedEntry ? currentMenu->selectedEntry->ordinal : 0; \
    auto savedMenu = getCurrentPackedMenu(); \

#define restorePreviousMenu() \
    restoreMenu(savedMenu, savedEntry); \

void showMenu(const BaseMenu& menu)
{
    swos.g_scanCode = 0;
    swos.controlWord = 0;
    swos.menuStatus = 0;

    menuMouseOnAboutToShowNewMenu();

    saveCurrentMenu();
    logInfo("Showing menu %#x [%s], previous menu is %#x", &menu, entryText(savedEntry), savedMenu);

    auto memoryMark = SwosVM::markAllMemory();

    swos.g_exitMenu = 0;
    activateMenu(&menu);

    while (!swos.g_exitMenu) {
        menuProcCycle();
#ifdef SWOS_TEST
        // ask unit tests how long they want to run menu proc
        if (SWOS_UnitTest::exitMenuProc())
            return;
#endif
    }

    swos.menuStatus = 1;
    swos.g_exitMenu = 0;
    SwosVM::releaseAllMemory(memoryMark);

    restorePreviousMenu();
    determineReachableEntries();

    menuMouseOnOldMenuRestored();


    logInfo("Menu %#x finished, restoring menu %#x", &menu, savedMenu);
}

// ShowMenu
//
// in:
//     A6 -> menu to show on screen (packed form)
//
// Shows this menu and blocks, returns only when it's exited.
// And then returns to the previous menu and it becomes the active menu.
//
void SWOS::ShowMenu()
{
    auto menu = A6.as<BaseMenu *>();
    showMenu(*menu);
}

// DrawSelected
//
// in:
//     A6 -> menu
//
// Draws a frame around the currently selected item.
//
void SWOS::DrawSelected()
{
    auto menu = A6.as<Menu *>();
    auto entry = menu->selectedEntry;
    if (entry && entry->width && entry->height) {
        D0 = swos.kColorTableShine[swos.menuCursorFrame & 7];
        if (flashCursor())
            swos.menuCursorFrame++;
        D1 = entry->x;
        D2 = entry->y;
        D3 = entry->width;
        D4 = entry->height;
        SAFE_INVOKE(DrawInnerFrame);
    }
}

// called by SWOS, when we're starting the game
void SWOS::ToMainGameLoop()
{
    logInfo("Starting the game...");

    saveCurrentMenu();
    safeInvokeWithSaved68kRegisters(StartMainGameLoop);
    restorePreviousMenu();
}

__declspec(naked) void SWOS::DoUnchainSpriteInMenus_OnEnter()
{
#ifdef SWOS_VM
    SwosVM::ecx = 0;
#else
    // only cx is loaded, but later whole ecx is tested; make this right
    _asm {
        xor ecx, ecx
        retn
    }
#endif
}

// ActivateMenu
//
// in:
//     A6 -> menu to activate
// globals:
//     g_currentMenu - destination for unpacked menu.
//
// Unpacks given static menu into g_currentMenu. Executes menu initialization function, if present.
//
void SWOS::ActivateMenu()
{
    activateMenu(A6.asPtr());
}

// SetCurrentEntry
//
//  in:
//      D0 - number of menu entry
//
//  Sets current entry in the current menu.
//
void SWOS::SetCurrentEntry()
{
    auto currentMenu = getCurrentMenu();
    assert(D0.asWord() < currentMenu->numEntries);

    auto newSelectedEntry = getMenuEntry(D0.asWord());
    currentMenu->selectedEntry = newSelectedEntry;;
}

void activateMenu(const void *menu)
{
    upackMenu(menu);

    auto currentMenu = getCurrentMenu();
    auto currentEntry = currentMenu->selectedEntry.getRaw();

    if (currentEntry <= 0xffff)
        currentMenu->selectedEntry = getMenuEntry(currentEntry);

    if (currentMenu->onInit) {
        A6 = swos.g_currentMenu;
        auto proc = currentMenu->onInit;
        SAFE_INVOKE(proc);
    }

    assert(currentMenu->numEntries < 256);

    determineReachableEntries();

    updateMatchControls();
}

void SWOS::InitMainMenu()
{
    InitMenuMusic();
    InitMainMenuStuff();
    // speed up starting up in debug mode
#ifdef DEBUG
    swos.menuFade = 0;
    setPalette(swos.g_workingPalette);
#else
    swos.menuFade = 1;
#endif
}

void SWOS::InitMainMenuStuff()
{
    ZeroOutStars();
    swos.twoPlayers = -1;
    swos.player1ClearFlag = 0;
    swos.player2ClearFlag = 0;
    swos.flipOnOff = 1;
    swos.inFriendlyMenu = 0;
    swos.isNationalTeam = 0;

    D0 = kGameTypeNoGame;
    InitCareerVariables();

    swos.menuStatus = 0;
    swos.menuFade = 0;
    swos.g_exitMenu = 0;
    swos.fireResetFlag = 0;
    swos.dseg_10E848 = 0;
    swos.dseg_10E846 = 0;
    swos.dseg_11F41C = -1;
    swos.coachOrPlayer = 1;
    SetDefaultNameAndSurname();
    swos.plNationality = kEng;       // English is the default
    swos.competitionOver = 0;
    swos.g_numSelectedTeams = 0;
    InitHighestScorersTable();
    swos.teamsLoaded = 0;
    swos.poolplyrLoaded = 0;
    swos.importTacticsFilename[0] = 0;
    swos.chooseTacticsTeamPtr = nullptr;
    InitUserTactics();

    activateMainMenu();
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
            nextEntryIndex = (&nextEntry->leftEntry)[nextEntryDirection];
        }
    }

    return nullptr;
}

static void invokeOnSelect(MenuEntry *entry)
{
    stopTitleSong();
    swos.menuCycleTimer = 1;    // set cycle timer to prevent InputText choking

    selectEntry(entry);
}

static bool isPlayMatchEntry(const MenuEntry *entry)
{
    return entry && entry->onSelect == PlayMatchSelected;
}

static void playMatchSelected(int playerNo)
{
    if (reinterpret_cast<TeamFile *>(swos.careerTeam)->teamControls == kComputerTeam)
        swos.pl1IsntPlayer = 1;

    swos.useColorTable = 0;
    swos.playerNumThatStarted = playerNo;
    swos.player1ClearFlag = 0;
    swos.player2ClearFlag = 0;
    SetExitMenuFlag();
}

static bool useMouseConfirmation(int playerNo)
{
    using namespace SwosVM;

    if (playerNo > 0) {
        auto controls = getGameControls(playerNo);
        if (controls == kMouse) {
            auto useMouse = showContinueAbortPrompt(cacheString("USE MOUSE CONTROLS"), cacheString("YES"), cacheString("NO"), {
                cacheString(""),
                cacheString("ARE YOU SURE YOU WANT TO PLAY"),
                cacheString("THE GAME USING THE MOUSE?"),
                cacheString(""),
                cacheString("SELECT ''PLAY MATCH'' WITH DESIRED"),
                cacheString("CONTROLLER TO USE IT IN THE GAME"),
            });

            if (!useMouse)
                disableGameControls(playerNo);

            return useMouse;
        }
    }

    return true;
}

// return true if it's a play match entry; if so, menu system backs out and we handle everything in here
static bool checkForPlayMatchEntry(const MenuEntry *activeEntry)
{
    if (isPlayMatchEntry(activeEntry)) {
        int playerNo = matchControlsSelected();

        if (!useMouseConfirmation(playerNo))
            return true;

        updateMatchControls();

        if (playerNo >= 0) {
            playMatchSelected(playerNo);
            return true;
        }
    }

    return false;
}

static int getControlMask(int entryControlMask)
{
    int controlMask = 0;

    if (swos.shortFire)
        controlMask |= 0x20;
    if (swos.fire)
        controlMask |= 0x01;
    if (swos.left)
        controlMask |= 0x02;
    if (swos.right)
        controlMask |= 0x04;
    if (swos.up)
        controlMask |= 0x08;
    if (swos.down)
        controlMask |= 0x10;

    if (swos.finalControlsStatus >= 0) {
        switch (swos.finalControlsStatus >> 1) {
        case 0:
            controlMask |= 0x100;   // up-right
            break;
        case 1:
            controlMask |= 0x80;    // down-right
            break;
        case 2:
            controlMask |= 0x200;   // down-left
            break;
        default:
            controlMask |= 0x40;    // up-left
        }
    }

    return controlMask & entryControlMask;
}

static void selectEntryWithControlMask(MenuEntry *entry)
{
    swos.controlsStatus = 0;

    if (entry->onSelect) {
        int controlMask = getControlMask(entry->controlMask);

        if (entry->controlMask == -1 || controlMask) {
            swos.controlsStatus = controlMask;
            invokeOnSelect(entry);
        }
    }
}

static MenuEntry *handleSwitchingToNextEntry(const MenuEntry *activeEntry, MenuEntry *nextEntry)
{
    // if no fire but there's a movement, try moving to the next entry
    if (!swos.fire && swos.finalControlsStatus >= 0) {
        // map direction values (down, right, left, up) to order in MenuEntry structure
        static const size_t nextDirectionOffsets[MenuEntry::kNumDirections] = { 2, 1, 3, 0, };

        // this will hold the offset of the next entry to move to (direction)
        int nextEntryDirection = nextDirectionOffsets[(swos.finalControlsStatus >> 1) & 3];
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

        if (entry->type1 == kEntryNoBackground) {
            DrawMenu();
        } else {
            A5 = entry;
            DrawMenuItem();
        }
    }
}

void SWOS::MenuProc()
{
    updateMouse();

    swos.g_videoSpeedIndex = 50;    // just in case

    ReadTimerDelta();
    DrawMenu();
    swos.menuCycleTimer = 0;        // must come after DrawMenu(), set to 1 to slow down input

    if (swos.menuFade) {
        FadeIn();
        swos.menuFade = 0;
    }

    CheckControls();
    updateSongState();

    auto currentMenu = getCurrentMenu();
    auto activeEntry = currentMenu->selectedEntry;
    MenuEntry *nextEntry = nullptr;

    // we deviate a bit from SWOS behavior here, as it seems like a bug to me: only the field in
    // the current menu is assigned but not the activeEntry variable, leading to a potential nullptr access
    if (!activeEntry && swos.finalControlsStatus >= 0 && swos.previousMenuItem)
        activeEntry = currentMenu->selectedEntry = swos.previousMenuItem;

    if (activeEntry) {
        if (checkForPlayMatchEntry(activeEntry))
            return;
        selectEntryWithControlMask(activeEntry);
        nextEntry = handleSwitchingToNextEntry(activeEntry, nextEntry);
    } else if (swos.finalControlsStatus < 0) {
        return;
    } else if (swos.previousMenuItem) {
        currentMenu->selectedEntry = swos.previousMenuItem;
        nextEntry = swos.previousMenuItem;
    }

    highlightEntry(currentMenu, nextEntry);
}

void SWOS::ExitPlayMatch()
{
    swos.g_exitGameFlag = 1;
    swos.player1ClearFlag = 0;
    swos.player2ClearFlag = 0;
    swos.useColorTable = 0;
    swos.isCareer = 0;

    if (swos.playMatchTeam1Ptr == swos.careerTeam || swos.playMatchTeam2Ptr == swos.careerTeam)
        swos.isCareer = 1;

    SetExitMenuFlag();

    resetMatchControls();
}

void SWOS::ExitEuropeanChampionshipMenu()
{
    activateMainMenu();
}

// in:
//      D0 -  word to convert
//      A1 -> buffer
// out:
//      A1 -> points to terminating zero in the buffer
//
void SWOS::Int2Ascii()
{
    int num = D0.asInt16();
    auto dest = A1.asPtr();

    if (num < 0) {
        num = -num;
        *dest++ = '-';
    } else if (!num) {
        dest[0] = '0';
        dest[1] = '\0';
        A1++;
        return;
    }

    auto end = dest;
    while (num) {
        auto quotRem = std::div(num, 10);
        num = quotRem.quot;
        *end++ = quotRem.rem + '0';
    }

    *end = '\0';
    A1 = end;

    for (end--; dest < end; dest++, end--)
        std::swap(*dest, *end);
}

void SWOS::AbortTextInputOnEscapeAndRightMouseClick()
{
    if (swos.lastKey == kKeyEscape || SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK) {
        swos.convertedKey = kKeyEscape;
        swos.g_inputingText = 0;
    }
}

bool inputNumber(MenuEntry *entry, int maxDigits, int minNum, int maxNum)
{
    assert(entry->type2 == kEntryNumber);

    int num = static_cast<int16_t>(entry->u2.number);
    assert(num >= minNum && num <= maxNum);

    swos.g_inputingText = -1;

    char buf[32];
    _itoa_s(num, buf, 10);
    buf[sizeof(buf) - 1] = '\0';
    assert(static_cast<int>(strlen(buf)) <= maxDigits);

    auto end = buf + strlen(buf);
    *end++ = -1;    // insert cursor
    *end = '\0';

    entry->type2 = kEntryString;
    entry->setString(buf);

    auto cursorPtr = end - 1;

    while (true) {
        updateControls();
        DrawMenu();
        SWOS::WaitRetrace();

        SWOS::GetKey();

        while (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK) {
            swos.lastKey = kKeyEscape;
            updateControls();
            SDL_Delay(15);
        }

        switch (swos.lastKey) {
        case kKeyEnter:
            memmove(cursorPtr, cursorPtr + 1, end - cursorPtr);
            entry->type2 = kEntryNumber;
            entry->u2.number = atoi(buf);
            swos.g_inputingText = 0;
            swos.controlWord = 0;
            return true;

        case kKeyEscape:
            A5 = entry;
            entry->type2 = kEntryNumber;
            entry->u2.number = num;
            swos.g_inputingText = 0;   // original SWOS leaves it set, wonder if it's significant...
            swos.controlWord = 0;
            return false;

        case kKeyLShift:
        case kKeyArrowLeft:
            if (cursorPtr != buf) {
                std::swap(cursorPtr[-1], cursorPtr[0]);
                cursorPtr--;
            }
            break;

        case kKeyRShift:
        case kKeyArrowRight:
            if (cursorPtr[1]) {
                std::swap(cursorPtr[0], cursorPtr[1]);
                cursorPtr++;
            }
            break;

        case kKeyHome:
        case kKeyArrowDown:
            memmove(buf + 1, buf, cursorPtr - buf);
            *(cursorPtr = buf) = kCursorChar;
            break;

        case kKeyEnd:
        case kKeyArrowUp:
            if (cursorPtr[1]) {
                memmove(cursorPtr, cursorPtr + 1, end - cursorPtr);
                *(cursorPtr = end - 1) = kCursorChar;
            }
            break;

        case kKeyBackspace:
            if (cursorPtr != buf) {
                memmove(cursorPtr, cursorPtr + 1, end - cursorPtr);
                *--cursorPtr = kCursorChar;
                end--;
            }
            break;

        case kKeyDelete:
            if (cursorPtr[1]) {
                memmove(cursorPtr, cursorPtr + 1, end - cursorPtr);
                *cursorPtr = kCursorChar;
                end--;
            }
            break;

        case kKeyMinus: case kKeyKpMinus:
            if (cursorPtr != buf || minNum >= 0)
                break;

            swos.convertedKey = '-';
            // assume fall-through

        case kKey0: case kKey1: case kKey2: case kKey3: case kKey4:
        case kKey5: case kKey6: case kKey7: case kKey8: case kKey9:
            if (end - buf - 1 < maxDigits) {
                *cursorPtr++ = swos.convertedKey;
                auto newValue = atoi(buf);

                if (newValue >= minNum && newValue <= maxNum && (newValue != 0 || end == buf + 1 || swos.lastKey != kKey0)) {
                    memmove(cursorPtr + 1, cursorPtr, end++ - cursorPtr + 1);
                    *cursorPtr = kCursorChar;
                } else {
                    *--cursorPtr = kCursorChar;
                }
            }
            break;
        }
    }
}
