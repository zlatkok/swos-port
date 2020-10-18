import os
import inspect

import Util
import Constants

from Token import Token
from Menu import Menu
from Entry import Entry
from VariableStorage import VariableStorage
from PreprocExpression import PreprocExpression
from StringTable import StringTable
from Preprocessor import Preprocessor
from Tokenizer import Tokenizer

# Contains everything a growing parser needs
class Parser:
    def __init__(self, inputPath):
        assert isinstance(inputPath, str)

        self.inputPath = inputPath
        self.inputFilename = os.path.basename(inputPath)

        self.stringTableLengths = set() # set of lengths all string tables used in the program (number of their strings)
        self.entryDotReferences = []
        self.menus = {}

        self.functions = set()      # this will contain names of all the functions declared
        self.entryRefDefLines = {}  # line numbers in input file where entry reference was used

        self.varStorage = VariableStorage()
        self.tokenizer = Tokenizer.fromFile(inputPath)
        self.defaultPropertyTokenizer = Tokenizer.empty()
        self.preprocessor = Preprocessor(self.tokenizer, inputPath, self.varStorage, self.parseExpression)

        self.parse()
        self.checkForUnusedVariables()
        self.checkForConflictingExportedMenuVariables()
        self.checkForZeroWidthHeightEntries()
        self.resolveReferences()

    def getFunctions(self):
        return self.functions

    def getStringTableLengths(self):
        return self.stringTableLengths

    def getMenus(self):
        return self.menus

    def showWarnings(self):
        return self.preprocessor.showWarnings()

    def parse(self):
        while not self.tokenizer.isAtEof():
            token = self.tokenizer.getNextToken()

            if token.string == '#':
                self.preprocessor.parsePreprocessorDirective()
            elif token.string == 'Menu':
                self.parseMenu()
            elif token.string == '}':
                self.error('unmatched block end', token)
            elif token.string == 'Entry':
                self.error('entries can only be used inside menus', token)
            else:
                self.parseAssignment(token)

    def parseMenu(self):
        menu = self.parseMenuHeader()
        self.parseMenuBody(menu)

    def parseMenuHeader(self):
        token = self.tokenizer.getNextToken('menu name')

        if token.string == '{':
            self.error('menu name missing', token)

        menuName = token.string

        if menuName in self.menus:
            self.error(f"menu `{menuName}' redeclared", token)

        self.verifyNonCppKeyword(token, 'menu name')

        token = self.tokenizer.expectIdentifier(token, "`{'", fetchNextToken=True)
        self.tokenizer.expectBlockStart(token)

        menu = Menu()
        self.menus[menuName] = menu
        menu.name = menuName

        return menu

    def parseMenuBody(self, menu):
        assert isinstance(menu, Menu)

        while True:
            token = self.tokenizer.getNextToken()
            if token.string == '}':
                break

            if token.string == '#':
                self.preprocessor.parsePreprocessorDirective(menu)
            elif token.string == 'Entry':
                self.parseEntry(menu, False)
            elif token.string == 'TemplateEntry':
                self.parseEntry(menu, True)
            elif token.string == 'ResetTemplate':
                self.handleResetTemplateEntry(menu)
            else:
                self.parseMenuVariablesAndProperties(menu, token)

        if Constants.kInitialEntry not in menu.properties:
            menu.properties[Constants.kInitialEntry] = 0
            menu.initialEntryToken = None

        self.verifyMenuHeaderType(menu)

    @staticmethod
    def verifyMenuHeaderType(menu):
        assert isinstance(menu, Menu)

        gotSwosFunc = False
        for func in filter(lambda func: func in menu.properties, Constants.kMenuFunctions):
            if menu.properties[func].startswith('$'):
                gotSwosFunc = True
            elif gotSwosFunc:
                Util.error(f"menu `{menu.name}' contains mixed type functions", menu.propertyTokens[func])

    def handleResetTemplateEntry(self, menu):
        assert isinstance(menu, Menu)

        menu.templateEntry = Entry()
        menu.templateActive = False

        entry, internalName = menu.createNewTemplateEntry(True)
        menu.entries[internalName] = entry

        nextToken = self.tokenizer.peekNextToken()
        if nextToken and nextToken.string == '{':
            self.tokenizer.getNextToken()
            token = self.tokenizer.getNextToken()
            self.tokenizer.expect('}', token, fetchNextToken=False)

    # We've detected a non-entry entity in the menu and it can be property or variable assignment.
    # Variables will be declared in menu scope.
    def parseMenuVariablesAndProperties(self, menu, token):
        assert isinstance(menu, Menu)
        assert isinstance(token, Token)

        idToken, exported = self.getMenuAssignmentTarget(token)
        token = self.tokenizer.expectIdentifier(idToken, fetchNextToken=False)
        token, entryNameToken, dotProperty = self.handleMenuDotVariable(idToken, token)

        token = self.tokenizer.getNextToken("equal sign (`=') or colon (`:')")
        if token.string not in '=:':
            self.error(f"expecting menu variable assignment or property, got `{token.string}'", token)

        isVariableAssignment = token.string == '='
        isDotVariable = entryNameToken is not None

        if exported and (not isVariableAssignment or isDotVariable):
            self.error("Menu properties can't be exported", idToken)

        if isVariableAssignment:
            self.parseMenuVariableAssignment(menu, idToken, entryNameToken, dotProperty, exported)
        else:
            self.parseMenuPropertyAssignment(menu, idToken)

    def getMenuAssignmentTarget(self, token):
        assert isinstance(token, Token)

        exported = False

        if token.string == 'export':
            exported = True
            token = self.tokenizer.getNextToken()
            self.verifyNonCppKeyword(token, 'exported variable name')

        return token, exported

    # Detects if the assignment target (given by idToken) is a "dot variable" in the form of `entry.field'.
    # Only one dot property is supported.
    def handleMenuDotVariable(self, idToken, token):
        assert isinstance(idToken, Token)
        assert isinstance(token, Token)

        entryNameToken = None
        dotProperty = ''

        nextToken = self.tokenizer.peekNextToken()
        if nextToken and nextToken.string == '.':
            entryNameToken = idToken

            self.tokenizer.getNextToken()
            token = self.tokenizer.getNextToken()

            dotProperty = token.string
            token = self.tokenizer.expectIdentifier(token, fetchNextToken=False)

        return token, entryNameToken, dotProperty

    def parseMenuVariableAssignment(self, menu, idToken, entryNameToken, dotProperty, exported):
        assert isinstance(menu, Menu)
        assert isinstance(idToken, Token)
        assert isinstance(entryNameToken, (Token, type(None)))
        assert isinstance(dotProperty, str)

        varName = idToken.string
        isDotVariable = entryNameToken is not None

        if not isDotVariable and self.varStorage.hasMenuVariable(menu, varName):
            self.error(f"variable `{varName}' redefined", idToken)

        value = self.parseExpression(menu)

        if isDotVariable:
            self.entryDotReferences.append((menu, entryNameToken.string, dotProperty, value, entryNameToken))
        else:
            var = self.varStorage.addMenuVariable(menu, varName, value, idToken)
            if exported:
                menu.exportedVariables[varName] = (value, idToken)
                var.referenced = True   # treat exported variables as referenced to suppress unused warning

    def parseMenuPropertyAssignment(self, menu, propertyToken):
        assert isinstance(menu, Menu)
        assert isinstance(propertyToken, Token)

        property = propertyToken.string

        if property not in Constants.kMenuProperties:
            self.error(f"unknown menu property: `{property}'", propertyToken)

        token = self.tokenizer.peekNextToken()
        if not token or token.string == '}':
            self.error(f"missing property value for `{property}'", propertyToken)

        isFunction = property in Constants.kMenuFunctions

        if isFunction:
            value = self.registerFunction()
        else:
            expressionStartToken = token

            delayedExpansionProperty = property in Constants.kMenuDefaults
            value = self.parseExpression(menu, not delayedExpansionProperty)

            if property in ('x', 'y'):
                value = Util.getNumericValue(value, expressionStartToken)

        property = self.expandMenuPropertyAliases(property)

        if property == Constants.kInitialEntry:
            menu.initialEntryToken = propertyToken

        menu.properties[property] = value
        menu.propertyTokens[property] = propertyToken

        self.handleInitialXYMenuProperties(menu, property, value)

    @staticmethod
    def expandMenuPropertyAliases(property):
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

        return property

    @staticmethod
    def handleInitialXYMenuProperties(menu, property, value):
        if property in ('x', 'y') and not menu.entries:
            setattr(menu, 'initial' + property.upper(), value)

    def parseAssignment(self, token):
        assert isinstance(token, Token)

        varToken = token
        varName = token.string

        if token.string == ')':
            self.error("unexpected `)'", token)

        token = self.tokenizer.expectIdentifier(token, "equal sign (`=')", fetchNextToken=True)

        if token.string != '=':
            self.error(f"expected equal `=', got `{token.string}'", token)

        value = self.parseExpression()

        self.varStorage.addGlobalVariable(varName, value, varToken)

    # parseEntry
    #
    # in:
    #     menu       - parent Menu instance (required)
    #     isTemplate - true if this is a template entry
    #
    # Parses a menu entry and adds it to the parent menu. Ends the program in case of any error.
    #
    def parseEntry(self, menu, isTemplate):
        assert isinstance(menu, Menu)
        assert isinstance(isTemplate, bool)

        token, name, nameToken = self.getEntryName(menu)
        self.verifyEntryName(menu, name, nameToken)

        token = self.tokenizer.expectBlockStart(token, fetchNextToken=False)

        entry = menu.addEntry(name, isTemplate)
        entry.token = nameToken or token
        if not isTemplate:
            self.assignDefaultEntryValues(menu, entry)

        propertiesSet = self.parseEntryBody(entry, menu)

        if not isTemplate:
            self.tryRemovingTemplateEntryType(menu, entry, propertiesSet)

        self.expandTextWidthAndHeight(entry, menu, propertiesSet, token)
        self.updateMenuPreviousFields(menu, entry, isTemplate)

    # If the user has explicitly specified entry type, remove any potential type that comes from the template entry.
    def tryRemovingTemplateEntryType(self, menu, entry, propertiesSet):
        assert isinstance(entry, Entry)
        assert isinstance(menu, Menu)
        assert isinstance(propertiesSet, set)

        if menu.templateActive:
            userSetTypes = propertiesSet.intersection(Constants.kEntryTypeProperties)
            if userSetTypes:
                for property in userSetTypes.symmetric_difference(Constants.kEntryTypeProperties):
                    setattr(entry, property, None)

    def parseEntryBody(self, entry, menu):
        assert isinstance(entry, Entry)
        assert isinstance(menu, Menu)

        propertiesSet = set()

        while True:
            token = self.tokenizer.getNextToken('entry property')
            if token.string == '}':
                break

            property = self.expandEntryPropertyAliases(token)

            if property == '#':
                self.preprocessor.parsePreprocessorDirective(menu)
                continue

            propertyToken = token
            propertiesSet.add(property)

            self.verifyEntryProperty(entry, property, token, propertiesSet)

            token = self.tokenizer.expectIdentifier(token, "colon (`:')", fetchNextToken=True)
            self.tokenizer.expectColon(token, fetchNextToken=False)

            result = self.getEntryPropertyValue(property, menu)

            if result == '}':
                self.error(f"missing property value for `{property}'", propertyToken)

            setattr(entry, property, result)

            if property in Constants.kNextEntryProperties:
                setattr(entry, property + 'Token', propertyToken)

        return propertiesSet

    @staticmethod
    def updateMenuPreviousFields(menu, entry, isTemplate):
        assert isinstance(menu, Menu)
        assert isinstance(entry, Entry)

        if not isTemplate:
            menu.previousX = entry.x
            menu.previousY = entry.y
            menu.previousWidth = entry.width
            menu.previousHeight = entry.height
            menu.previousOrdinal = entry.ordinal

    @staticmethod
    def expandEntryPropertyAliases(token):
        assert isinstance(token, Token)

        property = token.string
        if property == 'topEntry':  # I've made this mistake so many times that I've made it into an alias
            property = 'upEntry'
        elif property == 'bottomEntry':
            property = 'downEntry'

        return property

    def verifyEntryProperty(self, entry, property, token, propertiesSet):
        assert isinstance(entry, Entry)
        assert isinstance(property, str)
        assert isinstance(token, Token)
        assert isinstance(propertiesSet, set)

        if property == 'Entry':
            self.error('entries can not be nested', token)
        elif not hasattr(entry, property):
            self.error(f"unknown property `{property}'", token)

        if property in Constants.kImmutableEntryProperties:
            self.error(f"property `{property}' can not be assigned", token)

        self.checkConflictingProperties(entry, property, propertiesSet, token)

    def getEntryPropertyValue(self, property, menu):
        assert isinstance(property, str)
        assert isinstance(menu, Menu)

        if property in Constants.kEntryFunctions:
            result = self.registerFunction()
        elif property == 'stringTable':
            result = self.parseStringTable()
        else:
            result = self.parseExpression(menu)

        return result

    def getEntryName(self, menu):
        assert isinstance(menu, Menu)

        name = ''
        nameToken = None

        token = self.tokenizer.getNextToken()

        if token.string == '#':
            name = self.preprocessor.parsePreprocessorDirective(menu)
            name = str(name)
            nameToken = self.tokenizer.getNextToken()
            token = self.tokenizer.getNextToken()
        elif token.string != '{':
            name = token.string
            nameToken = token
            token = self.tokenizer.getNextToken()

        return token, name, nameToken

    def verifyEntryName(self, menu, name, nameToken):
        assert isinstance(menu, Menu)
        assert isinstance(name, str)
        assert isinstance(nameToken, (Token, type(None)))

        if name:
            if not name.isidentifier():
                self.error(f"`{name}' is not a valid entry name", nameToken)

            if name in menu.entries:
                self.error(f"entry `{name}' already declared", nameToken)

            self.verifyNonCppKeyword(nameToken, 'entry name')

    def hasActivePropertyFromTemplate(self, menu, entry, property):
        assert isinstance(menu, Menu)
        assert isinstance(entry, Entry)
        assert isinstance(property, str)

        return property in ('width', 'height') and menu.templateActive and \
            getattr(menu.templateEntry, property, 0) != 0

    # assignDefaultEntryValues
    #
    # in:
    #     menu  - parent Menu instance
    #     entry - new entry we're creating
    #
    # Assigns default values for a freshly encountered entry, right before its body will be parsed.
    #
    def assignDefaultEntryValues(self, menu, entry):
        assert isinstance(menu, Menu)
        assert isinstance(entry, Entry)

        entry.menuX = menu.properties['x']
        entry.menuY = menu.properties['y']

        hasActivePropertyFromTemplate = lambda property: \
            property in ('width', 'height') and menu.templateActive and getattr(menu.templateEntry, property, 0) != 0

        isPropertyDefaultAssignable = lambda property: \
            not entry.isTemplate() and property != 'ordinal' and (property != 'color' or not menu.templateActive) and \
            not hasActivePropertyFromTemplate(property)

        for property in Constants.kPreviousEntryFields:
            if isPropertyDefaultAssignable(property):
                defaultProperty = 'default' + property[0].upper() + property[1:]
                value = str(menu.properties[defaultProperty])
                assert value

                if defaultProperty in menu.propertyTokens:
                    token = menu.propertyTokens[defaultProperty]
                else:
                    token = self.tokenizer.getPreviousToken()

                self.defaultPropertyTokenizer.resetToString(value, token)
                expandedValue = self.parseExpression(menu, True, self.defaultPropertyTokenizer)

                setattr(entry, property, expandedValue)

    # error
    #
    # in:
    #     text        - text of the error to display
    #     tokenOrline - offending token, or the line number where error occurred (can be omitted)
    #
    # Prints formatted error to stdout, with input file path and offending line, and exits the program.
    #
    def error(self, text, tokenOrLine=None):
        assert isinstance(text, str)
        assert isinstance(tokenOrLine, (Token, int, type(None)))

        if isinstance(tokenOrLine, Token):
            Util.error(text, tokenOrLine)
        else:
            file = self.inputFilename
            line = tokenOrLine
            Util.error(text, file, line)

    # verifyNonCppKeyword
    #
    # in:
    #     token   - string that's being checked for C++ keyword
    #     context - optional context string, it is what this token represents (e.g. menu name, entry name...)
    #
    # Makes sure that the given token isn't a C++ keyword. If it is, displays an error and ends the program.
    #
    def verifyNonCppKeyword(self, token, context=None):
        assert isinstance(token, Token)
        assert context is None or isinstance(context, str) and context

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
                context = Util.withArticle(context)
                errorStr += f' as {context}'

            self.error(errorStr, token)

    # parseExpression
    #
    # in:
    #     menu            - current menu (if any)
    #     expandVariables - if false do not expand variables, copy everything verbatim
    #     tokenizer       - custom tokenizer to use (if not given will use standard one from the instance)
    #
    # Parses an expression and returns the result (as string). This isn't a real expression evaluator,
    # it will just make sure that the result is in the form C++ compiler can evaluate.
    #
    def parseExpression(self, menu=None, expandVariables=True, tokenizer=None):
        assert isinstance(menu, (Menu, type(None)))
        assert isinstance(tokenizer, (Tokenizer, type(None)))

        tokenizer = tokenizer or self.tokenizer

        result = ''
        singleOperand = True

        kOperators = '+-*/%&|^()'
        isOperand = lambda token: token is None or token.string not in kOperators

        if not tokenizer.gotTokensLeft():
            tokenizer.getNextToken('expression start')

        while True:
            token = tokenizer.getNextToken('expression operand')
            if token.string == '(':
                result = self.handleParenSubexpression(menu, expandVariables, tokenizer)
                singleOperand = False
            elif token.string == ')':
                self.error('unmatched parenthesis', token)
            elif token.string == '#':
                value = self.preprocessor.parsePreprocessorDirective(menu, onlyNonVoid=True)
                assert value is not None
                result = Util.formatToken(result, str(value))
            else:
                result = self.handleOperand(token, menu, result, isOperand, expandVariables, tokenizer)

            token = tokenizer.peekNextToken()
            if not token or isOperand(token) or token.string in (')', '}'):
                break

            tokenizer.getNextToken()
            result = Util.formatToken(result, token.string)
            singleOperand = False

        if result.endswith(' '):
            result = result[:-1]

        if not singleOperand and (result[0] != '(' or result[-1] != ')'):
            result = f'({result})'

        return result

    def handleParenSubexpression(self, menu, expandVariables, tokenizer):
        assert isinstance(menu, (Menu, type(None)))
        assert isinstance(tokenizer, Tokenizer)

        subResult = self.parseExpression(menu, expandVariables, tokenizer)

        token = tokenizer.getNextToken("closing parenthesis `)'")
        if token.string != ')':
            self.error('unmatched parenthesis', token)

        if subResult.endswith(' '):
            subResult = subResult[:-1]

        result = '(' + subResult + ')'

        return result

    def handleDotOperand(self, token, menu):
        assert isinstance(token, Token)
        assert isinstance(menu, (Menu, type(None)))

        value = token.string

        if value == 'swos':
            token = self.getSwosVariable(token, skipDot=False)
            return token.string

        if menu is None:
            self.error("dot operator (`.') can only be used inside menus", token)
        if value not in menu.entries:
            self.error(f"`{value}' is not recognized as an entry name", token)

        entry = menu.entries[value]
        token = self.tokenizer.getNextToken('menu property')

        if token.string == 'endY':
            value = f'({str(entry.y)} + {str(entry.height)})'
        else:
            if not hasattr(entry, token.string):
                self.error(f"entry `{value}' does not have property `{token.string}'", token)
            value = getattr(entry, token.string)

        return str(value)

    def getSwosVariable(self, token, skipDot=True):
        assert isinstance(token, Token)
        assert token.string == 'swos'

        if skipDot:
            token = self.tokenizer.getNextToken('.')
            token = self.tokenizer.expect('.', token)

        token = self.tokenizer.getNextToken('SWOS symbol')
        token.string = '$' + token.string
        return token

    def handleOperand(self, token, menu, result, isOperand, expandVariables, tokenizer):
        assert isinstance(token, Token)
        assert isinstance(menu, (Menu, type(None)))
        assert isinstance(result, str)
        assert callable(isOperand)
        assert isinstance(tokenizer, Tokenizer)

        unaryOp = None
        if token.string in ('+', '-', '~', '!'):
            unaryOp = token.string
            token = tokenizer.getNextToken()

        if not isOperand(token):
            self.error(f"operand expected, got `{token.string}'", token)

        expandIfNeeded = lambda value: self.varStorage.expandVariable(value, token, menu) if expandVariables else value

        value = expandIfNeeded(token.string)
        isString = Util.isString(value)

        nextToken = tokenizer.peekNextToken()
        if nextToken:
            if nextToken.string == '.':
                tokenizer.getNextToken()
                if isString:
                    while True:
                        value = self.handleStringFunction(value, token, tokenizer)
                        nextToken = tokenizer.peekNextToken()
                        if nextToken and nextToken.string == '.':
                            tokenizer.getNextToken()
                        else:
                            break
                else:
                    value = self.handleDotOperand(token, menu)
                    value = expandIfNeeded(value)
            elif nextToken.string == '[':
                tokenizer.getNextToken()
                value = self.handleStringIndexing(value, token, tokenizer)

        result = Util.formatToken(result, value)

        if unaryOp:
            # add parentheses to prevent double minus from turning into increment
            result = '(' + unaryOp + result.rstrip() + ')'

        return result

    def handleStringFunction(self, value, token, tokenizer):
        assert isinstance(value, (int, str))
        assert not isinstance(value, str) or Util.isString(value)
        assert isinstance(token, Token)
        assert isinstance(tokenizer, Tokenizer)

        token = tokenizer.getNextToken('string function')
        params = self.getFunctionParameters(tokenizer)

        def verifyNumParams(expected):
            assert isinstance(expected, int)
            assert isinstance(params, list)

            actual = len(params)
            if actual != expected:
                self.error(f'invalid number of parameters for {token.string}(), {expected} expected, got {actual}', token)

        if not hasattr(self, 'kStringFunctions'):
            self.kStringFunctions = {
                'first'      : lambda v: v[0] if v else '',
                'last'       : lambda v: v[-1] if v else '',
                'middle'     : lambda v: v[len(v) // 2] if v else '',
                'upper'      : lambda v: v.upper(),
                'lower'      : lambda v: v.lower(),
                'capitalize' : lambda v: v.capitalize(),
                'title'      : lambda v: v.title(),
                'zfill'      : lambda v, n: v.zfill(n),
                'left'       : lambda v, n: v[:n],
                'right'      : lambda v, n: v[-n:],
                'mid'        : lambda v, m, n: v[m:n],
            }

        handler = self.kStringFunctions.get(token.string)

        if not handler:
            self.error(f"`{token.string}' is not a known string function", token)

        expectedNumParams = len(inspect.signature(handler).parameters) - 1
        verifyNumParams(expectedNumParams)

        quote = value[0]
        value = Util.unquotedString(value)

        result = handler(value, *params)
        return f'{quote}{result}{quote}'

    def handleStringIndexing(self, value, token, tokenizer):
        assert isinstance(value, str) and Util.isString(value)
        assert isinstance(token, Token)
        assert isinstance(tokenizer, Tokenizer)

        token = tokenizer.getNextToken('string index value')
        index = Util.getNumericValue(token.string, token)

        token = tokenizer.getNextToken("`]'")
        tokenizer.expect(']', token, fetchNextToken=False)

        quote = value[0]
        value = Util.unquotedString(value)

        try:
            c = value[index]
        except IndexError:
            self.error(f"index {index} out of bounds for string `{value}'", token)

        return f'{quote}{c}{quote}'

    def getFunctionParameters(self, tokenizer):
        assert isinstance(tokenizer, Tokenizer)

        token = tokenizer.getNextToken("function parameter list start `('")
        token = tokenizer.expect('(', token, fetchNextToken=True)

        reportMissingParameterError = lambda: Util.error('function parameter missing', token)
        params = []

        while token.string != ')':
            if token.string == ',':
                reportMissingParameterError()

            if token.string == '#':
                value = self.preprocessor.parsePreprocessorDirective(menu=None, onlyNonVoid=True)
                assert value is not None
            else:
                value = token.string

            try:
                params.append(int(value))
            except ValueError:
                self.error(f"function parameter `{token.string}' is not numeric", token)

            token = self.tokenizer.getNextToken("`)' or `,'")
            if token.string == ',':
                token = self.tokenizer.getNextToken()
                if token.string == ')':
                    reportMissingParameterError()
            elif token.string != ')':
                Util.error(f"`{token.string}' unexpected, only `,' or `)' allowed", token)

        return params

    # checkConflictingProperties
    #
    # in:
    #     entry             - entry to check
    #     property          - new property that's being set
    #     userSetProperties - set of properties explicitly set by the user
    #     token             - token which defines property
    #
    # Checks for conflicting properties inside the entry, such as both text and sprite, basically for any pair of
    # members belonging to any of two entry struct's unions.
    #
    def checkConflictingProperties(self, entry, property, userSetProperties, token):
        assert isinstance(entry, Entry)
        assert isinstance(property, str)
        assert isinstance(userSetProperties, set)
        assert isinstance(token, Token)

        if property in Constants.kEntryTypeProperties:
            for property2 in Constants.kEntryTypeProperties:
                if property2 != property and getattr(entry, property2) is not None and property2 in userSetProperties:
                    self.error(f"can't use properties `{property}' and `{property2}' at the same time", token.line)

    # registerFunction
    #
    # Checks if a function inside the given token is an identifier and if it requires forward declaration in C++ file.
    # If so, checks it for uniqueness, if it's redefined reports an error and exits. Returns the function's name.
    #
    def registerFunction(self):
        kNameOfHAndleFunction = 'name of handler function'
        token = self.tokenizer.getNextToken(kNameOfHAndleFunction)
        self.verifyNonCppKeyword(token, kNameOfHAndleFunction)

        functionName, declareFunction, isRawFunction = self.handlePredeclaredFunction(token)

        if not isRawFunction and not functionName.startswith('$') and not functionName.isidentifier():
            self.error(f"expected handler function, got `{functionName}'", token)

        if declareFunction:
            self.functions.add(functionName)

        return functionName

    # If the function is prefixed with "swos." or tilde don't declare it (presumably it is declared elsewhere).
    def handlePredeclaredFunction(self, token):
        assert isinstance(token, Token)

        functionName = token.string
        declareFunction = True
        isRawFunction = False

        if token.string == '~' or token.string == 'swos':
            if token.string == 'swos':
                token = self.getSwosVariable(token)
            else:
                token = self.tokenizer.getNextToken('handler function')

            functionName = token.string
            declareFunction = False

            # send a quoted function as is
            if Util.isString(functionName):
                functionName = functionName[1:-1]
                isRawFunction = True

        return functionName, declareFunction, isRawFunction

    # parseStringTable
    #
    # Parses string table block and returns a filled-in string table object and a token that follows it.
    #
    def parseStringTable(self):
        token = self.tokenizer.getNextToken()
        self.verifyNonCppKeyword(token, 'string table name')

        if token.string == '~' or token.string == 'swos':
            if token.string == '~':
                token = self.tokenizer.getNextToken()
            else:
                token = self.getSwosVariable(token)

            stringTableVar = token.string
            native = True

            if stringTableVar.startswith('$'):
                native = False
                stringTableVar = stringTableVar[1:]

            stringTable = StringTable(stringTableVar, native)
            token = self.tokenizer.expectIdentifier(token, fetchNextToken=False)
        else:
            token = self.tokenizer.expect('[', token, fetchNextToken=True)

            token, variable, declareIndexVariable, externIndexVariable = self.getStringTableIndexVariable(token)
            token, initialValue = self.getStringTableInitialStringIndex(token)
            values = self.getStringTableStrings(token)
            nativeFlags = self.getNativeFlags(variable, values)

            removePrefix = lambda val: val[1:] if val.startswith('$') else val
            values = tuple(map(removePrefix, values))
            variable = removePrefix(variable)

            assert values and nativeFlags
            stringTable = StringTable(variable, True, declareIndexVariable, externIndexVariable, values, initialValue, nativeFlags)

            self.stringTableLengths.add(len(values))

        return stringTable

    def getStringTableIndexVariable(self, token):
        assert isinstance(token, Token)

        extern = False
        declare = True
        variable = token.string

        if variable in ('~', 'extern'):
            if variable == '~':
                declare = False
            else:
                extern = True
            token = self.tokenizer.getNextToken()
            variable = token.string
        elif variable == 'swos':
            token = self.getSwosVariable(token)
            variable = token.string
            declare = False

        token = self.tokenizer.expectIdentifier(token, "comma (`,')", fetchNextToken=True)
        token = self.tokenizer.expect(',', token, fetchNextToken=True)

        return token, variable, declare, extern

    def getStringTableInitialStringIndex(self, token):
        assert isinstance(token, Token)

        initialValue = 0

        try:
            initialValue = int(token.string, 0)

            token = self.tokenizer.getNextToken()
            token = self.tokenizer.expect(',', token, fetchNextToken=True)
        except ValueError:
            pass

        return token, initialValue

    def getStringTableStrings(self, token):
        assert isinstance(token, Token)

        values = []

        while True:
            string = token.string
            idString = string

            if string[0] == "'":
                string = '"' + string[1:-1] + '"'
            elif string == 'swos':
                token = self.getSwosVariable(token)
                string = token.string
                idString = string[1:]

            values.append(string)

            if not idString.isidentifier() and not Util.isString(string):
                self.error(f"expecting string or identifier, got `{token.string}'", token)

            token = self.tokenizer.getNextToken("`]' or `,'")
            if token.string == ']':
                break

            token = self.tokenizer.expect(',', token, fetchNextToken=True)

        return values

    def getNativeFlags(self, variable, values):
        assert isinstance(variable, str)
        assert isinstance(values, list)

        return tuple(map(lambda var: not var.startswith('$'), [variable] + values))

    # expandTextWidthAndHeight
    #
    # in:
    #     entry         - freshly parsed entry
    #     menu          - parent menu
    #     propertiesSet - all the properties that were set from the entry
    #     token         - last token of the entry, for error reporting purposes
    #
    # Expands @kTextWidth and @kTextHeight height constants. We can only reliably do that after the entire
    # entry has been parsed, and only then can we also reliably detect errors.
    #
    def expandTextWidthAndHeight(self, entry, menu, propertiesSet, token):
        assert isinstance(entry, Entry)
        assert isinstance(menu, Menu)
        assert isinstance(propertiesSet, set)
        assert isinstance(token, Token)

        w = '@kTextWidth'
        h = '@kTextHeight'

        if not self.hasUsedTextWidthHeightConstants(entry, w, h, propertiesSet):
            return

        textFlags = self.evaluateTextFlags(menu, entry, token)
        useBigFont = textFlags & Constants.kConstants['kBigFont']
        strings = self.getEntryStrings(entry, w, h, token)
        wVal, hVal = self.measureStrings(strings, useBigFont, token)
        self.expandTextWidthAndHeightConstants(entry, propertiesSet, w, h, wVal, hVal)

        for prop in propertiesSet:
            val = getattr(entry, prop)
            if isinstance(val, str):
                val = val.replace(f'({w})', f'({wVal})')
                val = val.replace(f'({h})', f'({hVal})')
                setattr(entry, prop, val)

    @staticmethod
    def hasUsedTextWidthHeightConstants(entry, w, h, propertiesSet):
        assert isinstance(entry, Entry)
        assert isinstance(w, str)
        assert isinstance(h, str)
        assert isinstance(propertiesSet, set)

        for prop in propertiesSet:
            val = getattr(entry, prop)
            if isinstance(val, str) and (w in val or h in val):
                return True

        return False

    def evaluateTextFlags(self, menu, entry, token):
        assert isinstance(menu, Menu)
        assert isinstance(entry, Entry)
        assert isinstance(token, Token)

        textFlags = Util.removeEnclosingParentheses(entry.textFlags)
        try:
            textFlags = int(textFlags, 0)
        except ValueError:
            self.defaultPropertyTokenizer.resetToString(textFlags, token)
            preprocExpression = PreprocExpression(self.defaultPropertyTokenizer, self.varStorage)
            nodes = preprocExpression.parse(menu)
            textFlags, _, _ = preprocExpression.evaluate(nodes)

        return textFlags

    def getEntryStrings(self, entry, w, h, token):
        assert isinstance(entry, Entry)
        assert isinstance(w, str)
        assert isinstance(h, str)
        assert isinstance(token, Token)

        if entry.text is None and entry.stringTable is None:
            self.error(f'non-text entry uses {w}/{h} constants', token)

        if entry.text is not None:
            strings = [entry.text]
        else:
            strings = entry.stringTable.values

        return strings

    def measureStrings(self, strings, useBigFont, token):
        assert isinstance(strings, (list, tuple))
        assert isinstance(token, Token)

        wVal = 0
        for string in strings:
            if not Util.isString(string):
                self.error(f"`{string}' is not a string, can't measure", token)
            string = Util.unquotedString(string)
            wVal = max(wVal, self.preprocessor.textWidth(string, useBigFont))

        hVal = self.preprocessor.textHeight(useBigFont)

        return wVal, hVal

    @staticmethod
    def expandTextWidthAndHeightConstants(entry, propertiesSet, w, h, wVal, hVal):
        assert isinstance(entry, Entry)
        assert isinstance(propertiesSet, set)
        assert isinstance(w, str)
        assert isinstance(h, str)
        assert isinstance(wVal, int)
        assert isinstance(hVal, int)

        for prop in propertiesSet:
            val = getattr(entry, prop)
            if isinstance(val, str):
                val = val.replace(f'({w})', f'({wVal})')
                val = val.replace(f'({h})', f'({hVal})')
                setattr(entry, prop, val)

    # checkForUnusedVariables
    #
    # Checks if there are unused variables and issues warnings if so.
    #
    def checkForUnusedVariables(self):
        if self.preprocessor.showWarnings():
            for varName, varToken in self.varStorage.getUnreferencedGlobalsInFile(self.inputPath):
                Util.warning(f"unused global variable `{varName}'", varToken)

            for menuName, varName, varToken in self.varStorage.getUnreferencedMenuVariablesInFile(self.inputPath):
                Util.warning(f"unused variable `{varName}' declared in menu `{menuName}'", varToken)

    def checkForConflictingExportedMenuVariables(self):
        for menu in self.menus.values():
            for property, (_, token) in menu.exportedVariables.items():
                entry = next((entry for entry in menu.entries.values() if entry.name == property), None)
                if entry:
                    self.error(f"can't export constant `{property}' since there is an entry with same name", token)

    def checkForZeroWidthHeightEntries(self):
        if self.preprocessor.disallowZeroWidthHeightEntries():
            for menu in self.menus.values():
                for entry in menu.entries.values():
                    if type(entry) == Entry and not entry.isTemplate():
                        noWidth = not entry.width or entry.width == '0'
                        noHeight = not entry.height or entry.height == '0'

                        if noWidth or noHeight:
                            name = '' if not entry.name else f" `{entry.name}'"
                            errorMessage = f"entry{name} has zero "
                            if noWidth:
                                errorMessage += 'width' if not noHeight else 'width and height'
                            else:
                                errorMessage += 'height'
                            self.error(errorMessage, entry.token)

    # resolveEntryReferences
    #
    # in:
    #     text     - string possibly containing entry references that need to be resolved
    #     menu     - parent menu
    #     refToken - token that's referencing the entry (if we have a reference)
    #     entry    - parent entry, if applicable
    #
    # Returns the given text with all the entry references replaced. Displays an error and ends the program
    # if an undefined reference is encountered.
    #
    def resolveEntryReferences(self, text, menu, refToken, entry=None):
        assert isinstance(text, str)
        assert isinstance(menu, Menu)
        assert refToken is None or isinstance(refToken, Token)
        assert not entry or isinstance(entry, Entry)

        result = ''

        for tokenString in Tokenizer.splitLine(text):
            if entry:
                if tokenString == '>':
                    if menu.lastEntry == entry:
                        self.error(f'referencing entry beyond the last one', refToken)
                    result = Util.formatToken(result, str(entry.ordinal + 1))
                    continue
                elif tokenString == '<':
                    if entry.ordinal == 0:
                        self.error(f'referencing entry before the first one', refToken)
                    result = Util.formatToken(result, str(entry.ordinal - 1))
                    continue

            if tokenString.isidentifier():
                entry = menu.entries.get(tokenString)
                if entry is None:
                    # try parsing it as an expression before giving up
                    self.defaultPropertyTokenizer.resetToString(text, refToken)
                    expandedValue = self.parseExpression(menu, True, self.defaultPropertyTokenizer)

                    entry = menu.entries.get(expandedValue)
                    if entry is None:
                        self.error(f"undefined entry reference: `{expandedValue}'", refToken)
                result = Util.formatToken(result, str(entry.ordinal))
            else:
                result = Util.formatToken(result, tokenString)

        if result.startswith('('):
            result = result.strip()[1:-1]

        try:
            return int(result, 0)
        except ValueError:
            try:
                return int(eval(result))
            except:
                return result

    # resolveEntryDotReferences
    #
    # Resolves all entry.property references.
    #
    def resolveEntryDotReferences(self):
        for menu, entryName, property, value, token in self.entryDotReferences:
            assert isinstance(value, str)

            # this shouldn't happen
            if entryName not in menu.entries:
                self.error(f"menu `{menu.name}' does not have entry `{entryName}'", token)

            value = value.strip('()')

            entry = menu.entries[entryName]
            if not hasattr(entry, property):
                self.error(f"entry `{entryName}' does not have property `{property}'", token)

            if property in Constants.kNextEntryProperties:
                if value != '-1' and value not in menu.entries:
                    self.error(f"entry `{value}' undefined", token)

                value = -1 if value == '-1' else menu.entries[value].ordinal

            setattr(entry, property, value)

    # resolveReferences
    #
    # Resolves all forward references to entries in all of the menus.
    #
    def resolveReferences(self):
        for menu in self.menus.values():
            initialEntry = str(menu.properties[Constants.kInitialEntry])
            menu.properties[Constants.kInitialEntry] = self.resolveEntryReferences(initialEntry, menu, menu.initialEntryToken)

            for entry in menu.entries.values():
                for property in Constants.kNextEntryProperties:
                    token = getattr(entry, property + 'Token')
                    oldValue = str(getattr(entry, property))
                    newValue = self.resolveEntryReferences(oldValue, menu, token, entry)
                    setattr(entry, property, newValue)

        self.resolveEntryDotReferences()
