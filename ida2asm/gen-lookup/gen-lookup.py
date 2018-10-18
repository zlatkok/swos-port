import os
import sys
import string
import random
import inspect
import operator
import subprocess


kTabSize = 4
kTokenLookupFilename = 'TokenLookup.h'
kTokenTypeEnumFilename = 'TokenTypeEnum.h'
kLineBreakLimit = 120
kNumTestLoops = 1500


def makePath(*args, **kwargs):
    return os.path.abspath(os.path.join(*args, **kwargs))


def getMutualPrefixLength(str1, str2):
    prefix = ''
    maxLen = min(len(str1), len(str2))

    for i in range(maxLen):
        if str1[i] != str2[i]:
            return i

    return maxLen


# token types that are not coming from the input file
def specialTokens():
    # keep T_ID first
    return ('T_ID', 'T_STRING', 'T_DUP_QMARK', 'T_DUP_STRUCT_INIT', 'T_COMMENT', 'T_NL', 'T_COMMA',
        'T_LBRACKET', 'T_RBRACKET', 'T_PLUS', 'T_MINUS', 'T_MULT', 'T_HEX', 'T_NUM', 'T_BIN', 'T_EOF')


def outputEnum(tokens):
    for token in tokens:
        out(f'    T_{token["id"].upper()},')

    for token in specialTokens():
        out(f'    {token},')


def outputCharTypeFunctions():
    out(r'''
static inline bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static inline bool isNumberStart(char c)
{
    return c >= '0' && c <= '9' || c == '-';
}

static inline bool isDelimiter(char c)
{
    return Util::isSpace(c) || c == ',' || c == '[' || c == '+' || c == '-' || c == '*' || c == ']' || c == '\'';
}''')


# keep this synced with Util::hash()
def simpleHash(str):
    h = 1021

    for i in range(len(str)):
        c = ord(str[i])
        h += c + i
        b = (h & 0x80000000) >> 31
        h = ((h << 1) | b) & 0xffffffff
        h ^= c + i

    return h


def outputTypeToString(tokens):
    out(r'''
const char *Token::typeToString(Token::Type type)
{
    switch (type) {''')
    for token in tokens:
        out(rf'        case T_{token["id"].upper()}: return "\"{token["id"]}\"";')

    out(r'''        case T_NONE: return "null token";
        case T_ID: return "identifier";
        case T_STRING: return "string";
        case T_DUP_QMARK: return "uninitialized data for dup macro `dup(?)'";
        case T_DUP_STRUCT_INIT: return "structure initialization for dup macro `dup(<0>)'";
        case T_COMMENT: return "comment";
        case T_NL: return "new line";
        case T_COMMA: return "comma (,)";
        case T_LBRACKET: return "left bracket ([)";
        case T_RBRACKET: return "right bracket (])";
        case T_PLUS: return "plus operator (+)";
        case T_MINUS: return "minus operator (-)";
        case T_MULT: return "multiply operator (*)";
        case T_HEX: return "hexadecimal number";
        case T_NUM: return "integer";
        case T_BIN: return "binary number";
        case T_EOF: return "end of file";
        default: assert(false); return "unknown token";
    }
}''')


def outputFilterComment():
    out(r'''
static const char *filterComment(const char *src, Token& token)
{
    auto dest = const_cast<char *>(token.text());

    for (size_t i = 0; i < token.textLength; i++, src++) {
        // be careful with comparing high raw hex constants, if it goes out of signed char range it compares false
        if (*src == '\x18' || *src == '\x19')
            dest[i] = '|';
        else if (*src == '\xcd' || *src == '\xcb')
            dest[i] = '=';
        else if (*src == '\x10')
            dest[i] = '>';
        else if (*src < 0)
            dest[i] = '_';
    }

    // leave final \n
    return src;
}
''')


def outputParsingRoutines():
    out(r'''
// handle strings, assume there's always at least a new line at the end
static const char *parseString(const char *src, char *dst, Token& token)
{
    size_t len = 0;
    bool inQuote = false;

    while (*src != '\r') {
        if (*src < 32) {
            // escape all the crap IDA puts in strings
            if (inQuote) {
                *dst++ = '\'';
                len++;
                inQuote = false;
            }

            if (len > 0) {
                *dst++ = ',';
                *dst++ = ' ';
                len += 2;
            }

            *dst++ = '0';
            len++;

            auto hiByte = static_cast<unsigned char>(*src) >> 4;
            if (hiByte) {
                *dst++ = hiByte > 9 ? 'a' + hiByte - 10 : '0' + hiByte;
                len++;
            }

            auto loByte = static_cast<unsigned char>(*src) & 0x0f;
            if (loByte || hiByte) {
                *dst++ = loByte > 9 ? 'a' + loByte - 10 : '0' + loByte;
                len++;
            }

            *dst++ = 'h';
            len++;

            src++;
        } else {
            if (*src == '\'') {
                if (src[1] != '\'')
                    break;
                else
                    src++;
            }

            if (!inQuote) {
                if (len > 0) {
                    *dst++ = ',';
                    *dst++ = ' ';
                    len += 2;
                }
                *dst++ = '\'';
                len++;
                inQuote = true;
            }

            *dst++ = *src++;
            len++;
        }
    }

    if (inQuote) {
        *dst++ = '\'';
        len++;
    };

    src += 1 + (src[1] == '\n');

    token.type = Token::T_STRING;
    token.textLength = len;
    return src;
}

static const char *parseNumber(const char *src, char *dst, Token& token)
{
    assert(isNumberStart(*src));
    auto start = src;

    if (*src == '+' || *src == '-') {
        if (!isDigit(src[1])) {
            *dst = *src;
            token.type = *src == '+' ? Token::T_PLUS : Token::T_MINUS;
            token.category = Token::Operator;
            token.textLength = 1;
            return src + 1;
        }

        *dst++ = *src++;
    }

    assert(isDigit(*src));

    bool isDec = true;
    bool isBin = true;

    const char *binEnd = nullptr;

    for (; !isDelimiter(*src); src++, dst++) {
        *dst = *src;

        if (*src == 'b' && isBin ) {
            binEnd = src + 1;
            isBin = false;
        }

        if (*src == 'h') {
            token.type = Token::T_HEX;
            token.textLength = src - start + 1;
            return src + 1;
        } else if (*src != '0' && *src != '1') {
            isBin = false;
            if (*src < '2' || *src > '9') {
                isDec = false;
                if ((*src < 'a' || *src > 'f') && (*src < 'A' || *src > 'F'))
                    break;
            }
        }
    }

    if (binEnd) {
        token.type = Token::T_BIN;
        token.textLength = binEnd - start;
        return binEnd;
    }

    if (!isDec) {
        src = start + (*start == '+' || *start == '-');
        while (isDigit(*src))
            src++;
    }

    token.textLength = src - start;
    token.type = Token::T_NUM;
    return src;
}

static const char *parseDup(const char *src, char *dst, Token& token)
{
    assert(src[0] == 'd' && src[1] == 'u' && src[2] == 'p' && src[3] == '(' && src[4] != ')');

    src += 4;
    token.category = Token::Dup;

    if (isDigit(*src) || (*src == '+' || *src == '-') && isDigit(src[1])) {
        src = parseNumber(src, dst, token);
    } else if (*src == '\'') {
        src = parseString(src + 1, dst, token);
    } else if (src[0] == '<' && src[1] == '0' && src[2] == '>' && src[3] == ')') {
        token.type = Token::T_DUP_STRUCT_INIT;
        *dst++ = '<';
        *dst++ = '0';
        *dst++ = '>';
        token.textLength = 3;
        return src + 4;
    } else if (*src == '?' && src[1] == ')') {
        *dst = '?';
        token.type = Token::T_DUP_QMARK;
        token.textLength = 1;
        return src + 2;
    } else {
        src -= 4;
        auto start = src;

        while (*src != ')' && *src != '\n')
            *dst++ = *src++;

        if (*src == ')')
            *dst++ = *src++;

        token.category = Token::Id;
        token.type = Token::T_ID;
        token.textLength = src - start;
        token.hash = Util::hash(start, token.textLength);

        return src;
    }

    while (*src != ')' && *src != '\n')
        src++;

    return src + (*src == ')');
}

static const char *parseId(const char *start, const char *src, char *dst, Token& token)
{
    while (!isDelimiter(*src))
        *dst++ = *src++;

    token.category = Token::Id;
    token.type = Token::T_ID;
    token.textLength = src - start;
    token.hash = Util::hash(start, token.textLength);

    return src;
}
''')


def lookupTokenPrologue():
    op = lambda ch, type: f'''
    }} else if (*p == '{ch}') {{
        token.type = Token::{type};
        token.textLength = 1;
        token.category = Token::Operator;
        *id = '{ch}';
        return p + 1;'''

    return r'''    memset(&token, 0, sizeof(Token));
    auto start = p;

    auto id = const_cast<char *>(token.text());

    while (*p == ' ' || *p == '\r' || *p == '\t')
        p++;

    // the order of tests is sorted by their relative occurrence in swos.asm ;)
    if (*p == '\r' || *p == '\n' ) {
        token.type = Token::T_NL;
        token.textLength = 2;
        token.category = Token::Whitespace;
        *id++ = '\r';
        *id = '\n';
        return p + 1 + (p[1] == '\n');
    } else if (isNumberStart(*p)) {
        token.category = Token::Number;
        return parseNumber(p, id, token);''' + \
    op(',', 'T_COMMA') + r'''
    } else if (*p == ';') {
        token.type = Token::T_COMMENT;
        // include leading whitespace too if it's a comment (we'll assume there's always new line at the end), but don't capture ending new line
        // also convert tabs to spaces since IDA adds them in a completely nonsensical manner
        if (p > start) {
            memcpy(id, start, p - start);
            id += p - start;
        }

        for (; *p != '\r'; p++)
            *id++ = *p == '\t' ? ' ' : *p;

        token.textLength = p - start;
        token.category = Token::Whitespace;
        return filterComment(start, token);''' + \
    op('[', 'T_LBRACKET') + op(']', 'T_RBRACKET') + r'''
    } else if (*p == '+') {
        // to disambiguate uses of + we will have to regard it as binary operator always, and numbers can't start with it
        token.textLength = 1;
        token.type = Token::T_PLUS;
        token.category = Token::Operator;
        *id = '+';
        return p + 1;
    } else if (*p == '\'') {
        token.category = Token::Id;
        return parseString(p + 1, id, token);
    } else if (p[0] == 'd' && p[1] == 'u' && p[2] == 'p' && p[3] == '(' && p[4] != ')') {
        return parseDup(p, id, token);''' + \
    op('*', 'T_MULT') + '''
    } else if (!*p) {
        token.type = Token::T_EOF;
        token.category = Token::Eof;
        return p;
    } else {
        start = p;
    }
'''


def outputLookupToken(tokens, useIfs):
    global out

    out(f'static inline const char *lookupToken{("","_withIfs")[useIfs]}(Token& token, const char *p)\n{{')
    out(lookupTokenPrologue())

    stem = ''
    indent = ' ' * kTabSize

    for token in tokens:
        word = token['id']
        prefixLength = getMutualPrefixLength(stem, word)

        while len(stem) > prefixLength + 1:
            indent = indent[:-kTabSize]
            stem = stem[:-1]
            if not len(stem) or len(stem) != prefixLength:
                out(indent + '}' + (f'\n{indent}break;' if not useIfs else ''))

        if len(stem) and len(stem) == prefixLength + 1:
            stem = stem[:-1] + word[prefixLength]
            out(indent[:-kTabSize] + (f"case '{stem[-1]}':" if not useIfs else f"}} else if (p[-1] == '{stem[-1]}') {{"))

        while len(stem) < len(word):
            stem += word[len(stem)]
            if useIfs:
                out(indent + f"if ((*id++ = *p++) == '{stem[-1]}') {{")
            else:
                out(indent + 'switch (*id++ = *p++) {')
                out(indent + f"case '{stem[-1]}':")
            indent += ' ' * kTabSize

        # assume there's a new line at the end
        out(indent + 'if (isDelimiter(*p)) {')
        indent += ' ' * kTabSize

        out(indent + f'token.category = Token::{token["category"]};')
        out(indent + f'token.type = Token::{token["type"]};')
        out(indent + 'token.textLength = p - start;')

        if token['category'] == 'Register' and 'subCategory' in token:
            registerType = token['subCategory']
            out(indent + f'token.registerType = Token::{registerType};')
            out(indent + f'token.registerSize = {str(token["registerSize"])};')
        elif token['category'] == 'Instruction' and 'subCategory' in token:
            instructionType = token['subCategory']
            out(indent + f'token.instructionType = Token::{instructionType};')
        elif token['category'] == 'Keyword' and 'subCategory' in token:
            keywordType = token['subCategory']
            out(indent + f'token.keywordType = Token::{keywordType};')

        out(indent + 'return p;')
        indent = indent[:-kTabSize]
        out(indent + '}')

    while len(stem) > 0:
        indent = indent[:-kTabSize]
        stem = stem[:-1]
        out(indent + '}')

    out(r'''
    return parseId(start, p - 1, id - 1, token);
}''')


# the main output routine
def outputTokensCppFile(outputDir, tokens):
    # verify that each enum member is included in print function; crude but it works :)
    lines = inspect.getsource(outputTypeToString)
    for specialToken in specialTokens():
        if specialToken not in lines:
            sys.exit(f'{specialToken} is not included in typeToString()!')

    if specialTokens()[0] != 'T_ID':
        sys.exit('T_ID must be first special token.')

    header = '// automatically generated, do not edit'
    global out

    with open(makePath(outputDir, kTokenTypeEnumFilename), 'w') as f:
        out = lambda *args, **kwargs: print(*args, **kwargs, file=f)
        out(header)
        outputEnum(tokens)

    with open(makePath(outputDir, kTokenLookupFilename), 'w') as f:
        out = lambda *args, **kwargs: print(*args, **kwargs, file=f)

        out(header + '\n\n#pragma once\n')
        out('#include <cassert>')
        out('#include "Util.h"')

        outputCharTypeFunctions()
        outputTypeToString(tokens)
        outputFilterComment()
        outputParsingRoutines()
        outputLookupToken(tokens, False)


def processInputFile(file):
    tokens = []
    tokenSet = set()
    lineNo = 1
    for line in file.readlines():
        line = line.strip()
        if line and line[0] != '#':
            line = line.split('#', 1)[0].strip()
            id, *category = line.split() + [None]
            type = 'T_' + id.upper()

            if type in specialTokens():
                sys.exit(f'Reserved token type {type} found in line {lineNo}.')
            elif category[0] not in ('Keyword', 'Ignore', 'Register', 'Instruction', 'StructUnion', 'SizeSpecifier'):
                sys.exit(f'Invalid category "{category[0]}" found in line {lineNo}.')
            elif id in tokenSet:
                sys.exit(f'Duplicate token {id} found in line {lineNo}')

            tokenSet.add(id)
            tokens.append({ 'type': type, 'id': id, 'category': category[0], 'line': lineNo })

            if category[0] == 'Register':
                if len(category) < 3:
                    sys.exit('Missing fields for register {id} in line {lineNo}.')
                tokens[-1]['registerSize'] = category[2]

            if category[1]:
                tokens[-1]['subCategory'] = category[1]
        lineNo += 1

    tokens.sort(key=lambda token: token['id'])
    return tokens, tokenSet


def outputTestString(testString):
    indent = ' ' * 4

    lead = 'const char kTestString[] = '
    right = kLineBreakLimit - len(lead) - 2
    out(lead + f'"{testString[:right]}"')

    remaining = len(testString) - right
    while remaining > 0:
        left = right
        right += kLineBreakLimit - len(indent) - 2
        # don't break escapes
        if right <= len(testString):
            for i in range(4, 0, -1):
                if testString[right - i] == '\\':
                    right -= i
                    break
            while testString[right - 1] == '\\':
                right -= 1
        remaining -= right - left
        out(f'{indent}"{testString[left:right]}"')

    out('" ";\n')   # make sure it ends with a new line


def outputTestTokens(testTokens):
    out('struct TestToken {')
    out('    const char *text;')
    out('    int textLength;')
    out('    Token::Type type;')
    out('    Token::Category category;')
    out('    int subCategory;    // only first union member can be initialized, so go with ordinary int')
    lead = '} static const kTestTokens[] = {'
    current = len(lead)
    out(lead, end='')

    for i, token in enumerate(testTokens):
        subCategory = 0
        if 'subCategory' in token:
            if token['category'] == 'Register':
                subCategory = f'({token["registerSize"]} << 8) | (int)Token::{token["subCategory"]}'
            else:
                if token['type'] == 'T_ID' or token['type'] == 'T_STRING':
                    prefix = '(int)'
                else:
                    prefix = 'Token::'
                subCategory = prefix + str(token['subCategory'])

        id = token['id']

        if token['type'] == 'T_COMMENT' and id.endswith('\\r\\n'):
            id = ' ' + id[:-4]

        if token['type'] == 'T_STRING':
            filteredId = ''
            for c in id:
                filteredId += c if ord(c) < 128 else r'\\x' + hex(ord(c))[2:]
            id = filteredId
            length = len(id.replace("\\'", "'").replace(r'\\', '\\'))
        else:
            length = eval("len('" + id + "')")

        category = 'Token::' + token['category']

        tokenString = f' /* {i} */ {{ "{id}", {length}, Token::{token["type"]}, {category}, {subCategory}, }}';

        current += len(tokenString) + 1
        if current >= kLineBreakLimit + 4:     # relax line limit a little bit for the tokens
            tokenString = '\n    ' + tokenString[1:]
            current = len(tokenString)
        out(tokenString + ',', end='')

    out('\n};\n')


def outputTestFunction(lookupFunction):
    out(f'static bool testTokens_{lookupFunction}()')
    out(r'''{
    auto p = kTestString;
    char tokenData[sizeof(Token) + sizeof(kTestString)];
    auto token = reinterpret_cast<Token *>(tokenData);
    constexpr int kMaxDisplayLength = 25;

    int i = 0;
    for (const auto& expectedToken: kTestTokens) {
        auto prev = p;
        p = ''', end=''); out(lookupFunction, end=''); out(r'''(*token, p);
        if (expectedToken.type != token->type || expectedToken.category != token->category ||
            expectedToken.subCategory != token->instructionType || expectedToken.textLength != token->textLength ||
            (token->textLength && memcmp(expectedToken.text, token->text(), token->textLength))) {
            std::cout << "Test failed at token #" << i << "!\n";
            const auto& str = std::string(prev);
            std::cout << "String: " << str.substr(0, kMaxDisplayLength) << (str.size() <= kMaxDisplayLength ? "" : "...") << '\n';
            std::cout << "Expected text: " << expectedToken.text << ", length: " << expectedToken.textLength <<
                ", actual text: " << token->string() << ", length: " << token->textLength << '\n';
            std::cout << "Expected type: " << Token::typeToString(expectedToken.type) <<
                ", actual type: " << Token::typeToString(token->type) << '\n';
            std::cout << "Expected category: " << expectedToken.category << ", actual category: " << token->category << '\n';
            std::cout << "Expected subcategory: " << expectedToken.subCategory << ", actual subcategory: "
                << token->instructionType << '\n';
            assert(false);
            return false;
        }
        i++;
    }

    return true;
}''')


def testLookupEpilogue(specificPart):
    return r'''    if (p == start && (*p == '+' || *p == '-'))
        p++;

    if (*p == '\'') {
        token.category = Token::Id;
        return parseString(p + 1, id, token);
    }

    // assume there's a new line at the end
    while (!isDelimiter(*p))
        *id++ = *p++;

    int length = p - start;
    memcpy(id, start, length);
    token.textLength = length;

    if (length == 1) {
        switch (*start) {
        case '[':
            token.type = Token::T_LBRACKET;
            break;
        case ']':
            token.type = Token::T_RBRACKET;
            break;
        case ',':
            token.type = Token::T_COMMA;
            break;
        case '*':
            token.type = Token::T_MULT;
            break;
        }
        if (token.type != Token::T_NONE) {
            token.category = Token::Operator;
            return p;
        }
    }
''' + specificPart + r'''
    } else {
        if (isNumberStart(*start))
            return parseNumber(start, id, token);
        else if (*start == '\'')
            return parseString(start + 1, id, token);
        else if (start[0] == 'd' && start[1] == 'u' && start[2] == 'p' && start[3] == '(' && start[4] != ')')
            return parseDup(start, id, token);
        else
            return parseId(start, p, id, token);
    }

    return p;
}
'''


def outputStdlibTestFunction(tokens):
    out(r'''
#include <unordered_map>
#include <tuple>
#include <utility>

static const std::unordered_map<std::string, std::tuple<Token::Type, Token::Category, int>> kKeywordMap = {''')

    for token in tokens:
        subCategory = 0
        if 'subCategory' in token:
            if token['category'] == 'Register':
                subCategory = f'({token["registerSize"]} << 8) | (int)Token::{token["subCategory"]}'
            else:
                subCategory = ('Token::' if token['type'] != 'T_ID' else '(int)') + token['subCategory']
        out(f'    {{ "{token["id"]}", {{ Token::{token["type"]}, Token::{token["category"]}, {subCategory} }} }},')

    out(r'''};

static const char *lookupTokenStdLib(Token& token, const char *p)
{''')
    out(lookupTokenPrologue())
    out(testLookupEpilogue('''
    auto str = std::string(start, p - start);

    auto it = kKeywordMap.find(str);

    if (it != kKeywordMap.end()) {
        token.type = std::get<0>(it->second);
        token.category = std::get<1>(it->second);
        token.instructionType = (Token::InstructionType)std::get<2>(it->second);'''))


def outputCustomHashTestFunction(tokens):
    out(r'''
#include <cstdint>

struct Keyword {
    const char *text;
    int length;
    Util::hash_t hash;
    Token::Type type;
    Token::Category category;
    int instructionType;
    bool operator<(const Keyword& other) const {
        return hash < other.hash;
    }
} static const kHashKeywords[] = {''')

    hashes = {}
    collisions = 0

    for token in tokens:
        id = token['id']
        hash = simpleHash(id)
        if hash in hashes:
            collisions += 1
        hashes[hash] = id
        token['hash'] = hash

    for token in sorted(tokens, key=operator.itemgetter('hash')):
        id = token['id']
        subCategory = 0
        if 'subCategory' in token:
            if token['category'] == 'Register':
                subCategory = f'({token["registerSize"]} << 8) | (int)Token::{token["subCategory"]}'
            else:
                subCategory = ('Token::' if token['type'] != 'T_ID' else '(int)') + token['subCategory']
        out(f'    {{ "{id}", {len(id)}, {token["hash"]}, Token::{token["type"]}, Token::{token["category"]}, {subCategory} }},')

    out(f'}};\n\n// collisions: {collisions}, total keywords: {len(tokens)}\nconstexpr int kNumCollisionsCustomHash = {collisions};')
    out(r'''
#include <algorithm>

struct Comp
{
    bool operator() (const Keyword& k, Util::hash_t hash) const {
        return k.hash < hash;
    }
    bool operator() (Util::hash_t hash, const Keyword& k) const {
        return hash < k.hash;
    }
};

static const char *lookupTokenCustomHash(Token& token, const char *p)
{''')
    out(lookupTokenPrologue())
    out(testLookupEpilogue(r'''
    auto h = Util::hash(start, length);
    auto range = std::equal_range(std::begin(kHashKeywords), std::end(kHashKeywords), h, Comp());

    const Keyword *keyword{};
    for (auto it = range.first; it != range.second; ++it) {
        if (it->length == length && !memcmp(start, it->text, length)) {
            keyword = it;
            break;
        }
    }

    if (keyword) {
        token.type = keyword->type;
        token.category = keyword->category;
        token.instructionType = (Token::InstructionType)keyword->instructionType;'''))


def gotGperf():
    try:
        return subprocess.call('gperf -v', stdout=subprocess.PIPE, stderr=subprocess.PIPE) == 0
    except FileNotFoundError:
        return False


def outputGperfTestFunction(tokens):
# %switch=1 seems to be negligibly slower
    gperfInput = r'''%language=C++
%readonly-tables
%struct-type
%compare-strncmp
%define lookup-function-name lookup
%define class-name Gperf
struct GperfToken { const char *name; Token::Type type; Token::Category category; int instructionType; };
%%
'''
    for token in tokens:
        subCategory = 0
        if 'subCategory' in token:
            if token['category'] == 'Register':
                subCategory = f'({token["registerSize"]} << 8) | (int)Token::{token["subCategory"]}'
            else:
                subCategory = ('Token::' if token['type'] != 'T_ID' else '(int)') + token['subCategory']
        gperfInput += f'{token["id"]}, Token::{token["type"]}, Token::{token["category"]}, {subCategory}\n'

    gperfInput += '%%'

    proc = subprocess.run('gperf', shell=True, input=gperfInput, stdout=subprocess.PIPE, encoding='utf-8', check=True)
    out()
    out(proc.stdout)

    out('static const char *lookupTokenGperf(Token& token, const char *p)\n{')
    out(lookupTokenPrologue())
    out(testLookupEpilogue('''
    if (auto gperfToken = Gperf::lookup(start, length)) {
        token.type = gperfToken->type;
        token.category = gperfToken->category;
        token.instructionType = (Token::InstructionType)gperfToken->instructionType;'''))


def outputMeasureFunction():
    out('''
#include <chrono>

template<typename F, typename... Args>
static int64_t measure(F func, Args&&... args)
{
    using namespace std::chrono;

    auto t1 = high_resolution_clock::now();
    func(std::forward<Args>(args)...);
    auto t2 = high_resolution_clock::now();
    return duration_cast<milliseconds>(t2 - t1).count();
}
''')


def outputPerformanceTest():
    out(r'''
    constexpr int kNumLoops = 500;
    static const std::pair<bool (*)(), const char *> functions[] = {
        { testTokens_lookupToken, "state machine (switch)" },
        { testTokens_lookupToken_withIfs, "state machine (ifs)" },
        { testTokens_lookupTokenStdLib, "std::unordered_map" },
        { testTokens_lookupTokenCustomHash, "custom hash function" },''')
    if gotGperf():
        out('        { testTokens_lookupTokenGperf, "gperf generated lookup" },')
    out(r'''    };

    size_t numCollisions = 0;
    for (size_t i = 0; i < kKeywordMap.bucket_count(); i++)
        numCollisions += std::max(static_cast<int>(kKeywordMap.bucket_size(i)) - 1, 0);

    std::cout << "Number of collisions for std::unordered_map: " << numCollisions << '\n';
    std::cout << "Number of collisions for custom hash: " << kNumCollisionsCustomHash << "\n\n";

    std::cout.imbue(std::locale(""));

    for (const auto& f : functions) {
        auto time = measure([&f, kNumLoops]() {
            for (int i = 0; i < kNumLoops; i++) {
                if (!f.first()) {
                    std::cout << "Unexpected error while testing " << f.first << '\n';
                    exit(1);
                }
            }
        });
        std::cout << "Time for " << f.second << ": " << time << "ms\n";
    }
''')


def createTestbed(outputDir, tokens, tokenSet, testPerformance):
    with open(makePath(outputDir, 'main.cpp'), 'w') as file:
        global out
        out = lambda *args, **kwargs: print(*args, **kwargs, file=file)

        out('// automatically generated, do not edit')
        out('// tester implementation for generated token lookup function\n')
        out('#include <vector>')
        out('#include <string>')
        out('#include <memory>')
        out('#include <iostream>')
        out('#include <array>')
        out('#include <cassert>')
        out('#include "Tokenizer.h"')
        out('#include "Util/Util.h"')
        out(f'#include "{kTokenLookupFilename}"\n')

        testTokens = []

        # generate strings based on tokens: first normal order, then reversed and then random
        for f in (lambda x: x, reversed, lambda x: random.sample(x, len(x))):
            for token in f(tokens):
                if token['id'] not in specialTokens():
                    testTokens.append(token)

        add = lambda id, type, category, subCategory: testTokens.append(
            { 'id': id, 'type': type, 'category': category, 'subCategory': subCategory })
        addTestId = lambda id: add(id, 'T_ID', 'Id', str(simpleHash(id)))
        addTestNumber = lambda num, type: testTokens.append({ 'type': type, 'category': 'Number', 'id': str(num).replace('_', '') })
        addComment = lambda comment: testTokens.append({ 'type': 'T_COMMENT', 'category': 'Whitespace', 'id': comment.replace(r'\t', ' ') + r'\r\n' })
        addDup = lambda id, type: testTokens.append({ 'id': id, 'type': type, 'category': 'Dup' })
        addString = lambda id: testTokens.append({ 'id': id, 'category': 'Id', 'type': 'T_STRING' })


        # some edge-case id's:
        addTestId('st')         # is a prefix of a keyword
        addTestId('a')          # single letter (+ is prefix)
        addTestId('xlatbr')     # contains keyword as a prefix
        addTestId('loo')        # common prefix of multiple keywords
        addTestId('aag')        # common prefix of multiple keywords + additional letter
        addTestId('aaaaam')     # two keywords combined
        addTestId('aDd')        # upper-case mix
        addTestId('maa')        # keyword backwards
        addTestId('xla365')     # keyword prefix continuing with a number
        addTestId('eb0afh')     #   - // - hex number
        # numbers
        addTestNumber('-101b', 'T_BIN')     # negative numbers
        addTestNumber('-303h', 'T_HEX')
        addTestNumber('0aBb30771539h', 'T_HEX')
        addTestNumber('-9', 'T_NUM')
        addTestNumber('9', 'T_NUM')
        addTestNumber('-0111bbbbh', 'T_HEX')
        addTestId('b')                      # some non-numbers
        addTestId('h')
        addTestId('e3f05fh')
        addTestId('A1ch')
        addTestNumber('0', 'T_NUM')
        addTestNumber('-0', 'T_NUM')
        addTestNumber('0', 'T_NUM')
        addTestNumber('0b', 'T_BIN')
        addTestNumber('11b', 'T_BIN')
        addTestNumber('00h', 'T_HEX')
        # comment
        testTokens.append({ 'id': r'\r\n', 'category': 'Whitespace', 'type': 'T_NL' })
        addComment(r' ; -----\t----123+900h-- abcdefgh\t')
        # struc/union
        add('struc', 'T_STRUC', 'Keyword', 'StructUnion')
        add('union', 'T_UNION', 'Keyword', 'StructUnion')
        # data size specifiers
        add('db', 'T_DB', 'Keyword', 'DataSizeSpecifier')
        add('dw', 'T_DW', 'Keyword', 'DataSizeSpecifier')
        add('dd', 'T_DD', 'Keyword', 'DataSizeSpecifier')

        # generate random strings as IDs
        for i in range(kNumTestLoops):
            idLen = random.randint(1, 25)

            # what if randomly generated string is equal to an existing reserved word?! ;)
            characterSet = string.ascii_letters + string.digits + '!#$%&()./:<=>?@^_`{|}~'
            while True:
                randomId = ''.join(random.choice(characterSet) for i in range(idLen))
                if randomId not in tokenSet:
                    break

            # and what if it's a number?
            if randomId[0].isdigit() or randomId[0] in ('+', '-'):
                randomId = random.choice(string.ascii_letters) + randomId

            addTestId(randomId)

            # add a random reserved word from time to time
            if not random.getrandbits(2):
                randomToken = random.choice(tokens)
                testTokens.append(randomToken)

            # as well as some numbers
            if not random.getrandbits(3):
                randomNumber = random.randint(-2 ** 31, 2 ** 31 - 1)
                type = random.randint(0, 2)
                numericRepresentations = (
                    (lambda n: '0' + bin(n)[2 + int(n < 0):] + 'b', 'T_BIN'),
                    (lambda n: n, 'T_NUM'),
                    (lambda n: '0' + hex(n)[2 + int(n < 0):] + 'h', 'T_HEX'))
                addTestNumber(numericRepresentations[type][0](randomNumber), numericRepresentations[type][1])

        # make sure there's no space at the end so we can check handling of zero terminator
        testString = ' '.join(map(operator.itemgetter('id'), testTokens))

        delimitersString = '\t '
        delimiterTokens = []

        # test everything mashed together without whitespace
        testDelimiters = (('+', 'T_PLUS', 'Operator'), ('b', 'T_ID', 'Id'), ('+', 'T_PLUS', 'Operator'), ('+', 'T_PLUS', 'Operator'),
            ('-', 'T_MINUS', 'Operator'), ('b', 'T_ID', 'Id'), ('-', 'T_MINUS', 'Operator'), ('-', 'T_MINUS', 'Operator'),
            ('ebx', 'T_EBX', 'Register'), (',', 'T_COMMA', 'Operator'), ('eax', 'T_EAX', 'Register'), ('[', 'T_LBRACKET', 'Operator'),
            ('eax', 'T_EAX', 'Register'), ('+', 'T_PLUS', 'Operator'), ('ebx', 'T_EBX', 'Register'), ('*', 'T_MULT', 'Operator'),
            ('ecx', 'T_ECX', 'Register'), ('-', 'T_MINUS', 'Operator'), ('edx', 'T_EDX', 'Register'), (']', 'T_RBRACKET', 'Operator'),
            ('esi', 'T_ESI', 'Register'))

        for token in testDelimiters:
            delimitersString += token[0]
            delimiterTokens.append({ 'id': token[0], 'type': token[1], 'category': token[2], })
            if token[1] == 'T_ID':
                delimiterTokens[-1]['subCategory'] = simpleHash(token[0])
            elif token[2] == 'Register':
                delimiterTokens[-1]['subCategory'] = 'GeneralRegister'
                delimiterTokens[-1]['registerSize'] = 4

        testString = delimitersString + ' ' + testString
        testTokens[0:0] = delimiterTokens

        # test strings, non-ASCII junk + quote escaping
        testStrings = ((r"'\xfe\x91hiii\x83'", r"0feh, 091h, 'hiii', 083h"),
            (r"'\xfa\xfbnlo'", r"0fah, 0fbh, 'nlo'"),
            (r"'bla\xfa'", r"'bla', 0fah"),
            (r"'bla\xfanta\xfburgla'", r"'bla', 0fah, 'nta', 0fbh, 'urgla'"),
            (r"'bla\xfaks\xfa\xfbi'", r"'bla', 0fah, 'ks', 0fah, 0fbh, 'i'"),
            (r"'bla\xfarma\xfa\xfbgh'", r"'bla', 0fah, 'rma', 0fah, 0fbh, 'gh'"),
            (r"'bla\x00\x01\x10ole'", r"'bla', 0h, 01h, 010h, 'ole'"),
            (r"'\xfa'", r"0fah"),
            (r"'blabla\'\''", r"'blabla''"),
            (r"'\'\'blabla'", r"''blabla'"),
            (r"'bla\'\'bla'", r"'bla'bla'"),
            (r"'\\'", r"'\\'"),
            (r"'\'\''", r"'''"),
        )
        for raw, result in testStrings:
            addString(result);
            testString += ' ' + raw

        # dup test cases
        dupTokens = (('dup(56)', 'T_NUM'), ('dup(-10)', 'T_NUM'), ('dup(+0aa7h)', 'T_HEX'), ('dup(110101b)', 'T_BIN'),
            ("dup('IRBT')", 'T_STRING'), ('dup(?)', 'T_DUP_QMARK'), ('dup(<0>)', 'T_DUP_STRUCT_INIT'), ('dup(blabla)', 'T_ID'),
            ('dup(-moar)', 'T_ID'), ('dup(fo+ul+l)', 'T_ID'))
        for token in dupTokens:
            cleanId = token[0][4:-1]
            addDup(cleanId, token[1])
            if token[1] == 'T_ID':
                testTokens[-1]['id'] = token[0]
                testTokens[-1]['category'] = 'Id'
                testTokens[-1]['subCategory'] = simpleHash(token[0])
            testString += ' ' + token[0]

        # number followed by keyword
        addTestNumber('356', 'T_NUM')
        add('eax', 'T_EAX', 'Register', 'GeneralRegister')
        testTokens[-1]['registerSize'] = 4
        testString += ' ' + '356eax'

        # number followed by non-keyword with keyword prefix
        addTestNumber('0bbh', 'T_HEX')
        addTestId('eaxz')
        testString += ' ' + '0bbheaxz'

        for i in range(len(testTokens)):
            if testTokens[i]['type'] == 'T_COMMENT':
                testTokens.insert(i + 1, { 'id': r'\r\n', 'category': 'Whitespace', 'type': 'T_NL' })
                i += 1

        outputTestString(testString)
        outputTestTokens(testTokens)

        if testPerformance:
            outputStdlibTestFunction(tokens)
            outputCustomHashTestFunction(tokens)
            outputLookupToken(tokens, True)
            if gotGperf():
                outputGperfTestFunction(tokens)

        outputTestFunction('lookupToken')
        if testPerformance:
            outputMeasureFunction();
            outputTestFunction('lookupTokenStdLib')
            outputTestFunction('lookupTokenCustomHash')
            outputTestFunction('lookupToken_withIfs')
            if gotGperf():
                outputTestFunction('lookupTokenGperf')

        out('int main()\n{')

        # do at least some check that hash functions match
        for s in ('No guts, no glory', 'Small onions in large quantities', 'Between a rock and a hard place', ):
            out(f'    assert(Util::hash("{s}", {str(len(s))}) == {simpleHash(s)});')
            out(f'    assert(Util::constHash("{s}") == {simpleHash(s)});')

        out('\n    if (!testTokens_lookupToken())')
        out('        return 1;')

        if testPerformance:
            outputPerformanceTest()

        out('    return 0;')
        out('}')


def getInputFilePath():
    if len(sys.argv) < 2:
        sys.exit('Missing input keywords file path.')

    return sys.argv[1]


def getOutputDir():
    if len(sys.argv) < 3:
        sys.exit('Missing output directory.')

    return sys.argv[2]


def testRequested():
    return len(sys.argv) >= 4 and 'test' in sys.argv[3].lower()


def performanceTestRequested():
    return len(sys.argv) >= 5 and 'performance' in sys.argv[4].lower()


# parameters: <tokens file> <output dir> [--test [--performance-test]]
# output dir is necessary since Meson has hard-coded expectancies from custom build tool
def main():
    inputFilePath = getInputFilePath()
    outputDir = getOutputDir()

    with open(inputFilePath, 'r') as file:
        tokens, tokenSet = processInputFile(file)

    testPerformance = performanceTestRequested()
    if testRequested():
        createTestbed(outputDir, tokens, tokenSet, testPerformance)
    else:
        outputTokensCppFile(outputDir, tokens)

    sys.exit(0)


if __name__ == '__main__':
    main()
