#include "JoypadConfig.h"
#include "Joypad.h"

static constexpr char kButtonsKey[] = "buttons";
static constexpr char kAxesKey[] = "axes";
static constexpr char kHatsKey[] = "hats";
static constexpr char kBallsKey[] = "trackBalls";

void JoypadConfig::initWithMapping(const char *mapping)
{
    SdlMappingParser parser(mapping);
    parser.traverse([this](auto output, auto input, auto index, auto hatMask, auto range, auto inverted) {
        if (output == SdlMappingParser::OutputType::kOther || input == SdlMappingParser::InputType::kOther)
            return;

        switch (output) {
        case SdlMappingParser::OutputType::kButtonA:
        case SdlMappingParser::OutputType::kButtonB:
        case SdlMappingParser::OutputType::kButtonStart:
        case SdlMappingParser::OutputType::kDpadUp:
        case SdlMappingParser::OutputType::kDpadDown:
        case SdlMappingParser::OutputType::kDpadLeft:
        case SdlMappingParser::OutputType::kDpadRight:
            {
                GameControlEvents event;
                switch (output) {
                case SdlMappingParser::OutputType::kButtonA: event = kGameEventKick; break;
                case SdlMappingParser::OutputType::kButtonB: event = kGameEventBench; break;
                case SdlMappingParser::OutputType::kButtonStart: event = kGameEventKick; break;
                case SdlMappingParser::OutputType::kDpadUp: event = kGameEventUp; break;
                case SdlMappingParser::OutputType::kDpadDown: event = kGameEventDown; break;
                case SdlMappingParser::OutputType::kDpadLeft: event = kGameEventLeft; break;
                case SdlMappingParser::OutputType::kDpadRight: event = kGameEventRight; break;
                }

                switch (input) {
                case SdlMappingParser::InputType::kButton:
                    m_buttons.emplace_back(index, event, inverted);
                    break;
                case SdlMappingParser::InputType::kHat:
                    findOrCreateHat(index).bindings.emplace_back(hatMask, event, inverted);
                    break;
                case SdlMappingParser::InputType::kAxis:
                    {
                        int from, to;
                        switch (range) {
                        case SdlMappingParser::Range::kFull:
                            if (!inverted) {
                                from = kDefaultDeadZone;
                                to = SDL_JOYSTICK_AXIS_MAX;
                            } else {
                                from = SDL_JOYSTICK_AXIS_MIN;
                                to = -kDefaultDeadZone;
                            }
                            break;
                        case SdlMappingParser::Range::kNegativeOnly:
                            if (!inverted) {
                                from = SDL_JOYSTICK_AXIS_MIN;
                                to = -kDefaultDeadZone;
                            } else {
                                from = SDL_JOYSTICK_AXIS_MIN + kDefaultDeadZone;
                                to = 0;
                            }
                            break;
                        case SdlMappingParser::Range::kPositiveOnly:
                            if (!inverted) {
                                from = kDefaultDeadZone;
                                to = SDL_JOYSTICK_AXIS_MAX;
                            } else {
                                from = 0;
                                to = SDL_JOYSTICK_AXIS_MAX - kDefaultDeadZone;
                            }
                            break;
                        }
                        auto& axis = findOrCreateAxis(index);
                        axis.intervals.emplace_back(from, to, event);
                        axis.intervals.emplace_back(from, to, event);
                    }
                    break;
                }
            }
            break;
        case SdlMappingParser::OutputType::kAxisLeftX:
        case SdlMappingParser::OutputType::kAxisLeftY:
        case SdlMappingParser::OutputType::kAxisRightX:
        case SdlMappingParser::OutputType::kAxisRightY:
            {
                GameControlEvents minEvent, maxEvent;
                switch (output) {
                case SdlMappingParser::OutputType::kAxisLeftX:
                case SdlMappingParser::OutputType::kAxisRightX:
                    maxEvent = kGameEventRight;
                    minEvent = kGameEventLeft;
                    break;
                case SdlMappingParser::OutputType::kAxisLeftY:
                    maxEvent = kGameEventDown;
                    minEvent = kGameEventUp;
                    break;
                case SdlMappingParser::OutputType::kAxisRightY:
                    maxEvent = kGameEventZoomOut;
                    minEvent = kGameEventZoomIn;
                    break;
                }

                switch (input) {
                    // these don't make much sense but we'll handle them anyway
                case SdlMappingParser::InputType::kButton:
                case SdlMappingParser::InputType::kHat:
                    {
                        if (input == SdlMappingParser::InputType::kButton) {
                            m_buttons.emplace_back(index, maxEvent, inverted);
                            m_buttons.emplace_back(index, minEvent, !inverted);
                        } else {
                            auto& hat = findOrCreateHat(index);
                            hat.bindings.emplace_back(hatMask, maxEvent, inverted);
                            hat.bindings.emplace_back(hatMask, minEvent, !inverted);
                        }
                    }
                    break;
                case SdlMappingParser::InputType::kAxis:
                    mapAxis(minEvent, maxEvent, index, range, inverted);
                    break;
                }
            }
        }
    });
}

// Assigns default configuration which will be pruned later according to what the joypad actually has.
void JoypadConfig::init()
{
    assert(m_buttons.empty() && m_axes.empty() && m_hats.empty() && m_balls.empty());

    m_buttons.emplace_back(0, kGameEventKick, false);
    m_buttons.emplace_back(1, kGameEventBench, false);

    m_axes.emplace_back(0);
    auto& horizontalAxis = m_axes.back();
    horizontalAxis.intervals.emplace_back(SDL_JOYSTICK_AXIS_MIN, -kDefaultDeadZone, kGameEventLeft);
    horizontalAxis.intervals.emplace_back(kDefaultDeadZone, SDL_JOYSTICK_AXIS_MAX, kGameEventRight);

    m_axes.emplace_back(1);
    auto& verticalAxis = m_axes.back();
    verticalAxis.intervals.emplace_back(SDL_JOYSTICK_AXIS_MIN, -kDefaultDeadZone, kGameEventUp);
    verticalAxis.intervals.emplace_back(kDefaultDeadZone, SDL_JOYSTICK_AXIS_MAX, kGameEventDown);

    m_hats.emplace_back(0);
    auto& hat = m_hats.back();
    hat.bindings.emplace_back(SDL_HAT_LEFT, kGameEventLeft, false);
    hat.bindings.emplace_back(SDL_HAT_RIGHT, kGameEventRight, false);
    hat.bindings.emplace_back(SDL_HAT_UP, kGameEventUp, false);
    hat.bindings.emplace_back(SDL_HAT_DOWN, kGameEventDown, false);

    m_balls.emplace_back(0);
    auto& ball = m_balls.back();
    ball.binding.xNegEvents = kGameEventLeft;
    ball.binding.xPosEvents = kGameEventRight;
    ball.binding.yNegEvents = kGameEventUp;
    ball.binding.yPosEvents = kGameEventDown;
}

void JoypadConfig::assign(const DefaultJoypadElementList& elements)
{
    clear();

    for (int i = 0; i < kNumDefaultGameControlEvents; i++) {
        const auto& element = elements[i];
        auto event = kDefaultGameControlEvents[i];

        switch (element.type) {
        case JoypadElement::kButton:
            assert(std::find_if(m_buttons.begin(), m_buttons.end(), [&element](const auto& button) { return button.index == element.index; }) == m_buttons.end());
            m_buttons.emplace_back(element.index, event, false);
            break;
        case JoypadElement::kHat:
            {
                auto& hat = findOrCreateHat(element.index);
                hat.bindings.emplace_back(element.hatMask, event);
            }
            break;
        case JoypadElement::kAxis:
            {
                auto& axis = findOrCreateAxis(element.index);

                int from = INT16_MAX - kDefaultDeadZone;
                int to = INT16_MAX;
                if (element.axisValue < 0) {
                    from = INT16_MIN;
                    to = INT16_MIN + kDefaultDeadZone;
                }

                axis.intervals.emplace_back(from, to, event);
            }
            break;
        case JoypadElement::kBall:
            {
                auto& ball = findOrCreateBall(element.index);
                if (element.ball.dx) {
                    auto& dest = element.ball.dx < 0 ? ball.binding.xNegEvents : ball.binding.xPosEvents;
                    dest = event;
                } else {
                    auto& dest = element.ball.dy < 0 ? ball.binding.yNegEvents : ball.binding.yPosEvents;
                    dest = event;
                }
            }
            break;
        }
    }
}

void JoypadConfig::clear()
{
    m_buttons.clear();
    m_hats.clear();
    m_axes.clear();
    m_balls.clear();
}

GameControlEvents JoypadConfig::events(const Joypad& joypad) const
{
    GameControlEvents events = kNoGameEvents;

    if (!joypad.isOpen())
        return events;

    return getButtonState(joypad) | getHatState(joypad) | getAxisState(joypad) | getBallState(joypad);
}

GameControlEvents JoypadConfig::allEventsMask() const
{
    auto allEvents = kNoGameEvents;

    for (const auto& button : m_buttons)
        allEvents |= button.events;

    for (const auto& hat : m_hats)
        for (const auto& binding : hat.bindings)
            allEvents |= binding.events;

    for (const auto& axis : m_axes)
        for (const auto& interval : axis.intervals)
            allEvents |= interval.events;

    for (const auto& ball : m_balls) {
        allEvents |= ball.binding.xPosEvents;
        allEvents |= ball.binding.xNegEvents;
        allEvents |= ball.binding.yPosEvents;
        allEvents |= ball.binding.yNegEvents;
    }

    return allEvents;
}

void JoypadConfig::canonize()
{
    removeEmptyEvents();

    sortElements(m_buttons);
    sortElements(m_hats);
    sortElements(m_axes);
    sortElements(m_balls);
}

bool JoypadConfig::primary() const
{
    return m_forPlayer == 1;
}

bool JoypadConfig::empty() const
{
    return m_buttons.empty() && m_hats.empty() && m_axes.empty() && m_balls.empty();
}

void JoypadConfig::loadFromIni(const CSimpleIni& ini, const char *sectionName)
{
    loadButtons(ini, sectionName);
    loadAxes(ini, sectionName);
    loadHats(ini, sectionName);
    loadBalls(ini, sectionName);
}

void JoypadConfig::saveToIni(CSimpleIni& ini, const char *sectionName)
{
    saveButtons(ini, sectionName);
    saveAxes(ini, sectionName);
    saveHats(ini, sectionName);
    saveBalls(ini, sectionName);
}

void JoypadConfig::setButtonEvents(int index, GameControlEvents events, bool inverted)
{
    auto it = std::find_if(m_buttons.begin(), m_buttons.end(), [index](const auto& button) { return button.index == index; });
    if (it != m_buttons.end()) {
        it->events = events;
        it->inverted = inverted;
    } else {
        m_buttons.emplace_back(index, events, inverted);
    }
}

std::pair<GameControlEvents, bool> JoypadConfig::getButtonEvents(int index) const
{
    auto it = std::find_if(m_buttons.begin(), m_buttons.end(), [index](const auto& button) { return button.index == index; });
    if (it != m_buttons.end())
        return { it->events, it->inverted };
    else
        return { kNoGameEvents, false };
}

bool JoypadConfig::operator==(const JoypadConfig& other) const
{
    return m_buttons == other.m_buttons && m_hats == other.m_hats && m_axes == other.m_axes && m_balls == other.m_balls;
}

bool JoypadConfig::operator!=(const JoypadConfig& other) const
{
    return !operator==(other);
}

auto JoypadConfig::getHatBindings(int hatIndex) -> HatBindingList *
{
    auto& hat = findOrCreateHat(hatIndex);
    return &hat.bindings;
}

auto JoypadConfig::getAxisIntervals(int axisIndex) -> AxisIntervalList *
{
    auto& axis = findOrCreateAxis(axisIndex);
    return &axis.intervals;
}

auto JoypadConfig::getBall(int index) -> BallBinding&
{
    return m_balls[index].binding;
}

void JoypadConfig::removeEmptyEvents()
{
    for (auto& hat : m_hats) {
        hat.bindings.erase(std::remove_if(hat.bindings.begin(), hat.bindings.end(), [](const auto& binding) {
            return binding.events == kNoGameEvents;
        }), hat.bindings.end());
    }

    m_hats.erase(std::remove_if(m_hats.begin(), m_hats.end(), [](const auto& hat) {
        return hat.bindings.empty();
    }), m_hats.end());

    for (auto& axis : m_axes) {
        axis.intervals.erase(std::remove_if(axis.intervals.begin(), axis.intervals.end(), [](const auto& interval) {
            return interval.events == kNoGameEvents;
        }), axis.intervals.end());
    }

    m_axes.erase(std::remove_if(m_axes.begin(), m_axes.end(), [](const auto& axis) {
        return axis.intervals.empty();
    }), m_axes.end());
}

template<typename C>
void JoypadConfig::sortElements(C& c)
{
    std::sort(c.begin(), c.end(), [](const auto& elem1, const auto& elem2) {
        assert(elem1.index != elem2.index);
        return elem1.index < elem2.index;
    });
}

void JoypadConfig::mapAxis(GameControlEvents minEvent, GameControlEvents maxEvent, int index, SdlMappingParser::Range range, bool inverted)
{
    int minFrom, minTo, maxFrom, maxTo;

    switch (range) {
    case SdlMappingParser::Range::kFull:
        minFrom = SDL_JOYSTICK_AXIS_MIN;
        minTo = -kDefaultDeadZone;
        maxFrom = kDefaultDeadZone;
        maxTo = SDL_JOYSTICK_AXIS_MAX;
        if (inverted)
            std::swap(minEvent, maxEvent);
        break;
    case SdlMappingParser::Range::kPositiveOnly:
        minFrom = kDefaultDeadZone / 2;
        minTo = (SDL_JOYSTICK_AXIS_MAX - kDefaultDeadZone) / 2;
        maxFrom = minTo + 1;
        maxTo = SDL_JOYSTICK_AXIS_MAX;
        if (inverted) {
            for (auto var : { &minFrom, &minTo, &maxFrom, &maxTo })
                *var = SDL_JOYSTICK_AXIS_MAX - *var;
            std::swap(minFrom, maxFrom);
            std::swap(minTo, maxTo);
        }
        break;
    case SdlMappingParser::Range::kNegativeOnly:
        minFrom = SDL_JOYSTICK_AXIS_MIN;
        minTo = (SDL_JOYSTICK_AXIS_MIN - kDefaultDeadZone) / 2;
        maxFrom = minTo + 1;
        maxTo = -kDefaultDeadZone / 2;
        if (inverted) {
            for (auto var : { &minFrom, &minTo, &maxFrom, &maxTo })
                *var = SDL_JOYSTICK_AXIS_MIN - *var;
            std::swap(minFrom, maxFrom);
            std::swap(minTo, maxTo);
        }
        break;
    }

    auto& axis = findOrCreateAxis(index);
    axis.intervals.emplace_back(minFrom, minTo, minEvent);
    axis.intervals.emplace_back(maxFrom, maxTo, maxEvent);
}

template<typename T>
T& JoypadConfig::findOrCreate(std::vector<T>& set, int index)
{
    auto it = std::find_if(set.begin(), set.end(), [index](const auto& elem) {
        return elem.index == index;
    });

    if (it == set.end()) {
        set.emplace_back(index);
        it = set.end() - 1;
    }

    return *it;
}

GameControlEvents JoypadConfig::getButtonState(const Joypad& joypad) const
{
    assert(joypad.isOpen());

    GameControlEvents events = kNoGameEvents;

    for (const auto& button : m_buttons) {
        auto value = joypad.getButton(button.index);
        if (value && !button.inverted || !value && button.inverted)
            events |= button.events;
    }

    return events;
}
GameControlEvents JoypadConfig::getHatState(const Joypad& joypad) const
{
    assert(joypad.isOpen());

    GameControlEvents events = kNoGameEvents;

    for (const auto& hat : m_hats) {
        auto hatPosition = joypad.getHat(hat.index);
        for (const auto& binding : hat.bindings) {
            if (binding.mask) {
                int target = binding.inverted ? 0 : binding.mask;
                if ((hatPosition & binding.mask) == target)
                    events |= binding.events;
            }
        }
    }

    return events;
}

GameControlEvents JoypadConfig::getAxisState(const Joypad& joypad) const
{
    assert(joypad.isOpen());

    GameControlEvents events = kNoGameEvents;

    for (const auto& axis : m_axes) {
        auto value = joypad.getAxis(axis.index);
        for (const auto& interval : axis.intervals)
            if (value >= interval.from && value <= interval.to)
                events |= interval.events;
    }

    return events;
}

GameControlEvents JoypadConfig::getBallState(const Joypad& joypad) const
{
    static GameControlEvents s_ballEvents;

    for (const auto& ball : m_balls) {
        int dx, dy;
        std::tie(dx, dy) = joypad.getBall(ball.index);

        if (dx < 0) {
            s_ballEvents &= ~ball.binding.xPosEvents;
            s_ballEvents |= ball.binding.xNegEvents;
        } else if (dx > 0) {
            s_ballEvents &= ~ball.binding.xNegEvents;
            s_ballEvents |= ball.binding.xPosEvents;
        }

        if (dy < 0) {
            s_ballEvents &= ~ball.binding.yPosEvents;
            s_ballEvents |= ball.binding.yNegEvents;
        } else if (dy > 0) {
            s_ballEvents &= ~ball.binding.yNegEvents;
            s_ballEvents |= ball.binding.yPosEvents;
        }
    }

    return s_ballEvents;
}

//
// ini file load/save
//

template<typename F>
bool JoypadConfig::fetchNumber(const char *& p, int& num, int base, F f)
{
    assert(base == 10 || base == 16);

    bool neg = false;
    if (*p == '-') {
        p++;
        neg = true;
    }

    if (!f(*p))
        return false;

    num = 0;
    do {
        num = num * base + *p - '0';
    } while (f(*++p));

    if (neg)
        num = -num;

    return true;
}

bool JoypadConfig::fetchDecimal(const char *& p, int& num)
{
    return fetchNumber(p, num, 10, isdigit);
}

bool JoypadConfig::fetchHex(const char *& p, int& num)
{
    return fetchNumber(p, num, 16, isxdigit);
}

bool JoypadConfig::fetchEvents(const char *& p, GameControlEvents& events)
{
    int intEvents;
    if (!fetchHex(p, intEvents))
        return false;

    auto result = convertEvents(events, intEvents);
    if (result && events == kNoGameEvents) {
        logWarn("Empty event encountered");
        return false;
    }

    return result;
}

bool JoypadConfig::fetchHatMask(const char *& p, int& mask)
{
    if (!fetchHex(p, mask))
        return false;

    constexpr int kMaxHatMask = SDL_HAT_UP | SDL_HAT_DOWN | SDL_HAT_LEFT | SDL_HAT_RIGHT;
    return mask <= kMaxHatMask;
}

bool JoypadConfig::expectChar(const char *& p, char c)
{
    return *p == c ? p++, true : false;
}


template<typename C>
void JoypadConfig::removeDuplicateIndices(C & c, const char * what)
{
    std::sort(c.begin(), c.end(), [](const auto& e1, const auto& e2) { return e1.index < e2.index; });

    auto last = std::unique(c.begin(), c.end());
    if (last != c.end()) {
        logWarn("Game controller %s with duplicate index removed", what);
        c.erase(last, c.end());
    }
}

bool JoypadConfig::removeOverlappingIntervals(AxisIntervalList& list)
{
    if (list.size() < 2)
        return false;

    std::sort(list.begin(), list.end());

    bool changed = false;
    int currentMax = list.front().to;

    for (size_t i = 1; i < list.size(); i++) {
        if (list[i].to <= currentMax) {
            list.erase(list.begin() + i--);
            changed = true;
        } else if (list[i].from <= currentMax) {
            list[i].from = currentMax;
            changed = true;
        }

        currentMax = list[i].to;
    }

    return changed;
}

// Parsing would go so much nicer with exceptions, but didn't want to enable them just for this.
void JoypadConfig::loadButtons(const CSimpleIni& ini, const char *sectionName)
{
    auto buttons = ini.GetValue(sectionName, kButtonsKey);
    if (!buttons)
        return;

    while (true) {
        int index;
        if (!fetchDecimal(buttons, index))
            break;

        if (!expectChar(buttons, ':'))
            break;

        GameControlEvents events;
        if (!fetchEvents(buttons, events))
            break;

        bool inverted = expectChar(buttons, 'i');

        m_buttons.emplace_back(index, events, inverted);

        if (!expectChar(buttons, ','))
            break;
    }

    if (*buttons)
        logWarn("Error while processing buttons, section: [%s], at: %s", sectionName, buttons);

    removeDuplicateIndices(m_buttons, "buttons");
}

void JoypadConfig::loadAxes(const CSimpleIni& ini, const char *sectionName)
{
    auto axes = ini.GetValue(sectionName, kAxesKey);
    if (!axes)
        return;

    while (true) {
        int index;
        if (!fetchDecimal(axes, index))
            break;

        if (!expectChar(axes, '['))
            break;

        m_axes.emplace_back(index);
        auto& intervals = m_axes.back().intervals;

        while (true) {
            int from;
            if (!fetchDecimal(axes, from))
                goto out;

            if (!expectChar(axes, ':'))
                goto out;

            int to;
            if (!fetchDecimal(axes, to))
                goto out;

            if (!expectChar(axes, ':'))
                goto out;

            GameControlEvents events;
            if (!fetchEvents(axes, events))
                goto out;

            intervals.emplace_back(from, to, events);

            if (!expectChar(axes, ','))
                break;
        }

        if (removeOverlappingIntervals(intervals))
            logWarn("Overlapping intervals eliminated from axis %d, section: [%s]", index, sectionName);

        if (!expectChar(axes, ']'))
            break;
    }

out:
    if (*axes)
        logWarn("Error while processing axes, section: [%s], at: %s", sectionName, axes);

    removeDuplicateIndices(m_axes, "axes");
}

void JoypadConfig::loadHats(const CSimpleIni& ini, const char *sectionName)
{
    auto hats = ini.GetValue(sectionName, kHatsKey);
    if (!hats)
        return;

    constexpr int kMaxHatValue = SDL_HAT_RIGHTUP | SDL_HAT_LEFTDOWN;

    while (true) {
        int index;
        if (!fetchDecimal(hats, index))
            break;

        if (!expectChar(hats, '['))
            break;

        m_hats.emplace_back(index);
        auto& bindings = m_hats.back().bindings;

        bool hatsMasksSeen[kMaxHatValue + 1] = { false };

        while (true) {
            int mask;
            if (!fetchHatMask(hats, mask))
                goto out;

            if (mask < 0 || mask > kMaxHatValue) {
                logWarn("Invalid hat mask value encountered: %#x", mask);
                goto out;
            }

            if (hatsMasksSeen[mask]) {
                logWarn("Duplicate hat mask found: %#x", mask);
                goto out;
            }

            if (!expectChar(hats, ':'))
                goto out;

            GameControlEvents events;
            if (!fetchEvents(hats, events))
                goto out;

            bool inverted = expectChar(hats, 'i');

            bindings.emplace_back(mask, events, inverted);

            if (!expectChar(hats, ','))
                break;
        }

        if (!expectChar(hats, ']'))
            break;
    }

out:
    if (*hats)
        logWarn("Error while processing hats, section: [%s], at: %s", sectionName, hats);

    removeDuplicateIndices(m_hats, "hats");
}

void JoypadConfig::loadBalls(const CSimpleIni& ini, const char *sectionName)
{
    auto balls = ini.GetValue(sectionName, kBallsKey);
    if (!balls)
        return;

    while (true) {
        int index;
        if (!fetchDecimal(balls, index))
            break;

        if (!expectChar(balls, '['))
            break;

        m_balls.emplace_back(index);
        auto& binding = m_balls.back().binding;

        const std::pair<const char *, GameControlEvents *> kBindingAxes[] = {
            { "X+", &binding.xPosEvents }, { "X-", &binding.xNegEvents },
            { "Y+", &binding.yPosEvents }, { "Y-", &binding.yNegEvents },
        };

        for (const auto& bindingAxis : kBindingAxes) {
            for (auto p = bindingAxis.first; *p; p++)
                if (!expectChar(balls, *p))
                    goto out;

            if (!fetchEvents(balls, *bindingAxis.second))
                goto out;
        }

        if (!expectChar(balls, ']'))
            break;
    }

out:
    if (*balls)
        logWarn("Error while processing trackballs, section: [%s], at: %s", sectionName, balls);

    removeDuplicateIndices(m_balls, "trackballs");
}

void JoypadConfig::saveButtons(CSimpleIni& ini, const char *sectionName) const
{
    if (m_buttons.empty())
        return;

    std::string buttons;
    buttons.reserve(m_buttons.size() * 6);

    for (const auto& button : m_buttons) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d:%x%s,", button.index, button.events, button.inverted ? "i" : "");
        buttons += buf;
    }

    if (!buttons.empty())
        buttons.erase(buttons.end() - 1);

    ini.SetValue(sectionName, kButtonsKey, buttons.c_str());
}

void JoypadConfig::saveAxes(CSimpleIni& ini, const char *sectionName) const
{
    if (m_axes.empty())
        return;

    std::string axes;
    axes.reserve(m_axes.size() * 30 + 2);

    for (const auto& axis : m_axes) {
        assert(!axis.intervals.empty());

        axes += std::to_string(axis.index);
        axes += '[';

        assert(!axis.intervals.empty());
        for (const auto& interval : axis.intervals) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%d:%d:%x,", interval.from, interval.to, interval.events);
            axes += buf;
        }

        if (!axis.intervals.empty())
            axes.erase(axes.end() - 1);

        axes += ']';
    }

    ini.SetValue(sectionName, kAxesKey, axes.c_str());
}

void JoypadConfig::saveHats(CSimpleIni& ini, const char *sectionName) const
{
    if (m_hats.empty())
        return;

    std::string hats;
    hats.reserve(m_hats.size() * 24);

    for (const auto& hat : m_hats) {
        hats += std::to_string(hat.index);
        hats += '[';

        assert(!hat.bindings.empty());
        for (const auto& binding : hat.bindings) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%x:%x%s,", binding.mask, binding.events, binding.inverted ? "i" : "");
            hats += buf;
        }

        if (!hat.bindings.empty())
            hats.erase(hats.end() - 1);

        hats += ']';
    }

    ini.SetValue(sectionName, kHatsKey, hats.c_str());
}

void JoypadConfig::saveBalls(CSimpleIni& ini, const char *sectionName) const
{
    if (m_balls.empty())
        return;

    std::string balls;
    balls.reserve(m_balls.size() * 28);

    for (const auto& ball : m_balls) {
        const auto& binding = ball.binding;
        char buf[64];
        snprintf(buf, sizeof(buf), "%d[X+:%x,X-:%x,Y+:%x,Y-:%x]", ball.index, binding.xPosEvents,
            binding.xNegEvents, binding.yPosEvents, binding.yNegEvents);
        balls += buf;
    }

    ini.SetValue(sectionName, kBallsKey, balls.c_str());
}
