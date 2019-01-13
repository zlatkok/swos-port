#include "menu.h"
#include "log.h"
#include "util.h"
#include "render.h"
#include "music.h"
#include "controls.h"

using ReachabilityMap = std::array<bool, 256>;
static ReachabilityMap m_reachableEntries;

static MouseWheelEntryList m_previousMouseWheelEntries;
static MouseWheelEntryList m_mouseWheelEntries;

void SWOS::FlipInMenu()
{
    frameDelay(2);
    updateScreen();
}

// this is actually a debugging function, but used by everyone to quickly exit SWOS :) (it even got into manual)
void SWOS::SetPrevVideoModeEndProgram()
{
    std::exit(0);
}

static void menuDelay()
{
#ifndef NDEBUG
    extern void checkMemory();

    assert(_CrtCheckMemory());
    checkMemory();
#endif

    if (!menuCycleTimer) {
        timerProc();
        timerProc();    // imitate int 8 ;)
    }
}

static void checkMouseWheelAction(const MenuEntry& entry, int scrollValue)
{
    for (const auto& mouseWheelEntry : m_mouseWheelEntries) {
        int ord = std::get<0>(mouseWheelEntry);

        if (ord == entry.ordinal) {
            int scrollUpEntry = std::get<1>(mouseWheelEntry);
            int scrollDownEntry = std::get<2>(mouseWheelEntry);

            assert(scrollUpEntry >= 0 && scrollUpEntry < 256);
            assert(scrollDownEntry >= 0 && scrollDownEntry < 256);

            if (scrollValue > 0) {
                assert(getMenuEntryAddress(scrollUpEntry)->onSelect);
                getMenuEntryAddress(scrollUpEntry)->onSelect();
            } else {
                assert(getMenuEntryAddress(scrollDownEntry)->onSelect);
                getMenuEntryAddress(scrollDownEntry)->onSelect();
            }
        }
    }
}

static bool mapCoordinatesToGameArea(int& x, int& y)
{
    int windowWidth, windowHeight;
    std::tie(windowWidth, windowHeight) = getWindowSize();

    SDL_Rect viewport;
    getViewport(viewport);

    int slackWidth = 0;
    int slackHeight = 0;

    if (viewport.x) {
        auto logicalWidth = kVgaWidth * windowHeight / kVgaHeight;
        slackWidth = (windowWidth - logicalWidth) / 2;
    } else if (viewport.y) {
        auto logicalHeight = kVgaHeight * windowWidth / kVgaWidth;
        slackHeight = (windowHeight - logicalHeight) / 2;
    }

    if (y < slackHeight || y >= windowHeight - slackHeight)
        return false;

    if (x < slackWidth || x >= windowWidth - slackWidth)
        return false;

    x = (x - slackWidth) * kVgaWidth / (windowWidth - 2 * slackWidth);
    y = (y - slackHeight) * kVgaHeight / (windowHeight - 2 * slackHeight);

    return true;
}

static void checkMousePosition()
{
    static int lastX = -1, lastY = -1;
    static bool clickedLastFrame;
    static bool selectedLastFrame = true;
    static Uint32 startingTicks = SDL_GetTicks();

    int x, y;
    auto buttons = SDL_GetMouseState(&x, &y);

    // make sure to discard initial activity while the window is initializing
    if ((x != lastX || y != lastY) && SDL_GetTicks() - startingTicks > 100)
        selectedLastFrame = false;

    lastX = x;
    lastY = y;

    if (clickedLastFrame) {
        if (!buttons) {
            clickedLastFrame = false;
            selectedLastFrame = false;
        }

        releaseFire();
        return;
    }

    auto currentMenu = getCurrentMenu();
    auto entries = reinterpret_cast<MenuEntry *>(g_currentMenu + sizeof(Menu));

    if (buttons & SDL_BUTTON_RMASK) {
        clickedLastFrame = true;
        SetExitMenuFlag();
        return;
    }

    if (selectedLastFrame && !(buttons & SDL_BUTTON_LMASK) && !mouseWheelAmount())
        return;

    if (!mapCoordinatesToGameArea(x, y))
        return;

    if (hasMouseFocus() && !isMatchRunning()) {
        for (size_t i = 0; i < currentMenu->numEntries; i++) {
            auto& entry = entries[i];
            bool insideEntry = x >= entry.x && x < entry.x + entry.width && y >= entry.y && y < entry.y + entry.height;

            if (insideEntry && !entry.invisible && m_reachableEntries[entry.ordinal]) {
                currentMenu->selectedEntry = &entry;
                selectedLastFrame = true;

                bool isPlayMatch = entry.type2 == kEntryString && entry.u2.string && !strcmp(entry.u2.string, "PLAY MATCH");

                if ((buttons & SDL_BUTTON_LMASK) && !isPlayMatch) {
                    pressFire();
                    clickedLastFrame = true;

                    // get rid of the damn click
                    SDL_PumpEvents();
                    while (SDL_GetMouseState(nullptr, nullptr)) {
                        SDL_Delay(10);
                        SDL_PumpEvents();
                    }

                    break;
                } else if (auto scrollValue = mouseWheelAmount()) {
                    checkMouseWheelAction(entry, scrollValue);
                }
            }
        }
    }
}

void setMouseWheelEntries(const MouseWheelEntryList& mouseWheelEntries)
{
    m_mouseWheelEntries = mouseWheelEntries;
}

int getStringPixelLength(const char *str)
{
    A0 = str;
    A1 = smallCharsTable;
    GetTextSize();
    return D7.asInt16();
}

void elideString(char *str, int len, int maxPixels)
{
    assert(str && maxPixels);

    if (len < 4)
        return;

    if (getStringPixelLength(str) <= maxPixels)
        return;
    else
        memcpy(str + len - 3, "...", 3);

    while (getStringPixelLength(str) > maxPixels && len >= 4) {
        str[len-- - 4] = '.';
        str[len] = '\0';
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

static const char *entryText(const MenuEntry *entry)
{
    static char buf[16];
    if (entry) {
        switch (entry->type1) {
        case kEntryString: return entry->u2.string;
        case kEntryMultiLineText: return reinterpret_cast<char *>(entry->u2.multiLineText) + 1;
        case kEntrySprite2:
        case kEntryNumber: return _itoa(entry->u2.number, buf, 10);
        }
    }

    return "/";
}

// Find and mark every reachable item starting from the initial one.
static void determineReachableEntries(const MenuBase *menu)
{
    std::fill(m_reachableEntries.begin(), m_reachableEntries.end(), false);

    static_assert(offsetof(MenuEntry, downEntry) == offsetof(MenuEntry, upEntry) + 1 &&
        offsetof(MenuEntry, upEntry) == offsetof(MenuEntry, rightEntry) + 1 &&
        offsetof(MenuEntry, rightEntry) == offsetof(MenuEntry, leftEntry) + 1 &&
        sizeof(MenuEntry::leftEntry) == 1, "MenuEntry(): assumptions about next entries failed");

    std::vector<std::pair<const MenuEntry *, int>> entryStack;
    entryStack.reserve(256);

    auto currentMenu = getCurrentMenu();
    auto currentEntry = currentMenu->selectedEntry;
    auto entries = reinterpret_cast<MenuEntry *>(g_currentMenu + sizeof(Menu));

    entryStack.emplace_back(currentEntry, 0);

    while (!entryStack.empty()) {
        const MenuEntry *entry;
        int direction;

        std::tie(entry, direction) = entryStack.back();

        assert(entry && direction >= 0 && direction <= 4);
        assert(entry->ordinal < 256);

        m_reachableEntries[entry->ordinal] = true;

        auto nextEntries = reinterpret_cast<const uint8_t *>(&entry->leftEntry);

        for (; direction < 4; direction++) {
            auto ord = nextEntries[direction];

            if (ord != 255 && !m_reachableEntries[ord]) {
                entryStack.emplace_back(&entries[ord], 0);
                break;
            }
        }

        if (direction >= 4) {
            entryStack.pop_back();
        } else {
            assert(entryStack.size() >= 2);
            entryStack[entryStack.size() - 2].second = direction + 1;
        }
    }
}

// these are macros instead of functions because we need to save these variables for each level of nesting;
// being functions they can't create local variables on callers stack, and then we'd need explicit stacks
#define saveCurrentMenu() \
    auto currentMenu = getCurrentMenu(); \
    auto savedEntry = currentMenu->selectedEntry; \
    auto savedMenu = g_currentMenuPtr; \

#define restorePreviousMenu() \
    A6 = savedMenu; \
    A5 = savedEntry; \
    SAFE_INVOKE(RestorePreviousMenu); \

// ShowMenu
//
// in:
//     A6 -> menu to show on screen (packed form)
//
// After showing this menu, returns to previous menu.
//
void SWOS::ShowMenu()
{
    g_scanCode = 0;
    controlWord = 0;
    menuStatus = 0;

    m_previousMouseWheelEntries = m_mouseWheelEntries;
    m_mouseWheelEntries.clear();

    auto newMenu = A6.asPtr();
    saveCurrentMenu();
    logInfo("Showing menu %#x [%s], previous menu is %#x", A6.data, entryText(savedEntry), savedMenu);

    g_exitMenu = 0;
    SAFE_INVOKE(PrepareMenu);

    while (!g_exitMenu)
        menuProcCycle();

    menuStatus = 1;
    g_exitMenu = 0;

    restorePreviousMenu();
    determineReachableEntries(savedMenu);

    m_mouseWheelEntries = m_previousMouseWheelEntries;

    logInfo("Menu %#x finished, restoring menu %#x", newMenu, g_currentMenuPtr);
}

// called by SWOS, when we're starting the game
void SWOS::ToMainGameLoop()
{
    logInfo("Starting the game...");

    saveCurrentMenu();
    save68kRegisters();

    SAFE_INVOKE(StartMainGameLoop);

    restore68kRegisters();
    restorePreviousMenu();
}

__declspec(naked) void SWOS::DoUnchainSpriteInMenus_OnEnter()
{
    // only cx is loaded, but later whole ecx is tested; make this right
    _asm {
        xor ecx, ecx
        retn
    }
}

// PrepareMenu
//
// in:
//     A6 -> menu to prepare (or activate)
// globals:
//     g_currentMenu - destination for unpacked menu.
//
// Unpack given static menu into g_currentMenu. Execute menu initialization function, if present.
//
void SWOS::PrepareMenu()
{
    g_currentMenuPtr = A6;
    auto packedMenu = A6.as<const MenuBase *>();

    A0 = A6;
    A1 = g_currentMenu;
    ConvertMenu();

    auto currentMenu = getCurrentMenu();
    auto currentEntry = reinterpret_cast<uint32_t>(currentMenu->selectedEntry);

    if (currentEntry <= 0xffff)
        currentMenu->selectedEntry = getMenuEntryAddress(currentEntry);

    if (currentMenu->onInit) {
        A6 = g_currentMenu;
        auto proc = currentMenu->onInit;
        SAFE_INVOKE(proc);
    }

    assert(currentMenu->numEntries < 256);

    determineReachableEntries(packedMenu);

    updateMatchControls();
}

void SWOS::InitMainMenu()
{
    InitMenuMusic();
    SAFE_INVOKE(InitMainMenuStuff);
    menuFade = 1;
}

static MenuEntry *findNextEntry(byte nextEntryIndex, int nextEntryDirection)
{
    auto currentMenu = getCurrentMenu();

    while (nextEntryIndex != 255) {
        auto nextEntry = getMenuEntryAddress(nextEntryIndex);

        if (!nextEntry->disabled && !nextEntry->invisible)
            return nextEntry;

        auto newDirection = (&nextEntry->leftDirection)[nextEntryDirection];
        if (newDirection != 255) {
            nextEntryIndex = (&nextEntry->leftEntryDis)[nextEntryDirection];
            nextEntryDirection = newDirection;
            assert(newDirection >= 0 && newDirection <= 3);
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

    save68kRegisters();
    auto func = entry->onSelect;

    menuCycleTimer = 1;     // set cycle timer to prevent InputText choking

    SAFE_INVOKE(func);
    restore68kRegisters();
}

static bool isPlayMatchEntry(const MenuEntry *entry)
{
    return entry && entry->onSelect == PlayMatchSelected;
}

static void playMatchSelected(int playerNo)
{
    if (reinterpret_cast<TeamFile *>(careerTeam)->teamControls == KComputerTeam)
        pl1IsntPlayer = 1;

    useColorTable = 0;
    playerNumThatStarted = playerNo;
    player1ClearFlag = 0;
    player2ClearFlag = 0;
    SetExitMenuFlag();
}

void SWOS::MenuProc()
{
    checkMousePosition();

    g_videoSpeedIndex = 50;     // just in case

    ReadTimerDelta();
    DrawMenu();
    menuCycleTimer = 0;         // must come after DrawMenu(), set to 1 to slow down input

    if (menuFade) {
        FadeIn();
        menuFade = 0;
    }

    CheckControls();
    updateSongState();

    auto currentMenu = getCurrentMenu();
    auto activeEntry = currentMenu->selectedEntry;
    MenuEntry *nextEntry = nullptr;

    if (activeEntry || finalControlsStatus >= 0 && !previousMenuItem) {
        if (isPlayMatchEntry(activeEntry)) {
            int playerNo = matchControlsSelected(activeEntry);
            updateMatchControls();

            if (playerNo >= 0) {
                playMatchSelected(playerNo);
                return;
            }
        }

        int controlMask = 0;
        controlsStatus = 0;

        if (activeEntry->onSelect) {
            if (shortFire)
                controlMask |= 0x20;
            if (fire)
                controlMask |= 0x01;
            if (left)
                controlMask |= 0x02;
            if (right)
                controlMask |= 0x04;
            if (up)
                controlMask |= 0x08;
            if (down)
                controlMask |= 0x10;

            if (finalControlsStatus >= 0) {
                switch (finalControlsStatus >> 1) {
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

            controlMask &= activeEntry->controlMask;

            if (activeEntry->controlMask == -1 || controlMask) {
                controlsStatus = controlMask;
                invokeOnSelect(activeEntry);
            }
        }

        // if no fire but there's movement, move to next entry
        if (!fire && finalControlsStatus >= 0) {
            static const size_t nextDirectionOffsets[4] = { 2, 1, 3, 0, };

            // will hold offset of next entry to move to (direction)
            int nextEntryDirection = nextDirectionOffsets[(finalControlsStatus >> 1) & 3];
            auto nextEntryIndex = (&activeEntry->leftEntry)[nextEntryDirection];

            nextEntry = findNextEntry(nextEntryIndex, nextEntryDirection);
        }
    } else if (finalControlsStatus < 0) {
        return;
    } else if (previousMenuItem) {
        currentMenu->selectedEntry = previousMenuItem;
        nextEntry = previousMenuItem;
    }

    if (nextEntry) {
        previousMenuItem = currentMenu->selectedEntry;
        currentMenu->selectedEntry = nextEntry;
        nextEntry->type1 == kEntryNoBackground ? DrawMenu() : DrawMenuItem();
    }
}

void SWOS::ExitPlayMatch()
{
    g_exitGameFlag = 1;
    player1ClearFlag = 0;
    player2ClearFlag = 0;
    useColorTable = 0;
    isCareer = 0;

    if (playMatchTeam1Ptr == careerTeam || playMatchTeam2Ptr == careerTeam)
        isCareer = 1;

    SetExitMenuFlag();

    resetMatchControls();
}

// in:
//      D0 - word to convert
//      A1 - buffer
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
        return;
    }

    auto end = dest;
    while (num) {
        auto quotRem = std::div(num, 10);
        num = quotRem.quot;
        *end++ = quotRem.rem + '0';
    }

    *end = '\0';

    for (end--; dest < end; dest++, end--)
        std::swap(*dest, *end);
}

void SWOS::InputText_23()
{
    if (lastKey == kKeyEscape || SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK) {
        convertedKey = kKeyEscape;
        inputingText = 0;
    }
}

bool inputNumber(MenuEntry *entry, int maxDigits, int minNum, int maxNum)
{
    assert(entry->type2 == kEntryNumber);

    int num = static_cast<int16_t>(entry->u2.number);
    assert(num >= minNum && num <= maxNum);

    inputingText = -1;

    char buf[32];
    _itoa_s(num, buf, 10);
    assert(static_cast<int>(strlen(buf)) <= maxDigits);

    auto end = buf + strlen(buf);
    *end++ = -1;    // insert cursor
    *end = '\0';

    entry->type2 = kEntryString;
    entry->u2.string = buf;

    auto cursorPtr = end - 1;

    while (true) {
        updateControls();
        DrawMenu();
        SWOS::WaitRetrace();

        SWOS::GetKey();

        while (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK) {
            lastKey = kKeyEscape;
            updateControls();
            SDL_Delay(15);
        }

        switch (lastKey) {
        case kKeyEnter:
            memmove(cursorPtr, cursorPtr + 1, end - cursorPtr);
            entry->type2 = kEntryNumber;
            entry->u2.number = atoi(buf);
            inputingText = 0;
            controlWord = 0;
            return true;

        case kKeyEscape:
            A5 = entry;
            entry->type2 = kEntryNumber;
            entry->u2.number = num;
            inputingText = 0;   // original SWOS leaves it set, wonder if it's significant...
            controlWord = 0;
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

            convertedKey = '-';
            // assume fall-through

        case kKey0: case kKey1: case kKey2: case kKey3: case kKey4:
        case kKey5: case kKey6: case kKey7: case kKey8: case kKey9:
            if (end - buf - 1 < maxDigits) {
                *cursorPtr++ = convertedKey;
                auto newValue = atoi(buf);

                if (newValue >= minNum && newValue <= maxNum && (newValue != 0 || end == buf + 1 || lastKey != kKey0)) {
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
