#pragma once

bool setStrictLogMode(bool active);

class LogSilencer {
    bool m_oldState;
public:
    LogSilencer() { m_oldState = setStrictLogMode(false); }
    ~LogSilencer() { setStrictLogMode(m_oldState); }
};
