# Converter from mnu file to C++ header
#
# usage: python mnu2h.py <input.mnu> <output.h>
#

import os
import re
import sys
import copy
import collections

kInitialEntry = 'initialEntry'
kOnInit = 'onInit'
kAfterDraw = 'afterDraw'
kOnDraw = 'onDraw'

kPreprocReservedWords = ('include', 'textWidth', 'textHeight', 'repeat', 'endRepeat', 'join')

kMenuFunctions = (kOnInit, kAfterDraw, kOnDraw)

kMenuDefaults = (   # expansion of these is delayed, so they can have different values for different entries
    'defWidth', 'defaultWidth', 'defHeight', 'defaultHeight',
    'defX', 'defaultX', 'defY', 'defaultY', 'defaultColor', 'defaultTextFlags',
)

kNextEntryProperties = ('leftEntry', 'rightEntry', 'upEntry', 'downEntry', 'skipLeft', 'skipRight', 'skipUp', 'skipDown')

kEntryFunctions = ('onSelect', 'beforeDraw', 'afterDraw')

kConstants = {
    # backgrounds
    'kNoBackground': 0, 'kNoFrame': 0, 'kGray': 7, 'kDarkBlue': 3, 'kLightBrownWithOrangeFrame': 4,
    'kLightBrownWithYellowFrame': 6, 'kRed': 10, 'kPurple': 11, 'kLightBlue': 13, 'kGreen': 14,

    # inner frames
    'kDarkGrayFrame': 0x10, 'kWhiteFrame': 0x20, 'kBlackFrame': 0x30, 'kBrownFrame': 0x40,
    'kLightBrownFrame': 0x50, 'kOrangeFrame': 0x60, 'kGrayFrame': 0x70, 'kNearBlackFrame': 0x80,
    'kVeryDarkGreenFrame': 0x90, 'kRedFrame': 0xa0, 'kBlueFrame': 0xb0, 'kPurpleFrame': 0xc0,
    'kSoftBlueFrame': 0xd0, 'kGreenFrame': 0xe0, 'kYellowFrame': 0xf0,

    # text colors, practically the same as inner frames (low nibble, and 0 is different)
    'kWhiteText': 0, 'kDarkGrayText': 1, 'kWhiteText': 2, 'kBlackText': 3, 'kBrownText': 4,
    'kLightBrownText': 5, 'kOrangeText': 6, 'kGrayText': 7, 'kNearBlackText': 8,
    'kVeryDarkGreenText': 9, 'kRedText': 10, 'kBlueText': 11, 'kPurpleText': 12,
    'kSoftBlueText': 13, 'kGreenText': 14, 'kYellowText': 15,
    'kCenterAligned': 0,
    'kLeftAligned': 1 << 15, 'kRightAligned': 1 << 14, 'kShowText': 1 << 9, 'kBlinkText': 1 << 13,
    'kBigText': 1 << 4, 'kBigFont': 1 << 4,

    # entry directions
    'kNoEntry': -1, 'kLeft': 0, 'kRight': 1, 'kUp': 2, 'kDown': 3,
}

class Token:
    def __init__(self, string, path='', line=0):
        assert(isinstance(string, (str, int)))
        assert(isinstance(path, str))
        assert(isinstance(line, int))

        self.string = str(string)
        self.line = line
        self.path = path
        self.inputFilename = os.path.basename(path)

    def __repr__(self):
        return f"'{self.string}' [{self.inputFilename}, line {self.line}]"

class Menu:
    def __init__(self):
        self.properties = { 'defaultWidth': 0, 'defaultHeight': 0, 'defaultX': 0,
            'defaultY': 0, 'defaultColor': 0, 'defaultTextFlags': 0, 'x': 0, 'y': 0 }
        self.exportedProperties = {}
        self.entries = {}
        self.variables = {}
        self.templateEntry = Entry()
        self.gotTemplate = False
        self.name = ''

        self.lastEntry = None

        self.onInit = 0
        self.afterDraw = 0
        self.onDraw = 0

        self.previousX = 0
        self.previousY = 0

class StringTable:
    def __init__(self, variable, values=(), initialValue=0):
        assert(isinstance(variable, str))
        assert(isinstance(values, (list, tuple)))
        assert(isinstance(initialValue, int))

        self.variable = variable
        self.values = values
        self.initialValue = initialValue

class Entry:
    def __init__(self, ordinal=-1, name=''):
        assert(isinstance(ordinal, int))
        assert(isinstance(name, str))

        self.name = name
        self.ordinal = ordinal

        for property in kNextEntryProperties:
            setattr(self, property + 'Token', None)

        self.x = 0
        self.y = 0
        self.width = 0
        self.height = 0

        self.text = None
        self.number = None
        self.sprite = None
        self.stringTable = None

        self.textFlags = 0
        self.template = False

        self.func1 = 0
        self.color = 0
        self.invisible = 0
        self.disabled = 0
        self.leftEntry = -1
        self.rightEntry = -1
        self.upEntry = -1
        self.downEntry = -1
        self.directionLeft = -1
        self.directionRight = -1
        self.directionUp = -1
        self.directionDown = -1
        self.skipLeft = -1
        self.skipRight = -1
        self.skipUp = -1
        self.skipDown = -1
        self.controlsMask = 0
        self.onSelect = 0
        self.beforeDraw = 0
        self.afterDraw = 0

class Variable:
    def __init__(self, value, token):
        assert(isinstance(value, str))
        assert(isinstance(token, Token))

        self.value = value
        self.token = token
        self.referenced = False

class ResetTemplateEntry:
    pass


# contains everything a growing parser needs
class Parser:
    def __init__(self, inputPath, outputPath=None):
        assert(isinstance(inputPath, str))
        assert(outputPath is None or isinstance(outputPath, str))

        self.inputPath = inputPath
        self.outputPath = outputPath
        self.inputFilename = os.path.basename(inputPath)

        self.outputFile = None
        if outputPath:
            try:
                self.outputFile = open(outputPath, 'w')
            except FileNotFoundError:
                sys.exit(f"Couldn't open {outputPath} for writing")

        self.currentToken = 0
        self.globals = {}           # global variables
        self.stringTables = set()   # set of all string tables used in the program, value is number of strings
        self.preprocVariables = collections.defaultdict(int)
        self.entryDotReferences = []

        self.menus = {}
        self.tokens = []

        self.atEof = False
        self.tokenized = False
        self.parsed = False

        self.functions = set()      # this will contain names of all the functions declared
        self.entryRefDefLines = {}  # line numbers in input file where entry reference was used

        self.fillCharWidthTables()

    # error
    #
    # in:
    #     tokenOrline - offending token, or the line number where error occurred
    #     text        - text of the error to display
    #
    # Prints formatted error to stdout, with input file path and offending line, and exits the program.
    #
    def error(self, tokenOrLine, text):
        assert(isinstance(tokenOrLine, (Token, int)))
        assert(isinstance(text, str))

        if isinstance(tokenOrLine, Token):
            file = tokenOrLine.inputFilename
            line = tokenOrLine.line
        else:
            file = self.inputFilename
            line = tokenOrLine

        sys.exit(f'{file}({line}): {text}')

    # warning
    #
    # in:
    #     text          - warning text
    #     line          - line number (can be omitted)
    #     inputFilename - name of the input file (use current one if not supplied)
    #
    # Issues a warning in input file at line number.
    #
    def warning(self, text, line=None, inputFilename=None):
        assert(line is None or isinstance(line, int))

        fname = inputFilename
        if not fname:
            fname = self.inputFilename

        lineStr = ''
        if line:
            lineStr = f'({line})'

        print(f'{fname}{lineStr}: warning: {text}')

    # getPreviousToken
    #
    # Returns previous token, which must exist. Assumes tokenization had already been done.
    #
    def getPreviousToken(self):
        assert(self.tokenized)
        assert(self.currentToken - 1 >= 0)

        return self.tokens[self.currentToken - 1]

    # getNextToken
    #
    # in:
    #     lookingFor - textual description of the expected token, can be null, in that case
    #                  generic error is shown in case EOF is encountered
    #
    # Returns the next token. Assumes the tokenization had already been done.
    # Sets atEof when end of file is reached.
    #
    def getNextToken(self, lookingFor=None):
        assert(lookingFor is None or isinstance(lookingFor, str))
        assert(self.tokenized)

        line = self.tokens[-1].line if len(self.tokens) else 1

        if not self.atEof and self.currentToken >= len(self.tokens):
            self.atEof = True
            return Token('end of file', self.inputFilename, line)

        if self.atEof:
            errorMessage = 'unexpected end of file'
            if lookingFor:
                errorMessage += f' while looking for {lookingFor}'
            self.error(line, errorMessage)

        token = self.tokens[self.currentToken]
        self.currentToken += 1

        return token

    # gotTokensLeft
    #
    # Returns true if there are any tokens left. Assumes the tokenization had already been done.
    #
    def gotTokensLeft(self):
        assert(self.tokenized)

        return self.currentToken < len(self.tokens)

    # expectBlockStart
    #
    # in:
    #     token - token to check
    #
    # Makes sure that the given token is a block start (opening brace). Displays an error and exits
    # the program if it isn't. Returns the token following the opening brace.
    # Assumes the tokenization had already been done.
    #
    def expectBlockStart(self, token):
        assert(isinstance(token, Token))
        assert(self.tokenized)

        if token.string != '{':
            self.error(token, f"block start `{{' expected, got `{token.string}'")

        return self.getNextToken()

    # expectColon
    #
    # in:
    #     token - token to test
    #
    # Makes sure that the given token is a colon. Returns the token following the colon.
    # Assumes the tokenization had already been done.
    #
    def expectColon(self, token):
        assert(isinstance(token, Token))
        assert(self.tokenized)

        if token.string != ':':
            error(token, f"expected colon `:', got `{token.string}'")

        return self.getNextToken()

    # expectIdentifier
    #
    # in:
    #     token        - Token to check
    #     expectedNext - optional string that describes next expected token
    #
    # Checks if the given token is a valid C++ identifier, if not displays an error and ends the program.
    # Assumes the tokenization had already been done.
    #
    def expectIdentifier(self, token, expectedNext=None):
        assert(isinstance(token, Token))
        assert(self.tokenized)

        if not isIdentifier(token.string):
            self.error(token, f"`{token.string}' is not a valid identifier")

        return self.getNextToken(expectedNext)

    # expect
    #
    # in:
    #     text  - expected string
    #     token - current token
    #
    # Makes sure that token contains the expected string. If not, complains and quits. Returns next token.
    # Assumes the tokenization had already been done.
    #
    def expect(self, text, token):
        assert(isinstance(text, str))
        assert(isinstance(token, Token))

        if token.string != text:
            self.error(token, f"expected `{text}', but got `{token.string}' instead")

        return self.getNextToken()

    # verifyNonCppKeyword
    #
    # in:
    #     token   - string that's being checked for C++ keyword
    #     context - optional context string, it is what this token represents (e.g. menu name, entry name...)
    #
    # Makes sure that the given token isn't a C++ keyword. If it is, displays an error and ends the program.
    # Assumes the tokenization had already been done.
    #
    def verifyNonCppKeyword(self, token, context=None):
        assert(isinstance(token, Token))
        assert(context is None or isinstance(context, str) and len(context))
        assert(self.tokenized)

        kCppKeywords = {
            'alignas', 'alignof', 'and', 'and_eq', 'asm', 'auto', 'bitand', 'bitor', 'bool', 'break', 'case',
            'catch', 'char', 'char8_t', 'char16_t', 'char32_t', 'class', 'compl', 'concept', 'const', 'consteval',
            'constexpr', 'const_cast', 'continue', 'decltype', 'default', 'delete', 'do', 'double', 'dynamic_cast',
            'else', 'enum', 'explicit', 'export', 'extern', 'false', 'float', 'for', 'friend', 'goto', 'if',
            'import', 'inline', 'int', 'long', 'module', 'mutable', 'namespace', 'new', 'noexcept', 'not', 'not_eq',
            'nullptr', 'operator', 'or', 'or_eq', 'private', 'protected', 'public', 'register', 'reinterpret_cast',
            'requires', 'return', 'short', 'signed', 'sizeof', 'static', 'static_assert', 'static_cast', 'struct',
            'switch', 'template', 'this', 'thread_local', 'throw', 'true', 'try', 'typedef', 'typeid', 'typename',
            'union', 'unsigned', 'using', 'virtual', 'void', 'volatile', 'wchar_t', 'while', 'xor', 'xor_eq'
        }

        if token.string in kCppKeywords:
            errorStr = f"can't use a C++ keyword `{token.string}'"

            if context:
                errorStr += f' as a'
                if context[0] in ('a', 'e', 'i', 'o', 'u'):
                    errorStr += 'n'
                errorStr += ' ' + context

            self.error(token, errorStr)

    # expandBuiltinVariable
    #
    # in:
    #     text - actual text of the built-in variable
    #     menu - menu in which context we're expanding (if any)
    #     line - line number, if given
    #
    # Expands built-in variable (variables that begin with '@') and returns expanded variable. Variable must be
    # valid or the execution stops.
    #
    def expandBuiltinVariable(self, text, menu=None, line=None):
        assert(isinstance(text, str))
        assert(menu is None or isinstance(menu, Menu))
        assert(len(text) and text[0] == '@')

        text = text[1:]

        result = kConstants.get(text, None)
        if result is not None:
            return str(result)

        if text == 'previousX' or text == 'prevX':
            result = menu.previousX
        elif text == 'previousY' or text == 'prevY':
            result = menu.previousY
        elif text == 'previousEndX' or text == 'prevEndX':
            result = f'({menu.previousX} + {menu.previousWidth})'
        elif text == 'previousEndY' or text == 'prevEndY':
            result = f'({menu.previousY} + {menu.previousHeight})'
        elif text == 'previousWidth' or text == 'prevWidth':
            result = menu.previousWidth
        elif text == 'previousHeight' or text == 'prevHeight':
            result = menu.previousHeight
        elif text == 'kScreenWidth':
            result = 320
        elif text == 'kScreenHeight':
            result = 200
        elif text == 'kAlloc':
            result = -1
        elif text == 'kTextWidth' or text == 'kTextHeight':
            result = '@' + text     # these are special case, and will be evaluated later
        else:
            self.warning(f"unknown internal variable `@{text}'", line)
            result = 0

        return f'({result})'

    # expandVariable
    #
    # in:
    #     textOrToken - token or string/int with the potential variable
    #     menu        - menu the potential variable belongs to (if any)
    #
    # Tests if the token contains a variable (be it built-in or user-defined), and if so expands it.
    # If not, simply returns it as is.
    #
    def expandVariable(self, textOrToken, menu=None):
        assert(isinstance(textOrToken, (Token, str, int)))
        assert(menu is None or isinstance(menu, Menu))

        text = textOrToken

        if isinstance(text, int):
            return str(text)

        if isinstance(text, Token):
            text = textOrToken.string

        # first lookup menu variables, then globals if not found
        varDicts = [ self.globals ]
        if menu is not None:
            varDicts.insert(0, menu.variables)

        if not text.startswith('@'):
            for varDict in varDicts:
                var = varDict.get(text, None)
                if var is not None:
                    text = var.value
                    var.referenced = True
                    break

        if text.startswith('@'):
            text = self.expandBuiltinVariable(text, menu, textOrToken.line if isinstance(textOrToken, Token) else 0)

        if text.lower() == 'true':
            text = '1'
        elif text.lower() == 'false':
            text = '0'

        return text

    # parseExpression
    #
    # in:
    #     token           - current token
    #     menu            - current menu (if any)
    #     expandVariables - if false do not expand variables, copy everything verbatim
    #     getTokenFunc    - function that retrieves tokens, will use getNextToken if not given
    #
    # Parses an expression starting with token and returns the result. Moves current token to the last one after the
    # expression. Returns result of the expression (as string) and the current token.
    #
    def parseExpression(self, token, menu=None, expandVariables=True, getTokenFunc=getNextToken):
        assert(isinstance(token, Token))
        assert(menu is None or isinstance(menu, Menu))
        assert(callable(getTokenFunc))

        result = ''

        kOperands = ('+', '-', '*', '/', '%', '&', '|', '^')

        isOperand = lambda token: token.string not in kOperands + ('(', ')')
        unmatchedParenthesis = lambda line: self.error(line, 'unmatched parenthesis')

        singleOperand = True

        while True:
            if token.string == '(':
                token = getTokenFunc(self)
                subResult, token = self.parseExpression(token, menu, expandVariables, getTokenFunc)

                if token.string != ')':
                    unmatchedParenthesis(token.line)

                token = getTokenFunc(self)

                if token.string == ')' and not self.gotTokensLeft():
                    unmatchedParenthesis(token.line)

                if subResult.endswith(' '):
                    subResult = subResult[:-1]

                result += '(' + subResult + ')'
                singleOperand = False
            elif token.string == ')':
                unmatchedParenthesis(token.line)
            elif token.string == '#':
                if getTokenFunc != Parser.getNextToken:
                    self.error(token, 'preprocessor directives unavailable in this context')
                token, value = self.parsePreprocessorDirective(self.getNextToken(), menu)
                if value is None:
                    self.error(token, f'void directive used in value expecting context')
                result = formatToken(result, str(value))
            else:
                if not isOperand(token):
                    error(token, f"operand expected, got `{token.string}'")

                # can't check it here, it's too restrictive, real solution is to check at output time
                # (but then we must save original tokens in order to properly report the error)
                #self.verifyNonCppKeyword(token)

                value = self.expandVariable(token, menu) if expandVariables else token.string
                token = getTokenFunc(self)

                if token.string == '.':
                    if menu is None:
                        self.error(token, "dot operator (`.') can only be used inside the menu")
                    if value not in menu.entries:
                        self.error(token, f"`{value}' is not recognized as an entry name")

                    entry = menu.entries[value]
                    token = getTokenFunc(self)

                    if token.string == 'endY':
                        value = str(entry.y) + '+' + str(entry.height)
                    else:
                        if not hasattr(entry, token.string):
                            self.error(token, f"entry `{value}' does not have property `{token.string}'")
                        value = getattr(entry, token.string)

                    token = getTokenFunc(self)

                result = formatToken(result, value)

            if isOperand(token) or token.string in (')', '}'):
                break

            result = formatToken(result, token.string)
            token = getTokenFunc(self)
            singleOperand = False

        if result.endswith(' '):
            result = result[:-1]

        if not singleOperand:
            result = f'({result})'

        return result, token

    # checkConflictingProperties
    #
    # in:
    #     entry    - entry to check
    #     property - new property that's being set
    #     line     - line number where property is defined
    #
    # Checks for conflicting properties inside the entry, such as both text and sprite, basically for any pair of
    # members belonging to any of two entry struct's unions.
    #
    def checkConflictingProperties(self, entry, property, line):
        assert(isinstance(entry, Entry))
        assert(isinstance(property, str))
        assert(isinstance(line, int))

        conflictingProperties = ('text', 'stringTable', 'number', 'sprite')
        if property in conflictingProperties:
            for prop in conflictingProperties:
                if prop != property and getattr(entry, prop) is not None:
                    self.error(line, f"can't use properties `{property}' and `{prop}' at the same time")

    # registerFunction
    #
    # in:
    #     token - token containing function name
    #
    # Checks if a function inside the given token is an identifier and if it requires forward declaration in C++ file.
    # If so, checks it for uniqueness, if it's redefined reports an error and exits.
    # Returns a pair: function name, token (which will be the next after the function).
    #
    def registerFunction(self, token):
        assert(isinstance(token, Token))

        self.verifyNonCppKeyword(token, 'name of function handler')

        declareFunction = True

        # if it's prefixed with tilde don't declare it (presumably it is declared elsewhere)
        if token.string == '~':
            token = self.getNextToken('function handler')
            declareFunction = False

        function = token.string
        if not isIdentifier(function):
            self.error(token, f"expected function handler, got `{function}'")

        if declareFunction:
            self.functions.add(function)

        return function, self.getNextToken()

    # parseStringTable
    #
    # in:
    #     token - initial token of string table block
    #
    # Parses string table block and returns a filled-in string table object and a token that follows it.
    #
    def parseStringTable(self, token):
        assert(isinstance(token, Token))

        self.verifyNonCppKeyword(token, 'string table name')

        if token.string == '~':
            token = self.getNextToken()
            stringTable = StringTable(token.string)
            token = self.expectIdentifier(token)
        else:
            token = self.expect('[', token)

            variable = token.string
            token = self.expectIdentifier(token, "comma (`,')")
            token = self.expect(',', token)

            initialValue = 0
            values = []

            try:
                initialValue = int(token.string, 0)

                token = self.getNextToken()
                token = self.expect(',', token)
            except ValueError:
                pass

            while True:
                if token.string[0] == "'":
                    token.string = '"' + token[1:-1] + '"'

                values.append(token.string)

                if not isIdentifier(token.string) and token.string[0] != '"':
                    self.error(token, f"expecting string or identifier, got `{token.string}'")

                token = self.getNextToken("`]' or `,'")

                if token.string == ']':
                    token = self.getNextToken()
                    break

                token = self.expect(',', token)

            if not len(values):
                self.error(token, 'string table without strings')

            stringTable = StringTable(variable, values, initialValue)

            self.stringTables.add(len(values))

        return stringTable, token

    # fillCharWidthTables
    #
    # Fills tables used for calculating string widths. Functions that do calculation later expect this to be filled.
    #
    def fillCharWidthTables(self):
        self.smallFontWidths = (
            0, 0, 0, 0, 0, 6, 0, 2, 3, 3, 6, 6, 2, 4, 2, 6, 6, 2, 6, 6, 6, 6, 6, 6, 6, 6, 2, 3,
            0, 0, 0, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6, 2, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
            6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 0, 6, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,
        )
        self.bigFontWidths = (
            0, 0, 0, 0, 0, 8, 0, 2, 4, 4, 8, 8, 3, 5, 3, 8, 8, 5, 8, 8, 8, 8, 8, 8, 8, 8, 3, 4,
            0, 0, 0, 8, 0, 8, 8, 8, 8, 8, 8, 8, 8, 5, 8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 7, 8, 8, 8,
            7, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8,
        )

    # textWidth
    #
    # in:
    #     text       - text to measure
    #     useBigFont - flag whether to use big or small font to measure
    #
    # Returns the width of the given text in pixels. Expects width tables to be filled.
    #
    def textWidth(self, text, useBigFont):
        assert(isinstance(text, str))

        spaceWidth = 4 if useBigFont else 3
        invalidCharWidth = 8 if useBigFont else 6
        table = self.bigFontWidths if useBigFont else self.smallFontWidths

        width = 0

        for c in text:
            c = ord(c) - 32
            if c == 0:
                width += spaceWidth
            elif c >= 0 and c < len(table):
                charWidth = table[c]
                if not charWidth:
                    charWidth = invalidCharWidth
                width += charWidth
            else:
                width += invalidCharWidth

        return width

    # textHeight
    #
    # in:
    #     useBigFont - flag whether to use big or small font to measure
    #
    # Returns the height of the text in pixels.
    #
    def textHeight(self, useBigFont):
        return 8 if useBigFont else 6

    # expandTextWidthAndHeight
    #
    # in:
    #     entry      - freshly parsed entry
    #     properties - all the properties that were set from the entry
    #     token      - last token of the entry, for error reporting purposes
    #
    # Expands @kTextWidth and @kTextHeight height constants. We can only reliably do that after the entire
    # entry has been parsed, and only then can we also reliably detect errors.
    #
    def expandTextWidthAndHeight(self, entry, properties, token):
        assert(isinstance(entry, Entry))
        assert(isinstance(properties, list))
        assert(isinstance(token, Token))

        w = '@kTextWidth'
        h = '@kTextHeight'

        usedConstants = False
        for prop in properties:
            val = getattr(entry, prop)
            if isinstance(val, str) and (w in val or h in val):
                usedConstants = True
                break

        if not usedConstants:
            return

        if entry.text is None and entry.stringTable is None:
            self.error(token, f'non-text entry uses {w}/{h} constants')

        try:
            useBigFont = int(removeEnclosingParentheses(entry.textFlags), 0) & kConstants['kBigFont']
        except ValueError:
            self.error(token, 'could not calculate text flags value')

        if entry.text is not None:
            strings = [entry.text]
        else:
            strings = entry.stringTable.values

        wVal = 0
        for string in strings:
            string = string[1:-1]
            wVal = max(wVal, self.textWidth(string, useBigFont))

        hVal = self.textHeight(useBigFont)

        for prop in properties:
            val = getattr(entry, prop)
            if isinstance(val, str):
                val = val.replace(f'({w})', f'({wVal})')
                val = val.replace(f'({h})', f'({hVal})')
                setattr(entry, prop, val)

    # parseEntry
    #
    # in:
    #     menu       - parent Menu instance
    #     isTemplate - if true this is a template entry
    #
    # Parses a menu entry and adds it to the parent menu. Ends the program in case of any error.
    #
    def parseEntry(self, menu, isTemplate):
        assert(isinstance(menu, Menu))
        assert(isinstance(isTemplate, bool))

        token = self.getNextToken()
        name = ''

        if token.string == '#':
            token, name = self.parsePreprocessorDirective(self.getNextToken(), menu)
            nameToken = self.getPreviousToken()
        elif token.string != '{':
            name = token.string
            nameToken = token
            token = self.getNextToken()

        if name:
            if not isIdentifier(name):
                self.error(nameToken, f"`{name}' is not a valid entry name")

            if name in menu.entries:
                self.error(nameToken, f"entry `{name}' already declared")

            self.verifyNonCppKeyword(nameToken, 'entry name')

        token = self.expectBlockStart(token)

        if isTemplate:
            entry = Entry()
            entry.template = True
            entry.width = entry.height = 1              # to avoid assert
            entry.ordinal = 30_000 + len(menu.entries)  # just a random big number to avoid clashes with non-templates
            internalName = f'{len(menu.entries):02}'
        else:
            entry = copy.deepcopy(menu.templateEntry)

            for property in ('x', 'y', 'width', 'height', 'color', 'textFlags'):
                if property != 'color' or not menu.gotTemplate:
                    value = str(menu.properties['default' + property[0].upper() + property[1:]])
                    tokens = tokenize(value) + ['']
                    tokenFeeder = lambda self: Token(tokens.pop(0))
                    expandedValue, _ = self.parseExpression(Token(tokens.pop(0)), menu, True, tokenFeeder)
                    setattr(entry, property, expandedValue)

            internalName = name
            if not name:
                internalName = str(len(menu.entries))

            entry.name = name
            entry.ordinal = 0

            for currentEntry in reversed(list(menu.entries.values())):
                if isinstance(currentEntry, Entry) and not currentEntry.template:
                    entry.ordinal = currentEntry.ordinal + 1
                    break

        menu.entries[internalName] = entry

        if not isTemplate:
            menu.lastEntry = entry

        properties = []

        while token.string != '}':
            property = token.string
            if property == 'topEntry':  # I've made this mistake so many times that I've made it into an alias
                property = 'upEntry'
            elif property == 'bottomEntry':
                property = 'downEntry'

            propertyToken = token
            properties.append(property)

            self.checkConflictingProperties(entry, property, token.line)

            if not hasattr(entry, property):
                self.error(token, f"unknown property `{property}'")

            if property == 'name' or property == 'ordinal':
                self.error(token, f"property `{property}' can not be assigned")

            token = self.expectIdentifier(token, "colon (`:')")
            token = self.expectColon(token)

            if property in kEntryFunctions:
                result, token = self.registerFunction(token)
            elif property == 'stringTable':
                entry.stringTable, token = self.parseStringTable(token)
                continue
            else:
                result, token = self.parseExpression(token, menu)

            if result == '}':
                self.error(propertyToken, f"missing property value for `{property}'")

            setattr(entry, property, result)

            if property in kNextEntryProperties:
                setattr(entry, property + 'Token', propertyToken)

        self.expandTextWidthAndHeight(entry, properties, token)

        if not isTemplate:
            menu.previousX = entry.x
            menu.previousY = entry.y
            menu.previousWidth = entry.width
            menu.previousHeight = entry.height

        return self.getNextToken()

    # getNumericValue
    #
    # in:
    #     text - a string that should contain numeric value
    #     line - line number in the input file where the string comes from
    #
    # Extracts a numeric value from the string. If the value could not be converted, reports an error and exits.
    #
    def getNumericValue(self, text, line):
        assert(isinstance(text, str))
        assert(isinstance(line, int))

        try:
            text = removeEnclosingParentheses(text)
            value = int(text, 0)
        except ValueError:
            self.error(line, f"`{text}' is not a numeric value'")

        return value

    # getBoolValue
    #
    # in:
    #     token - input token to get bool value from
    #
    # Extracts a boolean value from the given token and returns it
    #
    def getBoolValue(self, token):
        assert(isinstance(token, Token))

        text = token.string.lower()
        if text == 'true':
            value = True
        elif text == 'false':
            value = False
        else:
            num = self.getNumericValue(text, token.line)
            value = num != 0

        return value

    # parseMenu
    #
    # Parses menu and converts it into internal structure, to be ready for output. On any error will complain and quit.
    # Assumes tokenization has already been done.
    #
    def parseMenu(self):
        assert(self.tokenized)

        token = self.getNextToken('menu name')

        if token.string == '{':
            self.error(token, 'menu name missing')

        menuName = token.string
        menuLine = token.line

        if menuName in self.menus:
            self.error(token, f"menu `{menuName}' redeclared")

        self.verifyNonCppKeyword(token, 'menu name')

        token = self.expectIdentifier(token, "`{'")
        token = self.expectBlockStart(token)

        menu = Menu()
        self.menus[menuName] = menu
        menu.name = menuName

        kMenuProperties = kMenuFunctions + (kInitialEntry, 'x', 'y') + kMenuDefaults

        seenProperties = ()

        while token.string != '}':
            if token.string == 'Entry':
                token = self.parseEntry(menu, False)
            elif token.string == 'TemplateEntry':
                token = self.parseEntry(menu, True)
                menu.gotTemplate = True
            elif token.string == 'ResetTemplate':
                menu.templateEntry = Entry()
                menu.gotTemplate = False
            elif token.string == '#':
                token, _ = self.parsePreprocessorDirective(self.getNextToken(), menu)
                continue
            else:
                propertyToken = token
                property = token.string
                isFunction = property in kMenuFunctions

                exported = False
                if property == 'export':
                    exported = True
                    token = self.getNextToken()
                    property = token.string

                    self.verifyNonCppKeyword(token, 'exported variable name')

                token = self.expectIdentifier(token)

                entryName = ''
                if token.string == '.':
                    entryNameToken = token
                    entryName = property
                    token = self.getNextToken()

                    dotProperty = token.string
                    token = self.expectIdentifier(token)

                if token.string == '=':
                    if not entryName and property in menu.variables:
                        self.error(token, f"variable `{property}' redefined")

                    token = self.getNextToken()  # get rid of `='

                    value, token = self.parseExpression(token, menu)

                    if entryName:
                        self.entryDotReferences.append((menu, entryName, dotProperty, value, entryNameToken))
                    else:
                        if exported:
                            menu.exportedProperties[property] = (value, propertyToken)

                        menu.variables[property] = Variable(value, propertyToken)
                    continue

                if property not in kMenuProperties:
                    self.error(token, f"unknown menu property: `{property}'")

                token = self.expectColon(token)

                if token.string == '}':
                    self.error(token, "expecting property value, not end of block `}'")

                expressionLine = token.line
                if isFunction:
                    value, token = self.registerFunction(token)
                else:
                    delayedExpansionProperty = property in kMenuDefaults
                    value, token = self.parseExpression(token, menu, not delayedExpansionProperty)

                if property == 'x' or property == 'y':
                    value = self.getNumericValue(value, expressionLine)

                if property == 'defWidth':
                    property = 'defaultWidth'
                elif property == 'defHeight':
                    property = 'defaultHeight'
                elif property == 'defX':
                    property = 'defaultX'
                elif property == 'defY':
                    property = 'defaultY'
                elif property == 'defTextFlags':
                    property = 'defaultTextFlags'

                if property == kInitialEntry:
                    menu.initialEntryToken = propertyToken

                menu.properties[property] = value

        if kInitialEntry not in menu.properties:
            menu.properties[kInitialEntry] = 0
            menu.initialEntryToken = None

        return token

    # parseInclude
    #
    # in:
    #     token - initial token of include directive (filename to include)
    #
    # Parses include directive, opens the file and scoops all the tokens.
    #
    def parseInclude(self, token):
        assert(isinstance(token, Token))

        filename = token.string
        if filename[0] != '"' or filename[-1] != '"':
            self.error(token, f"expecting quoted file name, got `{filename}'")

        filename = filename[1:-1]
        path = os.path.join(os.path.dirname(self.inputPath), filename)

        if not os.path.isfile(path):
            path = os.path.abspath(filename)

        includeTokens = self.tokenize(path, token)
        self.tokens[self.currentToken:self.currentToken] = includeTokens

        return self.getNextToken(), None

    # parseTextWidthAndHeight
    #
    # in:
    #     token        - initial token of text width or height directive (must be opening brace)
    #     textOptional - flag determining if we can skip text
    #
    # Does the common part of the text width and height directives parsing, extracts the text
    # and font size and returns them for further processing.
    # Returns token, text, font size.
    #
    def parseTextWidthAndHeight(self, token, textOptional=False):
        assert(isinstance(token, Token))

        token = self.expect('(', token)
        text = ''
        fontSize = False

        if isString(token.string):
            text = token.string[1:-1]
            token = self.getNextToken("comma `,' or closing parentheses `)'")
            if token.string == ',':
                token = self.getNextToken()
                fontSize = token.string.lower()
                if fontSize == 'big':
                    fontSize = True
                elif fontSize == 'small':
                    fontSize = False
                else:
                    fontSize = self.getBoolValue(token)
        elif not textOptional:
            self.error(token, f"expected string, got `{text}'")

        token = self.expect(')', token)
        return token, text, fontSize

    # parseTextWidth
    #
    # in:
    #     token - initial token of "textWidth" directive (must be opening brace)
    #
    # Parses text width directive and returns width of the text in pixels.
    #
    def parseTextWidth(self, token):
        assert(isinstance(token, Token))

        token, text, useBigFont = self.parseTextWidthAndHeight(token, True)
        width = self.textWidth(text, useBigFont)
        return token, width

    # parseTextHeight
    #
    # in:
    #     token - initial token of "textHeight" directive (must be opening brace)
    #
    # Parses text height directive and returns height of the text in pixels.
    #
    def parseTextHeight(self, token):
        assert(isinstance(token, Token))

        token, _, useBigFont = self.parseTextWidthAndHeight(token)
        height = self.textHeight(useBigFont)
        return token, height

    # parseRepeatDirective
    #
    # in:
    #     token - initial token of "repeat" directive (must be repeat count)
    #     menu  - parent menu, if any
    #
    # Scans for "#repeatEnd" marker and repeats all tokens in between by the given number of times.
    # Optional parameter is a name of loop variable, which will default to `i' if not given.
    #
    def parseRepeatDirective(self, token, menu):
        repeatStart = self.currentToken - 3

        repeatCount = self.expandVariable(token, menu)
        repeatCount = self.getNumericValue(repeatCount, token.line)

        token = self.getNextToken()

        loopVariable = ''

        if token.string == ',':
            token = self.getNextToken()
            loopVariableToken = token
            loopVariable = token.string

            token = self.expectIdentifier(token)
            if loopVariable in kPreprocReservedWords:
                self.error(loopVariableToken, f"`{loopVariable}' is a reserved preprocessor word")

        if not loopVariable:
            loopVariable = 'i'

        self.preprocVariables[loopVariable] += 1
        repeatEnd = self.currentToken - 1
        references = []

        del self.tokens[repeatStart:repeatEnd]
        self.currentToken = repeatStart + 1
        start = self.currentToken - 1
        token = self.tokens[start]

        while True:
            if token.string == '#':
                token = self.getNextToken()

                varName = token.string
                varToken = token

                token = self.expectIdentifier(token)

                if varName == 'repeat':
                    token, _ = self.parseRepeatDirective(token, menu)
                    continue
                elif varName == 'endRepeat':
                    end = self.currentToken - 3
                    break
                elif varName == loopVariable:
                    references.append(self.currentToken - start - 3)
                    del self.tokens[self.currentToken - 2]
                    self.currentToken -= 1

                if varName not in kPreprocReservedWords and self.preprocVariables.get(varName, 0) == 0:
                    self.error(varToken, f"unknown preprocessor variable `{varName}'")

            token = self.getNextToken()

        repeatBlock = self.tokens[start:end]
        del self.tokens[repeatStart:end + 2]

        for i in range(0, repeatCount):
            for index in references:
                repeatBlock[index].string = str(i)

            insertIndex = start + i * len(repeatBlock)
            self.tokens[insertIndex:insertIndex] = copy.deepcopy(repeatBlock)

        self.preprocVariables[loopVariable] -= 1

        self.currentToken = start + 1
        return self.tokens[start], None

    # parseJoinDirective
    #
    # in:
    #     token - must be initial opening braces
    #
    # Creates a single token by concatenating a comma separated list of tokens. Whole construct is replaced by the
    # resulting token.
    #
    def parseJoinDirective(self, token):
        assert(isinstance(token, Token))

        start = self.currentToken - 3
        token = self.expect('(', token)
        joinedString = ''

        while True:
            joinedString += token.string
            token = self.getNextToken()

            if (token.string == ')'):
                token = self.getNextToken()
                break

            token = self.expect(',', token)

        end = self.currentToken - 2
        del self.tokens[start:end]

        self.currentToken = start + 2
        self.tokens[start].string = joinedString

        return self.tokens[start + 1], joinedString

    # parsePrintDirective
    #
    # in:
    #     token - initial token of the expression to print
    #     menu  - parent menu, if any
    #
    # Simply prints an expression that follows the directive. Useful for debugging.
    #
    def parsePrintDirective(self, token, menu):
        assert(isinstance(token, Token))

        line = token.line
        filename = token.inputFilename

        value, token = self.parseExpression(token, menu)

        print(f"{filename}({line}): `{value}'")
        return token, None

    # parsePreprocessorDirective
    #
    # in:
    #     token - token containing directive
    #     menu  - current menu we're in, if any
    #
    # After the initial character of preprocessor directive has been encountered, recognizes which
    # directive it was and processes it.
    #
    def parsePreprocessorDirective(self, token, menu=None):
        assert(isinstance(token, Token))

        directive = token.string
        token = self.expectIdentifier(token)

        if directive == 'include':
            return self.parseInclude(token)
        elif directive == 'textWidth':
            return self.parseTextWidth(token)
        elif directive == 'textHeight':
            return self.parseTextHeight(token)
        elif directive == 'repeat':
            return self.parseRepeatDirective(token, menu)
        elif directive == 'join':
            return self.parseJoinDirective(token)
        elif directive == 'print':
            return self.parsePrintDirective(token, menu)
        else:
            self.error(token, f"unknown preprocessor directive `{directive}'")

    # tokenize
    #
    # in:
    #     inputPath - path to the file to tokenize
    #     token     - if given, token of the include directive
    #
    # Opens a file whose path is given, tokenizes it and returns produced tokens in an array.
    #
    def tokenize(self, inputPath, token=None):
        assert(inputPath and isinstance(inputPath, str))
        assert(not self.parsed)

        lineNo = 1
        tokens = []
        inputFilename = os.path.basename(inputPath)

        try:
            with open(inputPath, 'r') as inputFile:
                for line in inputFile.readlines():
                    if not line.endswith('\n'):
                        self.warning('new line missing', lineNo, inputFilename)

                    prev = ''
                    for string in tokenize(line, lineNo):
                        tokens.append(Token(string, inputPath, lineNo))

                        if len(string) and isString(string):
                            if len(string) < 2 or string[-1] != string[0]:
                                self.error(tokens[-1], 'unterminated string')
                            if prev != 'include' and string != string.upper():
                                self.warning('non upper case string detected', lineNo, inputFilename)

                        prev = string

                    lineNo += 1

        except (FileNotFoundError, OSError):
            errorMessage = f'File {inputPath} not found.'
            if token:
                self.error(token, errorMessage[0].lower() + errorMessage[1:])
            else:
                sys.exit(errorMessage)

        return tokens

    # parse
    #
    # Parses the entire input file. Expects that the tokenization has been done.
    #
    def parse(self):
        token = self.getNextToken()

        while not self.atEof:
            if token.string == 'Menu':
                token = self.parseMenu()
            elif token.string == '}':
                self.error(token, 'unmatched block end')
            elif token.string == 'Entry':
                self.error(token, 'entries can only be used inside menus')
            else:
                varToken = token
                var = token.string

                if var == '#':
                    token, _ = self.parsePreprocessorDirective(self.getNextToken())
                    continue

                token = self.expectIdentifier(token, "equal sign (`=')")

                if token.string != '=':
                    self.error(token, f"expected equal `=', got `{token.string}'")

                token = self.getNextToken()

                value, token = self.parseExpression(token)

                self.globals[var] = Variable(value, varToken)
                continue

            token = self.getNextToken()

        self.parsed = True

    # checkForUnusedVariables
    #
    # Checks if there are unused variables and issues warnings if so.
    #
    def checkForUnusedVariables(self):
        for varName, var in self.globals.items():
            if not var.referenced and var.token.path == self.inputPath:
                self.warning(f"unused global variable `{varName}'", var.token.line, var.token.inputFilename)

        for menuName, menu in self.menus.items():
            for varName, var in menu.variables.items():
                if not var.referenced and var.token.path == self.inputPath:
                    self.warning(f"unused variable `{varName}' declared in menu `{menuName}'",
                        var.token.line, var.token.inputFilename)

    # processInputFile
    #
    # Reads in the input file, does the tokenization, and then invokes the parsing.
    #
    def processInputFile(self):
        assert(not self.tokenized)

        self.tokens = self.tokenize(self.inputPath)
        self.tokenized = True
        self.parse()
        self.checkForUnusedVariables()

    # getEntryStructs
    #
    # in:
    #     menuName - name of parent menu
    #     entry    - menu entry to process
    #
    # Compares the given entry element values to default values and returns a list of SWOS menu codes
    # (in the form of C++ code strings) of all the differences.
    #
    def getEntryStructs(self, menuName, entry):
        assert(isinstance(menuName, str))
        assert(isinstance(entry, Entry))

        result = []
        ord = f'{entry.ordinal:02}'

        if entry.text:
            text = str(entry.text).strip()
            if text == '-1' or text == '(-1)':
                text = 'reinterpret_cast<const char *>(-1)'
            result.append(f'EntryText et{ord}{{ {entry.textFlags}, {text} }};')

        if entry.stringTable:
            result.append(f'EntryStringTable est{ord}{{ {entry.textFlags}, ')
            if not len(entry.stringTable.values):
                result[-1] += f'reinterpret_cast<StringTable *>(&{entry.stringTable.variable}) }};'
            else:
                name = getStringTableName(menuName, entry)
                result[-1] += f'&{name} }};'

        if entry.number is not None:
            result.append(f'EntryNumber en{ord}{{ {entry.textFlags}, {entry.number} }};')

        if entry.sprite is not None:
            result.append(f'EntryForegroundSprite efs{ord}{{ {entry.sprite} }};')

        if entry.color:
            result.append(f'EntryColor ec{ord}{{ {entry.color} }};')

        if entry.invisible:
            result.append(f'EntryInvisible ei{ord}{{}};')

        if entry.leftEntry != -1 or entry.rightEntry != -1 or entry.upEntry != -1 or entry.downEntry != -1:
            result.append(f'EntryNextPositions ep{ord}{{ {entry.leftEntry}, {entry.rightEntry}, {entry.upEntry}, {entry.downEntry} }};')

        for direction in ('Left', 'Right', 'Up', 'Down'):
            newDirection = getattr(entry, 'direction' + direction)
            skip = getattr(entry, 'skip' + direction)

            if newDirection != -1 or skip != -1:
                if newDirection == -1:
                    newDirection = kConstants['k' + direction]

                result.append(f'Entry{direction}Skip e{direction[0]}s{ord}{{ {newDirection}, {skip} }};')

        if entry.controlsMask:
            result.append(f'EntryOnSelectFunctionWithMask eosfm{ord}{{ {entry.onSelect}, {entry.controlsMask} }};')
        elif entry.onSelect:
            result.append(f'EntryOnSelectFunction eosf{ord}{{ {entry.onSelect} }};')

        if entry.beforeDraw:
            result.append(f'EntryBeforeDrawFunction ebdf{ord}{{ {entry.beforeDraw} }};')

        if entry.afterDraw:
            result.append(f'EntryAfterDrawFunction eadf{ord}{{ {entry.afterDraw} }};')

        return result

    # resolveEntryReferences
    #
    # in:
    #     text     - string possibly containing entry references that need to be resolved
    #     menu     - parent menu
    #     refToken - token that's referencing the entry (if we have a reference)
    #     entry    - parent entry, if applicable
    #
    # Returns the text with entry references replaced. Displays an error and ends the program
    # if an undefined reference is encountered.
    #
    def resolveEntryReferences(self, text, menu, refToken, entry=None):
        assert(isinstance(text, str))
        assert(isinstance(menu, Menu))
        assert(refToken is None or isinstance(refToken, Token))
        assert(not entry or isinstance(entry, Entry))

        result = ''

        for token in tokenize(text):
            if entry:
                if token == '>':
                    if menu.lastEntry == entry:
                        self.error(refToken, f'referencing entry beyond the last one')
                    result = formatToken(result, str(entry.ordinal + 1))
                    continue
                elif token == '<':
                    if entry.ordinal == 0:
                        self.error(refToken, f'referencing entry before the first one')
                    result = formatToken(result, str(entry.ordinal - 1))
                    continue

            if isIdentifier(token):
                entry = menu.entries.get(token, None)
                if entry is None:
                    self.error(refToken, f"undefined entry reference: `{token}'")
                result = formatToken(result, str(entry.ordinal))
            else:
                result = formatToken(result, token)

        if result.startswith('('):
            result = result.strip()[1:-1]

        return int(result, 0)

    # resolveEntryDotReferences
    #
    # Resolves all entry.property references.
    #
    def resolveEntryDotReferences(self):
        for menu, entryName, property, value, token in self.entryDotReferences:
            assert(isinstance(value, str))

            if entryName not in menu.entries:
                self.error(token, f"menu `{menu.name}' doesn't contain entry `{entryName}'")

            entry = menu.entries[entryName]
            if not hasattr(entry, property):
                self.error(token, f"entry `{entryName}' doesn't contain property `{property}'")

            if property in kNextEntryProperties:
                if value != '-1' and value not in menu.entries:
                    self.error(token, f"entry `{value}' undefined")
                if value != '-1':
                    value = menu.entries[value].ordinal
                if value == '-1':
                    value = -1

            setattr(entry, property, value)

    # resolveReferences
    #
    # Resolves all forward references to entries in all of the menus.
    #
    def resolveReferences(self):
        for menu in self.menus.values():
            menu.properties[kInitialEntry] = self.resolveEntryReferences(str(menu.properties[kInitialEntry]), menu, menu.initialEntryToken)

            for entry in menu.entries.values():
                for property in kNextEntryProperties:
                    token = getattr(entry, property + 'Token')
                    oldValue = str(getattr(entry, property))
                    newValue = self.resolveEntryReferences(oldValue, menu, token, entry)
                    setattr(entry, property, newValue)

        self.resolveEntryDotReferences()

    # outputFunctionForwardDeclarations
    #
    # in:
    #     out - output function
    #
    # Outputs forward declarations of handler functions.
    #
    def outputFunctionForwardDeclarations(self, out):
        assert(callable(out))
        assert(self.parsed)

        for func in self.functions:
            out(f'static void {func}();')

        if len(self.functions):
            out('')

    # outputStringTableDeclarations
    #
    # in:
    #     out - output function
    #
    # Outputs C++ code for all the string tables gathered.
    #
    def outputStringTableDeclarations(self, out):
        assert(callable(out))
        assert(self.parsed)

        for numStrings in self.stringTables:
            out(f'struct StringTable{numStrings} : public StringTable {{')
            out(f'    const char *strings[{numStrings}];')
            out(f'    StringTable{numStrings}(int16_t *index, int16_t initialValue, '
                f'{", ".join(map(lambda i: f"const char *str{i}", range(0, numStrings)))}) :\n        StringTable(index, initialValue), '
                f'strings{{{", ".join(map(lambda i: f"str{i}", range(0, numStrings)))}}} {{}}')
            out('};')

        if len(self.stringTables):
            out('')

            for menuName, menu in self.menus.items():
                for entry in menu.entries.values():
                    if entry.stringTable:
                        st = entry.stringTable
                        numStrings = len(st.values)
                        if numStrings:
                            name = getStringTableName(menuName, entry)
                            output = f'extern int16_t {st.variable};\n'
                            output += f'StringTable{numStrings} {name} {{\n    &{st.variable}, {st.initialValue}, '

                            for value in st.values:
                                output += f'{value}, '

                            output = output[:-1] + '\n};'

                            out(output)

            out('')

    # outputMenu
    #
    # in:
    #     menuName - name of the menu (doesn't come prepackaged :))
    #     menu     - menu itself
    #     out      - output function
    #
    # Outputs C++ code for the parsed menu.
    #
    def outputMenu(self, menuName, menu, out):
        assert(isinstance(menuName, str))
        assert(isinstance(menu, Menu))
        assert(callable(out))
        assert(self.parsed)

        for func in kMenuFunctions:
            if not func in menu.properties:
                menu.properties[func] = 'nullptr'

        out(f'struct SWOS_Menu_{menuName} : public BaseMenu\n{{')
        out(f'    MenuHeader header{{ {menu.properties[kOnInit]}, {menu.properties[kAfterDraw]}, '
            f'{menu.properties[kOnDraw]}, {menu.properties[kInitialEntry]} }};')

        templateIndex = 0
        resetTemplateIndex = 0

        if menu.properties['x'] != 0 or menu.properties['y'] != 0:
            out(f"\n    MenuXY menuXY{{{menu.properties['x']}, {menu.properties['y']}}};")

        for entry in menu.entries.values():
            if entry is ResetTemplateEntry:
                out(f'\n    ResetTemplateEntry rte{resetTemplateIndex:02}{{}};')
                resetTemplateIndex += 1
                continue
            elif entry.template:
                out(f'\n    TemplateEntry te{templateIndex:02}{{}};')
                templateIndex += 1

            out(f'\n    Entry eb{entry.ordinal:02}{{ {entry.x}, {entry.y}, {entry.width}, {entry.height} }};')

            for entryStruct in self.getEntryStructs(menuName, entry):
                out('        ', entryStruct)

            out(f'    EntryEnd ee{entry.ordinal:02}{{}};')

        out('\n    MenuEnd menuEnd{};')
        out(f'}} static const {menuName};\n')

        # allow C++ to use all the symbolic entry names through a namespaced enum
        namespace = menuName[0].upper() + menuName[1:]
        if namespace == menuName:
            namespace += 'NS';

        out(f'namespace {namespace} {{')
        out('    enum Entries {')

        exportedOrdinals = False
        for entry in menu.entries.values():
            if not entry.template and entry.name:
                out(f'        {entry.name} = {entry.ordinal},')
                exportedOrdinals = True

        out('    };')

        if len(menu.exportedProperties) and exportedOrdinals:
            out()

        for property, valueToken in menu.exportedProperties.items():
            value, token = valueToken
            entry = next((entry for entry in menu.entries.values() if entry.name == property), None)

            if entry:
                self.error(token, f"can't export constant `{property}' since there is an entry with same name")

            out(f'    constexpr int {property} = {value};')

        out('}')


    # output
    #
    # in:
    #     outputPath - path of the output file
    #
    # Writes all the digested input to the specified output file.
    #
    def output(self):
        assert(self.parsed)

        self.resolveReferences()

        try:
            with open(self.outputPath, 'w') as f:
                out = lambda *args, **kwargs: print(*args, **kwargs, file=f)

                out(f'// automatically generated from {self.inputFilename}, do not edit')
                out('#pragma once\n')
                out('#include "menu.h"')
                out('#include "swossym.h"\n')
                out('using namespace SWOS_Menu;\n')
                out('#pragma pack(push, 1)\n')

                self.outputFunctionForwardDeclarations(out)
                self.outputStringTableDeclarations(out)

                if not len(self.menus):
                    self.warning('no menus found')

                for menuName, menu in self.menus.items():
                    self.outputMenu(menuName, menu, out)

                out('#pragma pack(pop)')

        except FileNotFoundError:
            sys.exit(f"Couldn't open {path} for writing")


# tokenize
#
# in:
#     line   - a line to process (string)
#     lineNo - number of the line in input file
#
# Returns a list of tokens present in the given line. For example:
#
# 'All mimsy were "the borogoves", {And \'the mome\' raths}; outgrabe' =>
# ['All', 'mimsy', 'were', '"the borogoves"', ',', '{', 'And', "'the mome'", 'raths', '}', ';', 'outgrabe']
#
def tokenize(line, lineNo=0):
    assert(isinstance(line, str))
    assert(isinstance(lineNo, int))

    line = line.split(';', 1)[0]
    line = line.split('//', 1)[0]
    line = line.strip()
    tokens = re.findall(r'(?:@)?[\w]+|[{},():]|"[^"]+(?:"|$)|\'[^\']+(?:\'|$)|[-+]*\d+|[^\s\w]', line)

    return tokens


# isIdentifier
#
# in:
#     string - a string to check
#
# Returns true if the given string looks like a valid C++ identifier.
#
def isIdentifier(string):
    assert(isinstance(string, str))

    return re.match('[a-zA-Z_][\w]*', string)


# isString
#
# in:
#     string - a string to check`
#
# Returns true if the given string looks like a quoted string.
#
def isString(string):
    assert(string and isinstance(string, str))

    return string[0] == '"' and string[-1] == '"' or string[0] == "'" and string[-1] == "'"


# removeEnclosingParentheses
#
# in:
#    string - a string to remove enclosing parentheses from
#
# Returns a string with balanced, enclosing parentheses removed.
#
def removeEnclosingParentheses(string):
    while string[0] == '(' and string[-1] == ')':
        string = string[1:-1]

    return string


# formatToken
#
# in:
#     result - string that's being assembled
#     text   - text of the token that's to be added to result
#
# Concatenates text to result making sure the whitespace is inserted properly, and returns it.
#
def formatToken(result, text):
    assert(isinstance(result, str))
    assert(isinstance(text, str))

    if text == ')' and result.endswith(' '):
        result = result[:-1]

    result += text

    if text != '(':
        result += ' '

    return result


# getStringTableName
#
# in:
#     menuName - name of parent menu
#     entry    - entry for which we're deciding string table name
#
# Forms and returns a unique string table name for the entry.
#
def getStringTableName(menuName, entry):
    assert(isinstance(menuName, str))
    assert(isinstance(entry, Entry))

    name = menuName + '_' + entry.name

    if not entry.name:
        name += f'{entry.ordinal:02}'

    return name + '_stringTable'


# getInputAndOutputFile
#
# Fetches input and output file parameters from the command line and returns them as a list.
# If they're not present, complains and exits the program.
#
def getInputAndOutputFile():
    if len(sys.argv) < 3:
        sys.exit('Missing output file path')

    if len(sys.argv) < 2:
        sys.exit('Missing input file path')

    return sys.argv[1], sys.argv[2]


def main():
    if sys.version_info < (3, 6):
        sys.exit('Python 3.6 required (dictionaries that maintain insertion order)')

    inputPath, outputPath = getInputAndOutputFile()
    parser = Parser(inputPath, outputPath)

    parser.processInputFile()
    parser.output()


if __name__ == '__main__':
    main()
