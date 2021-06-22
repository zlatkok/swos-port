#include "configureAxisMenu.h"
#include "joypads.h"
#include "util.h"
#include "text.h"
#include "gameControlEvents.h"
#include "textInput.h"
#include "selectGameControlEventsMenu.h"
#include "configureAxis.mnu.h"

static int m_joypadIndex;
static SDL_JoystickID m_joypadId;
static int m_axisIndex;
static int m_numAxes;

static int m_currentInterval;
static JoypadConfig::AxisIntervalList *m_intervals;

static int m_maxIntervalDigits;
static int m_maxAxesDigits;

using namespace ConfigureAxisMenu;

void showConfigureAxisMenu(int joypadIndex, int axisIndex)
{
    m_joypadIndex = joypadIndex;
    m_axisIndex = axisIndex;

    showMenu(configureAxisMenu);
}

static void initMenu();
static void initIntervals();
static void applyCurrentAxis();
static void applyCurrentInterval();
static void setHeader();
static void setAxisLabel();
static void setIntervalLabel();
static void resizeAxisBox();
static void resizeIntervalBox();
static void centerAxisNavigationBoxes();
static void centerIntervalNavigationBoxes();
static void setCurrentAxisBox();
static void setCurrentIntervalBox();
static void setFromToIntervalBoxes();
static void updateEventsBox();
static bool exitIfDisconnected();

static void configureAxisMenuOnInit()
{
    m_numAxes = joypadNumAxes(m_joypadIndex);
    m_joypadId = joypadId(m_joypadIndex);
    m_maxAxesDigits = numDigits(m_numAxes);

    if (!exitIfDisconnected()) {
        initIntervals();
        initMenu();
    }
}

static void configureAxisMenuOnRestore()
{
    if (!exitIfDisconnected())
        initMenu();
}

static void configureAxisMenuOnDraw()
{
    exitIfDisconnected();
}

static void goToPreviousInterval()
{
    if (m_currentInterval > 0) {
        m_currentInterval--;
        applyCurrentInterval();
    }
}

static void goToNextInterval()
{
    if (m_currentInterval < static_cast<int>(m_intervals->size()) - 1) {
        m_currentInterval++;
        applyCurrentInterval();
    }
}

static void inputCurrentInterval()
{
    auto entry = A5.asMenuEntry();

    if (inputNumber(*entry, m_maxIntervalDigits, 1, m_intervals->size())) {
        m_currentInterval = entry->fg.number - 1;
        applyCurrentInterval();
    }
}

static void goToPreviousAxis()
{
    if (m_axisIndex > 0) {
        m_axisIndex--;
        applyCurrentAxis();
    }
}

static void goToNextAxis()
{
    if (m_axisIndex < m_numAxes - 1) {
        m_axisIndex++;
        applyCurrentAxis();
    }
}

static void inputCurrentAxis()
{
    auto entry = A5.asMenuEntry();
    auto oldAxis = entry->fg.number;

    if (inputNumber(*entry, m_maxAxesDigits, 1, m_numAxes) && entry->fg.number != oldAxis) {
        m_currentInterval = entry->fg.number - 1;
        applyCurrentAxis();
    }
}

// in: A5 -> input entry
static void inputIntervalBoundary(bool inputFrom)
{
    auto entry = A5.asMenuEntry();

    auto boundary = &(*m_intervals)[m_currentInterval].from;
    auto otherBoundary = &(*m_intervals)[m_currentInterval].to;

    if (!inputFrom)
        std::swap(boundary, otherBoundary);

    auto oldString = entry->string();
    entry->type = kEntryNumber;
    entry->setNumber(*boundary);

    if (inputNumber(*entry, 6, INT16_MIN, INT16_MAX)) {
        *boundary = static_cast<int16_t>(entry->fg.number);

        if ((inputFrom && *boundary > *otherBoundary) || (!inputFrom && *boundary < *otherBoundary))
            std::swap(*boundary, *otherBoundary);

        entry->type = kEntryString;
        entry->fg.string = oldString;

        setFromToIntervalBoxes();
    }
}

static void inputIntervalStart()
{
    assert(A5.asMenuEntry()->ordinal == fromInput);
    inputIntervalBoundary(true);
}

static void inputIntervalEnd()
{
    assert(A5.asMenuEntry()->ordinal == toInput);
    inputIntervalBoundary(false);
}

static void pickEvents()
{
    auto setFunction = [](int index, GameControlEvents events, bool) {
        (*m_intervals)[index].events = events;
    };

    auto getFunction = [](int& index, char *titleBuf, int titleBufSize) {
        assert(m_intervals && !m_intervals->empty());

        index = std::min<int>(index, m_intervals->size() - 1);
        snprintf(titleBuf, titleBufSize, "AXIS %d INTERVAL %d", m_axisIndex + 1, index + 1);

        auto events = (*m_intervals)[index].events;
        return std::make_pair(events, false);
    };

    updateGameControlEvents(m_currentInterval, setFunction, getFunction, exitIfDisconnected);
    updateEventsBox();
}

static void createInterval()
{
    m_intervals->emplace_back();
    m_currentInterval = m_intervals->size() - 1;
    applyCurrentInterval();
}

static void deleteCurrentInterval()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "AXIS %d INTERVAL %d OF", m_axisIndex + 1, m_currentInterval + 1);
    bool confirmation = showContinueAbortPrompt("DELETE INTERVAL", swos.aOk, swos.aCancel,
        { "YOU ARE ABOUT TO DELETE", buf, joypadName(m_joypadIndex) });

    if (confirmation) {
        m_intervals->erase(m_intervals->begin() + m_currentInterval);

        if (m_intervals->empty())
            m_intervals->emplace_back();

        if (m_currentInterval >= static_cast<int>(m_intervals->size()))
            m_currentInterval--;

        applyCurrentInterval();
    }
}

static void initMenu()
{
    setHeader();
    setAxisLabel();
    resizeAxisBox();
    centerAxisNavigationBoxes();

    applyCurrentAxis();
}

void initIntervals()
{
    m_intervals = joypadAxisIntervals(m_joypadIndex, m_axisIndex);
    assert(m_intervals);

    if (m_intervals->empty())
        m_intervals->emplace_back();

    m_currentInterval = 0;
}

static void applyCurrentAxis()
{
    setCurrentAxisBox();
    initIntervals();
    applyCurrentInterval();
}

static void applyCurrentInterval()
{
    setIntervalLabel();
    resizeIntervalBox();
    centerIntervalNavigationBoxes();
    setCurrentIntervalBox();
    setFromToIntervalBoxes();
    updateEventsBox();
}

static void setHeader()
{
    auto headerEntry = getMenuEntry(header);
    auto buf = headerEntry->string();
    snprintf(buf, kStdMenuTextSize, "CONFIGURE AXIS FOR %s", joypadName(m_joypadIndex));

    int textLen = getStringPixelLength(buf);
    textLen = std::max<int>(headerEntry->width, textLen + 8);
    headerEntry->width = std::min(textLen, kMenuScreenWidth);

    headerEntry->x = (kMenuScreenWidth - headerEntry->width) / 2;
}

static void setAxisLabel()
{
    auto buf = getMenuEntry(axisLabel)->string();
    snprintf(buf, kStdMenuTextSize, "AXIS: (MAX %d)", m_numAxes);

    getMenuEntry(currentAxis)->setNumber(m_axisIndex + 1);
}

static void setIntervalLabel()
{
    auto buf = getMenuEntry(intervalLabel)->string();
    snprintf(buf, kStdMenuTextSize, "INTERVAL: (MAX %zu)", m_intervals->size());
}

static void resizeAxisBox()
{
    auto entry = getMenuEntry(currentAxis);
    entry->width = m_maxAxesDigits * kSmallDigitMaxWidth + kSmallCursorWidth + 4;
}

static void resizeIntervalBox()
{
    m_maxIntervalDigits = numDigits(m_intervals->size());

    auto entry = getMenuEntry(currentInterval);
    entry->width = m_maxIntervalDigits * kSmallDigitMaxWidth + kSmallCursorWidth + 4;
}

static void centerNavigationBoxes(const std::vector<int>& entryIndices)
{
    assert(!entryIndices.empty() && std::all_of(entryIndices.begin(), entryIndices.end() - 1, [](int index) {
        return getMenuEntry(index)->y == (getMenuEntry(index) + 1)->y; }));

    int totalWidth = 0;
    for (auto entryIndex : entryIndices)
        totalWidth += getMenuEntry(entryIndex)->width;

    totalWidth += 2 * kRowGap;
    int x = (kMenuScreenWidth - totalWidth) / 2;

    for (auto entryIndex : entryIndices) {
        auto entry = getMenuEntry(entryIndex);
        entry->x = x;
        x += entry->width + kRowGap;
    }
}

static void centerAxisNavigationBoxes()
{
    centerNavigationBoxes({ prevAxis, currentAxis, nextAxis });
}

static void centerIntervalNavigationBoxes()
{
    centerNavigationBoxes({ prevInterval, currentInterval, nextInterval });
}

void setCurrentAxisBox()
{
    getMenuEntry(currentAxis)->setNumber(m_axisIndex + 1);
}

static void setCurrentIntervalBox()
{
    getMenuEntry(currentInterval)->setNumber(m_currentInterval + 1);
}

static void setFromToIntervalBoxes()
{
    auto from = (*m_intervals)[m_currentInterval].from;
    auto to = (*m_intervals)[m_currentInterval].to;

    auto fromEntry = getMenuEntry(fromInput);
    auto toEntry = getMenuEntry(toInput);

    SDL_itoa(from, fromEntry->string(), 10);
    SDL_itoa(to, toEntry->string(), 10);
}

static void updateEventsBox()
{
    auto buf = getMenuEntry(events)->string();
    auto events = (*m_intervals)[m_currentInterval].events;

    gameControlEventToString(events, buf, kStdMenuTextSize);
}

static bool exitIfDisconnected()
{
    if (joypadDisconnected(m_joypadId)) {
        SetExitMenuFlag();
        return true;
    }

    return false;
}
