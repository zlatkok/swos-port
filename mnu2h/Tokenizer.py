"""Breaks raw input into basic parsing units -- tokens."""

import os
import re
import copy

import Util
from Token import Token

class Tokenizer:
    kOperators = tuple(sorted(
        (
            '(', ')', '{', '}', '~', '!', '?', ':', ',', '-', '+', '*', '/', '%',
            '#', '|', '&', '^', '<', '>', '=',
            '*=', '/=', '%=', '==', '^=', '!=', '++', '+=', '--', '-=', '|=',
            '&&', '||', '&=', '<<', '>>', '=>', '<=', '>=',
            '>>=', '<<=', '|>>', '<<|',
            '|>>=', '<<=|',
        ), key=len, reverse=True)
    )

    @staticmethod
    def fromFile(inputPath):
        return Tokenizer(inputPath)

    @staticmethod
    def empty():
        return Tokenizer(None)

    def __init__(self, inputPath):
        assert isinstance(inputPath, (str, type(None)))

        self.inputPath = inputPath

        self.showWarnings = True
        self.currentTokenIndex = 0
        self.tokens = []

        if inputPath:
            self.tokens = self.tokenizeFile(inputPath)

        self.updateEofFlag()

    def __repr__(self):
        repr = f'Tokenizer input file: "{self.inputPath}", current token index: {self.currentTokenIndex}, '
        repr += f'at EOF: {self.atEof}, tokens:\n'
        if self.tokens:
            for i, token in enumerate(self.tokens):
                newLine = '\n' if i < len(self.tokens) - 1 else ''
                marker = '->' if i == self.currentTokenIndex else '  '
                repr += f' {marker} {i:3d}: {token}{newLine}'
        else:
            repr += '  <empty>'

        return repr

    # resetToString
    #
    # in:
    #     inputString   - string to tokenize
    #     templateToken - token containing file/line info that each new token will inherit
    #
    # Tokenizes string and rewinds the state to start returning the newly generated tokens.
    #
    def resetToString(self, inputString, templateToken):
        assert isinstance(inputString, str)
        assert isinstance(templateToken, Token)

        stringTokens = self.tokenizeString(inputString, templateToken)
        self.tokens = stringTokens

        self.currentTokenIndex = 0
        self.updateEofFlag()

    # includeFile
    #
    # in:
    #     path - path to the file to include
    #
    # Tokenizes the file with the given path, and inserts tokens at the current token index.
    #
    def includeFile(self, path):
        assert path and isinstance(path, str)

        includedTokens = self.tokenizeFile(path)
        self.tokens[self.currentTokenIndex:self.currentTokenIndex] = includedTokens
        self.updateEofFlag()

    def getInputPath(self):
        return self.inputPath

    def isAtEof(self):
        return self.atEof

    def getCurrentTokenIndex(self):
        return self.currentTokenIndex

    def getNumTokens(self):
        return len(self.tokens)

    def setCurrentTokenIndex(self, newTokenIndex):
        self.currentTokenIndex = newTokenIndex
        self.updateEofFlag()

    def getTokenRange(self, start, end):
        assert isinstance(start, int)
        assert isinstance(end, int)
        return self.tokens[start:end]

    def deleteToken(self, index):
        assert isinstance(index, int)
        self.deleteTokens(index, index + 1)

    def deleteTokens(self, start, end):
        del self.tokens[start:end]
        self.currentTokenIndex = min(len(self.tokens), self.currentTokenIndex)
        self.updateEofFlag()

    def insertTokenAt(self, index, token):
        assert isinstance(index, int)
        assert isinstance(token, Token)

        self.tokens.insert(index, token)
        self.updateEofFlag()

    def peekTokenAt(self, index):
        return self.tokens[index] if 0 <= index < len(self.tokens) else None

    def setTokenAt(self, index, token):
        assert isinstance(index, int)
        assert isinstance(token, Token)

        self.tokens[index] = token

    def setTokenTextAt(self, index, text):
        assert isinstance(index, int) and index in range(len(self.tokens))
        assert isinstance(text, (str, list))

        newToken = copy.copy(self.tokens[index])
        newToken.string = text
        self.tokens[index] = newToken

    def updateEofFlag(self):
        self.atEof = self.currentTokenIndex >= len(self.tokens)

    # getPreviousToken
    #
    # Returns previous token without affecting current token index.
    # Previous token should exist if this function is called.
    #
    def getPreviousToken(self):
        assert self.currentTokenIndex > 0
        return self.tokens[self.currentTokenIndex - 1] if self.currentTokenIndex > 0 else None

    # gotTokensLeft
    #
    # Returns true if there are any tokens left.
    #
    def gotTokensLeft(self):
        return self.currentTokenIndex < len(self.tokens)

    # peekNextToken
    #
    # Return the next token without changing current token. If there are no more tokens, return None.
    #
    def peekNextToken(self):
        return self.tokens[self.currentTokenIndex] if self.gotTokensLeft() else None

    # getNextToken
    #
    # in:
    #     lookingFor - textual description of the expected token for error reporting,
    #                  if it's null, a generic error will be shown in case EOF is encountered
    #
    # Returns the next token, and bumps the token index if successful.
    # If there are no more tokens, reports an error.
    #
    def getNextToken(self, lookingFor=None):
        assert isinstance(lookingFor, (str, type(None)))
        assert self.atEof or self.currentTokenIndex < len(self.tokens)

        line = self.tokens[-1].line if self.tokens else 1

        if self.atEof:
            errorMessage = 'unexpected end of file'

            if lookingFor:
                lookingFor = Util.withArticle(lookingFor)
                errorMessage += f' while looking for {lookingFor}'

            filename = self.tokens[-1].path if self.tokens else os.path.basename(self.inputPath)
            Util.error(errorMessage, filename, line)
        elif self.currentTokenIndex >= len(self.tokens) - 1:
            self.atEof = True

        token = self.tokens[self.currentTokenIndex]
        self.currentTokenIndex += 1

        return token

    # splitLine
    #
    # in:
    #     line - a line to process (string)
    #
    # Returns a list of sub-strings present in the given line that will form tokens. For example:
    #
    # 'All mimsy were "the borogoves", {And \'the mome\' raths}; outgrabe' =>
    # ['All', 'mimsy', 'were', '"the borogoves"', ',', '{', 'And', "'the mome'", 'raths', '}', ';', 'outgrabe']
    #
    @staticmethod
    def splitLine(line):
        assert isinstance(line, str)

        line = line.split('//', 1)[0].strip()
        tokens = []

        i = 0
        while i < len(line):
            if line[i].isspace():
                i += 1
                continue

            tokenLen = 0

            if line[i] == '"' or line[i] == "'":
                tokenLen = len(line) - i
                quote = line[i]

                for j in range(i + 1, len(line)):
                    if line[j] == quote and line[j - 1] != '\\':
                        tokenLen = j - i + 1
                        break
            elif not line[i].isalnum():
                for op in Tokenizer.kOperators:
                    if op == line[i : i + len(op)]:
                        tokenLen = len(op)
                        break

            if not tokenLen:
                match = re.search(r'(?:@)?[\w]+|[{},():]|[-+]*\d+|[^\s\w]', line[i:])
                if match:
                    tokenLen = match.end() if match.start() == 0 else match.start()
                else:
                    # don't think this can ever be reached
                    tokenLen = len(line) - i

            assert tokenLen > 0
            tokens.append(line[i : i + tokenLen])
            i += tokenLen

        return tokens

    # expect
    #
    # in:
    #     text            - expected string
    #     token           - current token to test
    #     quote           - quote expected string if true
    #     fetchNextToken  - return next token if this is true and we got the expected string
    #     customErrorText - custom text to show on error (if overriding)
    #
    # Makes sure that the token contains the expected string. If not, complains and quits. Returns next token.
    #
    def expect(self, text, token, customErrorText='', fetchNextToken=False, quote=True):
        assert isinstance(text, str)
        assert isinstance(token, Token)
        assert isinstance(customErrorText, str)

        if token.string != text:
            if customErrorText:
                errorMessage = customErrorText
            else:
                if quote:
                    text = f"`{text}'"
                errorMessage = f"expected {text}, but got `{token.string}' instead"

            Util.error(errorMessage, token)

        return self.getNextToken() if fetchNextToken else token

    def expectBlockStart(self, token, fetchNextToken=False):
        assert isinstance(token, Token)
        return self.expect('{', token, f"block start `{{' expected, got `{token.string}'", fetchNextToken)

    def expectColon(self, token, fetchNextToken=False):
        assert isinstance(token, Token)
        return self.expect(':', token, f"expected colon `:', got `{token.string}'", fetchNextToken)

    # expectIdentifier
    #
    # in:
    #     token          - token to check
    #     expectedNext   - optional string that describes next expected token (for better error reporting)
    #     fetchNextToken - flag whether to fetch next token if this one is identifier
    #
    # Checks if the given token is a valid C++ identifier, if not displays an error and ends the program.
    # Returns the token following the identifier.
    #
    def expectIdentifier(self, token, expectedNext=None, fetchNextToken=False):
        assert isinstance(token, Token)
        assert isinstance(expectedNext, (str, type(None)))

        if not token.string.isidentifier():
            Util.error(f"`{token.string}' is not a valid identifier", token)

        if fetchNextToken:
            token = self.getNextToken(expectedNext)

        return token

    # tokenizeFile
    #
    # in:
    #     path - path of the file to tokenize
    #
    # Opens the given file, tokenizes it and returns produced tokens.
    #
    def tokenizeFile(self, path):
        assert path and isinstance(path, str)

        lineNo = 1
        tokens = []

        try:
            with open(path, 'r') as inputFile:
                for line in inputFile.readlines():
                    prev = ''
                    upperCaseWarning = False

                    for string in self.splitLine(line):
                        tokens.append(Token(string, path, lineNo))

                        if string:
                            isQuoteOpened = string[0] in '"\''
                            if isQuoteOpened:
                                if len(string) < 2 or string[0] != string[-1]:
                                    Util.error('unterminated string', path, lineNo)
                                if prev not in ('include', '~', 'print', '#') and string != string.upper():
                                    upperCaseWarning = True
                            elif string == 'warningsOff' and prev == '#':
                                self.showWarnings = False
                            elif string == 'warningsOn' and prev == '#':
                                self.showWarnings = True

                        prev = string

                    if upperCaseWarning and self.showWarnings:
                        Util.warning('non upper case string detected', path, lineNo)
                    if not line.endswith('\n') and self.showWarnings:
                        Util.warning('new line missing', path, lineNo)

                    lineNo += 1

        except FileNotFoundError:
            Util.error(f'File {path} not found')
        except OSError:
            Util.error(f'Error reading {path}')

        return tokens

    # tokenizeString
    #
    # in:
    #     inputString   - string containing input to tokenize
    #     templateToken - token containing data to use for each new token
    #
    #  Tokenizes the given string and returns resulting tokens array.
    #
    def tokenizeString(self, inputString, templateToken):
        assert isinstance(inputString, str)
        assert isinstance(templateToken, Token)

        tokens = []
        lines = inputString.split()

        for line in lines:
            for string in self.splitLine(line):
                tokens.append(Token(string, templateToken.path, templateToken.line))

        return tokens
