#pragma once

#include "SymbolAction.h"
#include "Tokenizer.h"
#include "Struct.h"
#include "DefinesMap.h"
#include "References.h"
#include "Segment.h"
#include "SymbolFileParser.h"
#include "OutputItem/OutputItem.h"

class IdaAsmParser
{
public:
    IdaAsmParser(const SymbolFileParser& symbolFileParser, SymbolTable& symbolTable,
        const Tokenizer& tokenizer, StructStream& structs, DefinesMap& defines);

    void parse();
    size_t lineCount() const;
    const OutputItemStream& outputItems() const;
    References& references();
    const References& references() const;
    const SegmentSet& segments() const;
    CToken *firstSegment() const;
    CToken *currentSegment() const;
    bool ok() const;
    String missingEndRangeSymbol() const;
    String foundEndRangeSymbol() const;
    size_t errorLine() const;
    std::string errorDescription() const;

private:
    CToken *skipLeadingEmptyLines();
    CToken *parseStruct(CToken *token, TokenList& comments);
    CToken *parseInstruction(CToken *token, TokenList& comments);
    CToken *parseDefine(CToken *token, TokenList& comments);
    CToken *parseSegment(CToken *token, TokenList& comments);
    CToken *parseDirective(CToken *token, TokenList& comments);
    CToken *parseLabel(CToken *token, TokenList& comments);
    CToken *parseProc(CToken *token, TokenList& comments);
    CToken *parseEndProc(CToken *token, TokenList& comments);
    CToken *parseDataItem(CToken *token, TokenList& comments, Token::NoBreakStatus noBreakStatus);
    CToken *parseDataItemElements(CToken *token);
    CToken *parseReplacementDataItemElements(const String& dataReplacement, CToken *token);
    CToken *parseStackVariable(CToken *token, TokenList& comments);
    CToken *ignoreLine(CToken *token);
    CToken *handleSymbolActions(SymbolAction action, const String& rangeEnd, CToken *token);
    void checkProcHookStart(CToken *token, SymbolAction action, const String& packedProcData);
    CToken *checkProcHookInsertion(CToken *token);
    bool isLocalVariable(const char *str, size_t len);
    void addReference(CToken *token, bool sizeOperator = false);
    void outputNullProc();
    CToken *outputSaveCppRegistersInstructions(CToken *token);
    void outputRestoreCppRegistersInstructions();
    template <typename F>
    std::pair<CToken *, CToken *> outputCallInstruction(CToken *token, F fillNameProc);
    CToken *skipUntilNewLine(CToken *token);
    CToken *skipUntilEof(CToken *token);
    CToken *skipUntilSymbol(CToken *token, const String& sym);
    std::pair<CToken *, size_t> getNextSignificantToken(CToken *token);
    std::pair<CToken *, std::vector<CToken *>> collectComments(CToken *token);
    CToken *handleSymbolRemoval(CToken *token, SymbolAction action, const String& sym);
    void clearCollectedOutput(CToken *token);

    void markExports();

    void updateLineCount(CToken *token);
    inline void error(const std::string& what, CToken *token);
    void expect(Token::Type type, CToken *token);
    void expect(const char *str, size_t length, CToken *token);
    template<size_t N> void expect(const char(&str)[N], CToken *token) { expect(str, N - 1, token); }
    void expectTextToken(CToken *token);
    void expectNumber(CToken *token);
    void expectedCustom(const char *what, CToken *token);
    void unexpected(CToken *token);
    void verifyHookLine(CToken *token);
    std::string tokenToString(CToken *token);

    const SymbolFileParser& m_symbolFileParser;
    SymbolTable& m_symbolTable;
    const Tokenizer& m_tokenizer;
    StructStream& m_structs;
    DefinesMap& m_defines;
    References m_references;

    OutputItemStream m_outputItems;

    size_t m_lineNo = 1;
    std::tuple<size_t, size_t, bool, CToken *> m_lastAnonDataItem{};
    CToken *m_currentProc = nullptr;
    CToken *m_restoreRegsProc = nullptr;
    std::vector<String> m_localVars;

    int m_currentHookLine = -1;
    String m_currentProcHook;

    SegmentSet m_segments;
    CToken *m_currentSegment = nullptr;
    CToken *m_firstSegment = nullptr;

    String m_missingEndRangeSymbol;
    String m_foundEndRangeSymbol;

    std::string m_errorDesc;
    size_t m_errorLine = 0;
};
