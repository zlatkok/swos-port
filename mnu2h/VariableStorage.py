import Constants
import Util

from Token import Token
from Menu import Menu
from Variable import Variable

class VariableStorage:
    def __init__(self):
        self.globals = {}
        self.preprocVariables = {}
        self.loopVariables = {}
        self.menusWithVariables = set()

    def getGlobalVariable(self, name):
        assert isinstance(name, str)
        return self.globals.get(name)

    def addGlobalVariable(self, name, value, token):
        assert isinstance(name, str)
        assert isinstance(value, (int, str))
        assert isinstance(token, Token)

        var = Variable(name, value, token)
        self.globals[name] = var
        return var

    @staticmethod
    def hasMenuVariable(menu, name):
        assert isinstance(menu, Menu)
        assert isinstance(name, str)

        return name in menu.variables

    @staticmethod
    def getMenuVariable(menu, name):
        assert isinstance(menu, Menu)
        assert isinstance(name, str)

        return menu.variables.get(name)

    def addMenuVariable(self, menu, name, value, token):
        assert isinstance(menu, Menu)
        assert isinstance(name, str)
        assert isinstance(value, (int, str))
        assert isinstance(token, Token)

        var = Variable(name, value, token)
        menu.variables[name] = var
        self.menusWithVariables.add(menu)
        return var

    def getPreprocVariable(self, name):
        assert isinstance(name, str)
        return self.preprocVariables.get(name)

    def addPreprocVariable(self, name, value, token):
        assert isinstance(name, str)
        assert isinstance(value, int)
        assert isinstance(token, Token)

        var = Variable(name, 0, token)
        self.preprocVariables[name] = var
        return var

    def hasPreprocVariable(self, name):
        assert isinstance(name, str)

        return name in self.preprocVariables

    def hasLoopVariable(self, name):
        assert isinstance(name, str)

        return name in self.loopVariables

    def addLoopVariable(self, name):
        assert isinstance(name, str)

        var = self.getPreprocVariable(name)
        assert var

        self.loopVariables[name] = var
        return var

    def destroyLoopVariable(self, name):
        assert isinstance(name, str)

        del self.loopVariables[name]
        del self.preprocVariables[name]

    def setLoopVariablesAsUnreferenced(self):
        for var in self.loopVariables.values():
            var.referenced = False

    def getReferencedLoopVariables(self):
        return { var for var in self.loopVariables.values() if var.referenced }

    def getUnreferencedGlobalsInFile(self, inputPath):
        return self.getUnreferencedVariablesInFile(inputPath, self.globals)

    def getUnreferencedMenuVariablesInFile(self, inputPath):
        unreferencedMenuVars = []

        for menu in self.menusWithVariables:
            for varName, varToken in self.getUnreferencedVariablesInFile(inputPath, menu.variables):
                unreferencedMenuVars.append((menu.name, varName, varToken))

        return unreferencedMenuVars

    # expandVariable
    #
    # in:
    #     text  - string with the potential variable
    #     token - token for the error reporting purposes
    #     menu  - menu the potential variable belongs to (if any)
    #
    # Tests if the token contains a variable (be it built-in or user-defined), and if so expands it.
    # If not, simply returns it as is.
    #
    def expandVariable(self, text, token, menu=None):
        assert isinstance(token, Token)
        assert menu is None or isinstance(menu, Menu)

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
            text = self.expandBuiltinVariable(text, token, menu)

        if text.lower() == 'true':
            text = '1'
        elif text.lower() == 'false':
            text = '0'

        return text

    # expandBuiltinVariable
    #
    # in:
    #     text  - text of the built-in variable
    #     token - token associated with the variable
    #     menu  - menu in which context we're expanding (if any)
    #
    # Expands built-in variable (variables that begin with '@') and returns expanded variable. Variable must be
    # valid or the execution stops.
    #
    def expandBuiltinVariable(self, text, token, menu=None):
        assert isinstance(text, str)
        assert text and text[0] == '@'
        assert isinstance(token, Token)
        assert menu is None or isinstance(menu, Menu)

        text = text[1:]

        result = Constants.kConstants.get(text, None)
        if result is not None:
            return str(result)

        checkedMenu = lambda: self.verifyMenu(menu, token)

        if text in ('previousX', 'prevX'):
            result = checkedMenu().previousX
        elif text in ('previousY', 'prevY'):
            result = checkedMenu().previousY
        elif text in ('previousEndX', 'prevEndX'):
            result = f'({checkedMenu().previousX} + {checkedMenu().previousWidth})'
        elif text in ('previousEndY', 'prevEndY'):
            result = f'({checkedMenu().previousY} + {checkedMenu().previousHeight})'
        elif text in ('previousWidth', 'prevWidth'):
            result = checkedMenu().previousWidth
        elif text in ('previousHeight', 'prevHeight'):
            result = checkedMenu().previousHeight
        elif text in ('previousOrdinal', 'previousOrd', 'prevOrdinal', 'prevOrd'):
            result = checkedMenu().previousOrdinal
        elif text in ('currentOrdinal', 'currentOrd', 'curOrdinal', 'curOrd'):
            result = checkedMenu().getLastEntryOrdinal()
        elif text == 'kScreenWidth':
            result = 320
        elif text == 'kScreenHeight':
            result = 200
        elif text == 'kAlloc':
            result = -1
        elif text in ('kTextWidth', 'kTextHeight'):
            result = '@' + text     # these are special case, and will be evaluated later
        else:
            Util.warning(f"unknown internal variable `@{text}'", token)
            result = 0

        if isinstance(result, int):
            return str(result)
        else:
            return f'({result})'

    # verifyMenu
    #
    # in:
    #     menu  - menu to verify
    #     token - token associated with the point of check
    #
    # Verify that the given menu is valid or report an error.
    # Returns given menu so it can be used in its place.
    #
    @staticmethod
    def verifyMenu(menu, token):
        assert isinstance(menu, (Menu, type(None)))
        assert isinstance(token, Token)

        if not menu:
            Util.error('attempting to use a menu variable outside of a menu', token)

        return menu

    @staticmethod
    def getUnreferencedVariablesInFile(inputPath, vars):
        assert isinstance(inputPath, str)
        assert isinstance(vars, dict)

        isVariableReferenced = lambda var: not var.referenced and var.token.path == inputPath
        return [(varName, var.token) for varName, var in vars.items() if isVariableReferenced(var)]
