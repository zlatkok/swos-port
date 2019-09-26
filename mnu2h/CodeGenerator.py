import os

import Util
import Constants

from Entry import Entry
from Entry import ResetTemplateEntry
from Menu import Menu
from Parser import Parser

kStubIfdef =  'SWOS_STUB_MENU_DATA'
kPackPragmaOn = '#pragma pack(push, 1)'
kPackPragmaOff = '#pragma pack(pop)'

class CodeGenerator:
    def __init__(self, parser, inputPath, outputPath):
        assert isinstance(parser, Parser)
        assert isinstance(inputPath, str)
        assert isinstance(outputPath, str)

        self.parser = parser
        self.inputPath = inputPath
        self.outputPath = outputPath

    @staticmethod
    def output(parser, inputPath, outputPath):
        codeGenerator = CodeGenerator(parser, inputPath, outputPath)
        codeGenerator.doOutput()

    # doOutput
    #
    # Generates C++ code from all the digested input from the parser and writes it to the specified output file.
    #
    def doOutput(self):
        try:
            with open(self.outputPath, 'w') as f:
                out = lambda *args, **kwargs: print(*args, **kwargs, file=f)

                inputFilename = os.path.basename(self.inputPath)
                out(f'// automatically generated from {inputFilename}, do not edit')
                out('#pragma once\n')
                out('#include "menu.h"')
                out('#include "swossym.h"\n')
                out('using namespace SWOS_Menu;\n')

                self.outputFunctionForwardDeclarations(out)
                self.outputStringTableDeclarations(out)

                out(kPackPragmaOn)

                if not self.parser.getMenus() and self.parser.showWarnings():
                    Util.warning('no menus found', self.inputPath)

                for menuName, menu in self.parser.getMenus().items():
                    self.outputMenu(menuName, menu, out)

                out(kPackPragmaOff)

                self.outputFunctionForwardDeclarations(out, True)

        except FileNotFoundError:
            Util.error(f"couldn't open file for writing", self.outputPath)

    # outputFunctionForwardDeclarations
    #
    # in:
    #     out  - output function
    #     stub - if true, output ifdef'd stubs for testing
    #
    # Outputs forward declarations of handler functions, or empty bodies if requested.
    #
    def outputFunctionForwardDeclarations(self, out, stub=False):
        assert callable(out)

        functions = self.parser.getFunctions()

        if functions:
            if stub:
                out(f'\n#ifdef {kStubIfdef}')

            for func in functions:
                body = ' {}' if stub else ';'
                out(f'static void {func}(){body}')

            out('#endif' if stub else '')

    # outputStringTableDeclarations
    #
    # in:
    #     out - output function
    #
    # Outputs C++ code for all the string tables gathered.
    #
    def outputStringTableDeclarations(self, out):
        assert callable(out)

        stringTableLengths = self.parser.getStringTableLengths()

        # don't forget to pack string tables structs as well!
        if stringTableLengths:
            out(kPackPragmaOn)

        self.outputStringTableStructs(stringTableLengths, out)

        if stringTableLengths:
            out()

        self.outputStringTableVariables(out)

        if stringTableLengths:
            out(kPackPragmaOff)

    @staticmethod
    def outputStringTableStructs(stringTableLengths, out):
        assert callable(out)

        for numStrings in stringTableLengths:
            out(f'struct StringTable{numStrings} : public StringTable {{')
            out(f'    const char *strings[{numStrings}];')
            out(f'    StringTable{numStrings}(int16_t *index, int16_t initialValue, '
                f'{", ".join(map(lambda i: f"const char *str{i}", range(0, numStrings)))}) :\n        StringTable(index, initialValue), '
                f'strings{{{", ".join(map(lambda i: f"str{i}", range(0, numStrings)))}}} {{}}')
            out('};')

    def outputStringTableVariables(self, out):
        assert callable(out)

        for menuName, menu in self.parser.getMenus().items():
            for entry in menu.entries.values():
                if entry.stringTable:
                    st = entry.stringTable
                    numStrings = len(st.values)
                    if numStrings:
                        name = Util.getStringTableName(menuName, entry)
                        output = f'extern int16_t {st.variable};\n'
                        output += f'static StringTable{numStrings} {name} {{\n    &{st.variable}, {st.initialValue}, '

                        for value in st.values:
                            if value[0] == value[-1] == "'":
                                value = f'"{value[1:-1]}"'
                            output += f'{value}, '

                        output = output[:-1] + '\n};\n'
                        output += f'#ifdef {kStubIfdef}\n'
                        output += f'static int16_t {st.variable};\n'
                        output += '#endif'

                        out(output)

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
        assert isinstance(menuName, str)
        assert isinstance(menu, Menu)
        assert callable(out)

        for func in Constants.kMenuFunctions:
            if not func in menu.properties:
                menu.properties[func] = 'nullptr'

        out(f'struct SWOS_Menu_{menuName} : public BaseMenu\n{{')
        out(f'    MenuHeader header{{ {menu.properties[Constants.kOnInit]}, {menu.properties[Constants.kOnReturn]}, '
            f'{menu.properties[Constants.kOnDraw]}, {menu.properties[Constants.kInitialEntry]} }};')

        templateIndex = 0
        resetTemplateIndex = 0

        outputMenuXY = lambda x, y, ord: out(f'\n    MenuXY menuXY{ord:02}{{{x}, {y}}};')
        if menu.initialX != 0 or menu.initialY != 0:
            outputMenuXY(menu.initialX, menu.initialY, 0)

        currentMenuX = menu.initialX
        currentMenuY = menu.initialY

        for entry in menu.entries.values():
            if isinstance(entry, ResetTemplateEntry):
                out(f'\n    ResetTemplateEntry rte{resetTemplateIndex:02}{{}};')
                resetTemplateIndex += 1
                continue
            elif entry.isTemplate():
                out(f'\n    TemplateEntry te{templateIndex:02}{{}};')
                templateIndex += 1

                # set width/height to non-zero value to avoid triggering the assert
                for property in ('width', 'height'):
                    value = getattr(entry, property)
                    if not value or value == '0':
                        setattr(entry, property, 1)
            else:
                if entry.menuX != currentMenuX or entry.menuY != currentMenuY:
                    outputMenuXY(entry.menuX, entry.menuY, entry.ordinal)
                    currentMenuX = entry.menuX
                    currentMenuY = entry.menuY

            out(f'\n    Entry eb{entry.ordinal:02}{{ {entry.x}, {entry.y}, {entry.width}, {entry.height} }};')

            for entryStruct in self.getEntryStructs(menuName, entry):
                out('        ', entryStruct)

            out(f'    EntryEnd ee{entry.ordinal:02}{{}};')

        out('\n    MenuEnd menuEnd{};')
        out(f'}} static const {menuName};\n')

        self.outputMenuVariables(menuName, menu, out)

    @staticmethod
    def outputMenuVariables(menuName, menu, out):
        assert isinstance(menuName, str)
        assert isinstance(menu, Menu)
        assert callable(out)

        # allow C++ to use all the symbolic entry names through a namespaced enum
        namespace = menuName[0].upper() + menuName[1:]
        if namespace == menuName:
            namespace += 'NS'

        out(f'namespace {namespace} {{')
        out('    enum Entries {')

        exportedOrdinals = False
        for entry in menu.entries.values():
            if not entry.isTemplate() and entry.name:
                out(f'        {entry.name} = {entry.ordinal},')
                exportedOrdinals = True

        out('    };')

        if menu.exportedVariables and exportedOrdinals:
            out()

        for property, (value, token) in menu.exportedVariables.items():
            out(f'    constexpr int {property} = {value};')

        out('}')

    # getEntryStructs
    #
    # in:
    #     menuName - name of parent menu
    #     entry    - menu entry to process
    #
    # Compares the given entry element values to default values and returns a list of SWOS menu codes
    # (in the form of C++ code strings) of all the differences.
    #
    @staticmethod
    def getEntryStructs(menuName, entry):
        assert isinstance(menuName, str)
        assert isinstance(entry, Entry)

        result = []
        ord = f'{entry.ordinal:02}'

        if entry.text:
            text = str(entry.text).strip()
            if text in ('-1', '(-1)'):
                text = 'reinterpret_cast<const char *>(-1)'
            result.append(f'EntryText et{ord}{{ {entry.textFlags}, {text} }};')

        if entry.stringTable:
            result.append(f'EntryStringTable est{ord}{{ {entry.textFlags}, ')
            if not entry.stringTable.values:
                result[-1] += f'reinterpret_cast<StringTable *>(&{entry.stringTable.variable}) }};'
            else:
                name = Util.getStringTableName(menuName, entry)
                result[-1] += f'&{name} }};'

        if entry.number is not None:
            result.append(f'EntryNumber en{ord}{{ {entry.textFlags}, {entry.number} }};')

        if entry.sprite is not None:
            result.append(f'EntryForegroundSprite efs{ord}{{ {entry.sprite} }};')

        if entry.customDrawForeground is not None:
            result.append(f'EntryCustomForegroundFunction ecff{ord} {{ { entry.customDrawForeground} }};')

        if entry.customDrawBackground is not None:
            result.append(f'EntryCustomBackgroundFunction ecbf{ord}{{ {entry.customDrawBackground} }};')

        if entry.color and entry.color != '0':
            result.append(f'EntryColor ec{ord}{{ {entry.color} }};')

        if entry.invisible:
            result.append(f'EntryInvisible ei{ord}{{}};')

        if entry.leftEntry != -1 or entry.rightEntry != -1 or entry.upEntry != -1 or entry.downEntry != -1:
            result.append(f'EntryNextPositions ep{ord}{{ {entry.leftEntry}, {entry.rightEntry}, {entry.upEntry}, {entry.downEntry} }};')

        for direction in ('Left', 'Right', 'Up', 'Down'):
            newDirection = str(getattr(entry, 'direction' + direction))
            skip = str(getattr(entry, 'skip' + direction))

            if newDirection != '-1' or skip != '-1':
                if newDirection == '-1':
                    newDirection = Constants.kConstants['k' + direction]

                result.append(f'Entry{direction}Skip e{direction[0].lower()}s{ord}{{ {skip}, {newDirection} }};')

        if entry.controlsMask:
            result.append(f'EntryOnSelectFunctionWithMask eosfm{ord}{{ {entry.onSelect}, {entry.controlsMask} }};')
        elif entry.onSelect:
            result.append(f'EntryOnSelectFunction eosf{ord}{{ {entry.onSelect} }};')

        if entry.beforeDraw:
            result.append(f'EntryBeforeDrawFunction ebdf{ord}{{ {entry.beforeDraw} }};')

        if entry.onReturn:
            result.append(f'EntryOnReturnFunction eadf{ord}{{ {entry.onReturn} }};')

        return result
