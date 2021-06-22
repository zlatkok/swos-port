#include "configureHatMenu.h"
#include "joypads.h"
#include "text.h"
#include "util.h"
#include "gameControlEvents.h"
#include "textInput.h"
#include "selectGameControlEventsMenu.h"
#include "configureHat.mnu.h"

static int m_joypadIndex;
static SDL_JoystickID m_joypadId;
static int m_hatIndex;

static int m_currentBinding;
static int m_maxBindingDigits;

static JoypadConfig::HatBindingList *m_bindings;

using namespace ConfigureHatMenu;

void showConfigureHatMenu(int joypadIndex, int hatIndex)
{
    m_joypadIndex = joypadIndex;
    m_hatIndex = hatIndex;

    showMenu(configureHatMenu);
}

static void initMenu();
static void setHeader();
static void setBindingLabel();
static void setCurrentBindingValue();
static void initBindingFields();
static void applyBindingData();
static void updateEventsBox();
static void updateMask();
static void setMask(bool activate, int labelIndex, int statusIndex);
static bool exitIfDisconnected();

static void configureHatMenuOnInit()
{
    m_joypadId = joypadId(m_joypadIndex);
    m_bindings = joypadHatBindings(m_joypadIndex, m_hatIndex);
    assert(m_bindings);

    if (!exitIfDisconnected()) {
        if (m_bindings->empty())
            m_bindings->emplace_back();

        m_currentBinding = 0;
        m_maxBindingDigits = numDigits(m_bindings->size());

        initMenu();
    }
}

static void configureHatMenuOnRestore()
{
    if (!exitIfDisconnected())
        initMenu();
}

static void configureHatMenuOnDraw()
{
    exitIfDisconnected();
}

static void inputCurrentBinding()
{
    auto entry = A5.asMenuEntry();

    if (inputNumber(*entry, m_maxBindingDigits, 1, m_bindings->size()))
        m_currentBinding = entry->fg.number - 1;
}

static void pickEvents()
{
    auto setFunction = [](int index, GameControlEvents events, bool) {
        (*m_bindings)[index].events = events;
    };

    auto getFunction = [](int& index, char *titleBuf, int titleBufSize) {
        assert(m_bindings && !m_bindings->empty());

        index = std::min<int>(index, m_bindings->size() - 1);
        snprintf(titleBuf, titleBufSize, "HAT %d BINDING %d", m_hatIndex + 1, index + 1);

        const auto& binding = (*m_bindings)[index];
        return std::make_pair(binding.events, binding.inverted);
    };

    updateGameControlEvents(m_currentBinding, setFunction, getFunction);
    updateEventsBox();
}

static void showPreviousBinding()
{
    if (m_currentBinding > 0) {
        m_currentBinding--;
        applyBindingData();
    }
}

static void showNextBinding()
{
    if (m_currentBinding < static_cast<int>(m_bindings->size()) - 1) {
        m_currentBinding++;
        applyBindingData();
    }
}

static void toggleEvent()
{
    auto entry = A5.asMenuEntry();

    int index = entry->ordinal - firstEventStatus;
    if (entry->ordinal >= firstEventLabel && entry->ordinal <= lastEventLabel)
        index = entry->ordinal - firstEventLabel;

    assert(static_cast<size_t>(index) <= lastEventLabel - firstEventLabel);

    bool active = getMenuEntry(firstEventStatus + index)->string() == swos.aOn;

    int mask;

    switch (firstEventStatus + index) {
    case leftStatus:  mask = SDL_HAT_LEFT; break;
    case rightStatus: mask = SDL_HAT_RIGHT; break;
    case upStatus:    mask = SDL_HAT_UP; break;
    case downStatus:  mask = SDL_HAT_DOWN; break;
    default:
        assert(false);
        return;
    }

    auto& bindingMask = (*m_bindings)[m_currentBinding].mask;
    if (active)
        bindingMask &= ~mask;
    else
        bindingMask |= mask;

    setMask(!active, firstEventLabel + index, firstEventStatus + index);
}

static void toggleInverted()
{
    SDL_UNUSED auto entry = A5.asMenuEntry();

    assert(entry->ordinal == invertedLabel || entry->ordinal == invertedStatus);

    bool active = getMenuEntry(invertedStatus)->string() == swos.aOn;

    (*m_bindings)[m_currentBinding].inverted = !active;
    setMask(!active, invertedLabel, invertedStatus);
}

static void createBinding()
{
    m_bindings->emplace_back();
    m_currentBinding = m_bindings->size() - 1;
    applyBindingData();
}

static void deleteCurrentBinding()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "HAT %d BINDING %d OF", m_hatIndex + 1, m_currentBinding + 1);
    bool confirmation = showContinueAbortPrompt("DELETE BINDING", swos.aOk, swos.aCancel,
        { "YOU ARE ABOUT TO DELETE", buf, joypadName(m_joypadIndex) });

    if (confirmation) {
        m_bindings->erase(m_bindings->begin() + m_currentBinding);

        if (m_bindings->empty())
            m_bindings->emplace_back();

        if (m_currentBinding >= static_cast<int>(m_bindings->size()))
            m_currentBinding--;

        applyBindingData();
    }
}

static void initMenu()
{
    setHeader();
    initBindingFields();
    applyBindingData();
}

static void setHeader()
{
    auto headerEntry = getMenuEntry(header);
    auto name = joypadName(m_joypadIndex);
    auto buf = headerEntry->string();

    // joypad names with spaces... >_<
    int count = snprintf(buf, kStdMenuTextSize, "CONFIGURE HAT %d (%s", m_hatIndex + 1, name);
    count = std::min(count, kStdMenuTextSize - 2);

    while (buf[count - 1] == ' ')
        count--;

    buf[count] = ')';
    buf[count + 1] = '\0';

    auto width = std::min(getStringPixelLength(buf) + 8, kMenuScreenWidth);
    headerEntry->width = std::max<int>(headerEntry->width, width);
    headerEntry->x = (kMenuScreenWidth - headerEntry->width) / 2;
}

static void resizeCurrentBindingField()
{
    auto entry = getMenuEntry(currentBinding);
    entry->width = m_maxBindingDigits * kSmallDigitMaxWidth + kSmallCursorWidth + 4;
}

static void repositionBindingFields()
{
    static const int kBindingFields[] = { prevBinding, currentBinding, nextBinding };

    int totalWidth = 0;
    for (auto entryIndex : kBindingFields)
        totalWidth += getMenuEntry(entryIndex)->width;

    totalWidth += 2 * kRowGap;
    int x = (kMenuScreenWidth - totalWidth) / 2;

    for (auto entryIndex : kBindingFields) {
        auto entry = getMenuEntry(entryIndex);
        entry->x = x;
        x += entry->width + kRowGap;
    }
}

static void initBindingFields()
{
    resizeCurrentBindingField();
    repositionBindingFields();
}

static void applyBindingData()
{
    setBindingLabel();
    setCurrentBindingValue();
    updateMask();
    updateEventsBox();
}

static void setBindingLabel()
{
    auto buf = getMenuEntry(bindingLabel)->string();
    snprintf(buf, kStdMenuTextSize, "CURRENT BINDING: (TOTAL %zu)", m_bindings->size());
}

static void setCurrentBindingValue()
{
    auto entry = getMenuEntry(currentBinding);
    entry->setNumber(m_currentBinding + 1);
}

static void updateMask()
{
    auto mask = (*m_bindings)[m_currentBinding].mask;
    bool inverted = (*m_bindings)[m_currentBinding].inverted;

    setMask((mask & SDL_HAT_UP) != 0, upLabel, upStatus);
    setMask((mask & SDL_HAT_DOWN) != 0, downLabel, downStatus);
    setMask((mask & SDL_HAT_LEFT) != 0, leftLabel, leftStatus);
    setMask((mask & SDL_HAT_RIGHT) != 0, rightLabel, rightStatus);
    setMask(inverted, invertedLabel, invertedStatus);
}

static void updateEventsBox()
{
    auto destBuffer = getMenuEntry(events)->string();
    auto events = (*m_bindings)[m_currentBinding].events;
    gameControlEventToString(events, destBuffer, kStdMenuTextSize);
}

static void setMask(bool activate, int labelIndex, int statusIndex)
{
    auto color = activate ? kOnColor : kOffColor;
    getMenuEntry(labelIndex)->setBackgroundColor(color);
    auto status = getMenuEntry(statusIndex);
    status->setBackgroundColor(color);
    status->setString(activate ? swos.aOn : swos.aOff);
}

static bool exitIfDisconnected()
{
    if (joypadDisconnected(m_joypadId)) {
        SetExitMenuFlag();
        return true;
    }

    return false;
}
