#include "Tokenizer.h"
// contains generated token lookup function:
// static inline const char *lookupToken(Token& token, const char *p)
#include "TokenLookup.h"

int Token::parseInt() const
{
    assert(textLength > 0);
    assert(type == Token::T_HEX || type == Token::T_NUM || type == Token::T_BIN);
    assert(category == Number || category == Dup);

    int value = 0;

    auto p = text();
    auto length = textLength;
    bool neg = false;

    if (*p == '+' || *p == '-') {
        p++;
        length--;
        neg = *p == '-';
    }

    if (type == Token::T_HEX) {
        assert(tolower(text()[textLength - 1]) == 'h');

        for (; p < text() + length - 1; p++) {
            int c;

            if (*p >= '0' && *p <= '9')
                c = *p - '0';
            else
                c = 10 + (*p | 0x20) - 'a';

            value = value * 16 + c;
        }
    } else if (type == Token::T_NUM) {
        for (; p < text() + length; p++)
            value = value * 10 + *p - '0';
    } else {
        assert(tolower(text()[textLength - 1]) == 'b');
        for (; p < text() + length - 1; p++)
            value = value * 2 + *p - '0';
    }

    return neg ? -value : value;
}

int Token::dataSize() const
{
    switch (type) {
    case Token::T_BYTE:
    case Token::T_DB:
        return 1;
    case Token::T_WORD:
    case Token::T_DW:
        return 2;
    case Token::T_DWORD:
    case Token::T_DD:
        return 4;
    case Token::T_FWORD:
        return 6;
    case Token::T_QWORD:
    case Token::T_DQ:
        return 8;
    case Token::T_TBYTE:
    case Token::T_DT:
    case Token::T_PARA:
        return 16;
    case Token::T_PAGE:
        return 256;
    default:
        return -1;
    }
}

auto Tokenizer::tokenize(const char *data, long size, long hardSize) -> TokenRange
{
    assert(hardSize < 0 || hardSize >= size);

    // size needed for tokens should never be more than sizeof(Token) * (input size / 2)
    // if it's all one token => sizeof(Token) + input size
    // if it's max number of tokens (one letter, one space sequence) => (n / 2) * sizeof(Token) + n / 2
    // ie. (n / 2) * (sizeof(Token) + 1)
    // assuming input size >> sizeof(Token)

    assert(!m_tokenData && data && size);

    // +1 token for EOF
    m_tokenDataSize = (size / 2) * (sizeof(Token) + 1) + sizeof(Token);
    m_tokenData.reset(new char[m_tokenDataSize]);

    auto tokenSentinel = m_tokenData.get() + m_tokenDataSize;
    auto token = m_beginToken = reinterpret_cast<Token *>(m_tokenData.get());
    auto p = data;
    auto softSentinel = p + size;

    while (p < softSentinel) {
        p = lookupToken(*token, p);
        advance(token);
    }

    if (hardSize < 0)
        token = makeEofToken(token);

    assert(p == softSentinel && p[-1] == '\n');
    auto softLimit = token;

    m_start = m_beginToken;
    m_end = softLimit;

    if (hardSize < 0)
        return { softLimit, softLimit };

    auto hardSentinel = data + hardSize;

    while (p < hardSentinel) {
        p = lookupToken(*token, p);
        advance(token);
    }

    token = makeEofToken(token);

    auto hardLimit = token;
    assert(reinterpret_cast<char *>(hardLimit) < tokenSentinel && p == hardSentinel && p[-1] == '\n');

    return { softLimit, hardLimit };
}

std::tuple<std::string, bool, bool> Tokenizer::determineBlockLimits(const TokenRange& limits)
{
    enum State { kInProc, kInNoBreakBlock, kSeenLimitCheckpoint, kNumStates, kNone };
    std::array<int, kNumStates> states{};

    bool noBreakContinued = false;

    CToken *comment{};
    m_start = begin();

    for (auto token = begin(); token < limits.first; advance(token)) {
        // we're guaranteed that block limits will end with new lines
        if (token->isId()) {
            auto next = token->next();

            if (next->type == Token::T_SEGMENT) {
                states[kSeenLimitCheckpoint] = 1;
            } else if (next->type == Token::T_PROC) {
                states[kInProc]++;
                states[kSeenLimitCheckpoint] = 1;
            } else if (next->type == Token::T_ENDP) {
                if (!states[kInProc])
                    m_start = comment ? comment : skipUntilNewLine(next->next())->next();
                else
                    states[kInProc]--;
                states[kSeenLimitCheckpoint] = 1;
            } else {
                bool isDataItem;
                std::tie(isDataItem, token) = isDataItemLine(token);

                if (isDataItem)
                    states[kSeenLimitCheckpoint] = 1;
            }
            token = skipUntilNewLine(token);
        } else if (token->category == Token::Whitespace) {
            bool starting;
            if (isNoBreakMarker(token, starting)) {
                if (starting) {
                    states[kInNoBreakBlock]++;
                } else {
                    if (!states[kInNoBreakBlock]) {
                        m_start = skipUntilNewLine(token->next())->next();
                        noBreakContinued = true;
                    } else {
                        states[kInNoBreakBlock]--;
                    }
                }
            } else {
                if (!comment)
                    comment = token;
            }
        } else if (token->isEof()) {
            break;
        } else {
            comment = nullptr;

            if (!states[kSeenLimitCheckpoint]) {
                bool isInstruction = token->category == Token::Instruction;
                bool isDataItem = false;

                if (!isInstruction)
                    std::tie(isDataItem, token) = isDataItemLine(token);

                token = skipUntilNewLine(token);

                if (isDataItem || isInstruction)
                    m_start = token->next();
            } else {
                token = skipUntilNewLine(token->next());
            }
        }
    }

    m_end = limits.first;

    states[kSeenLimitCheckpoint] = 0;
    bool noBreakOverflow = states[kInNoBreakBlock] != 0;

    for (auto token = limits.first; token < limits.second; advance(token)) {
        if (!states[kInProc] && !states[kInNoBreakBlock] && states[kSeenLimitCheckpoint])
            break;

        if (token->isId()) {
            auto next = token->next();

            if (next->type == Token::T_PROC) {
                states[kInProc]++;
                states[kSeenLimitCheckpoint] = 1;
            } else if (next->type == Token::T_ENDP) {
                if (!--states[kInProc])
                    m_end = skipUntilNewLine(next->next())->next();
                states[kSeenLimitCheckpoint] = 1;
            } else {
                bool isDataItem;
                std::tie(isDataItem, token) = isDataItemLine(token);

                if (isDataItem)
                    states[kSeenLimitCheckpoint] = 1;
            }
            token = skipUntilNewLine(token);
        } else if (token->category == Token::Whitespace) {
            if (!comment)
                comment = token;

            bool starting;
            if (isNoBreakMarker(token, starting)) {
                if (starting)
                    states[kInNoBreakBlock]++;
                else if (!--states[kInNoBreakBlock])
                    m_end = skipUntilNewLine(token->next())->next();
            }
        } else if (token->isEof()) {
            break;
        } else {
            comment = nullptr;

            if (!states[kSeenLimitCheckpoint]) {
                bool isInstruction = token->category == Token::Instruction;
                bool isDataItem = false;

                if (!isInstruction)
                    std::tie(isDataItem, token) = isDataItemLine(token);

                token = skipUntilNewLine(token);

                if (isDataItem || isInstruction)
                    m_end = token->next();
            } else {
                token = skipUntilNewLine(token->next());
            }
        }
    }

    assert(m_start && m_start < limits.first);
    assert(m_end && m_end <= limits.second);
    assert(m_start->type == Token::T_PROC || m_start->isComment() || m_start->isNewLine() || m_start->isId());
    assert(m_end == end() || m_end->type == Token::T_ENDP || m_end->isComment() || m_end->isNewLine() || m_end->isId());

    std::string error;

    if (states[kInProc])
        error = "unterminated procedure encountered";
    else if (states[kInNoBreakBlock])
        error = "unterminated no-break tag encountered";

    return { error, noBreakContinued, noBreakOverflow };
}

CToken *Tokenizer::begin() const
{
    assert(m_start);
    return m_start;
}

CToken *Tokenizer::end() const
{
    assert(m_end);
    return m_end;
}

std::pair<bool, CToken *> Tokenizer::isDataItemLine(CToken *token)
{
    bool result = false;

    while (!token->isNewLine()) {
        if (token->isDataSizeSpecifier() || token->category == Token::Dup || *token == "<>")
            result = true;

        advance(token);
    }

    return { result, token };
}

bool Tokenizer::isNoBreakMarker(CToken *token, bool& starting)
{
    if (!token->isComment())
        return false;

    const char kBreakMarker[] = { ';', ' ', '$', 'n', 'o', '-', 'b', 'r', 'e', 'a', 'k' };

    if (token->textLength >= sizeof(kBreakMarker) + 1) {
        size_t i = 0;
        while (i < token->textLength && token->text()[i] != ';')
            i++;

        if (token->textLength - i == sizeof(kBreakMarker) + 1 && !memcmp(token->text() + i, kBreakMarker, sizeof(kBreakMarker)) &&
            (token->endsWith('{') || token->endsWith('}'))) {
            starting = token->endsWith('{');
            return true;
        }
    }

    return false;
}

CToken *Tokenizer::skipUntilNewLine(CToken *token)
{
    while (!token->isNewLine())
        advance(token);

    return token;
}

Token *Tokenizer::makeEofToken(Token *token)
{
    memset(token, 0, sizeof(Token));
    token->type = Token::T_EOF;
    return token->next();
}

size_t Token::flattenTokenList(const TokenList& tokenList, char *dest)
{
    size_t length = 0;

    for (const auto& token : tokenList) {
        assert(token->type == Token::T_NL || token->isComment());
        assert(token->textLength);

        memcpy(dest, token->text(), token->textLength);
        dest += token->textLength;
        length += token->textLength;
    }

    return length;
}

size_t Token::tokenListLength(const TokenList& tokenList)
{
    size_t length = 0;

    for (const auto& token : tokenList) {
        assert(token->isNewLine() || token->isComment());
        length += token->textLength;
    }

    return length;
}
