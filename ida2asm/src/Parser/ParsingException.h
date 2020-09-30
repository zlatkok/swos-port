#pragma once

class ParsingException
{
public:
    ParsingException(const std::string& error, CToken *token) : m_error(error), m_token(token) {}
    std::string error() const { return m_error; }
    CToken *token() const { return m_token; }

private:
    CToken *m_token;
    const std::string m_error;
};
