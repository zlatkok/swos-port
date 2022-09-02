import Util
import Constants
import Parser.Common

from Menu import Menu
from Token import Token
from Tokenizer import Tokenizer
from Variable import Variable
from StringTable import StringTable
from VariableStorage import VariableStorage

from Parser.ExpressionParser import ExpressionParser
from Preprocessor.Preprocessor import Preprocessor
from Preprocessor.ExpressionHandler import ExpressionHandler as PreprocExpressionHandler

class EntryParser:
    def __init__(self, tokenizer, preprocessor, expressionParser, varStorage):
        assert isinstance(tokenizer, Tokenizer)
        assert isinstance(preprocessor, Preprocessor)
        assert isinstance(expressionParser, ExpressionParser)
        assert isinstance(varStorage, VariableStorage)

        self.tokenizer = tokenizer
        self.preprocessor = preprocessor
        self.expressionParser = expressionParser
        self.varStorage = varStorage

        self.defaultPropertyTokenizer = Tokenizer.empty()

        self.init()

    def init(self):
        self.functions = set()
        self.stringTableLength = 0

        self.entry = None

    # parse
    #
    # in:
    #     menu       - parent Menu instance (required)
    #     isTemplate - true if this is a template entry
    #
    # Parses a menu entry and adds it to the parent menu. Ends the program in case of any error.
    #
    def parse(self, menu, isTemplate):
        assert isinstance(menu, Menu)
        assert isinstance(isTemplate, bool)

        self.init()

        self.menu = menu

        token, name, nameToken = Parser.Common.getName(self.tokenizer, self.preprocessor, 'entry name', menu)
        self.verifyEntryName(name, nameToken)

        token = self.tokenizer.expectBlockStart(token, fetchNextToken=False)

        self.entry = menu.addEntry(name, isTemplate)
        self.entry.token = nameToken or token
        self.entry.forceColor = False

        if not isTemplate:
            self.assignDefaultEntryValues()

        propertiesSet = self.parseEntryBody(isTemplate)

        if not isTemplate:
            self.tryRemovingTemplateEntryType(propertiesSet)

        self.expandTextWidthAndHeight(propertiesSet, token)
        self.updateMenuPreviousFields(isTemplate)

    # If the user has explicitly specified entry type, remove any potential type that comes from the template entry.
    def tryRemovingTemplateEntryType(self, propertiesSet):
        assert isinstance(propertiesSet, set)

        if self.menu.templateActive:
            userSetTypes = propertiesSet.intersection(Constants.kEntryTypeProperties)
            if userSetTypes:
                for property in userSetTypes.symmetric_difference(Constants.kEntryTypeProperties):
                    setattr(self.entry, property, None)

    def parseEntryBody(self, isTemplate):
        propertiesSet = set()

        while True:
            token = self.tokenizer.getNextToken('entry property')
            if token.string == '}':
                break

            property = self.expandEntryPropertyAliases(token)

            if property == '#':
                self.preprocessor.parsePreprocessorDirective(self.menu)
                continue

            propertyToken = token
            propertiesSet.add(property)

            self.verifyEntryProperty(property, token, propertiesSet)

            token = self.tokenizer.expectIdentifier(token, "colon (`:')", fetchNextToken=True)
            self.tokenizer.expectColon(token, fetchNextToken=False)

            result = self.getEntryPropertyValue(property)

            if result == '}':
                Util.error(f"missing property value for `{property}'", propertyToken)

            setattr(self.entry, property, result)
            if property == 'color':
                self.updateForceColor(result, isTemplate)
            elif property == 'onSelect' and self.entry.boolOption:
                Util.error('onSelect is reserved in boolOption entries', propertyToken)

            if property in Constants.kNextEntryProperties:
                setattr(self.entry, property + 'Token', propertyToken)

        return propertiesSet

    def updateForceColor(self, result, isTemplate):
        assert isinstance(result, str)

        self.entry.forceColor = not isTemplate and result == '0' and \
            self.menu.templateActive and getattr(self.menu.templateEntry, 'color', None) != 0

    def updateMenuPreviousFields(self, isTemplate):
        if not isTemplate:
            self.menu.previousX = self.entry.x
            self.menu.previousY = self.entry.y
            self.menu.previousWidth = self.entry.width
            self.menu.previousHeight = self.entry.height
            self.menu.previousOrdinal = self.entry.ordinal

    @staticmethod
    def expandEntryPropertyAliases(token):
        assert isinstance(token, Token)

        property = token.string
        if property == 'topEntry':  # I've made this mistake so many times that I've made it into an alias
            property = 'upEntry'
        elif property == 'bottomEntry':
            property = 'downEntry'

        return property

    def verifyEntryProperty(self, property, token, propertiesSet):
        assert isinstance(property, str)
        assert isinstance(token, Token)
        assert isinstance(propertiesSet, set)

        if property == 'Entry':
            Util.error('entries can not be nested', token)
        elif not hasattr(self.entry, property):
            Util.error(f"unknown property `{property}'", token)

        if property in Constants.kImmutableEntryProperties:
            Util.error(f"property `{property}' can not be assigned", token)

        self.checkConflictingProperties(property, propertiesSet, token)

    def getEntryPropertyValue(self, property):
        assert isinstance(property, str)

        valueToken = self.tokenizer.peekNextToken()

        if property in Constants.kEntryFunctions:
            result, declareFunction = Parser.Common.registerFunction(self.tokenizer)
            if declareFunction:
                self.functions.add(result)
        elif property == 'stringTable':
            result = self.parseStringTable()
            self.checkForWhitespaceInStrings(property, result.values, valueToken)
        elif property == 'multilineText':
            result = self.parseMultilineText()
            self.checkForWhitespaceInStrings(property, result, valueToken)
        elif property == 'boolOption':
            result = self.parseBoolOptionEntry()
        else:
            result = self.expressionParser.parse(self.menu)
            self.checkForWhitespaceInStrings(property, result, valueToken)

        return result

    def checkForWhitespaceInStrings(self, property, value, valueToken):
        assert isinstance(property, str)
        assert isinstance(value, (str, int, list, tuple))
        assert isinstance(valueToken, (Token, None))

        if self.preprocessor.showWarnings:
            if not isinstance(value, (list, tuple)):
                value = [value]

            for val in value:
                if Util.isString(val) and len(val) > 2 and {val[0], val[-1]} < {"'", '"'} and \
                    len(val[1:-1].strip()) != len(val) - 2:
                    Util.warning('leading/trailing whitespace detected in a string', valueToken)

    def verifyEntryName(self, name, nameToken):
        assert isinstance(name, (str, type(None)))
        assert isinstance(nameToken, (Token, type(None)))

        if name:
            if not name.isidentifier():
                Util.error(f"`{name}' is not a valid entry name", nameToken)

            if name in self.menu.entries:
                Util.error(f"entry `{name}' already declared", nameToken)

            Util.verifyNonCppKeyword(name, nameToken, 'entry name')

    # assignDefaultEntryValues
    #
    # Assigns default values for a freshly encountered entry, right before its body will be parsed.
    #
    def assignDefaultEntryValues(self):
        self.entry.menuX = self.menu.properties['x']
        self.entry.menuY = self.menu.properties['y']

        hasActivePropertyFromTemplate = lambda property: \
            property in ('width', 'height') and self.menu.templateActive and getattr(self.menu.templateEntry, property, 0) != 0

        isPropertyDefaultAssignable = lambda property: \
            not self.entry.isTemplate() and property != 'ordinal' and (property != 'color' or not self.menu.templateActive) and \
            not hasActivePropertyFromTemplate(property)

        for property in Constants.kMenuDefaults:
            if isPropertyDefaultAssignable(property):
                defaultProperty = 'default' + property[0].upper() + property[1:]
                if defaultProperty in self.menu.properties:
                    value = str(self.menu.properties[defaultProperty])
                    assert value

                    if defaultProperty in self.menu.propertyTokens:
                        token = self.menu.propertyTokens[defaultProperty]
                    else:
                        token = self.tokenizer.getPreviousToken()

                    self.defaultPropertyTokenizer.resetToString(value, token)
                    expandedValue = self.expressionParser.parse(self.menu, True, self.defaultPropertyTokenizer)

                    setattr(self.entry, property, expandedValue)

    # checkConflictingProperties
    #
    # in:
    #     property          - new property that's being set
    #     userSetProperties - set of properties explicitly set by the user
    #     token             - token which defines property
    #
    # Checks for conflicting properties inside the entry, such as both text and sprite, basically for
    # any pair of members belonging to any of two entry struct's unions.
    #
    def checkConflictingProperties(self, property, userSetProperties, token):
        assert isinstance(property, str)
        assert isinstance(userSetProperties, set)
        assert isinstance(token, Token)

        if property in Constants.kEntryTypeProperties:
            for property2 in Constants.kEntryTypeProperties:
                if property2 != property and getattr(self.entry, property2) is not None and property2 in userSetProperties:
                    Util.error(f"can't use properties `{property}' and `{property2}' at the same time", token)

    # parseMultilineText
    #
    # Parses multi line text block and returns a list of strings. Block is defined by a list of strings
    # separated by comma, all inside square brackets.
    #
    def parseMultilineText(self):
        token = self.tokenizer.getNextToken()
        if token.string in ('0', '@kNull'):
            return []
        token = self.tokenizer.expect('[', token, fetchNextToken=True)
        values = self.getStringList(token, True)
        values = list(map(Util.unquotedString, values))
        return values

    # parseBoolOptionEntry
    #
    # Parses bool option block and returns a list containing get/set functions and description.
    #
    def parseBoolOptionEntry(self):
        getFunction = self.tokenizer.getNextToken()
        comma = self.tokenizer.expectIdentifier(getFunction, expectedNext='comma', fetchNextToken=True)
        setFunction = self.tokenizer.expect(',', comma, fetchNextToken=True)
        comma = self.tokenizer.expectIdentifier(setFunction, expectedNext='comma', fetchNextToken=True)
        description = self.tokenizer.expect(',', comma, fetchNextToken=True)
        if not Util.isString(description.string):
            Util.error('expected string description', description)
        return (getFunction.string, setFunction.string, description.string)

    # parseStringTable
    #
    # Parses string table block and returns a filled-in string table object and a token that follows it.
    #
    def parseStringTable(self):
        token = self.tokenizer.getNextToken()
        Util.verifyNonCppKeyword(token.string, token, 'string table name')

        if token.string == '~' or token.string == 'swos':
            if token.string == '~':
                token = self.tokenizer.getNextToken()
            else:
                token = Parser.Common.getSwosVariable(self.tokenizer)

            stringTableVar = token.string
            native = True

            if Variable.isSwos(stringTableVar):
                native = False
                stringTableVar = Variable.extractSwosVar(stringTableVar)

            stringTable = StringTable(stringTableVar, native)
            token = self.tokenizer.expectIdentifier(token, fetchNextToken=False)
        else:
            token = self.tokenizer.expect('[', token, fetchNextToken=True)

            token, variable, declareIndexVariable, externIndexVariable = self.getStringTableIndexVariable(token)
            token, initialValue = self.getStringTableInitialStringIndex(token)
            values = self.getStringList(token, False)
            nativeFlags = self.getNativeFlags(variable, values)

            removeSwosVar = lambda val: Variable.extractSwosVar(val) if Variable.isSwos(val) else val
            values = tuple(map(removeSwosVar, values))
            variable = removeSwosVar(variable)

            assert values and nativeFlags
            stringTable = StringTable(variable, True, declareIndexVariable, externIndexVariable, values, initialValue, nativeFlags)

            self.stringTableLength = len(values)

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
            token = Parser.Common.getSwosVariable(self.tokenizer)
            variable = token.string
            declare = False

        token = self.tokenizer.expectIdentifier(token, "comma (`,')", fetchNextToken=True)
        token = self.tokenizer.expect(',', token, fetchNextToken=True)

        return token, variable, declare, extern

    def getStringTableInitialStringIndex(self, token):
        assert isinstance(token, Token)

        initialValue = 0
        negative = False

        while token.string == '-':
            negative = not negative
            token = self.tokenizer.getNextToken()

        try:
            initialValue = int(token.string, 0)
            if negative:
                initialValue = -initialValue

            token = self.tokenizer.getNextToken()
            token = self.tokenizer.expect(',', token, fetchNextToken=True)
        except ValueError:
            pass

        return token, initialValue

    def getStringList(self, token, onlyLiterals):
        assert isinstance(token, Token)

        values = []

        while True:
            string = token.string
            idString = string

            if string[0] == string[-1] == "'":
                string = '"' + string[1:-1] + '"'
            elif string == 'swos':
                if onlyLiterals:
                    Util.error('SWOS variables not supported', token)
                token = Parser.Common.getSwosVariable(self.tokenizer)
                string = token.string
                idString = string[1:]

            values.append(string)

            if not Util.isString(string):
                if onlyLiterals:
                    Util.error(f"expected string literal, got `{token.string}'", token)
                if not idString.isidentifier():
                    Util.error(f"expected string or identifier, got `{token.string}'", token)

            token = self.tokenizer.getNextToken("`]' or `,'")
            if token.string == ']':
                break

            token = self.tokenizer.expect(',', token, fetchNextToken=True)

        return values

    @staticmethod
    def getNativeFlags(variable, values):
        assert isinstance(variable, str)
        assert isinstance(values, list)

        return tuple(map(lambda var: not Variable.isSwos(var), [variable] + values))

    # expandTextWidthAndHeight
    #
    # in:
    #     menu          - parent menu
    #     propertiesSet - all the properties that were set from the entry
    #     token         - last token of the entry, for error reporting purposes
    #
    # Expands @kTextWidth and @kTextHeight height constants. We can only reliably do that after the entire
    # entry has been parsed, and only then can we also reliably detect errors.
    #
    def expandTextWidthAndHeight(self, propertiesSet, token):
        assert isinstance(propertiesSet, set)
        assert isinstance(token, Token)

        w = '@kTextWidth'
        h = '@kTextHeight'

        if not self.hasUsedTextWidthHeightConstants(w, h, propertiesSet):
            return

        textFlags = self.evaluateTextFlags(token)
        useBigFont = textFlags & Constants.kConstants['kBigFont']
        strings = self.getEntryStrings(w, h, token)
        wVal, hVal = self.measureStrings(strings, useBigFont, token)
        self.expandTextWidthAndHeightConstants(propertiesSet, w, h, wVal, hVal)

        for prop in propertiesSet:
            val = getattr(self.entry, prop)
            if isinstance(val, str):
                val = val.replace(f'({w})', f'({wVal})')
                val = val.replace(f'({h})', f'({hVal})')
                setattr(self.entry, prop, val)

    def hasUsedTextWidthHeightConstants(self, w, h, propertiesSet):
        assert isinstance(w, str)
        assert isinstance(h, str)
        assert isinstance(propertiesSet, set)

        for prop in propertiesSet:
            val = getattr(self.entry, prop)
            if isinstance(val, str) and (w in val or h in val):
                return True

        return False

    def evaluateTextFlags(self, token):
        assert isinstance(token, Token)

        textFlags = Util.removeEnclosingParentheses(self.entry.textFlags)
        try:
            textFlags = int(textFlags, 0)
        except ValueError:
            self.defaultPropertyTokenizer.resetToString(textFlags, token)
            preprocExpressionHandler = PreprocExpressionHandler(self.defaultPropertyTokenizer, self.varStorage, self.preprocessor.showWarnings)
            textFlags = preprocExpressionHandler.parseAndEvaluate(self.menu)
            if not isinstance(textFlags, int):
                Util.error("text flags must be integer", token)

        return textFlags

    def getEntryStrings(self, w, h, token):
        assert isinstance(w, str)
        assert isinstance(h, str)
        assert isinstance(token, Token)

        if self.entry.text is None and self.entry.stringTable is None:
            Util.error(f'non-text entry uses {w}/{h} constants', token)

        if self.entry.text is not None:
            strings = [self.entry.text]
        else:
            strings = self.entry.stringTable.values

        return strings

    def measureStrings(self, strings, useBigFont, token):
        assert isinstance(strings, (list, tuple))
        assert isinstance(token, Token)

        wVal = 0
        for string in strings:
            if not Util.isString(string):
                Util.error(f"`{string}' is not a string, can't measure", token)
            string = Util.unquotedString(string)
            wVal = max(wVal, self.preprocessor.textWidth(string, useBigFont))

        hVal = self.preprocessor.textHeight(useBigFont)

        return wVal, hVal

    def expandTextWidthAndHeightConstants(self, propertiesSet, w, h, wVal, hVal):
        assert isinstance(propertiesSet, set)
        assert isinstance(w, str)
        assert isinstance(h, str)
        assert isinstance(wVal, int)
        assert isinstance(hVal, int)

        for prop in propertiesSet:
            val = getattr(self.entry, prop)
            if isinstance(val, str):
                val = val.replace(f'({w})', f'({wVal})')
                val = val.replace(f'({h})', f'({hVal})')
                setattr(self.entry, prop, val)
