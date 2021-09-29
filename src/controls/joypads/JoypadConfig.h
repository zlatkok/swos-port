#pragma once

#include "gameControlEvents.h"
#include "SdlMappingParser.h"
#include "JoypadElementValue.h"

class Joypad;

class JoypadConfig
{
public:
    JoypadConfig(int forPlayer = 1) : m_forPlayer(forPlayer) {}
    void initWithMapping(const char *mapping);
    void init();
    void assign(const DefaultJoypadElementList& elements);
    void clear();

    GameControlEvents events(const Joypad& joypad) const;
    GameControlEvents allEventsMask() const;

    void canonize();

    bool primary() const;
    bool secondary() { return !primary(); }
    bool empty() const;

    void loadFromIni(const CSimpleIni& ini, const char *sectionName);
    void saveToIni(CSimpleIni& ini, const char *sectionName);

    void setButtonEvents(int index, GameControlEvents events, bool inverted);
    std::pair<GameControlEvents, bool> getButtonEvents(int index) const;

    bool operator==(const JoypadConfig& other) const;
    bool operator!=(const JoypadConfig& other) const;

    struct HatBinding
    {
        HatBinding(int mask = 0, GameControlEvents events = kNoGameEvents, bool inverted = false)
            : events(events), mask(mask), inverted(inverted) {}
        bool operator==(const HatBinding& other) const {
            return events == other.events && mask == other.mask && inverted == other.inverted;
        }
        GameControlEvents events;
        int16_t mask;
        bool inverted;
    };
    using HatBindingList = std::vector<HatBinding>;

    HatBindingList *getHatBindings(int hatIndex);

    struct AxisInterval
    {
        AxisInterval(int16_t from = INT16_MAX - kDefaultDeadZone, int16_t to = INT16_MAX, GameControlEvents events = kNoGameEvents)
            : from(from), to(to), events(events) {}
        bool operator==(const AxisInterval& other) const {
            return from == other.from && to == other.to && events == other.events;
        }
        bool operator<(const AxisInterval& other) const {
            return from < other.from && to < other.to;
        }
        int16_t from;
        int16_t to;
        GameControlEvents events;
    };
    using AxisIntervalList = std::vector<AxisInterval>;

    AxisIntervalList *getAxisIntervals(int axisIndex);

    struct BallBinding
    {
        bool operator==(const BallBinding& other) const {
            return xPosEvents == other.xPosEvents && xNegEvents == other.xNegEvents &&
                yPosEvents == other.yPosEvents && yNegEvents == other.yNegEvents;
        }
        GameControlEvents xPosEvents;
        GameControlEvents xNegEvents;
        GameControlEvents yPosEvents;
        GameControlEvents yNegEvents;
    };

    BallBinding& getBall(int index);

private:
    static constexpr int kDefaultDeadZone = 8'000;

    struct Button
    {
        Button(int index, GameControlEvents events, bool inverted) : index(index), events(events), inverted(inverted) {}
        bool operator==(const Button& other) const {
            return index == other.index && events == other.events && inverted == other.inverted;
        }
        int index;
        GameControlEvents events;
        bool inverted;
    };
    struct Hat
    {
        Hat(int index) : index(index) {}
        bool operator==(const Hat& other) const {
            return index == other.index && bindings == other.bindings;
        }
        int index;
        HatBindingList bindings;
    };
    struct Axis
    {
        Axis(int index) : index(index) {}
        bool operator==(const Axis& other) const {
            return index == other.index && intervals == other.intervals;
        }
        int index;
        std::vector<AxisInterval> intervals;
    };
    struct Ball
    {
        Ball(int index) : index(index) {}
        bool operator==(const Ball& other) const {
            return index == other.index && binding == other.binding;
        }
        int index;
        BallBinding binding;
    };

    void removeEmptyEvents();
    template<typename C>
    void sortElements(C& c);
    void mapAxis(GameControlEvents minEvent, GameControlEvents maxEvent, int index, SdlMappingParser::Range range, bool inverted);

    template<typename T> T& findOrCreate(std::vector<T>& set, int index);
    Hat& findOrCreateHat(int index) { return findOrCreate(m_hats, index); }
    Axis& findOrCreateAxis(int index) { return findOrCreate(m_axes, index); }
    Ball& findOrCreateBall(int index) { return findOrCreate(m_balls, index); }

    GameControlEvents getButtonState(const Joypad& joypad) const;
    GameControlEvents getHatState(const Joypad& joypad) const;
    GameControlEvents getAxisState(const Joypad& joypad) const;
    GameControlEvents getBallState(const Joypad& joypad) const;

    template<typename F>
    static bool fetchNumber(const char *& p, int& num, int base, F f);
    static bool fetchDecimal(const char *& p, int& num);
    static bool fetchHex(const char *& p, int& num);
    static bool fetchEvents(const char *& p, GameControlEvents& events);
    static bool fetchHatMask(const char *& p, int& mask);
    static bool expectChar(const char *& p, char c);

    template<typename C>
    static void removeDuplicateIndices(C& c, const char *what);
    static bool removeOverlappingIntervals(AxisIntervalList& list);

    void loadButtons(const CSimpleIni& ini, const char *sectionName);
    void loadAxes(const CSimpleIni& ini, const char *sectionName);
    void loadHats(const CSimpleIni& ini, const char *sectionName);
    void loadBalls(const CSimpleIni& ini, const char *sectionName);

    void saveButtons(CSimpleIni& ini, const char *sectionName) const;
    void saveAxes(CSimpleIni& ini, const char *sectionName) const;
    void saveHats(CSimpleIni& ini, const char *sectionName) const;
    void saveBalls(CSimpleIni& ini, const char *sectionName) const;

    std::vector<Button> m_buttons;
    std::vector<Hat> m_hats;
    std::vector<Axis> m_axes;
    std::vector<Ball> m_balls;
    int m_forPlayer;
};
