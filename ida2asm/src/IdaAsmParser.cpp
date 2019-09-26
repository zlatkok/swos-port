#include "IdaAsmParser.h"
#include "Util.h"
#include "DataItem.h"
#include "ParsingException.h"
#include "OutputWriter/OutputFactory.h"

constexpr int kDefinesCapacity = 610;

IdaAsmParser::IdaAsmParser(const SymbolFileParser& symbolFileParser, const Tokenizer& tokenizer, StructStream& structs, DefinesMap& defines)
    : m_symbolFileParser(symbolFileParser), m_tokenizer(tokenizer), m_structs(structs), m_defines(defines)
{
}

void IdaAsmParser::parse()
{
    TokenList comments;
    m_lineNo = 1;

    try {
        for (auto token = skipLeadingEmptyLines(); token < m_tokenizer.end(); advance(token)) {
            auto next = token->next();

            switch (token->type) {
            case Token::T_COMMENT:
                comments.push_back(token);
                assert(next < m_tokenizer.end() && next->isNewLine());
                continue;

            case Token::T_NL:
                comments.push_back(token);
                m_lineNo++;
                continue;

            case Token::T_ID:
                assert(next < m_tokenizer.end());

                if (token->lastChar() == ':' && token->textLength > 1) {
                    token = parseLabel(token, comments);
                } else if (token->lastChar() == '=' && token->textLength > 1) {
                    token = parseStackVariable(token, comments);
                } else if (next->keywordType == Token::StructUnion) {
                    token = parseStruct(token, comments);
                } else if (next->type == Token::T_SEGMENT || next->type == Token::T_ENDS) {
                    token = parseSegment(token, comments);
                } else if (token->type == Token::T_ALIGN || token->text()[0] == '.') {
                    token = parseDirective(token, comments);
                } else if (next->isId() && *(token->next()) == "=") {
                    token = parseDefine(token, comments);
                } else if (next->type == Token::T_PROC) {
                    token = parseProc(token, comments);
                } else if (next->type == Token::T_ENDP) {
                    token = parseEndProc(token, comments);
                } else if (next->category == Token::Keyword && next->keywordType == Token::DataSizeSpecifier || next->isId()) {
                    token = parseDataItem(token, comments);
                } else {
                    expectedCustom("something good", token);
                }
                break;

            case Token::T_EOF:
                continue;

            default:
                switch (token->category) {
                case Token::Instruction:
                    token = parseInstruction(token, comments);
                    break;
                case Token::Ignore:
                    token = ignoreLine(token);
                    m_lineNo += token->isNewLine();
                    continue;   // careful not to drop accumulated leading comments
                case Token::Keyword:
                    if (token->type == Token::T_ALIGN || token->type == Token::T_ASSUME) {
                        token = parseDirective(token, comments);
                        break;
                    } else if (token->keywordType == Token::DataSizeSpecifier) {
                        token = parseDataItem(token, comments);
                        break;
                    }
                    // assume fall-through
                default:
                    unexpected(token);
                }
                comments.clear();
            }

            expect(Token::T_NL, token);
            m_lineNo++;
        }
    } catch (ParsingException& e) {
        m_errorDesc = e.error();
        m_errorLine = m_lineNo;
        updateLineCount(e.token());
    }

    if (!comments.empty()) {
        // pack trailing comments as a fake label
        m_outputItems.addTrailingComments(comments);
    }

    // if IDA would put some trailing comments at the end of the file (not preceding any output) they would be lost
    m_references.seal();
    m_defines.seal();

    markExports();
}

size_t IdaAsmParser::lineCount() const
{
    return m_lineNo - 1;
}

const OutputItemStream& IdaAsmParser::outputItems() const
{
    return m_outputItems;
}

References& IdaAsmParser::references()
{
    return m_references;
}

const References& IdaAsmParser::references() const
{
    return m_references;
}

auto IdaAsmParser::segments() const -> const SegmentSet&
{
    return m_segments;
}

CToken *IdaAsmParser::firstSegment() const
{
    return m_firstSegment;
}

CToken *IdaAsmParser::currentSegment() const
{
    return m_currentSegment;
}

bool IdaAsmParser::ok() const
{
    return m_errorLine == 0;
}

size_t IdaAsmParser::errorLine() const
{
    return m_errorLine;
}

std::string IdaAsmParser::errorDescription() const
{
    return m_errorDesc;
}

CToken *IdaAsmParser::skipLeadingEmptyLines()
{
    auto token = m_tokenizer.begin();

    while (token->isNewLine()) {
        m_lineNo++;
        advance(token);
    }

    return token;
}

CToken *IdaAsmParser::parseStruct(CToken *token, TokenList& comments)
{
    assert(token && token->next()->keywordType == Token::StructUnion);
    assert(token->next()->type == Token::T_STRUC || token->next()->type == Token::T_UNION);

    int structLine = m_lineNo;
    bool isUnion = token->next()->type == Token::T_UNION;

    auto structName = token;
    CToken *comment{};

    token = token->next()->next();
    if (token->isComment()) {
        comment = token;
        advance(token);
    }

    m_structs.addStruct(structName, comments, comment, isUnion);
    comments.clear();

    while (token < m_tokenizer.end()) {
        if (token->category == Token::Whitespace) {
            assert(token->isComment() || token->isNewLine());
            if (token->isNewLine()) {
                m_lineNo++;
                if (token < m_tokenizer.end() && token->next()->isComment())
                    m_structs.addComment(token);
            } else {
                m_structs.addComment(token);
            }
            advance(token);
            continue;
        }

        if (*token == *structName) {
            expect(Token::T_ENDS, token->next());
            return token->next()->next();
        }

        if (token->category == Token::Register)
            error("register names are not allowed as struct members", token);
        else if (*token == "width" || *token == "type" || *token == "name")
            error(std::string(1, '`') + token->string() + "' is not allowed as a struct member", token);

        expectTextToken(token);
        CToken *fieldName{};

        // check if it's an anonymous field
        if (token->category != Token::Keyword || token->keywordType != Token::DataSizeSpecifier) {
            fieldName = token;
            advance(token);
        }

        CToken *type{};
        int size = 0;

        if (token->isId())
            type = token;
        else
            size = token->dataSize();

        advance(token);
        if (token->category != Token::Number && (!token->isId() || *token != "?"))
            expectedCustom("`?' or size", token);

        CToken *dup{};
        if (token->category == Token::Number) {
            if (type)
                m_structs.disallowDup();
            dup = token;
            advance(token);
            expect(Token::T_DUP_QMARK, token);
        }

        advance(token);
        CToken *comment{};

        if (token->isComment()) {
            comment = token;
            advance(token);
        }

        expect(Token::T_NL, token);
        m_structs.addField(fieldName, size, comment, type, dup);

        if (token->next()->isComment())
            m_structs.addComment(token);

        advance(token);
        m_lineNo++;
    }

    assert(token->type == Token::T_ENDS);

    // there's never a comment after `ends' so we won't bother handling it

    return token->next();
}

CToken *IdaAsmParser::parseInstruction(CToken *token, TokenList& comments)
{
    assert(token->category == Token::Instruction);

    checkProcHookInsertion(token);

    CToken *prefix{};

    if (token->instructionType == Token::PrefixInstruction) {
        prefix = token;
        advance(token);
    }

    if (token->category != Token::Instruction)
        expectedCustom("instruction", token);

    auto instructionToken = token;
    advance(token);

    Instruction::OperandTypes opTypes = {};
    Instruction::OperandSizes opSizes = {};
    Instruction::OperandTokens opTokens = { token, token, token, token, token, token };
    CToken *comment{};
    size_t operandNo = 0;
    bool sizeOperator = false;

    auto addOperandToken = [&token, &operandNo, &opTokens] { opTokens[2 * operandNo + 1] = token->next(); };
    auto setSize = [&opSizes, &operandNo, &token, this](size_t size) {
        if (opSizes[operandNo] > 0 && opSizes[operandNo] != size)
            error(std::string("inconsistent operand size: ") + std::to_string(size) + " (was " +
                std::to_string(opSizes[operandNo]) + ')', token);
        Util::assignSize(opSizes[operandNo], size);
    };

    auto checkBranchSpecifier = [&token, this](const std::string& what, CToken *instruction) {
        if (instruction->instructionType != Token::BranchInstruction)
            error(what + " used with a non-branch instruction " + instruction->string(), token);
    };

    while (!token->isNewLine()) {
        switch (token->type) {
        case Token::T_ID:
            opTypes[operandNo] |= instructionToken->instructionType == Token::BranchInstruction ? Instruction::kLabel : Instruction::kVariable;
            // need to look it up to know the size
            // opSizes[operandNo] = 4;
            addOperandToken();
            addReference(token, sizeOperator);

            break;

        case Token::T_SHORT:
            opTypes[operandNo] |= Instruction::kShort;
            opSizes[operandNo] = 1;
            addOperandToken();
            checkBranchSpecifier("`short'", instructionToken);
            break;

        case Token::T_FAR:
        case Token::T_NEAR:
            // just ignore these
            checkBranchSpecifier("`far' or `near'", instructionToken);
            advance(token);
            expect(Token::T_PTR, token);
            break;

        case Token::T_BYTE:
        case Token::T_WORD:
        case Token::T_DWORD:
        case Token::T_TBYTE:
        case Token::T_QWORD:
        case Token::T_FWORD:
            {
                opTypes[operandNo] |= Instruction::kPointer;
                auto size = token->dataSize();
                setSize(size);
                addOperandToken();
                advance(token);
                expect(Token::T_PTR, token);
                addOperandToken();
            }
            break;

        case Token::T_LBRACKET:
            opTypes[operandNo] |= Instruction::kIndirect;
            addOperandToken();
            break;

        case Token::T_RBRACKET:
            if (!(opTypes[operandNo] & Instruction::kIndirect))
                unexpected(token);
            addOperandToken();
            break;

        case Token::T_SIZE:
            sizeOperator = true;
            // assume fall-through
        case Token::T_PLUS:
        case Token::T_MULT:
        case Token::T_OFFSET:
        case Token::T_STRING:
        case Token::T_SMALL:
        case Token::T_LARGE:
            addOperandToken();
            break;

        case Token::T_SEG:
            // this seems to be unsupported in later MASM versions, so remove it
            do {
                advance(token);
            } while (!token->isNewLine());
            return token;

        case Token::T_COMMA:
            if (operandNo < opTokens.size())
                operandNo++;
            else
                unexpected(token);

            if (token->next()->isNewLine())
                unexpected(token->next());

            opTokens[2 * operandNo] = token->next();
            break;

        case Token::T_COMMENT:
            comment = token;
            expect(Token::T_NL, token->next());
            break;

        default:
            if (token->category == Token::Register) {
                opTypes[operandNo] |= Instruction::kRegister;
                // in case it's something like mov al, byte ptr [esi+ecx+35]
                if (!(opTypes[operandNo] & Instruction::kIndirect))
                    setSize(token->registerSize);
                addOperandToken();
            } else if (token->category == Token::Number) {
                opTypes[operandNo] |= Instruction::kLiteral;
                addOperandToken();
            } else {
                assert(false);
            }
        }
        advance(token);
    }

    expect(Token::T_NL, token);

    assert(opTokens[1] >= opTokens[0] && opTokens[3] >= opTokens[2] && (opTokens[2] == opTokens[3] || opTokens[3] >= opTokens[0]));

    if (m_restoreRegsProc && instructionToken->type == Token::T_RETN)
        outputRestoreCppRegistersInstructions();

    m_outputItems.addInstruction(comments, comment, prefix, instructionToken, opSizes, opTypes, opTokens);
    return token;
}

CToken *IdaAsmParser::parseDefine(CToken *token, TokenList& comments)
{
    assert(token->isId() && *token->next() == "=");

    auto name = token;
    auto value = token->next()->next();

    bool inverted = false;
    if (value->type == Token::T_NOT) {
        inverted = true;
        advance(value);
    }

    expectNumber(value);
    token = value->next();

    CToken *comment{};
    if (token->isComment()) {
        comment = token;
        advance(token);
    }

    m_defines.add(comments, comment, name, value, inverted);

    comments.clear();
    return token;
}

CToken *IdaAsmParser::parseSegment(CToken *token, TokenList& comments)
{
    assert(token->next()->type == Token::T_SEGMENT || token->next()->type == Token::T_ENDS);

    auto begin = token;
    CToken *end{}, *comment{};

    while (!token->isNewLine()) {
        if (token->isComment()) {
            comment = token;
            advance(token);
            end = token;
            assert(token->isNewLine());
            break;
        } else {
            end = token->next();
        }
        advance(token);
    }

    expect(Token::T_NL, token);

    m_outputItems.addSegment(comments, comment, begin, end);

    if (begin->next()->type == Token::T_SEGMENT) {
        m_segments.add({ begin, end });
        m_currentSegment = begin;
        if (!m_firstSegment)
            m_firstSegment = begin;
    } else {
        assert(begin->next()->type == Token::T_ENDS);
        m_segments.remove(begin);
        m_currentSegment = nullptr;
    }

    comments.clear();
    return token;
}

CToken *IdaAsmParser::parseDirective(CToken *token, TokenList& comments)
{
    assert(token->isText() && (token->text()[0] == '.' || token->type == Token::T_ALIGN || token->type == Token::T_ASSUME));

    if (!token->isComment() && !token->isText() && token->type != Token::T_ALIGN && token->type != Token::T_ASSUME)
        unexpected(token);

    auto begin = token;
    advance(token);

    while (!token->isComment() && !token->isNewLine())
        advance(token);

    auto end = token;
    CToken *comment{};

    if (token->isComment()) {
        comment = token;
        advance(token);
        end = token;
    }

    assert(token->isNewLine());
    m_outputItems.addDirective(comments, comment, begin, end);

    comments.clear();
    return token;
}

CToken *IdaAsmParser::parseLabel(CToken *token, TokenList& comments)
{
    assert(token && token->isId());

    if (token->isLocalLabel())
        checkProcHookInsertion(token);
    else
        m_references.addLabel(token);

    auto name = token;
    advance(token);

    CToken *comment{};
    if (token->isComment()) {
        comment = token;
        advance(token);
    }

    m_outputItems.addLabel(comments, comment, name);

    comments.clear();
    expect(Token::T_NL, token);
    return token;
}

CToken *IdaAsmParser::parseProc(CToken *token, TokenList& comments)
{
    assert(token->isText() && token->next()->type == Token::T_PROC);
    assert(!m_currentProc);
    assert(m_localVars.empty());

    auto action = m_symbolFileParser.symbolAction(token);

    if (!(action.first & SymbolFileParser::kRemove)) {
        m_references.addProc(token);
        m_currentProc = token;

        if (action.first & SymbolFileParser::kInsertCall) {
            verifyHookLine(token);
            m_currentProcHookLine = *reinterpret_cast<int32_t *>(action.second.c_str());
            m_currentLineHook = m_lineNo + m_currentProcHookLine;
        }
    }

    token = token->next()->next();

    if (token->type == Token::T_NEAR || token->type == Token::T_FAR) {
        // ignore proc type
        advance(token);
    }

    CToken *comment{};
    if (token->isComment()) {
        comment = token;
        advance(token);
    }

    if (!(action.first & SymbolFileParser::kRemove)) {
        m_outputItems.addProc(comments, comment, m_currentProc);

        // there's a left over int 3 at the beginning of one function so get rid of it
        auto nextToken = getNextSignificantToken(token);
        if (nextToken.first->type == Token::T_INT) {
            auto param = nextToken.first->next();
            if (param->category == Token::Number && param->parseInt() == 3) {
                // leave it for abort functions
                if (!m_currentProc->startsWith("Fatal") && !m_currentProc->startsWith("Endless")) {
                    m_lineNo += nextToken.second;
                    comments.clear();
                    return skipUntilNewLine(nextToken.first->next()->next());
                }
            }
        }
    }

    comments.clear();

    if ((action.first & SymbolFileParser::kRemove) || (action.first & SymbolFileParser::kNull)) {
        // the proc better be terminated and without sub-procs :)
        while (token->type != Token::T_ENDP) {
            m_lineNo += token->isNewLine();
            advance(token);
        }

        token = skipUntilNewLine(token);

        if (action.first & SymbolFileParser::kNull)
            outputNullProc();

        if (!action.second.empty())
            token = skipUntilSymbol(token, action.second == "@end" ? nullptr : &action.second);
    } else if (action.first & SymbolFileParser::kSaveCppRegisters) {
        token = outputSaveCppRegistersInstructions(token);
        m_restoreRegsProc = m_currentProc;
    }

    if (action.first & SymbolFileParser::kOnEnter) {
        CToken *hookProc;

        std::tie(hookProc, token) = outputCallInstruction(token, [this](auto nameToken) {
            auto buffer = const_cast<char *>(nameToken->text());

            memcpy(buffer, m_currentProc->text(), m_currentProc->textLength);
            memcpy(buffer + m_currentProc->textLength, Util::kOnEnterSuffix, sizeof(Util::kOnEnterSuffix) - 1);

            nameToken->textLength = m_currentProc->textLength + sizeof(Util::kOnEnterSuffix) - 1;
        });

        m_references.addReference(hookProc);
    }

    return token;
}

CToken *IdaAsmParser::parseEndProc(CToken *token, TokenList& comments)
{
    assert(token->isText() && token->next()->type == Token::T_ENDP);

    auto name = token;
    advance(token);

    verifyHookLine(name);
    m_currentLineHook = -1;

    expect(Token::T_ENDP, token);
    advance(token);

    CToken *comment{};
    if (token->isComment()) {
        comment = token;
        advance(token);
    }

    m_outputItems.addEndProc(comments, comment, name);
    m_localVars.clear();

    assert(m_currentProc);
    m_currentProc = nullptr;
    m_restoreRegsProc = nullptr;

    comments.clear();
    expect(Token::T_NL, token);
    return token;
}

CToken *IdaAsmParser::parseDataItem(CToken *token, TokenList& comments)
{
    assert(token->isId() || token->category == Token::Keyword && token->keywordType == Token::DataSizeSpecifier);

    CToken *name{};
    if (token->isId()) {
        name = token;
        advance(token);
    }

    // this really belongs to the element, but it's used so rarely (and at most once) that we can save some memory by having it here
    CToken *structName{};
    int size = 0;

    if (token->isId()) {
        structName = token;
        advance(token);
    } else {
        size = token->dataSize();
        if (size < 0)
            expectedCustom("data size specifier", token);
        advance(token);
    }

    if (name) {
        auto action = m_symbolFileParser.symbolAction(name);
        bool removed = action.first == SymbolFileParser::kRemove;

        if (removed) {
            auto untilSymbol = action.second;
            token = !untilSymbol.empty() ? skipUntilSymbol(token, &untilSymbol) : skipUntilNewLine(token);
            comments.clear();
            return token;
        }

        m_references.addVariable(name, size, structName);
    }

    CToken *lineComment{};
    for (auto t = token; !t->isNewLine(); advance(t)) {
        if (t->isComment()) {
            lineComment = t;
            assert(t->next()->isNewLine());
            break;
        }
    }

    bool isUninitializedValue = *token == '?';
    if (!structName && (token->isNumber() || isUninitializedValue) && getNextSignificantToken(token->next()).second) {
        bool sameAsPrevItem = !name && (!lineComment || lineComment->endsWith(';')) && comments.empty() &&
            std::get<0>(m_lastAnonDataItem) == m_lineNo - 1 && std::get<1>(m_lastAnonDataItem) == size &&
            std::get<2>(m_lastAnonDataItem) == isUninitializedValue && *std::get<3>(m_lastAnonDataItem) == *token;
        m_lastAnonDataItem = { m_lineNo, size, isUninitializedValue, token };

        if (sameAsPrevItem) {
            auto lastDataItem = m_outputItems.lastDataItem();
            assert(lastDataItem);
            assert(lastDataItem->numElements() == 1);

            auto item = lastDataItem->begin();

            assert(item);
            if (!item->dup())
                item->increaseDup();
            item->increaseDup();

            return lineComment ? lineComment->next() : token->next();
        }
    }

    m_outputItems.addDataItem(comments, lineComment, name, structName, size);
    comments.clear();

    while (!token->isNewLine()) {
        if (token->isComment()) {
            advance(token);
            break;
        }

        size_t dup = 0;
        int offset = 0;
        bool isOffset = false;
        auto dataElement = token;

        if (token->type == Token::T_OFFSET) {
            isOffset = true;
            advance(token);

            expectTextToken(token);
            dataElement = token;
            addReference(token);

            if (token->next()->type == Token::T_PLUS) {
                token = token->next()->next();
                expectNumber(token);
                offset = token->parseInt();
            }
        } else if (token->next()->category == Token::Dup) {
            expectNumber(token);
            dup = token->parseInt();

            advance(token);
            dataElement = token;
            if (token->isId())
                addReference(token);
        }

        if (dataElement->category != Token::Id && dataElement->category != Token::Number && dataElement->category != Token::Dup)
            expectedCustom("label or numeric constant", dataElement);

        m_outputItems.addDataElement(dataElement, isOffset, offset, dup);
        advance(token);

        if (token->type == Token::T_COMMA)
            advance(token);
    }

    return token;
}

CToken *IdaAsmParser::parseStackVariable(CToken *token, TokenList& comments)
{
    assert(token && token->isText() && token->textLength > 1 && token->lastChar() == '=');

    String name(token->text(), token->textLength - 1);
    advance(token);

    auto sizeOrStruct = token;
    if (!token->isId() && (token->category != Token::Keyword || token->keywordType != Token::SizeSpecifier))
        expectedCustom("size specifier or struct name", token);
    advance(token);

    expect(Token::T_PTR, token);
    advance(token);

    expectNumber(token);
    auto offset = token;
    advance(token);

    CToken *comment{};
    if (token->isComment()) {
        comment = token;
        advance(token);
    }

    m_outputItems.addStackVariable(comments, comment, name, sizeOrStruct, offset);
    m_localVars.push_back(name);

    comments.clear();
    expect(Token::T_NL, token);
    return token;
}

CToken *IdaAsmParser::ignoreLine(CToken *token)
{
    // preserve the comment if present
    while (token->category != Token::Whitespace)
        advance(token);

    return token;
}

void IdaAsmParser::checkProcHookInsertion(CToken *token)
{
    if (m_currentLineHook < static_cast<int>(m_lineNo))
        verifyHookLine(token);

    if (m_currentProc && m_currentLineHook == m_lineNo) {
        auto hookProc = outputCallInstruction(token, [this](auto nameToken) {
            auto buffer = const_cast<char *>(nameToken->text());

            memcpy(buffer, m_currentProc->text(), m_currentProc->textLength);

            auto lineNumber = std::to_string(m_currentProcHookLine);

            buffer[m_currentProc->textLength] = '_';
            memcpy(buffer + m_currentProc->textLength + 1, lineNumber.c_str(), lineNumber.length());

            nameToken->textLength = m_currentProc->textLength + 1 + lineNumber.length();
        });

        m_references.addReference(hookProc.first);
        m_currentLineHook = -1;
    }
}

bool IdaAsmParser::isLocalVariable(const char *str, size_t len)
{
    for (const auto& localVar : m_localVars)
        if (localVar.length() == len && !memcmp(localVar.str(), str, len))
            return true;

    return false;
}

void IdaAsmParser::addReference(CToken *token, bool sizeOperator)
{
    auto ptr = token->text();
    auto len = token->textLength;

    if (!sizeOperator && !token->isLocalLabel()) {
        int dotIndex = token->indexOf('.');
        if (dotIndex >= 0)
            len = dotIndex;

        if (token->startsWith('(')) {
            ptr++;
            len--;
        }

        if (!isLocalVariable(ptr, len))
            m_references.addReference({ ptr, len });
    }
}

void IdaAsmParser::outputNullProc()
{
    static const char tokenBuff[sizeof(Token) + 4] = {
        (char)Token::T_RETN, 0, Token::Instruction, Token::BranchInstruction, 0, 0, 0, 4, 0, 0, 0, 'r', 'e', 't', 'n',
    };
    static_assert(sizeof(Token) == 11, "Fix retn token");
    auto retnToken = reinterpret_cast<CToken *>(tokenBuff);

    m_outputItems.addInstruction({}, {}, {}, retnToken, {}, {}, {});
    m_outputItems.addEndProc({}, {}, m_currentProc);
    m_currentProc = nullptr;
}

CToken *IdaAsmParser::outputSaveCppRegistersInstructions(CToken *token)
{
    static const char tokenBuff[8 * sizeof(Token) + 4 * 4 + 4 * 3] = {
        (char)Token::T_PUSH, 0, Token::Instruction, Token::GeneralInstruction, 0, 0, 0, 4, 0, 0, 0, 'p', 'u', 's', 'h',
        (char)Token::T_EBX, 0, Token::Register, Token::GeneralRegister, 0, 0, 0, 3, 0, 0, 0, 'e', 'b', 'x',
        (char)Token::T_PUSH, 0, Token::Instruction, Token::GeneralInstruction, 0, 0, 0, 4, 0, 0, 0, 'p', 'u', 's', 'h',
        (char)Token::T_ESI, 0, Token::Register, Token::GeneralRegister, 0, 0, 0, 3, 0, 0, 0, 'e', 's', 'i',
        (char)Token::T_PUSH, 0, Token::Instruction, Token::GeneralInstruction, 0, 0, 0, 4, 0, 0, 0, 'p', 'u', 's', 'h',
        (char)Token::T_EDI, 0, Token::Register, Token::GeneralRegister, 0, 0, 0, 3, 0, 0, 0, 'e', 'd', 'i',
        (char)Token::T_PUSH, 0, Token::Instruction, Token::GeneralInstruction, 0, 0, 0, 4, 0, 0, 0, 'p', 'u', 's', 'h',
        (char)Token::T_EBP, 0, Token::Register, Token::GeneralRegister, 0, 0, 0, 3, 0, 0, 0, 'e', 'b', 'p',
    };
    static_assert(sizeof(Token) == 11, "Fix save registers tokens");

    CToken *instructionTokens[4] = {
        reinterpret_cast<CToken *>(tokenBuff),
        reinterpret_cast<CToken *>(tokenBuff + 2 * sizeof(Token) + 4 + 3),
        reinterpret_cast<CToken *>(tokenBuff + 4 * sizeof(Token) + 2 * 4 + 2 * 3),
        reinterpret_cast<CToken *>(tokenBuff + 6 * sizeof(Token) + 3 * 4 + 3 * 3),
    };

    auto result = collectComments(token);
    auto comments = result.second;
    token = result.first;

    for (const auto instructionToken : instructionTokens)
        m_outputItems.addInstruction(instructionToken == instructionTokens[0] ? comments : TokenList{}, {}, {}, instructionToken,
            {}, { Instruction::kRegister }, { instructionToken->next(), instructionToken->next()->next() });

    return token;
}

void IdaAsmParser::outputRestoreCppRegistersInstructions()
{
    static const char tokenBuff[8 * sizeof(Token) + 8 * 3] = {
        (char)Token::T_POP, 0, Token::Instruction, Token::GeneralInstruction, 0, 0, 0, 3, 0, 0, 0, 'p', 'o', 'p',
        (char)Token::T_EBP, 0, Token::Register, Token::GeneralRegister, 0, 0, 0, 3, 0, 0, 0, 'e', 'b', 'p',
        (char)Token::T_POP, 0, Token::Instruction, Token::GeneralInstruction, 0, 0, 0, 3, 0, 0, 0, 'p', 'o', 'p',
        (char)Token::T_EDI, 0, Token::Register, Token::GeneralRegister, 0, 0, 0, 3, 0, 0, 0, 'e', 'd', 'i',
        (char)Token::T_POP, 0, Token::Instruction, Token::GeneralInstruction, 0, 0, 0, 3, 0, 0, 0, 'p', 'o', 'p',
        (char)Token::T_ESI, 0, Token::Register, Token::GeneralRegister, 0, 0, 0, 3, 0, 0, 0, 'e', 's', 'i',
        (char)Token::T_POP, 0, Token::Instruction, Token::GeneralInstruction, 0, 0, 0, 3, 0, 0, 0, 'p', 'o', 'p',
        (char)Token::T_EBX, 0, Token::Register, Token::GeneralRegister, 0, 0, 0, 3, 0, 0, 0, 'e', 'b', 'x',
    };
    static_assert(sizeof(Token) == 11, "Fix restore registers tokens");

    CToken *instructionTokens[4] = {
        reinterpret_cast<CToken *>(tokenBuff),
        reinterpret_cast<CToken *>(tokenBuff + 2 * (sizeof(Token) + 3)),
        reinterpret_cast<CToken *>(tokenBuff + 4 * (sizeof(Token) + 3)),
        reinterpret_cast<CToken *>(tokenBuff + 6 * (sizeof(Token) + 3)),
    };

    for (const auto instructionToken : instructionTokens)
        m_outputItems.addInstruction({}, {}, {}, instructionToken, {}, { Instruction::kRegister },
            { instructionToken->next(), instructionToken->next()->next() });
}

template <typename F>
std::pair<CToken *, CToken *> IdaAsmParser::outputCallInstruction(CToken *token, F fillNameProc)
{
    assert(m_currentProc && m_currentProc->textLength);

    static char tokenBuff[2 * sizeof(Token) + 4 + 200] = {
        (char)Token::T_CALL, 0, Token::Instruction, Token::BranchInstruction, 0, 0, 0, 4, 0, 0, 0, 'c', 'a', 'l', 'l',
        (char)Token::T_ID, 1, Token::Id,
    };
    static_assert(sizeof(Token) == 11, "Fix on enter hook tokens");
    assert(m_currentProc->textLength <= sizeof(tokenBuff) - 2 * sizeof(Token) - 4);

    auto callToken = reinterpret_cast<CToken *>(tokenBuff);
    auto procNameToken = callToken->next();

    fillNameProc(procNameToken);

    procNameToken->hash = Util::hash(procNameToken->text(), procNameToken->textLength);

    auto comments = collectComments(token);
    token = comments.first;

    m_outputItems.addInstruction(comments.second, {}, {}, callToken, {}, { Instruction::kLabel }, { procNameToken, procNameToken->next() });

    return { procNameToken, token };
}

CToken *IdaAsmParser::skipUntilNewLine(CToken *token)
{
    while (!token->isNewLine())
        advance(token);

    return token;
}

CToken *IdaAsmParser::skipUntilSymbol(CToken *token, const String *sym)
{
    CToken *prevLeadingNewLine{}, *prevNewLine{};
    int prevLineNo;

    // don't skip leading comments of the terminating symbol
    while (!token->isEof()) {
        if (token->isNewLine()) {
            prevNewLine = token;
            auto next = token->next();

            if (next->isComment() && !prevLeadingNewLine) {
                prevLeadingNewLine = token;
                prevLineNo = m_lineNo;
            }

            if (sym && next->isId() && next == *sym)
                break;

            m_lineNo++;
        } else if (!token->isComment()) {
            prevLeadingNewLine = nullptr;
        }

        advance(token);
    }

    if (token->isEof()) {
        if (sym) {
            error(std::string("end range symbol `") + sym->string() + "' unmatched", token);
        } else {
            assert(prevNewLine);
            token = prevNewLine;
            m_lineNo--;
        }
    } else if (prevLeadingNewLine) {
        m_lineNo = prevLineNo;
        token = prevLeadingNewLine;
    }

    return token;
}

std::pair<CToken *, size_t> IdaAsmParser::getNextSignificantToken(CToken *token)
{
    size_t numNewLines = 0;

    while (token->category == Token::Whitespace) {
        numNewLines += token->type == Token::T_NL;
        advance(token);
    }

    return { token, numNewLines };
}

std::pair<CToken *, std::vector<CToken *>> IdaAsmParser::collectComments(CToken *token)
{
    if (!token->isNewLine())
        return {};

    std::vector<CToken *> comments;
    auto prev = token;
    advance(token);

    while (token->category == Token::Whitespace) {
        comments.push_back(token);
        m_lineNo += token->type == Token::T_NL;
        prev = token;
        advance(token);
    }

    return { prev, comments };
}

void IdaAsmParser::markExports()
{
    for (const auto& exp : m_symbolFileParser.exports())
        m_references.markExport(exp);
}

void IdaAsmParser::updateLineCount(CToken *token)
{
    assert(token >= m_tokenizer.begin());
    while (token < m_tokenizer.end()) {
        m_lineNo += token->isNewLine();
        advance(token);
    }
}

inline void IdaAsmParser::error(const std::string& what, CToken *token)
{
#ifndef NDEBUG
    __debugbreak();
#endif
    throw ParsingException(what, token);
}

void IdaAsmParser::expect(Token::Type type, CToken *token)
{
    if (token->type != type) {
        auto message = std::string("expecting ") + Token::typeToString(type) + ", got " + tokenToString(token);
        error(message, token);
    }
}

void IdaAsmParser::expect(const char *str, size_t length, CToken *token)
{
    if (token->textLength != length || memcmp(token->text(), str, length)) {
        auto quoted = new char[length + 3];
        memcpy(quoted + 1, str, length);
        quoted[0] = '`';
        quoted[length + 1] = '\'';
        quoted[length + 2] = '\0';
        expectedCustom(quoted, token);
    }
}

void IdaAsmParser::expectTextToken(CToken *token)
{
    if (!token->isText())
        expectedCustom("text token", token);
}

void IdaAsmParser::expectedCustom(const char *what, CToken *token)
{
    auto message = std::string("expecting ") + what  + ", got " + tokenToString(token);
    error(message, token);
}

void IdaAsmParser::expectNumber(CToken *token)
{
    if (token->type != Token::T_HEX && token->type != Token::T_NUM && token->type != Token::T_BIN)
        expectedCustom("numeric value", token);
}

void IdaAsmParser::unexpected(CToken *token)
{
    error(std::string("unexpected token ") + token->typeToString() + " encountered", token);
}

void IdaAsmParser::verifyHookLine(CToken *token)
{
    if (m_currentLineHook >= 0)
        error("insertion point for call passed", token);
}

std::string IdaAsmParser::tokenToString(CToken *token)
{
    std::string str(token->typeToString());

    if (token->isId() || token->type == Token::T_STRING || token->category == Token::Number)
        str += " `" + token->string() + '\'';

    return str;
}
