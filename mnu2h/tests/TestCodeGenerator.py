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
                {'id': 'header',  'type': 'MenuHeader', 'params': ['0', '0', '0', '0']},
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
            onDestroy: DirtWeydaar
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
                stringTable: [ swos.start, "HOLD", 'ME', "NOW" ]
            }
            Entry {
                customDrawForeground: drawYellow
                customDrawBackground: swos.drawRed
                downEntry: >
                upEntry: <
            }
            TemplateEntry {
                stringTable: [ four, "1", "2", '3', '4' ]
            }
            Entry CasaBrava {
                invisible: true
                onReturn: returnOfThePhantom
                beforeDraw: prepareWell
                controlMask: #{@kControlDown | @kControlRight | @kControlFire}:#x
                onSelect: pumpItUp
                textFlags: @kBigText
                text: @kAlloc
                leftEntry: <
                rightEntry: CasaNostra
                downEntry: CasaNostra
                upEntry: CasaNostra
            }
            Entry MoreThanMeetsTheEye {
                y: #{2 * @prevY + 100}
                text: swos.aMenuMusic
            }
        }
        Menu Bravo {
            onInit: swos.someCoolInit
            initialEntry: MoonLander

            defaultLeftEntry: 12
            defaultRightEntry: 15
            defaultTopEntry: Spacy

            Entry {
                x: #{0x64} + 2
                width: 25
                height: 36
                stringTable: [ mirror, 'WARM', MY, "HEART" ]
                color: @kRed
                onSelect: uncleSamNeedsYOU
            }
            Entry MoonLander {
                x: #{"3 + @prevX"}:t
                number: 1969
            }
            Entry Secret {
                text: Admirer
            }
            Entry Spacy {
                stringTable: ~Video
            }
        }
        Menu Tango {
            onInit: ~specialForces
            export TangoAndCash = 1989
            x: 123
            y: 55

            TemplateEntry {
                stringTable: [ ~Indianapolis, 2002, "1", swos.aWinners ]
                color: @kLightBrownWithYellowFrame
                beforeDraw: aperitif
            }

            Entry Madera {
                color: @kNoBackground
                sprite: 1001
            }
            Entry Durmitor {
                color: @kRed
                multilineText: [ 'Gael', 'Monfils' ]
            }
            Entry Jerry {
                menuSpecificSprite: 0x100
            }

            ResetTemplate {}

            x: 125
            y: 35

            Entry Avalanche {
                color: @kNoBackground
                skipLeft: 2
                directionRight: 1
                skipRight: 0
                multilineText: 0
            }

            entryAlias Landslide = Avalanche
            entryAlias Lovac = Madera

            Entry {
                stringTable: swos.SmoothJazz101DaPierre
                directionRight: @kDown
            }
            Entry {
                stringTable: [ extern flannel, -2, "WOOPTEE", "DOO" ]
            }
            Entry {
                text: @kNull
            }
            Entry {
                multilineText: @kNull
            }
            Entry {
                boolOption: getSpeed, setSpeed, "I said speed, speed Give me what I need"
            }
        }
        Menu Delta {}
        Menu mali {}
    ''', {
        'functions': (
            'someCoolInit', 'hereIAm', 'danceSnake', 'DirtWeydaar', 'uncleSamNeedsYOU', 'drawYellow',
            'returnOfThePhantom', 'prepareWell', 'pumpItUp', 'aperitif',
        ),
        'menus': ['Charlie', 'Bravo', 'Tango', 'Delta', 'mali'],
        'stringTables': ((2, 3, 4), (  # lengths when declared
            # declared / extern / elements
            (False, False, 'Charlie_CasaNostra', 'reinterpret_cast<int16_t *>(SwosVM::Offsets::start)', '0', "'HOLD'", '"ME"', '"NOW"'),
            (True, False, 'Bravo_00', '&mirror', '0', '"WARM"', 'MY', '"HEART"'),
            (False, False, 'Tango_30000', '&Indianapolis', '2002', '"1"', 'reinterpret_cast<const char *>(SwosVM::Offsets::aWinners)'),
            (False, True, 'Tango_05', '&flannel', -2, '"WOOPTEE"', '"DOO"'),
        )),
        'namedEntries': {
            'Charlie': (('CasaNostra', 0), ('CasaBrava', 2), ('MoreThanMeetsTheEye', 3)),
            'Bravo': (('MoonLander', 1),),
            'Tango': (('Madera', 0), ('Durmitor', 1), ('Jerry', 2), ('Avalanche', 3), ('Lovac', 0), ('Landslide', 3),),
        },
        'exportedVariables': {
            'Charlie': (('kBlazeOfGlory', '1990'), ('SantaFe', '2194'),),
            'Tango': (('TangoAndCash', '1989'),),
        },
        'menuElements': {
            'Bravo': [
                {'id': 'header', 'type': 'MenuHeader',  'params': ['SwosVM::Procs::someCoolInit', '0', '0', '1']},

                {'id': 'eb00',   'type': 'Entry',       'params': ['102', '0', '25', '36']},
                {'id': 'est00',  'type': 'EntryStringTableNative', 'params': ['0', '&Bravo_00_stringTable']},
                {'id': 'ec00',   'type': 'EntryColor',  'params': ['10']},
                {'id': 'ep00',   'type': 'EntryNextPositions', 'params': ['12', '15', '3', '255']},
                {'id': 'eosf00', 'type': 'EntryOnSelectFunctionNative', 'params': ['uncleSamNeedsYOU']},
                {'id': 'ee00',   'type': 'EntryEnd',    'params': []},

                {'id': 'eb01',   'type': 'Entry',       'params': ['105', '0', '0', '0']},
                {'id': 'en01',   'type': 'EntryNumber', 'params': ['0', '1969']},
                {'id': 'ep01',   'type': 'EntryNextPositions', 'params': ['12', '15', '3', '255']},
                {'id': 'ee01',   'type': 'EntryEnd',    'params': []},

                {'id': 'eb02',   'type': 'Entry',       'params': ['0', '0', '0', '0']},
                {'id': 'et02',   'type': 'EntryTextNative', 'params': ['0', 'Admirer']},
                {'id': 'ep02',   'type': 'EntryNextPositions', 'params': ['12', '15', '3', '255']},
                {'id': 'ee02',   'type': 'EntryEnd',    'params': []},

                {'id': 'eb03',   'type': 'Entry',       'params': ['0', '0', '0', '0']},
                {'id': 'est03',  'type': 'EntryStringTableNative', 'params': ['0', 'reinterpret_cast<StringTable *>(&Video)']},
                {'id': 'ep03',   'type': 'EntryNextPositions', 'params': ['12', '15', '3', '255']},
                {'id': 'ee03',   'type': 'EntryEnd',    'params': []},

                {'id': 'menuEnd', 'type': 'MenuEnd',    'params': []},
            ],
            'Charlie': [
                {'id': 'header', 'type': 'MenuHeaderV2', 'params': ['kMenuHeaderV2Mark', 'someCoolInit',
                    'hereIAm', 'danceSnake', 'DirtWeydaar', '2', 'true', 'true', 'true', 'true']},

                {'id': 'eb00',   'type': 'Entry',       'params': ['49', '158', '99', '17']},
                {'id': 'est00',  'type': 'EntryStringTableNative', 'params': ['0', '&Charlie_CasaNostra_stringTable']},
                {'id': 'ec00',   'type': 'EntryColor',  'params': ['11']},
                {'id': 'ee00',   'type': 'EntryEnd',    'params': []},

                {'id': 'eb01',   'type': 'Entry',       'params': ['59', '168', '99', '17']},
                {'id': 'ecff01', 'type': 'EntryCustomForegroundFunctionNative', 'params': ['drawYellow']},
                {'id': 'ecbf01', 'type': 'EntryCustomBackgroundFunction', 'params': ['SwosVM::Procs::drawRed']},
                {'id': 'ep01',   'type': 'EntryNextPositions', 'params': ['255', '255', '0', '2']},
                {'id': 'ee01',   'type': 'EntryEnd',    'params': []},

                {'id': 'te00',   'type': 'TemplateEntry', 'params': []},
                {'id': 'eb30002', 'type': 'Entry',       'params': ['0', '0', '1', '1']},
                {'id': 'est30002', 'type': 'EntryStringTableNative', 'params': ['0', '&Charlie_30002_stringTable']},
                {'id': 'ee30002', 'params': [], 'type': 'EntryEnd'},

                {'id': 'eb02',   'type': 'Entry',       'params': ['69', '178', '99', '17']},
                {'id': 'et02',   'type': 'EntryText',   'params': ['16', '-1']},
                {'id': 'ei02',   'type': 'EntryInvisible', 'params': []},
                {'id': 'ep02',   'type': 'EntryNextPositions', 'params': ['1', '0', '0', '0']},
                {'id': 'eosfm02', 'type': 'EntryOnSelectFunctionWithMaskNative', 'params': ['pumpItUp', '0x15']},
                {'id': 'ebdf02', 'type': 'EntryBeforeDrawFunctionNative', 'params': ['prepareWell']},
                {'id': 'eadf02', 'type': 'EntryOnReturnFunctionNative', 'params': ['returnOfThePhantom']},
                {'id': 'ee02',   'type': 'EntryEnd',              'params': [], },

                {'id': 'eb03',   'type': 'Entry',       'params': ['79', '456', '99', '17']},
                {'id': 'et03',   'type': 'EntryText',   'params': ['0', 'SwosVM::Offsets::aMenuMusic']},
                {'id': 'ee03',   'type': 'EntryEnd',    'params': []},

                {'id': 'menuEnd', 'type': 'MenuEnd',    'params': []},
            ],
            'Tango': [
                {'id': 'header',  'type': 'MenuHeaderV2', 'params': ['kMenuHeaderV2Mark', 'specialForces',
                    '0', '0', '0', '0', 'true', 'false', 'false', 'false']},
                {'id': 'menuXY00', 'type': 'MenuXY',    'params': ['123', '55'],},

                {'id': 'te00',    'type': 'TemplateEntry', 'params': []},
                {'id': 'eb30000', 'type': 'Entry',      'params': ['0', '0', '1', '1']},
                {'id': 'est30000', 'type': 'EntryStringTableNative', 'params': ['0', '&Tango_30000_stringTable']},
                {'id': 'ec30000', 'type': 'EntryColor', 'params': ['6']},
                {'id': 'ebdf30000', 'type': 'EntryBeforeDrawFunctionNative', 'params': ['aperitif']},
                {'id': 'ee30000', 'type': 'EntryEnd',   'params': []},

                {'id': 'eb00',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'efs00',   'type': 'EntryForegroundSprite', 'params': ['1001']},
                {'id': 'ec00',    'type': 'EntryColor', 'params': ['0']},
                {'id': 'ee00',    'type': 'EntryEnd',   'params': []},

                {'id': 'eb01',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'emt01',   'type': 'EntryMultilineTextNative', 'params':
                    ['0', 'reinterpret_cast<MultilineText *>(&Tango_Durmitor_multilineText)']},
                {'id': 'ec01',    'type': 'EntryColor', 'params': ['10']},
                {'id': 'ee01',    'type': 'EntryEnd',   'params': []},

                {'id': 'eb02',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'emss02',  'type': 'EntryMenuSpecificSprite', 'params': []},
                {'id': 'ee02',    'type': 'EntryEnd',    'params': []},

                {'id': 'rte00',   'type': 'ResetTemplateEntry', 'params': []},

                {'id': 'menuXY03', 'type': 'MenuXY',    'params': ['125', '35']},

                {'id': 'eb03',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'emt03',   'type': 'EntryMultilineText', 'params': ['0', 'nullptr']},
                {'id': 'els03',   'type': 'EntryLeftSkip', 'params': ['2', '0']},
                {'id': 'ers03',   'type': 'EntryRightSkip', 'params': ['0', '1']},
                {'id': 'ee03',    'type': 'EntryEnd',   'params': []},

                {'id': 'eb04',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'est04',   'type': 'EntryStringTable', 'params': ['0', 'SwosVM::Offsets::SmoothJazz101DaPierre']},
                {'id': 'ers04',   'type': 'EntryRightSkip',   'params': ['255', '3']},
                {'id': 'ee04',    'type': 'EntryEnd',   'params': []},

                {'id': 'eb05',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'est05',   'type': 'EntryStringTableNative', 'params': ['0', '&Tango_05_stringTable']},
                {'id': 'ee05',    'type': 'EntryEnd',   'params': []},

                {'id': 'eb06',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'et06',    'type': 'EntryText',  'params': ['0', 'nullptr']},
                {'id': 'ee06',    'type': 'EntryEnd',   'params': []},

                {'id': 'eb07',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'emt07',   'type': 'EntryMultilineText', 'params': ['0', 'nullptr']},
                {'id': 'ee07',    'type': 'EntryEnd',   'params': []},

                {'id': 'eb08',    'type': 'Entry',      'params': ['0', '0', '0', '0']},
                {'id': 'ebo08',   'type': 'EntryBoolOption', 'params':
                    ['getSpeed', 'setSpeed', '"I said speed, speed Give me what I need"']},
                {'id': 'ee08',    'type': 'EntryEnd',   'params': []},

                {'id': 'menuEnd', 'type': 'MenuEnd',    'params': []},
            ],
            'Delta': [
                {'id': 'header',  'type': 'MenuHeader', 'params': ['0', '0', '0', '0']},
                {'id': 'menuEnd', 'type': 'MenuEnd',    'params': []},
            ],
            'mali': [
                {'id': 'header',  'type': 'MenuHeader', 'params': ['0', '0', '0', '0']},
                {'id': 'menuEnd', 'type': 'MenuEnd',    'params': []},
            ],
        },
    }),
)
kTestIndices = range(len(kTestData))
kInputFile = 'Ibn'
kOutputFile = 'Khaldun'

kMenuRegex = re.compile(r'''
    struct\s+SwosMenu_(?P<name>\w+)\s* : \s*public\s+BaseMenu\b\s*
    (?P<contents>{[^#]+(?=\#ifndef\s*SWOS_STUB_MENU_DATA)).*?
    \#ifndef\s+SWOS_STUB_MENU_DATA
    \s*static\s+const\s+(?P<declarationName>\w+)\n
    \#endif\n;\n
    \s*namespace\s+(?P<enumName>\w+?)\s*{\s*
    enum\s+Entries\s*{
    (?P<enumEntries>[^}]+)}\s*;
    ([^\/]+\/\/.*?\n(?P<entryReferences>(?:\s*static\s+auto\s*&\s.*?reinterpret_cast<[^>]+>.*?MenuEntry[^;]+;)+))?
    (?P<constants>([^}]+))?
    ''',
    re.DOTALL | re.VERBOSE)
kFuncRegex = re.compile(r'static void (\w+)\s*\(\s*\)\s*;')
kStringTableDeclarationPattern = r'struct\s+StringTableNative{length}\s*:\s*public\s+StringTableNative\s*{{(.*?)}};'
kEnumEntryRegex = re.compile(r'(\w+)\s*=\s*(\d+)\s*,')
kEntryReferenceRegex = re.compile(r'\s*static\s+auto\s*&\s+(\w+)[^\d]+(\d+)\s*\*[^;]+;')
kConstantsRegex = re.compile(r'constexpr\s+int\s+(\w+)\s*=\s*(\d+)')
kMenuElementsRegex = re.compile(r'''
    (?P<type>\w+)\s+(?P<id>\w+)\s*{\s*
    (?P<params>[^}]+\s*)?
    }?}\s*;\s*
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
        start = output.find('using namespace SwosMenu;')
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
                self.verifyPragmaPackAroundStringTable(stringTableRegex, output, i)
            for declared, extern, *stringTableContent in expectedStringTables[1]:
                name = stringTableContent[0]
                controlVar = stringTableContent[1]
                initialValue = stringTableContent[2]
                strings = stringTableContent[3:]
                length = len(stringTableContent) - 3

                if declared or extern:
                    storageClass = 'static' if declared else 'extern'
                    self.assertNotEqual(output.find(f'{storageClass} int16_t {controlVar.replace("&", "")}'), -1)

                stringTablePattern = rf'StringTableNative{length}\s+{name}_stringTable\s+{{\s*{re.escape(controlVar)}\s*,\s*{initialValue}\s*,\s*'
                for string in strings:
                    if string[0] == string[-1] == "'":
                        string = f'"{string[1:-1]}"'
                    stringTablePattern += rf'{re.escape(string)}\s*,\s*'
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
                enumValues, entryReferenceValues = menusWithNamedEntries[menuName]

                self.assertTrue(all(ref[0].endswith('Entry') for ref in entryReferenceValues))

                for entryName, expectedIndex in entryIndices:
                    enumEntryNames = list(map(operator.itemgetter(0), enumValues))
                    self.assertEqual(enumEntryNames, list(map(lambda ref: ref[0][:-5], entryReferenceValues)))
                    self.assertIn(entryName, enumEntryNames)

                    index = enumEntryNames.index(entryName)
                    actualEntryIndex = enumValues[index][1]
                    self.assertEqual((entryName, int(actualEntryIndex)), (entryName, expectedIndex))
                    self.assertEqual(actualEntryIndex, entryReferenceValues[index][1])
        else:
            self.assertFalse(menusWithNamedEntries)

    def getMenusWithNamedEntries(self, output):
        assert isinstance(output, str)

        matches = kMenuRegex.finditer(output)
        menusWithNamedEntries = {}

        for match in matches:
            menuName = match['name']
            self.assertEqual(menuName, match['declarationName'])

            enumName = match['enumName']
            if menuName[0].isupper():
                enumName = enumName[:-2]
            else:
                enumName = enumName[0].lower() + enumName[1:]

            self.assertEqual(menuName, enumName)

            enumEntries = match['enumEntries']
            enumValues = kEnumEntryRegex.findall(enumEntries)

            entryReferences = match['entryReferences']
            entryReferenceValues = kEntryReferenceRegex.findall(entryReferences) if entryReferences else []

            if enumValues:
                menusWithNamedEntries[menuName] = (enumValues, entryReferenceValues)

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
                    paramList = match['params'].replace('{', '')
                    # comma separated list, but not if comma is within quotes (' or ") (doesn't handle escaped quotes)
                    paramList = re.findall(r'((?:"[^"]+")|(?:\'[^\']+\')|(?:[^,]+))(?:\s*,\s*)?', paramList)
                    paramList = map(str.strip, paramList)
                    paramList = filter(None, paramList)

                    element['params'] = list(paramList)
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
            (r'struct\s+SwosMenu_\w+\s*:\s*public\s*BaseMenu', False, False, True,  None),
            (r'#ifndef SWOS_STUB_MENU_DATA',                    True,  False, False, None),
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

        expectedFunctions = kTestData[index][1].get('functions')

        if expectedFunctions:
            match = kStubDefineRegex.search(output)
            self.assertTrue(match)
            for expectedFunction in expectedFunctions:
                self.assertIn(expectedFunction, match[0])

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
            for arg in call.args:
                output += arg
            end = call.kwargs.get('end', '\n')
            output += end

        return parser, output
