#include "Tokenizer.h"
// contains generated token lookup function:
// static inline const char *lookupToken(Token& token, const char *p)
#include "TokenLookup.h"

int Token::parseInt() const
{
    assert(textLength > 0);
    assert(type == Token::T_HEX || type == Token::T_NUM || type == Token::T_BIN);
    assert(category == kNumber || category == kDup);
    assert(type != Token::T_HEX || tolower(text()[textLength - 1]) == 'h');
    assert(type != Token::T_BIN || tolower(text()[textLength - 1]) == 'b');

    return Util::parseInt(text(), textLength);
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

    // +1 token for EOF, +1 token for (potential) new line
    m_tokenDataSize = (size / 2) * (sizeof(Token) + 2) + sizeof(Token);
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
        token = makeToken(token, Token::T_EOF);

    assert(p == softSentinel && p[-1] == '\n');
    auto softLimit = token;

    m_start = m_beginToken;
    m_end = softLimit;

    if (hardSize < 0)
        return { softLimit, softLimit };

    auto hardSentinel = data + hardSize;

    auto lastToken = m_start;
    while (p < hardSentinel) {
        p = lookupToken(*token, p);
        lastToken = token;
        advance(token);
    }

    if (!lastToken->isNewLine())
        token = makeToken(token, Token::T_NL);

    token = makeToken(token, Token::T_EOF);

    auto hardLimit = token;
    assert(reinterpret_cast<char *>(hardLimit) < tokenSentinel && p == hardSentinel && p[-1] == '\n');

    return { softLimit, hardLimit };
}

void Tokenizer::tokenize(const char *data, size_t size, char *tokensArea)
{
    assert(data && size && tokensArea);

    const auto sentinel = data + size;
    auto token = reinterpret_cast<Token *>(tokensArea);

    for (auto p = data; p < sentinel; advance(token))
        p = lookupToken(*token, p);

    makeToken(token, Token::T_NL);
}

std::tuple<std::string, bool, bool> Tokenizer::determineBlockLimits(const TokenRange& limits)
{
    bool noBreakOverflow;

    auto [state, noBreakContinued, comment] = determineBlockStart(limits);
    std::tie(state, noBreakOverflow) = determineBlockEnd(limits, state, comment);

    assert(m_start && m_start < limits.first);
    assert(m_end && m_end <= limits.second);
    assert(m_start->type == Token::T_PROC || m_start->isComment() || m_start->isNewLine() || m_start->isId());
    assert(m_end == end() || m_end->type == Token::T_ENDP || m_end->isComment() || m_end->isNewLine() || m_end->isId());

    std::string error;

    if (state[kInProc])
        error = "unterminated procedure encountered";
    else if (state[kInNoBreakBlock])
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

auto Tokenizer::determineBlockStart(const TokenRange& limits) -> std::tuple<BlockState, bool, CToken *>
{
    BlockState state{};
    bool noBreakContinued = false;

    CToken *comment{};
    m_start = begin();

    for (auto token = begin(); token < limits.first; advance(token)) {
        // we're guaranteed that block limits will end with new lines
        if (token->isId()) {
            auto next = token->next();

            if (next->type == Token::T_SEGMENT) {
                state[kSeenLimitCheckpoint] = 1;
                state[kTrailingDebris] = 0;
            } else if (next->type == Token::T_PROC) {
                state[kInProc]++;
                state[kSeenLimitCheckpoint] = 1;
            } else if (next->type == Token::T_ENDP) {
                if (!state[kInProc])
                    m_start = comment ? comment : skipUntilNewLine(next->next())->next();
                else
                    state[kInProc]--;
                state[kSeenLimitCheckpoint] = 1;
                state[kTrailingDebris] = 0;
            } else {
                bool isDataItem;
                std::tie(isDataItem, token) = isDataItemLine(token);

                if (isDataItem) {
                    state[kSeenLimitCheckpoint] = 1;
                    state[kTrailingDebris] = 0;
                }
            }
            token = skipUntilNewLine(token);
        } else if (token->category == Token::kWhitespace) {
            if (token->noBreakStatus == Token::kStartNoBreak) {
                state[kInNoBreakBlock]++;
            } else if (token->noBreakStatus == Token::kEndNoBreak) {
                if (!state[kInNoBreakBlock]) {
                    m_start = skipUntilNewLine(token->next())->next();
                    noBreakContinued = true;
                } else {
                    state[kInNoBreakBlock]--;
                }
            } else {
                if (!comment)
                    comment = token;
            }
        } else if (token->isEof()) {
            break;
        } else {
            comment = nullptr;
            if (token->type != Token::T_ALIGN)
                state[kTrailingDebris] = 1;

            if (!state[kSeenLimitCheckpoint]) {
                bool isInstruction = token->category == Token::kInstruction;
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

    return { state, noBreakContinued, comment };
}

auto Tokenizer::determineBlockEnd(const TokenRange& limits, BlockState state, CToken *comment) -> std::pair<BlockState, bool>
{
    m_end = limits.first;

    state[kSeenLimitCheckpoint] = 0;
    bool noBreakOverflow = state[kInNoBreakBlock] != 0;

    if (state[kInProc] || state[kInNoBreakBlock] || state[kTrailingDebris]) {
        for (auto token = limits.first; token < limits.second; advance(token)) {
            if (!state[kInProc] && !state[kInNoBreakBlock] && state[kSeenLimitCheckpoint])
                break;

            if (token->isId()) {
                auto next = token->next();

                if (next->type == Token::T_PROC) {
                    state[kInProc]++;
                    state[kSeenLimitCheckpoint] = 1;
                } else if (next->type == Token::T_ENDP) {
                    if (!--state[kInProc])
                        m_end = skipUntilNewLine(next->next())->next();
                    state[kSeenLimitCheckpoint] = 1;
                } else {
                    bool isDataItem;
                    std::tie(isDataItem, token) = isDataItemLine(token);

                    if (isDataItem)
                        state[kSeenLimitCheckpoint] = 1;
                }
                token = skipUntilNewLine(token);
            } else if (token->category == Token::kWhitespace) {
                if (!comment)
                    comment = token;

                if (token->noBreakStatus == Token::kStartNoBreak)
                    state[kInNoBreakBlock]++;
                else if (token->noBreakStatus == Token::kEndNoBreak && !--state[kInNoBreakBlock])
                    m_end = skipUntilNewLine(token->next())->next();
            } else if (token->isEof()) {
                break;
            } else {
                comment = nullptr;

                if (!state[kSeenLimitCheckpoint]) {
                    bool isInstruction = token->category == Token::kInstruction;
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
    }

    return { state, noBreakOverflow };
}

std::pair<bool, CToken *> Tokenizer::isDataItemLine(CToken *token)
{
    bool result = false;

    while (!token->isNewLine()) {
        if (token->isDataSizeSpecifier() || token->category == Token::kDup || *token == "<>")
            result = true;

        advance(token);
    }

    return { result, token };
}

CToken *Tokenizer::skipUntilNewLine(CToken *token)
{
    while (!token->isNewLine())
        advance(token);

    return token;
}

Token *Tokenizer::makeToken(Token *token, Token::Type type)
{
    memset(token, 0, sizeof(Token));
    token->type = type;
    return token->next();
}

size_t Token::flattenTokenList(const TokenList& tokenList, char *dest)
{
    size_t length = 0;

    for (const auto& token : tokenList) {
        assert(token->type == Token::T_NL || token->isComment());
        assert(token->textLength);

        token->copyText(dest);
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
