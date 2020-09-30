#pragma once

#include "Util.h"

struct Token;
typedef const Token CToken;
typedef std::vector<CToken *> TokenList;
typedef std::pair<CToken *, CToken *> TokenRange;

#pragma pack(push, 1)
struct Token
{
    enum Type : uint16_t {
        T_NONE = 0,
#include "TokenTypeEnum.h"
    };

    enum Category : uint8_t {
        kWhitespace,
        kInstruction,
        kRegister,
        kDup,
        kIgnore,
        kKeyword,
        kOperator,
        kNumber,
        kId,
        kEof,
    };
    // keep as a full 32-bit int value so we can use it for initialization in tests
    enum InstructionType : int32_t {
        kGeneralInstruction,
        kBranchInstruction,
        kPrefixInstruction,
        kShiftRotateInstruction,
    };
    enum RegisterType : uint8_t {
        kGeneralRegister,
        kSegmentRegister,
    };
    enum KeywordType {
        kGeneralKeyword,
        kStructUnion,
        kSizeSpecifier,
        kDataSizeSpecifier,
    };
    enum NoBreakStatus {
        kNoBreakNotPresent,
        kStartNoBreak,
        kEndNoBreak,
    };

    inline const char *text() const {
        return (char *)(this + 1);
    }
    inline void copyText(char *buf) const {
        memcpy(buf, text(), textLength);
    }
    inline char firstChar() const {
        assert(textLength > 0);
        return text()[0];
    }
    inline bool startsWith(char c) const {
        return firstChar() == c;
    }
    template <size_t N> inline bool startsWith(const char (&str)[N]) const {
        return textLength >= N - 1 && !memcmp(text(), str, N - 1);
    }
    inline char lastChar() const {
        assert(textLength > 0);
        return text()[textLength - 1];
    }
    inline bool endsWith(char c) const {
        return lastChar() == c;
    }
    inline int indexOf(char c) const {
        assert(textLength > 0);
        const char *p = (char *)memchr(text(), c, textLength);
        return p ? p - text() : -1;
    }
    inline bool contains(char c) const {
        return indexOf(c) >= 0;
    }
    inline static bool isText(Token::Type type) {
        return type <= T_ID;
    }
    inline bool isText() const {
        return Token::isText(type);
    }
    inline bool isId() const {
        return type == T_ID;
    }
    inline bool isLocalLabel() const {
        return isText() && textLength > 2 && text()[0] == '@' && text()[1] == '@';
    }
    inline bool isNumber() const {
        return isNumber(type);
    }
    inline static bool isNumber(const Token::Type type) {
        return type == T_NUM || type == T_HEX || type == T_BIN;
    }
    inline bool isComment() const {
        return type == T_COMMENT;
    }
    inline bool isDataSizeSpecifier() const {
        return category == kKeyword && keywordType == kDataSizeSpecifier;
    }
    inline bool isNewLine() const {
        return type == T_NL;
    }
    inline bool isEof() const {
        return type == T_EOF;
    }
    inline std::string string() const {
        return std::string(text(), textLength);
    }
    inline Token *next() const {
        return (Token *)((char *)(this + 1) + textLength);
    }
    inline bool operator==(const Token& rhs) const {
        return textLength == rhs.textLength && !memcmp(this, &rhs, sizeof(*this) + textLength);
    }
    inline bool operator==(char c) const {
        return textLength == 1 && text()[0] == c;
    }
    template <size_t N>
    inline bool operator==(const char (&str)[N]) const {
        return (category == kWhitespace || ((category == kKeyword || category == kId) && hash == Util::constHash(str))) &&
            textLength == N - 1 && !memcmp(text(), str, N - 1);
    }
    template <size_t N>
    inline bool operator!=(const char(&str)[N]) const {
        return !(*this == str);
    }

    int parseInt() const;
    int dataSize() const;

    static size_t flattenTokenList(const TokenList& tokenList, char *dest);
    static size_t tokenListLength(const TokenList& tokenList);

    // generated
    static const char *typeToString(Token::Type type);

    const inline char *typeToString() const {
        return typeToString(type);
    }

    Type type;
    Category category;
    union {
        InstructionType instructionType;
        struct {
            RegisterType registerType;
            uint8_t registerSize;
        };
        KeywordType keywordType;
        NoBreakStatus noBreakStatus;
        Util::hash_t hash;
    };
    uint32_t textLength;  // length of text which is trailing the struct
};
#pragma pack(pop)

ENABLE_FLAGS(Token::Type)

static inline void advance(CToken *& token)
{
    token = token->next();
}

static inline void advance(Token*& token)
{
    token = token->next();
}

class Tokenizer
{
public:
    static constexpr char kBreakMarker[] = { ';', ' ', '$', 'n', 'o', '-', 'b', 'r', 'e', 'a', 'k' };

    TokenRange tokenize(const char *data, long size, long hardSize = -1);
    static void tokenize(const char *data, size_t size, char *tokensArea);
    std::tuple<std::string, bool, bool> determineBlockLimits(const TokenRange& limits);
    CToken *begin() const;
    CToken *end() const;

private:
    enum BlockProperties { kInProc, kInNoBreakBlock, kSeenLimitCheckpoint, kTrailingDebris, kNumProperties, kNone };
    using BlockState = std::array<int, kNumProperties>;

    std::tuple<BlockState, bool, CToken *> determineBlockStart(const TokenRange& limits);
    std::pair<BlockState, bool> determineBlockEnd(const TokenRange& limits, BlockState state, CToken *comment);

    static std::pair<bool, CToken *> isDataItemLine(CToken *token);
    static CToken *skipUntilNewLine(CToken *token);
    static Token *makeToken(Token *token, Token::Type type);

    std::unique_ptr<char[]> m_tokenData;
    Token *m_beginToken = nullptr;
    size_t m_tokenDataSize = 0;

    CToken *m_start = nullptr;
    CToken *m_end = nullptr;
};
