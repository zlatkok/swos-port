#pragma once

class OutputException
{
public:
    OutputException(const std::string& what, const char *file = nullptr)
        : m_what(what), m_file(file) {}
    const std::string& what() const { return m_what; }
    const char *file() const { return m_file; }
private:
    const std::string m_what;
    const char *m_file;
};
