import re
import unittest
import operator
import functools

from unittest import mock
from ddt import ddt, data

from TestHelper import getParserWithData
from CodeGenerator import CodeGenerator

import TestHelper
import Util

kPackPragmaOn = '#pragma pack(push, 1)'
kPackPragmaOff = '#pragma pack(pop)'

kTestData = (
    ('', {}),
    ('Menu HanSolo { }', {
        'menus': [ 'HanSolo' ],
        'menuElements': {
            'HanSolo': [
                {'id': 'header',  'type': 'MenuHeader', 'params': ['nullptr', 'nullptr', 'nullptr', '0']},
                {'id': 'menuEnd','type': 'MenuEnd', 'params': []},
            ],
        },
    }),
    ('''
        #warningsOff
        Menu Charlie {
            onInit: someCoolInit
            onReturn: hereIAm
            onDraw: danceSnake
            initialEntry: CasaBrava

            export kBlazeOfGlory = 1990
            export SantaFe = 2194

            defaultX: @prevX + 10
            defaultY: @prevY + 10
            defaultWidth: 99
            defaultHeight: 17

            Entry CasaNostra {
                x: 49
                y: 158
                color: 11
                stringTable: [ start, "HOLD", 'ME', "NOW" ]
            }
            Entry {
                customDrawForeground: drawYellow
                customDrawBackground: drawRed
                downEntry: >
                upEntry: <
            }
            Entry CasaBrava {
                invisible: true
                onReturn: returnOfThePhantom
                beforeDraw: prepareWell
                controlsMask: 0x15
                onSelect: pumpItUp
                textFlags: @kBigText
                text: @kAlloc
                leftEntry: <
                rightEntry: CasaNostra
                downEntry: CasaNostra
                upEntry: CasaNostra
            }
        }
        Menu Bravo {
            onInit: someCoolInit
            initialEntry: MoonLander
            Entry {
                width: 25
                height: 36
                stringTable: [ mirror, 'WARM', MY, "HEART" ]
                color: @kRed
                onSelect: uncleSamNeedsYOU
            }
            Entry MoonLander {
                number: 1969
            }
        }
        Menu Tango {
            onInit: ~specialForces
            export TangoAndCash = 1989
            x: 123
            y: 55

            TemplateEntry {
                stringTable: [ Indianapolis, 2002, "1" ]
                color: @kLightBrownWithYellowFrame
                beforeDraw: aperitif
            }

            Entry Madera {
                sprite: 1001
            }

            ResetTemplate {}

            x: 125
            y: 35

            Entry Avalanche {
                skipLeft: 2
                directionRight: 1
                skipRight: 0
            }
        }
        Menu Delta {}
    ''', {
        'functions': (
            'someCoolInit', 'hereIAm', 'danceSnake', 'uncleSamNeedsYOU', 'drawYellow', 'drawRed',
            'returnOfThePhantom', 'prepareWell', 'pumpItUp', 'aperitif',
        ),
        'menus': ['Charlie', 'Bravo', 'Tango', 'Delta'],
        'stringTables': ((1, 3), (
            ('Charlie_CasaNostra', 'start', '0', '"HOLD"', '"ME"', '"NOW"'),
            ('Bravo_00', 'mirror', '0', '"WARM"', 'MY', '"HEART"'),
            ('Tango_30000', 'Indianapolis', '2002', '"1"'),
        )),
        'namedEntries': {
            'Charlie': (('CasaNostra', 0), ('CasaBrava', 2),),
            'Bravo': (('MoonLander', 1),),
            'Tango': (('Madera', 0), ('Avalanche', 1),),
        },
        'exportedVariables': {
            'Charlie': (('kBlazeOfGlory', '1990'), ('SantaFe', '2194'),),
            'Tango': (('TangoAndCash', '1989'),),
        },
        'menuElements': {
            'Bravo': [
                {'id': 'header', 'type': 'MenuHeader',  'params': ['someCoolInit', 'nullptr', 'nullptr', '1']},

                {'id': 'eb00',   'type': 'Entry',       'params': ['0', '0', '25', '36']},
                {'id': 'est00',  'type': 'EntryStringTable', 'params': ['0', '&Bravo_00_stringTable']},
                {'id': 'ec00',   'type': 'EntryColor',  'params': ['10']},
                {'id': 'eosf00', 'type': 'EntryOnSelectFunction', 'params': ['uncleSamNeedsYOU']},
                {'id': 'ee00',   'type': 'EntryEnd',    'params': []},

                {'id': 'eb01',   'type': 'Entry',       'params': ['0', '0', '0', '0']},
                {'id': 'en01',   'type': 'EntryNumber', 'params': ['0', '1969']},
                {'id': 'ee01',   'type': 'EntryEnd',    'params': []},

                {'id': 'menuEnd', 'type': 'MenuEnd',    'params': []},
            ],
            'Charlie': [
                {'id': 'header', 'type': 'MenuHeader',  'params': ['someCoolInit', 'hereIAm', 'danceSnake', '2']},

                {'id': 'eb00',   'type': 'Entry',       'params': ['49', '158', '99', '17']},
                {'id': 'est00',  'type': 'EntryStringTable', 'params': ['0', '&Charlie_CasaNostra_stringTable']},
                {'id': 'ec00',   'type': 'EntryColor',  'params': ['11']},
                {'id': 'ee00',   'type': 'EntryEnd',    'params': []},

                {'id': 'eb01',   'type': 'Entry',       'params': ['59', '168', '99', '17']},
                {'id': 'ecff01', 'type': 'EntryCustomForegroundFunction', 'params': ['drawYellow']},
                {'id': 'ecbf01', 'type': 'EntryCustomBackgroundFunction', 'params': ['drawRed']},
                {'id': 'ep01',   'type': 'EntryNextPositions', 'params': ['-1', '-1', '0', '2']},
                {'id': 'ee01',   'type': 'EntryEnd',    'params': []},

                {'id': 'eb02',   'type': 'Entry',       'params': ['69', '178', '99', '17']},
                {'id': 'et02',   'type': 'EntryText',   'params': ['16', 'reinterpret_cast<const char *>(-1)']},
                {'id': 'ei02',   'type': 'EntryInvisible', 'params': []},
                {'id': 'ep02',   'type': 'EntryNextPositions', 'params': ['1', '0', '0', '0']},
                {'id': 'eosfm02', 'type': 'EntryOnSelectFunctionWithMask', 'params': ['pumpItUp', '0x15']},
                {'id': 'ebdf02', 'type': 'EntryBeforeDrawFunction', 'params': ['prepareWell']},
                {'id': 'eadf02', 'type': 'EntryOnReturnFunction', 'params': ['returnOfThePhantom']},
                {'id': 'ee02',   'type': 'EntryEnd',    'params': []},

                {'id': 'menuEnd', 'type': 'MenuEnd',    'params': []},
            ],
            'Tango': [
                {'id': 'header',  'type': 'MenuHeader', 'params': ['specialForces', 'nullptr', 'nullptr', '0']},
                {'id': 'menuXY00', 'type': 'MenuXY',    'params': ['123', '55'],},

                {'id': 'te00',    'type': 'TemplateEntry', 'params': []},
                {'id': 'eb30000', 'type': 'Entry',      'params': ['0', '0', '1', '1']},
                {'id': 'est30000', 'type': 'EntryStringTable', 'params': ['0', '&Tango_30000_stringTable']},
                {'id': 'ec30000', 'type': 'EntryColor', 'params': ['6']},
                {'id': 'ebdf30000', 'type': 'EntryBeforeDrawFunction', 'params': ['aperitif']},
                {'id': 'ee30000', 'type': 'EntryEnd',   'params': []},

                {'id': 'eb00',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'efs00',   'type': 'EntryForegroundSprite', 'params': ['1001']},
                {'id': 'ee00',    'type': 'EntryEnd',   'params': []},

                {'id': 'rte00',   'type': 'ResetTemplateEntry', 'params': []},

                {'id': 'menuXY01', 'type': 'MenuXY',    'params': ['125', '35']},

                {'id': 'eb01',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'els01',   'type': 'EntryLeftSkip', 'params': ['2', '0']},
                {'id': 'ers01',   'type': 'EntryRightSkip', 'params': ['0', '1']},
                {'id': 'ee01',    'type': 'EntryEnd',   'params': []},

                {'id': 'menuEnd', 'type': 'MenuEnd',    'params': []},
            ],
            'Delta': [
                {'id': 'header',  'type': 'MenuHeader', 'params': ['nullptr', 'nullptr', 'nullptr', '0']},
                {'id': 'menuEnd', 'type': 'MenuEnd',    'params': []},
            ],
        },
    }),
)
kTestIndices = range(len(kTestData))
kInputFile = 'Ibn'
kOutputFile = 'Khaldun'

kMenuRegex = re.compile(r'''
    struct\s+SWOS_Menu_(?P<name>\w+)\s* : \s*public\s+BaseMenu\b\s*
    (?P<contents>{.*?})
    \s*static\s+const\s+(?P<declarationName>\w+);
    \s*namespace\s+(?P<enumName>\w+)NS\s*{\s*
    enum\s+Entries\s*{
    (?P<enumEntries>[^}]+)}\s*;
    (?P<constants>([^}]+))?
    ''',
    re.MULTILINE | re.DOTALL | re.VERBOSE)
kFuncRegex = re.compile(r'static void (\w+)\s*\(\s*\)\s*;')
kStringTableDeclarationPattern = r'struct\s+StringTable{length}\s*:\s*public\s+StringTable\s*{{(.*?)}};'
kEnumEntryRegex = re.compile(r'(\w+)\s*=\s*(\d+)\s*,')
kConstantsRegex = re.compile(r'constexpr\s+int\s+(\w+)\s*=\s*(\d+)')
kMenuElementsRegex = re.compile(r'''
    (?P<type>\w+)\s+(?P<id>\w+)\s*{\s*
    (?P<params>[^}]+\s*)?
    }\s*;\s*
    ''', re.MULTILINE | re.DOTALL | re.VERBOSE)
kStubDefineRegex = re.compile(r'#ifdef SWOS_STUB_MENU_DATA([^#]+)#endif')

@ddt
class TestCodeGenerator(unittest.TestCase):
    def setUp(self):
        self.outputs = []

        for input, _ in kTestData:
            _, output = self.getCodeGeneratorOutput(input)
            self.outputs.append(output)

    @mock.patch('CodeGenerator.open')
    def testOutputFileError(self, mockOpen):
        mockOpen.side_effect = FileNotFoundError
        parser = getParserWithData('')
        generateOutput = functools.partial(CodeGenerator.output, parser, kInputFile, kOutputFile)
        expectedMessage = f"{kOutputFile}: couldn't open file for writing"
        TestHelper.assertExitWithError(self, generateOutput, expectedMessage)

    @data(*kTestIndices)
    def testHeader(self, index):
        output = self.outputs[index]
        lines = output.split('\n')
        self.assertTrue(lines[0].startswith('//'))
        self.assertIn(f'automatically generated from {kInputFile}', lines[0])
        self.assertEqual(lines[1], '#pragma once')
        start = output.find('using namespace SWOS_Menu;')
        self.assertNotRegex(output[:start], kMenuRegex)

    @mock.patch('CodeGenerator.open')
    @mock.patch('builtins.print')
    @data(
        ('', [[None, 'no menus found']]),
        ('#warningsOff', []),
        ('a = 15', [[1, "unused global variable `a'"], [None, 'no menus found']]),
        ('#warningsOff a = 15', []),
    )
    def testWarnings(self, testData, mockPrint, mockOpen):
        input, rawExpectedWarnings = testData

        expectedWarnings = []
        for warning in rawExpectedWarnings:
            warning = Util.formatMessage(warning[1], kInputFile, warning[0], True)
            expectedWarnings.append(warning)

        parser = getParserWithData(input, filePath=kInputFile)
        CodeGenerator.output(parser, kInputFile, kOutputFile)

        actualWarnings = []
        for call in mockPrint.call_args_list:
            if not call[1]:
                actualWarnings.append(call[0][0])

        self.assertEqual(actualWarnings, expectedWarnings)

    @data(*kTestIndices)
    def testPragmaPush(self, index):
        output = self.outputs[index]

        start = output.find(kPackPragmaOn)
        self.assertNotEqual(start, -1)
        end = output.rfind(kPackPragmaOff, start + len(kPackPragmaOn))
        self.assertNotEqual(end, -1)

        self.assertNotRegex(output[:start], kMenuRegex)
        self.assertNotRegex(output[end:], kMenuRegex)

    @data(*kTestIndices)
    def testIncludesAndNamespaceBeforeStructs(self, index):
        output = self.outputs[index]

        index = 0
        while True:
            nextIndex = output.find('#include', index)
            if nextIndex < 0:
                break

            index = nextIndex + 8

        self.assertNotEqual(index, 0)
        index = output.find('using namespace', index)
        self.assertNotEqual(index, -1)
        self.assertNotRegex(r'\bstruct\b', output[:index])

    @data(*kTestIndices)
    def testFunctions(self, index):
        output = self.outputs[index]

        expectedFunctions = kTestData[index][1].get('functions')
        if expectedFunctions:
            actualFunctions = kFuncRegex.findall(output)
            self.assertEqual(len(actualFunctions), len(expectedFunctions))
            for expectedFunction in expectedFunctions:
                self.assertIn(expectedFunction, actualFunctions)
        else:
            self.assertNotRegex(output, kFuncRegex)

    @data(*kTestIndices)
    def testMenus(self, index):
        output = self.outputs[index]

        expectedMenus = kTestData[index][1].get('menus')
        if expectedMenus:
            actualMenus = kMenuRegex.findall(output)
            actualMenus = list(map(operator.itemgetter(0), actualMenus))
            self.assertEqual(expectedMenus, actualMenus)
        else:
            self.assertNotRegex(output, kMenuRegex)

    @data(*kTestIndices)
    def testStringTables(self, index):
        output = self.outputs[index]

        expectedStringTables = kTestData[index][1].get('stringTables')
        if expectedStringTables:
            for i, length in enumerate(expectedStringTables[0]):
                stringTableRegex = re.compile(kStringTableDeclarationPattern.format(length=length), re.DOTALL | re.MULTILINE)
                self.assertRegex(output, stringTableRegex)
                self.verifyPragmaPackAroundStringTable(stringTableRegex, output, i);
            for stringTableContent in expectedStringTables[1]:
                name = stringTableContent[0]
                controlVar = stringTableContent[1]
                initialValue = stringTableContent[2]
                strings = stringTableContent[3:]
                length = len(stringTableContent) - 3

                self.assertNotEqual(output.find(f'extern int16_t {controlVar}'), -1)

                stringTablePattern = rf'StringTable{length}\s+{name}_stringTable\s+{{\s*&\s*{controlVar}\s*,\s*{initialValue}\s*,\s*'
                for string in strings:
                    stringTablePattern += rf'{string}\s*,\s*'
                stringTablePattern += r'[^}]*};'

                self.assertRegex(output, stringTablePattern)
        else:
            self.assertNotRegex(output, kStringTableDeclarationPattern.format(length=r'\d+'))

    def verifyPragmaPackAroundStringTable(self, stringTableRegex, output, i):
        assert isinstance(stringTableRegex, type(re.compile('')))
        assert isinstance(output, str)
        assert isinstance(i, int)

        match = stringTableRegex.search(output)
        if i == 0:
            self.assertEqual(output[match.start() - len(kPackPragmaOn) - 1 : match.start() - 1], kPackPragmaOn)
        else:
            self.assertIn(kPackPragmaOn, output[:match.start() - 1])
        self.assertIn(output[match.end() + 1:], output)

    @data(*kTestIndices)
    def testEntriesEnum(self, index):
        output = self.outputs[index]
        menusWithNamedEntries = self.getMenusWithNamedEntries(output)

        expectedNamedEntries = kTestData[index][1].get('namedEntries')
        if expectedNamedEntries:
            self.assertEqual(len(expectedNamedEntries), len(menusWithNamedEntries))

            for menuName, entryIndices in expectedNamedEntries.items():
                self.assertIn(menuName, menusWithNamedEntries)
                enumValues = menusWithNamedEntries[menuName]

                for entryName, expectedIndex in entryIndices:
                    enumEntryNames = list(map(operator.itemgetter(0), enumValues))
                    self.assertIn(entryName, enumEntryNames)
                    index = enumEntryNames.index(entryName)
                    actualEntryIndex = enumValues[index][1]
                    self.assertEqual(int(actualEntryIndex), expectedIndex)
        else:
            self.assertFalse(menusWithNamedEntries)

    def getMenusWithNamedEntries(self, output):
        assert isinstance(output, str)

        matches = kMenuRegex.finditer(output)
        menusWithNamedEntries = {}

        for match in matches:
            menuName = match['name']
            self.assertEqual(menuName, match['declarationName'])
            self.assertEqual(menuName, match['enumName'])

            enumEntries = match['enumEntries']
            enumValues = kEnumEntryRegex.findall(enumEntries)

            if enumValues:
                menusWithNamedEntries[menuName] = enumValues

        return menusWithNamedEntries

    @data(*kTestIndices)
    def testExportedVariables(self, index):
        output = self.outputs[index]
        menusWithExportedVariables = self.getMenusWithExportedVariables(output)

        expectedExportedVariableEntries = kTestData[index][1].get('exportedVariables')
        if expectedExportedVariableEntries:
            self.assertEqual(len(expectedExportedVariableEntries), len(menusWithExportedVariables))

            for menuName, exportedVariablesAndValues in expectedExportedVariableEntries.items():
                self.assertIn(menuName, menusWithExportedVariables)
                exportedVariableValues = menusWithExportedVariables[menuName]

                for variable, expectedValue in exportedVariablesAndValues:
                    exportedVariableNames = list(map(operator.itemgetter(0), exportedVariableValues))
                    self.assertIn(variable, exportedVariableNames)
                    index = exportedVariableNames.index(variable)
                    actualValue = exportedVariableValues[index][1]
                    self.assertEqual(actualValue, expectedValue)
        else:
            self.assertFalse(menusWithExportedVariables)

    @staticmethod
    def getMenusWithExportedVariables(output):
        assert isinstance(output, str)

        matches = kMenuRegex.finditer(output)
        menusWithExportedVariables = {}

        for match in matches:
            menuName = match['name']
            exportedVariablesData = match['constants']
            exportedVariables = kConstantsRegex.findall(exportedVariablesData)

            if exportedVariables:
                menusWithExportedVariables[menuName] = exportedVariables

        return menusWithExportedVariables

    @data(*kTestIndices)
    def testMenuElements(self, index):
        output = self.outputs[index]
        actualMenuElements = self.getMenuElements(output)

        expectedMenuElements = kTestData[index][1].get('menuElements')
        if expectedMenuElements:
            self.maxDiff = None
            self.assertEqual(actualMenuElements, expectedMenuElements)
        else:
            self.assertFalse(actualMenuElements)

    def getMenuElements(self, output):
        assert isinstance(output, str)

        matches = kMenuRegex.finditer(output)
        menuElements = {}

        for match in matches:
            menuName = match['name']
            menuContents = match['contents']
            menuElements[menuName] = []
            seenMenuIds = set()

            for match in kMenuElementsRegex.finditer(menuContents):
                id = match['id']
                type = match['type']

                self.assertNotIn(id, seenMenuIds)
                seenMenuIds.add(id)

                element = {}
                element['id'] = id
                element['type'] = match['type']
                element['params'] = []

                if match['params']:
                    element['params'] = list(map(str.strip, match['params'].split(',')))
                    if type == 'Entry':
                        element['params'] = list(str(eval(expr)) for expr in element['params'])

                menuElements[menuName].append(element)

        return menuElements

    @data(*kTestIndices)
    def testSpacing(self, index):
        output = self.outputs[index]

        inMenu = False
        inEntry = False

        kRegexStateChange = (
            # regex, other fields contain booleans for expected & new values for inMenu & inEntry (if changing)
            (r'struct\s+SWOS_Menu_\w+\s*:\s*public\s*BaseMenu', False, False, True,  None),
            (r'}\s*static\s+const\s+\w+\s*;',                   True,  False, False, None),
            (r'Entry\s+\w+\s*{[^}]+}\s*;',                      True,  False, None,  True),
            (r'EntryEnd\s+\w+\s*{\s*}\s*;',                     True,  True,  None,  False),
            (r'static\s+void\s+\w+\(\)',                        False, False, None,  None),
        )

        for line in output.split('\n'):
            if not line:
                continue

            for regex, expectedInMenu, expectedInEntry, newInMenu, newInEntry in kRegexStateChange:
                if re.search(regex, line):
                    self.assertEqual(expectedInMenu, inMenu)
                    self.assertEqual(expectedInEntry, inEntry)
                    if newInMenu is not None:
                        inMenu = newInMenu
                    if newInEntry is not None:
                        inEntry = newInEntry
                    break
            else:
                if inMenu:
                    match = re.match(r'\s*({|})', line)
                    if match:
                        self.assertEqual(match.start(), 0)
                    else:
                        self.assertTrue(line.startswith(' ' * (8 if inEntry else 4)))

        self.assertFalse(inMenu)
        self.assertFalse(inEntry)

    @data(*kTestIndices)
    def testStubs(self, index):
        output = self.outputs[index]

        expectedStringTables = kTestData[index][1].get('stringTables')
        expectedFunctions = kTestData[index][1].get('functions')

        numExpectedStubDefs = 0
        if expectedStringTables:
            numExpectedStubDefs += len(expectedStringTables[1])
        if expectedFunctions:
            numExpectedStubDefs += 1

        matches = kStubDefineRegex.findall(output)

        self.assertEqual(len(matches), numExpectedStubDefs)

        currentMatch = 0
        if expectedStringTables:
            for stringTable in expectedStringTables[1]:
                controlVar = stringTable[1]
                self.assertIn(controlVar, matches[currentMatch])
                currentMatch += 1

        if expectedFunctions:
            for expectedFunction in expectedFunctions:
                self.assertIn(expectedFunction, matches[currentMatch])

    @data(*kTestIndices)
    def testSingleNewLineAtTheEndOfTheFile(self, index):
        output = self.outputs[index]
        self.assertEqual(output[-1], '\n')
        self.assertNotEqual(output[-2], '\n')

    @staticmethod
    @mock.patch('CodeGenerator.open')
    @mock.patch('builtins.print')
    def getCodeGeneratorOutput(input, mockPrint=None, mockOpen=None):
        assert isinstance(input, str)

        parser = getParserWithData(input, filePath=kInputFile)
        CodeGenerator.output(parser, kInputFile, kOutputFile)

        output = ''
        for call in mockPrint.call_args_list:
            for arg in call[0]:
                output += arg
            output += '\n'

        return parser, output
