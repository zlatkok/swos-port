#include "JoypadConfig.h"
#include "joypads.h"
#include "text.h"
#include "textInput.h"
#include "selectGameControlEventsMenu.h"
#include "configureHatMenu.h"
#include "configureAxisMenu.h"
#include "configureTrackballMenu.h"
#include "joypadQuickConfigMenu.h"
#include "testJoypadMenu.h"
#include "joypadConfig.mnu.h"
#ifdef VIRTUAL_JOYPAD
# include "VirtualJoypad.h"
#endif

using namespace JoypadConfigMenu;

static PlayerNumber m_player;
static int m_joypadIndex;
static SDL_JoystickID m_joypadId;   // must save id to detect disconnects
static const char *m_joypadName;
static JoypadConfig *m_config;

static int m_currentHat;
static int m_currentButton;
static int m_currentAxis;
static int m_currentBall;

static int m_numHats;
static int m_numButtons;
static int m_numAxes;
static int m_numBalls;

static int m_maxElementNumDigits;

static Uint32 m_resetTicks;

struct JoypadElementEntry
{
    int startIndex;
    int currentIndex;
    int configureIndex;
    int numberOfIndex;
    int *currentVar;
    const int *numberOf;
};

static const std::array<JoypadElementEntry, 4> kElementEntries = {{
    { hatsLabel, currentHat, configureHats, numHats, &m_currentHat, &m_numHats },
    { buttonsLabel, currentButton, configureButtons, numButtons, &m_currentButton, &m_numButtons },
    { axesLabel, currentAxis, configureAxes, numAxes, &m_currentAxis, &m_numAxes },
    { ballsLabel, currentBall, configureBalls, numBalls, &m_currentBall, &m_numBalls },
}};

void showJoypadConfigMenu(PlayerNumber player, int joypadIndex)
{
    m_player = player;
    m_joypadIndex = joypadIndex;

    showMenu(joypadConfigMenu);
}

static void initJoypadElements();
static void initializeMenu();
static void fillJoypadBasicInfo();
static void initializeControlElements();
static void updateJoypadElementValues();
static void updatePowerLevel();
static int hideAbsentElements();
static void centerElementsVertically(int numVisibleElements);
static std::pair<int, int> measureMaxElements();
static void resizeElementsToFit(int maxLabelLength);
static void resizeNumElementFields();
static void centerElementsHorizontally();
static void repositionVisibleElements(int startY, int singleLinHeight);
static MenuEntry *getNextEntryInLine(MenuEntry *entry);
static void fixSelectedEntry();
static bool exitIfDisconnected();

static void joypadConfigMenuOnInit()
{
    m_joypadId = joypadId(m_joypadIndex);
    m_config = joypadConfig(m_joypadIndex);

    if (!exitIfDisconnected()) {
        initJoypadElements();
        initializeMenu();
    }
}

static void joypadConfigMenuOnReturn()
{
    if (!exitIfDisconnected())
        initializeMenu();
}

static void joypadConfigMenuOnDraw()
{
    if (!exitIfDisconnected()) {
        updatePowerLevel();

        if (SDL_GetTicks() <= m_resetTicks)
            drawMenuTextCentered(kMenuScreenWidth / 2, kResetTextY, "CONTROLS RESET!");
    }
}

static void initJoypadElements()
{
    m_numHats = joypadNumHats(m_joypadIndex);
    m_numButtons = joypadNumButtons(m_joypadIndex);
    m_numAxes = joypadNumAxes(m_joypadIndex);
    m_numBalls = joypadNumBalls(m_joypadIndex);

    m_currentHat = 0;
    m_currentButton = 0;
    m_currentAxis = 0;
    m_currentBall = 0;
}

static void initializeMenu()
{
    fillJoypadBasicInfo();
    initializeControlElements();
    updateJoypadElementValues();
    fixSelectedEntry();
}

static void fillJoypadBasicInfo()
{
    m_joypadName = joypadName(m_joypadIndex);
    copyStringToEntry(name, m_joypadName);

    auto guidStr = getMenuEntry(guid)->string();
    auto joyGuid = joypadGuid(m_joypadIndex);
    SDL_JoystickGetGUIDString(joyGuid, guidStr, kStdMenuTextSize);

    SDL_itoa(m_joypadId, idEntry.string(), 10);

    updatePowerLevel();

    copyStringToEntry(configuration, m_config->primary() ? "PRIMARY" : "SECONDARY");
}

static void updatePowerLevel()
{
    copyStringToEntry(powerLevel, joypadPowerLevel(m_joypadIndex));
}

static void initializeControlElements()
{
    int numVisibleElemens = hideAbsentElements();
    int maxLabelLength;
    std::tie(m_maxElementNumDigits, maxLabelLength) = measureMaxElements();
    resizeNumElementFields();
    resizeElementsToFit(maxLabelLength);
    centerElementsVertically(numVisibleElemens);
    centerElementsHorizontally();
}

template<typename F>
static inline void forEachElementAtLine(int line, MenuEntry *first, MenuEntry *last, F f)
{
    assert(first && last && first <= last);

    while (first <= last) {
        if (first->y == line)
            f(first);
        first++;
    }
}

static int hideAbsentElements()
{
    auto lastEntry = getMenuEntry(lastElementEntry);

    int numVisibleElements = 0;

    for (const auto& elem : kElementEntries) {
        if (!*elem.numberOf) {
            auto entry = getMenuEntry(elem.startIndex);
            auto line = entry->y;

            forEachElementAtLine(line, entry, lastEntry, [](auto entry) { entry->hide(); });
        } else {
            numVisibleElements++;
        }
    }

    return numVisibleElements;
}

static void centerElementsVertically(int numVisibleElements)
{
    if (!numVisibleElements)
        return;

    auto firstEntry = getMenuEntry(kElementEntries[0].startIndex);
    auto secondEntry = getMenuEntry(kElementEntries[1].startIndex);
    int singleLineHeight = secondEntry->y - firstEntry->y;

    int spaceTaken = singleLineHeight * numVisibleElements;
    int startY = getMenuEntry(lastInfoEntry)->endY();
    int spaceAvaliable = getMenuEntry(topmostButton)->y - startY;

    startY += (spaceAvaliable - spaceTaken) / 2;

    repositionVisibleElements(startY, singleLineHeight);
}

static std::pair<int, int> measureMaxElements()
{
    int maxLabelLength = 0;
    int maxElement = 0;

    for (const auto& elem : kElementEntries) {
        if (*elem.numberOf) {
            maxElement = std::max(maxElement, *elem.numberOf);

            auto labelEntry = getMenuEntry(elem.startIndex);
            int labelLength = getStringPixelLength(labelEntry->string());
            maxLabelLength = std::max(maxLabelLength, labelLength);
        }
    }

    return { numDigits(maxElement), maxLabelLength };
}

static void resizeElementsToFit(int maxLabelLength)
{
    for (const auto& elem : kElementEntries) {
        if (*elem.numberOf) {
            auto entry = getMenuEntry(elem.startIndex);
            entry->width = maxLabelLength + 2;
            int x = entry->endX() + 1;

            while (entry = getNextEntryInLine(entry)) {
                entry->x = x;
                x += entry->width + 1;
            }

            getMenuEntry(elem.configureIndex)->x += 4;
        }
    }
}

static void resizeNumElementFields()
{
    if (m_maxElementNumDigits) {
        int maxWidth = kSmallDigitMaxWidth * m_maxElementNumDigits;

        for (const auto& elem : kElementEntries) {
            if (*elem.numberOf) {
                auto currentEntry = getMenuEntry(elem.currentIndex);
                currentEntry->width = maxWidth + kSmallCursorWidth + 4;
                auto numEntry = getMenuEntry(elem.numberOfIndex);
                numEntry->width = maxWidth + 2;
            }
        }
    }
}

static void centerElementsHorizontally()
{
    for (const auto& elem : kElementEntries) {
        if (*elem.numberOf) {
            auto firstEntry = getMenuEntry(elem.startIndex);
            auto lastEntry = getMenuEntry(lastElementEntry);

            int startX = firstEntry->x;
            int endX = 0;

            forEachElementAtLine(firstEntry->y, firstEntry, lastEntry, [&endX](auto entry) { endX = entry->endX(); });

            int width = endX - startX;
            int newX = (kMenuScreenWidth - width) / 2;
            int delta = newX - startX;

            forEachElementAtLine(firstEntry->y, firstEntry, lastEntry, [delta](auto entry) { entry->x += delta; });
        }
    }
}

static void repositionVisibleElements(int startY, int singleLinHeight)
{
    auto lastEntry = getMenuEntry(lastElementEntry);

    for (const auto& elem : kElementEntries) {
        auto entry = getMenuEntry(elem.startIndex);
        if (entry->visible()) {
            forEachElementAtLine(entry->y, entry, lastEntry, [startY](auto entry) { entry->y = startY; });
            startY += singleLinHeight;
        }
    }
}

static MenuEntry *getNextEntryInLine(MenuEntry *entry)
{
    int line = entry->y;
    auto lastEntry = getMenuEntry(lastElementEntry);

    while ((++entry)->y != line && entry <= lastEntry)
        ;

    return entry <= lastEntry ? entry : nullptr;
}

static void fixSelectedEntry()
{
    auto& selectedEntry = getCurrentMenu()->selectedEntry;
    if (!selectedEntry->selectable()) {
        for (const auto& elem : kElementEntries) {
            auto entry = getMenuEntry(elem.configureIndex);
            if (entry->selectable()) {
                selectedEntry = entry;
                return;
            }
        }
        selectedEntry = getMenuEntry(quickConfig);
    }
}

static bool exitIfDisconnected()
{
    if (joypadDisconnected(m_joypadId)) {
        SetExitMenuFlag();
        return true;
    }

    return false;
}

static void updateJoypadElementValues()
{
    for (const auto& elem : kElementEntries) {
        auto numberOfEntry = getMenuEntry(elem.numberOfIndex);
        numberOfEntry->setNumber(*elem.numberOf);

        auto currentEntry = getMenuEntry(elem.currentIndex);
        currentEntry->setNumber(*elem.currentVar + 1);
    }
}

struct ElementData {
    MenuEntry *currentEntry;
    int *currentVar;
    const int *maxVar;
};

// in: A5 -> +/- entry
static ElementData getElementEntryData()
{
    auto entry = A5.asMenuEntry();

    assert(entry->type == kEntryString && (!strcmp(entry->string(), "+") || !strcmp(entry->string(), "-")));

    for (auto elem : kElementEntries) {
        auto currentEntry = getMenuEntry(elem.currentIndex);
        if (currentEntry->y == entry->y) {
            assert(currentEntry->visible());
            return { currentEntry, elem.currentVar, elem.numberOf };
        }
    }

    assert(false);
    return {};
}

// in: A5 -> increase current element entry "+" button
static void increaseElementIndex()
{
    auto entryData = getElementEntryData();
    if (*entryData.currentVar < *entryData.maxVar - 1)
        entryData.currentEntry->setNumber(++*entryData.currentVar + 1);
}

// in: A5 -> decrease current element entry "-" button
static void decreaseElementIndex()
{
    auto entryData = getElementEntryData();
    if (*entryData.currentVar > 0)
        entryData.currentEntry->setNumber(--*entryData.currentVar + 1);
}

static void inputCurrentElementIndex()
{
    auto entry = A5.asMenuEntry();

    auto it = std::find_if(kElementEntries.begin(), kElementEntries.end(), [entry](const auto& elem) {
        return entry->ordinal == elem.currentIndex;
    });
    assert(it != kElementEntries.end());

    if (it != kElementEntries.end() && inputNumber(*entry, m_maxElementNumDigits, 1, *it->numberOf))
        *it->currentVar = entry->fg.number - 1;
}

static void configureJoypadButton()
{
    auto setFunction = [](int index, GameControlEvents events, bool inverted) {
        m_config->setButtonEvents(index, events, inverted);
    };
    auto getFunction = [](int& index, char *titleBuf, int bufSize) {
        index = std::min(index, m_numButtons - 1);
        snprintf(titleBuf, bufSize, "FOR BUTTON %d %s", index + 1, m_joypadName);
        m_currentButton = index;
        return m_config->getButtonEvents(index);
    };

    updateGameControlEvents(m_currentButton, setFunction, getFunction, exitIfDisconnected, true);
}

// A5 -> configure entry
static void configureSelected()
{
    auto entry = A5.asMenuEntry();

    auto it = std::find_if(kElementEntries.begin(), kElementEntries.end(), [entry](const auto& elem) {
        return entry->ordinal == elem.configureIndex;
    });
    assert(it != kElementEntries.end());

    if (it != kElementEntries.end()) {
        switch (it->configureIndex) {
        case configureHats:
            showConfigureHatMenu(m_joypadIndex, m_currentHat);
            break;
        case configureButtons:
            configureJoypadButton();
            break;
        case configureAxes:
            showConfigureAxisMenu(m_joypadIndex, m_currentAxis);
            break;
        case configureBalls:
            showConfigureTrackballMenu(m_joypadIndex, m_currentBall);
            break;
        default:
            assert(false);
        }
        updateJoypadElementValues();
    }
}

static void joypadQuickConfig()
{
#ifdef VIRTUAL_JOYPAD
    VirtualJoypadEnabler enabler(m_joypadIndex);
#endif

    auto result = promptForDefaultJoypadEvents(m_player, m_joypadIndex);
    if (result.first)
        m_config->assign(result.second);
}

static void resetConfigToDefault()
{
    m_resetTicks = SDL_GetTicks() + 500;
    resetJoypadConfig(m_joypadIndex);
}

static void testJoypad()
{
#ifdef VIRTUAL_JOYPAD
    VirtualJoypadEnabler enabler(m_joypadIndex);
#endif

    showTestJoypadMenu(m_joypadIndex);
}
