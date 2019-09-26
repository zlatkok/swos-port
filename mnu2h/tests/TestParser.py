import os
import unittest

from unittest import mock
from ddt import ddt, data

from Token import Token
from Parser import Parser
from StringTable import StringTable
from TestHelper import getParserWithData as getParserWithData

import TestHelper
import Util

@ddt
class TestParser(unittest.TestCase):
    def testEmptyFile(self):
        getParserWithData(ensureNewLine=False)

    @data(
        # note: ` will be replaced with \n (improves readability)
        # top level parsing errors
        ('}', 1, 'unmatched block end'),
        ('Entry', 1, 'entries can only be used inside menus'),
        ('ping', 1, "unexpected end of file while looking for an equal sign (`=')"),
        ('ping ` pong', 2, "expected equal `=', got `pong'"),
        ('ping = ``', 1, "unexpected end of file while looking for an expression start"),
        ('ping = -+``', 1, "operand expected, got `-'"),
        ('ping =`(57 24``', 2, 'unmatched parenthesis'),
        ('a =`#include "something"', 2, "directive `include' not allowed in this context"),
        ('a = )', 1, 'unmatched parenthesis'),
        ('a = (5``()', 3, 'unmatched parenthesis'),
        ('a = (5))', 1, "unexpected `)'"),
        ('a = (((`5`))', 3, "unexpected end of file while looking for a closing parenthesis `)'"),
        ('a = +', 1, "operand expected, got `+'"),
        ('a`=`"HEY"[', 3, 'unexpected end of file while looking for a string index value'),
        ('a`=`"YOU".', 3, 'unexpected end of file while looking for a string function'),
        ('a`=`"BMX".run', 3, "unexpected end of file while looking for a function parameter list start `('"),
        ('a`=`"CAR".lower(', 3, 'unexpected end of file'),
        ('a`=`"TOY".lower(1)', 3, 'invalid number of parameters for lower(), 0 expected, got 1'),
        ('a`=`"OUR".lower(brmbrm)', 3, "function parameter `brmbrm' is not numeric"),
        ('a`=`"OUT".lower(,)', 3, 'function parameter missing'),
        ('a`=`"STD".lower(#)', 3, "unknown preprocessor directive `)'"),
        ('a`=`"CUT".lower(12,)', 3, 'function parameter missing'),
        ('a`=`"BUT".lower(12 yeees?)', 3, "`yeees' unexpected, only `,' or `)' allowed"),
        ('a`=`"DIP".brmbrm(3,5)', 3, "`brmbrm' is not a known string function"),
        ('a`=`"GOW"[49]', 3, "index 49 out of bounds for string `GOW'"),

        # menu errors
        ('Menu', 1, 'unexpected end of file while looking for a menu name'),
        ('Menu `{}', 2, 'menu name missing'),
        ('Menu Tetrahedron {}`Menu Tetrahedron {}', 2, "menu `Tetrahedron' redeclared"),
        ('Menu virtual }', 1, "can't use a C++ keyword `virtual' as a menu name"),
        ('Menu `25 }', 2, "`25' is not a valid identifier"),
        ('Menu Foreign `Affair', 2, "block start `{' expected, got `Affair'"),
        ('Menu`Tetrahedron`{`export`default = 56 }', 5, "can't use a C++ keyword `default' as an exported variable name"),
        ('Menu Tetrahedron`{ 1 = 2 }', 2, "`1' is not a valid identifier"),
        ('Menu Tetrahedron`{ bomba`.`25 = 2 }', 4, "`25' is not a valid identifier"),
        ('Menu Tetrahedron`{`export`x: 23 }', 4, "Menu properties can't be exported"),
        ('Menu Tetrahedron`{ myVar = 2`myVar = 3 }', 3, "variable `myVar' redefined"),
        ('Menu Tetrahedron`{ myVar: 2 }', 2, "unknown menu property: `myVar'"),
        ('Menu Tetrahedron`{ x:` }', 2, "missing property value for `x'"),
        ('Menu Tetrahedron`{ `x `oops }', 4, "expecting menu variable assignment or property, got `oops'"),
        ('Menu Tetrahedron`{`Entry {`width:`@kTextWidth + 3`number: 156 }`}', 3,
            'non-text entry uses @kTextWidth/@kTextHeight constants'),
        ('Menu Tetrahedron`{`Entry {`width:`@kTextWidth + 3`text: "GLITCH"`textFlags: "WOOP-WOOP"}`}', 3,
            "unable to convert string `\"WOOP-WOOP\"' to integer"),
        ('Menu Tetrahedron`{`Entry {`stringTable: [ initVar, aStr1, aStr2 ]`width: @kTextWidth`}`}', 3,
            "`aStr1' is not a string, can't measure"),
        ('Menu Tetrahedron`{`Entry sonicBoom`{}`sonicBoom.leftEntry = vastEmptiness`}', 5, "entry `vastEmptiness' undefined"),
        ('Menu Tetrahedron`{`Entry sho {`x:1337 }`go.x = 93 }', 5, "menu `Tetrahedron' does not have entry `go'"),
        ('`x=`@previousEndX', 3, 'attempting to use a menu variable outside of a menu'),
        ('yummie =`IceCream`.`Stracciatella', 2, "dot operator (`.') can only be used inside menus"),
        ('owie`=`@previousX', 3, 'attempting to use a menu variable outside of a menu'),
        ('#warningsOff`Menu Tetrahedron`{`Entry`BlodyIsland`{}`export`BlodyIsland`=1850`}', 8,
            "can't export constant `BlodyIsland' since there is an entry with same name"),

        # entry errors
        ('Menu Tetrahedron { yummie`=`IceCream`.Stracciatella', 3, "`IceCream' is not recognized as an entry name"),
        ('Menu Tetrahedron {`Entry`Dot`{ x : Dot`.`dimensions', 6, "entry `Dot' does not have property `dimensions'"),
        ('Menu Tetrahedron { Entry`{ upEntry: < } }', 2, 'referencing entry before the first one'),
        ('Menu Tetrahedron { Entry`{ downEntry: > } }', 2, 'referencing entry beyond the last one'),
        ('Menu Tetrahedron { Entry`{ downEntry: Loonie } }', 2, "undefined entry reference: `Loonie'"),
        ('Menu Tetrahedron { Entry`Zyx{} Zyx.prp = 156 }', 2, "entry `Zyx' does not have property `prp'"),
        ('Menu Sonny`{ Entry`22 {} }', 3, "`22' is not a valid entry name"),
        ('Menu Sonny`{ Entry`#{23} {} }', 3, "`23' is not a valid entry name"),
        ('Menu Sonny`{ Entry Tubbs {}`Entry`Tubbs {} }', 4, "entry `Tubbs' already declared"),
        ('Menu Sonny`{ Entry`switch } }', 3, "can't use a C++ keyword `switch' as an entry name"),
        ('Menu Sonny`{ Entry`Tubbs`{`Entry {} } }', 5, 'entries can not be nested'),
        ('Menu Sonny`{ Entry`Tubbs`{ unknownProperty: 56 }', 4, "unknown property `unknownProperty'"),
        ('Menu Sonny`{ Entry Tubbs`{ name: "RENAMED" }', 3, "property `name' can not be assigned"),
        ('Menu Sonny`{ Entry Tubbs`{`text: "MORNING AFTER"`number: 1 }', 5,
            "can't use properties `number' and `text' at the same time"),
        ('Menu Sonny`{ Entry`{`text`:`} }', 4, "missing property value for `text'"),
        ('Menu Sonny`{ Entry`{`text`}`}', 5, "expected colon `:', got `}'"),
        ('Menu Sonny`{ Entry gurt`{}`}', 2, "entry `gurt' has zero width and height"),
        ('Menu Sonny`{ Entry gurtz`{ width: 153 }`}', 2, "entry `gurtz' has zero height"),
        ('Menu Sonny`{ Entry gurtzl`{ height: 442 }`}', 2, "entry `gurtzl' has zero width"),

        # string table errors
        ('Menu Djoker`{`Entry { stringTable:`auto } }', 4, "can't use a C++ keyword `auto' as a string table name"),
        ('Menu Djoker`{`Entry { stringTable:`~`25 } }', 5, "`25' is not a valid identifier"),
        ('Menu Djoker`{`Entry { stringTable:`abrakadabra } }', 4, "expected `[', but got `abrakadabra' instead"),
        ('Menu Djoker`{`Entry { stringTable: [`25`} }', 4, "`25' is not a valid identifier"),
        ('Menu Djoker`{`Entry { stringTable: [ abrakadabra`DJ } }', 4, "expected `,', but got `DJ' instead"),
        ('Menu Djoker`{`Entry { stringTable: [ abrakadabra,`12 } }', 4, "expected `,', but got `}' instead"),
        ('Menu Djoker`{`Entry { stringTable: [ abrakadabra,`DJ, } }', 4, "expecting string or identifier, got `}'"),
        ('Menu Djoker`{`Entry { stringTable: [ abrakadabra,`DJ, "HMM" } }', 4, "expected `,', but got `}' instead"),

        # function errors
        ('Menu Hightower`{`onInit:', 3, "missing property value for `onInit'"),
        ('Menu Hightower`{`onInit: else', 3, "can't use a C++ keyword `else' as a name of handler function"),
        ('Menu Hightower`{`onInit: ~', 3, 'unexpected end of file while looking for a handler function'),
        ('Menu Hightower`{`onInit: 25_33', 3, "expected handler function, got `25_33'"),

        # preprocessor errors
        ('#', 1, 'unexpected end of file'),
        ('#22', 1, "unknown preprocessor directive `22'"),
        ('#rumbasambachachacha', 1, "unknown preprocessor directive `rumbasambachachacha'"),
        ('#rumbasambachachacha something', 1, "unknown preprocessor directive `rumbasambachachacha'"),
        ('#include ', 1, 'unexpected end of file while looking for an include filename'),
        ('#include`what?', 2, "expecting quoted file name, got `what'"),
        ('#{201}`:blabla', 2, "invalid format specifier: `blabla'"),
        ('#{202}`:', 2, 'unexpected end of file while looking for a width specification'),
        ('#join`(`bob`,`#{203}:+5)', 1, "`bob +203' is not a valid identifier"),

        ('#textWidth', 1, "unexpected end of file while looking for `('"),
        ('#textWidth`pft', 2, "expected `(', but got `pft' instead"),
        ('#textWidth(', 1, "unexpected end of file"),
        ('#textWidth(`blabla', 2, "expected `)', but got `blabla' instead"),
        ('#textWidth(`"SHAWSHANK"', 2, "unexpected end of file while looking for a comma `,' or closing parentheses `)'"),
        ('#textWidth("SHAWSHANK"`eek', 2, "expected `)', but got `eek' instead"),
        ('#textWidth(`"SHAWSHANK"`,', 3, 'unexpected end of file while looking for a font size'),
        ('#textWidth("SHAWSHANK"`,`blurg`', 3, "`blurg' is not a numeric value'"),

        ('#textHeight', 1, "unexpected end of file while looking for `('"),
        ('#textHeight pft', 1, "expected `(', but got `pft' instead"),
        ('#textHeight(`', 1, "unexpected end of file"),
        ('#textHeight(blabla', 1, "expected a string, but got `blabla' instead"),
        ('#textHeight("SHAWSHANK"', 1, "unexpected end of file while looking for a comma `,' or closing parentheses `)'"),
        ('#textHeight(``"SHAWSHANK" eek', 3, "expected `)', but got `eek' instead"),
        ('#textHeight(`"SHAWSHANK",', 2, 'unexpected end of file while looking for a font size'),
        ('#textHeight(``"SHAWSHANK",`blurg', 4, "`blurg' is not a numeric value'"),

        ('#join', 1, "unexpected end of file while looking for `('"),
        ('#join(', 1, 'unexpected end of file'),
        ('#join(`glb', 2, "unexpected end of file while looking for `)' or `,'"),
        ('#join(`glb`,', 3, 'unexpected end of file'),
        ('#join(`glb,`)', 3, 'value to join missing'),
        ('#join(`,`)', 2, 'value to join missing'),
        ('#join(,`,`,)', 1, 'value to join missing'),
        ('#join(`)', 2, "value to join missing"),
        ('#join(beton`)', 1, "unexpected end of file while looking for an equal sign (`=')"),
        ('#join(pesak`malter)', 2, "`malter' unexpected, only `,' or `)' allowed"),

        ('#repeat', 1, 'repeat count is missing'),
        ('#repeat`repeat', 2, "repeat count expression must not contain uninitialized variables (`repeat')"),
        ('#repeat`repeat`repeat', 2, "repeat count expression must not contain uninitialized variables (`repeat')"),
        ('#{uni3=5}`#repeat`uni1`+`uni2`*`uni3`#endRepeat', 3,
            "repeat count expression must not contain uninitialized variables (`uni1', `uni2')"),
        ('Menu King`{`#repeat`5`Entry`{ x: #i } }', 3, 'unterminated repeat directive'),
        ('#repeat`+', 2, "unable to convert token `+' to integer"),
        ('#repeat #+', 1, "unable to convert token `#' to integer"),
        ('#repeat`#`join(', 2, "unable to convert token `#' to integer"),
        ('bum=23 `#repeat `bum `=>', 4, 'unexpected end of file while looking for a loop variable'),
        ('#repeat `25`=>`join', 4, "`join' is a reserved preprocessor word"),
        ('bum=53 `#repeat `bum `=> `join `#endRepeat', 5, "`join' is a reserved preprocessor word"),
        ('#{i=1,j=2,k=3}`#repeat`25 #endRepeat', 2, 'could not assign loop variable implicitly'),
        ('#repeat`25`#endRepeat', 1, 'empty repeat block'),
        ('#repeat`2`=>`klokan`#repeat`3`=>`klokan`#endRepeat', 8, "`klokan' already used as a loop variable"),
        ('#repeat`10`something`#ohNoes', 4, "unknown preprocessor variable `ohNoes'"),
        ('#repeat`11`hmm`#eval`0*0`oops`#endRepeat', 5, "expected `{', but got `0' instead"),
        ('#repeat`14`hmm`#eval{0*0 oops`#endRepeat', 4, "expected `}', but got `oops' instead"),

        ('#print', 1, 'missing expression to print'),
        ('#print,', 1, 'missing expression to print'),

        ('#eval', 1, "unexpected end of file while looking for `{'"),
        ('#{', 1, 'unexpected end of file while looking for a primary expression'),
        ('#{`i', 2, "unexpected end of file while looking for `}'"),
        ('#eval`{i', 2, "unexpected end of file while looking for `}'"),
        ('#eval`{i`+', 3, 'unexpected end of file while looking for a primary expression'),
        ('#{`5`=`25`}', 3, "operator `=' expects lvalue"),
        ('#{`10`/`0`}', 4, 'division by zero'),
        ('#{`i=7,`11`/`(`14`-`i`*`2`)`}', 6, 'division by zero'),
        ('#{`2`?`}', 4, "unable to convert token `}' to integer"),
        ('#{(`2`?`3`:`4`)`=`15`}', 8, "operator `=' expects lvalue"),
        ('#{`(`}', 3, "unable to convert token `}' to integer"),
        ('#{`)`}', 2, "unable to convert token `)' to integer"),
        ('#`{`"OY"`}', 3, 'unable to convert string `"OY"\' to integer'),
        ('node="LEFT"`#{2`*`node}', 4, "unable to convert token `node' to integer"),
        ('#{5`--`}', 2, "operator `--' expects lvalue"),
        ('#{`i`++`=`88`}', 4, "operator `=' expects lvalue"),
        ('#{`i`=`44`,`(`i`+`16`)`=`33`}', 11, "operator `=' expects lvalue"),
        ('#{(`i`,`70`)`=`77`}', 6, "operator `=' expects lvalue"),
        ('#{(`i`=`5`,`i`=`"10"`)`=`"25"`}', 10, "operator `=' expects lvalue"),
        ('#`{`0`&&', 4, 'unexpected end of file while looking for a primary expression'),
    )
    def testParsingErrors(self, testData):
        input, expectedLine, expectedError = testData
        input = input.replace('`', '\n')
        allowEmptyEntries = 'has zero' not in expectedError
        expectedError = TestHelper.formatError(TestHelper.kTestFilename, expectedLine, expectedError)
        TestHelper.assertExitWithError(self, lambda: getParserWithData(input, allowDimensionlessEntries=allowEmptyEntries), expectedError)

    def testNonExistingInclude(self):
        kInputFile = 'bum'
        kIncludeFile = 'puc'

        def customOpen(file, *args, **kwargs):
            if file == kInputFile:
                return mock.DEFAULT
            raise FileNotFoundError

        mockOpen = mock.mock_open(read_data=f'#include "{kIncludeFile}"\n')
        mockOpen.side_effect = customOpen

        with mock.patch(f'Tokenizer.open', mockOpen):
            path = os.path.abspath(kIncludeFile)
            expectedError = f'File {path} not found'
            TestHelper.assertExitWithError(self, lambda: Parser(kInputFile), expectedError)

    @data(
        ('`a`+', 3, "expected equal `=', got `+'"),
        ('`Menu', 2, 'unexpected end of file while looking for a menu name'),
        ('`Menu`{}', 3, 'menu name missing'),
        ('``pumpItUp', 3, "unexpected end of file while looking for an equal sign (`=')"),
        ('', 0, None),
        ('a = 5', 0, None),
        ('#{a`=`"0x5a"}', 0, None),
        ('#{a`=`"a5"}', 3, "unable to convert string `\"a5\"' to integer"),
    )
    def testIncludes(self, testData):
        kMainInputFile = 'sine'
        kIncludeFile = 'cosine'

        includedContents, expectedLine, expectedError = testData
        includedContents = includedContents.replace('`', '\n')
        if expectedError:
            expectedError = TestHelper.formatError(kIncludeFile, expectedLine, expectedError)

        mockOpen = mock.mock_open()

        firstCall = True
        def fakeReadline():
            nonlocal firstCall
            result = (f'#warningsOff #include "{kIncludeFile}"',) if firstCall else includedContents.split('\n')
            firstCall = False
            return result

        mockOpen.return_value.readlines = fakeReadline
        with mock.patch(f'Tokenizer.open', mockOpen):
            if expectedError:
                TestHelper.assertExitWithError(self, lambda: Parser(kMainInputFile), expectedError)
            else:
                Parser(kMainInputFile)

    kTestData = (
        ('', {'func': set(), 'st': set(), 'menu': []}),
        ('''#warningsOff Menu m1 {} atEnd=9+9''', {
                'func': set(),
                'st': set(),
                'menu': [('m1', 0, 0, {})]
            }
        ),
        ('''#warningsOff
            Menu m1
            {
                onInit: wakeUpInTheMorning
                defaultWidth: 50
                defaultX: @prevX + 1
                defaultY: 123

                Entry e1 {
                    x: 25
                    onSelect: takeMeHome
                    stringTable: [ g_soundOff, aOn, aOff ]
                    downEntry: >
                    rightEntry: e2
                }

                e1.width = 53

                Entry e2 {
                    stringTable: ~autoReplaysStrTable
                    beforeDraw: thinkYoureLuckyPunk
                    onSelect: ~"I'm so special!"
                    width: 150
                    height: 100
                    upEntry: <
                    downEntry: >
                }

                Entry e3 {
                    y: @prevY + 2
                    width: @prevWidth + 10
                    height: @prevHeight - 5
                    color: @prevEndX
                    textFlags: @prevEndY
                    // make sure to use both kinds of quotes
                    stringTable: [ Igloo, 2, "WHITE", \'KNUCKLE\', "RIDE", \']\' ]
                    leftEntry: @kTextWidth
                    upEntry: <
                }

                e1.width = 54
                e3.downEntry = e1
                e3.rightEntry = -1
            }
            Menu m2 {
                onReturn: iJustGotBack
                onInit: ~"int catchMeIfYouCan(int, int, void *)"
                y: 23
                defaultX: 50
                defaultHeight: 5

                Entry numba1 {
                    y: 25
                    height: 10
                    text: "Hey you!"
                }
                #repeat 3 => entryIndex
                    Entry #join(e, #entryIndex) {
                        y: @prevEndY + 2
                    }
                #endRepeat
                Entry numba2 {
                    text: "ONCE MUNICIPAL"
                    textFlags: @kBigText
                    width: @kTextWidth
                    y: numba1.y
                }
                e0.x = 72
                e0.upEntry = e1.upEntry
                e1.upEntry = e0.upEntry
            }
            Menu m3 {
                defaultX: 5
                defaultY: 6
                defaultWidth: 11
                defaultHeight: 12
                defaultColor: @kGreen

                TemplateEntry {
                    width: 42
                    number: 99
                    color: @kRed
                }

                Entry krang1 {}

                ResetTemplate

                Entry krang2 {}

                Entry krang3 {
                    width: 700
                    height: 800
                    text: "trick"
                    leftEntry: (5+7)/2*3
                }
            }''', {
                'func': { 'wakeUpInTheMorning', 'takeMeHome', 'thinkYoureLuckyPunk', 'iJustGotBack' },
                'st': {
                    'm1': (
                        ('e1', 'g_soundOff', 0, 'aOn', 'aOff'),
                        ('e3', 'Igloo', 2, '"WHITE"', '"KNUCKLE"', '"RIDE"', '"]"'),
                    ),
                },
                'menu': [
                    # default values expansion is delayed, hence it needs to be a string
                    ('m1', 3, 3, { 'onInit': 'wakeUpInTheMorning', 'defaultWidth': '50' }),
                    ('m2', 5, 5, { 'onReturn': 'iJustGotBack', 'y': 23 }),
                    ('m3', 3, 5, {}),
                ],
                'entry': {
                    'm1': (
                        ('e1', (('x', 25), ('y', 123), ('width', 54), ('downEntry', 1), ('rightEntry', 1))),
                        ('e2', (('x', 26), ('y', 123), ('width', 150), ('height', '100'), ('downEntry', 2), ('upEntry', 0))),
                        ('e3', (('x', 27), ('y', 125), ('width', 160), ('height', '95'), ('color', '176'), ('textFlags', '223'),
                        ('leftEntry', '55'), ('upEntry', 1), ('downEntry', 0), ('rightEntry', -1), )),
                    ),
                    'm2': (
                        ('numba1', (('x', 50), ('y', 25), ('height', 10), ('text', '"Hey you!"'))),
                        ('e0', (('x', 72), ('y', 37), ('height', 5), ('upEntry', -1))),
                        ('e1', (('x', 50), ('y', 44), ('height', 5), ('upEntry', -1))),
                        ('e2', (('x', 50), ('y', 51), ('height', 5))),
                        ('numba2', (('x', 50), ('y', 25), ('height', 5), ('width', 100))),
                    ),
                    'm3': (
                        ('krang1', (('x', '5'), ('y', '6'), ('width', '42'), ('height', '12'), ('color', '0'))),
                        ('krang2', (('x', '5'), ('y', '6'), ('width', '11'), ('height', '12'), ('color', '14'))),
                        ('krang3', (('x', '5'), ('y', '6'), ('width', '700'), ('height', '800'), ('color', '14'), ('leftEntry', 18))),
                    ),
                },
            }
        )
    )

    @data(*kTestData)
    def testFunctions(self, testData):
        input, expectedFunctions = extractExpectedData(testData, 'func')
        parser = getParserWithData(input)
        actualFunctions = parser.getFunctions()
        self.assertEqual(actualFunctions, expectedFunctions)

    @data(*kTestData)
    def testStringTables(self, testData):
        input, expectedStringTables = extractExpectedData(testData, 'st')
        parser = getParserWithData(input)
        actualStringTableLengths = parser.getStringTableLengths()

        if expectedStringTables:
            totalStringTables = 0
            for menuName, entryStringTable in expectedStringTables.items():
                self.assertIn(menuName, parser.menus)
                menu = parser.menus[menuName]
                totalStringTables += len(entryStringTable)

                for entryName, variable, initialValue, *values in entryStringTable:
                    self.assertIn(entryName, menu.entries)
                    entry = menu.entries[entryName]
                    stringTable = entry.stringTable

                    self.assertIsInstance(stringTable, StringTable)

                    self.assertEqual(stringTable.variable, variable)
                    self.assertEqual(stringTable.initialValue, initialValue)
                    self.assertEqual(stringTable.values, values)

            self.assertEqual(len(actualStringTableLengths), totalStringTables)

    @data(*kTestData)
    def testMenus(self, testData):
        input, expectedMenus = extractExpectedData(testData, 'menu')
        parser = getParserWithData(input)
        actualMenus = parser.getMenus()

        expectedAndActualIterator = zip(expectedMenus, actualMenus.items())
        for (expectedMenuName, numEntries, totalNumEntries, properties), (actualMenuName, menu) in expectedAndActualIterator:
            self.assertEqual(expectedMenuName, actualMenuName)
            self.assertEqual(numEntries, menu.numEntries())
            self.assertEqual(totalNumEntries, len(menu.entries))
            for property, value in properties.items():
                self.assertIn(property, menu.properties)
                self.assertEqual(value, menu.properties[property])

    @data(*kTestData)
    def testEntries(self, testData):
        input, expectedEntryProperties = extractExpectedData(testData, 'entry')
        if not expectedEntryProperties:
            return

        parser = getParserWithData(input)

        for menu in parser.menus.values():
            self.assertIn(menu.name, expectedEntryProperties)
            for entryName, entryProperties in expectedEntryProperties[menu.name]:
                self.assertIn(entryName, menu.entries)
                entry = menu.entries[entryName]
                for property, expectedValue in entryProperties:
                    self.assertTrue(hasattr(entry, property))
                    actualValue = getattr(entry, property)
                    if type(actualValue) is str and actualValue.startswith('('):
                        actualValue = eval(actualValue)
                    self.assertEqual(str(actualValue), str(expectedValue))

    @mock.patch('builtins.print')
    @data(
        ('luckyStar = 33', "unused global variable `luckyStar'"),
        ('luckyStar = 33 Menu XIV { y: luckyStar }', None),
        ('Menu ZvezdaKec { starLight = 59 }', "unused variable `starLight' declared in menu `ZvezdaKec'"),
        ('Menu ZvezdaKec { starLight = 59 initialEntry: starLight }', None),
        ('Menu ZvezdaKec { export starLight = 59}', None),
        ('heavenlyBody = @allRight Menu FlashBack { x: heavenlyBody }', "unknown internal variable `@allRight'"),
        ('heavenlyBody = @kScreenWidth Menu FlashBack { x: heavenlyBody }', None),
        ('#warningsOff Pirate1 = @kAlloc', None),
        ('#warningsOff Pirate2 = @kAlloc #warningsOn', "unused global variable `Pirate2'"),
    )
    def testWarnings(self, testData, mockPrint):
        input, expectedWarning = testData
        getParserWithData(input)

        if expectedWarning:
            mockPrint.assert_called_once()
            actualOutput = mockPrint.call_args[0][0]
            self.assertRegex(actualOutput, rf'\w+\(\d+\): warning: {expectedWarning}')
        else:
            mockPrint.assert_not_called()

    @data(
        ('Menu Bag`{`Entry Pocket {}`}', 3, "entry `Pocket' has zero width and height"),
        ('Menu Bag`{`Entry Pocket2 {}`} #disallowZeroWidthHeightEntries', 3, "entry `Pocket2' has zero width and height"),
        ('Menu Bag`{`Entry Pocket3 {}`} #allowZeroWidthHeightEntries', None, ''),
        ('Menu Bag`{`Entry Pocket3 { width: 113`height: 114 }`} #allowZeroWidthHeightEntries', None, ''),
        ('Menu Bag`{`Entry Pocket4 { width: 115`height: 116 }`} #disallowZeroWidthHeightEntries', None, ''),
    )
    def testAllowDimensionlessEntries(self, testData):
        input, expectedLine, expectedError = testData
        input = input.replace('`', '\n')
        if expectedError:
            expectedError = TestHelper.formatError(TestHelper.kTestFilename, expectedLine, expectedError)
            TestHelper.assertExitWithError(self, lambda: getParserWithData(input, allowDimensionlessEntries=False), expectedError)
        else:
            getParserWithData(input, allowDimensionlessEntries=False)

    @data((('''
        #warningsOff

        global = #{2 * 20 + 5}
        aString = 'Hello, world!'
        kScreenWidth = @kScreenWidth
        kScreenHeight = @kScreenHeight

        Menu Night {
            shadows = true
            sun = false
            go = #join('"Auf ', Wiedersehen, ' Monty"')
            hawk = #textWidth("HAWK", 1)
            #repeat 2
                #join(q_,#{i}:04) = #{10 + 5 * i}
            #endRepeat
            Entry Glib { y: 17 height: 29 }
            Medjed = "forbearance".mid(3,7).title()
            alloc = @kAlloc
            hinge = Glib.endY
            binge = #{2*hinge}
        }

        Menu Day {
            sun = true
            skies = "Blueee".left(4)
            #{forTea = 2}
            #repeat 3
                #join(p_,#i) = #{dark = 3 * i}
            #endRepeat

            pOrdInit = @prevOrd
            Entry {}
            pOrd0 = @previousOrdinal
            Entry {}
            pOrd1 = @previousOrd
            Entry {}
            pOrd2 = @prevOrdinal
        }

        #join(joined, #join(Hyb, rid), #{3 * dark}, #{dark / 6}) = 100
        c = 'Hitmeh'[2]
        global2 = 2 * global
        #{hybrid = global2 + 125}
        last=#join((,1,3,')')
    '''), {
        'globals': (
            ('global', '45'), ('aString', "'Hello, world!'"), ('joinedHybrid181', '100'), ('c', "'t'"),
            ('global2', '(2 * 45)'), ('last', '(13)'), ('kScreenWidth', '320'), ('kScreenHeight', '200'),
        ),
        'menu': (
            ('Night', (
                ('shadows', '1'), ('sun', '0'), ('go', '"Auf Wiedersehen Monty"'), ('alloc', '-1'),
                ('q_0000', '10'), ('q_0001', '15'), ('hawk', '32'), ('Medjed', '"Bear"'),
                ('hinge', '(17 + 29)'), ('binge', '92'),
            )),
            ('Day', (
                ('sun', '1'), ('skies', '"Blue"'), ('p_0', '0'), ('p_1', '3'), ('p_2', '6'),
                ('pOrdInit', '0'), ('pOrd0', '0'), ('pOrd1', '1'), ('pOrd2', '2'),
            )),
        ),
        'preproc': (('dark', 6), ('forTea', 2), ('hybrid', 215)),
    }))
    def testVariables(self, testData):
        input, expectedData = testData
        parser = getParserWithData(input)
        varStorage = parser.varStorage

        def verifyVariables(expectedVariables, actualVariables):
            self.assertEqual(len(expectedVariables), len(actualVariables))
            for varName, expectedValue in expectedVariables:
                self.assertIn(varName, actualVariables)
                self.assertEqual(expectedValue, actualVariables[varName].value)

        globals = expectedData['globals']
        verifyVariables(globals, varStorage.globals)

        for menuName, expectedMenuVars in expectedData['menu']:
            menu = next(menu for menu in varStorage.menusWithVariables if menu.name == menuName)
            verifyVariables(expectedMenuVars, menu.variables)

        preprocVars = expectedData['preproc']
        verifyVariables(preprocVars, varStorage.preprocVariables)

        self.assertNotIn('i', varStorage.preprocVariables)

    @data(
        ('Menu A {}', 0),
        ('Menu A { Entry {} Entry {} }', 2),
        ('Menu A { Entry {} TemplateEntry {} Entry {} TemplateEntry {} Entry {} }', 3),
    )
    def testGetNextEntryOrdinal(self, testData):
        input, expectedIndex = testData
        parser = getParserWithData(input)

        self.assertEqual(len(parser.menus), 1)
        self.assertIn('A', parser.menus)
        menu = parser.menus['A']

        actualIndex = menu.getNextEntryOrdinal()
        self.assertEqual(actualIndex, expectedIndex)

    @data(
        ('switch', 'a swimming pad', True),
        ('rabbit', 'a fox hunt companion', False),
        ('private', 'a barbecue stand', True),
        ('friend', 'a whipping cream', True),
        ('bubble', 'a form of air transport', False),
        ('catch', 'a throw', True),
        ('throw', 'a catch', True),
        ('whistle', 'a way to catch fish', False),
        ('int', 'a stamina', True),
        ('power', 'a fishing tool', False),
        ('auto', 'a bicycle', True),
        ('weather', 'an excuse', False),
        ('import', 'an export', True),
        ('default', 'a get-out-of-dept for free card', True),
        ('imaginary', 'an escapism', False),
        ('float', 'an ocean exploring tool', True),
        ('this', 'a that', True),
        ('landing gear', 'a propellant', False),
        ('module', 'a space station', True),
        ('case', 'a riot barricade', True),
        ('bucket', 'an anti-tank armour', False),
        ('register', 'a breaking and entering tool', True),
        ('double', 'an errand boy', True),
        ('union', 'a private enterprise', True),
    )
    def testVerifyNonCppKeyword(self, testData):
        word, context, isCppKeyword = testData
        parser = getParserWithData()

        expectedToken = Token(word, 'heaven', 22)
        errored = False

        def errorHandler(actualErrorStr, actualToken):
            self.assertEqual(actualToken, expectedToken)

            expectedErrorStr = f"can't use a C++ keyword `{expectedToken.string}'"
            if context:
                expectedErrorStr += f' as {context}'
            self.assertEqual(actualErrorStr, expectedErrorStr)

            nonlocal errored
            errored = True

        parser.error = errorHandler
        parser.verifyNonCppKeyword(expectedToken, context)
        self.assertEqual(isCppKeyword, errored)

    @data(
        ('defWidth', 'defaultWidth'),
        ('defaultWidth', 'defaultWidth'),
        ('defHeight', 'defaultHeight'),
        ('defaultHeight', 'defaultHeight'),
        ('defX', 'defaultX'),
        ('defaultX', 'defaultX'),
        ('defY', 'defaultY'),
        ('defaultY', 'defaultY'),
        ('defTextFlags', 'defaultTextFlags'),
        ('defaultTextFlags', 'defaultTextFlags'),
        ('chuchumigos', 'chuchumigos'),
        ('oceanrain', 'oceanrain'),
    )
    def testMenuPropertyAliases(self, testData):
        property, expected = testData
        actual = Parser.expandMenuPropertyAliases(property)
        self.assertEqual(actual, expected)

    @data(
        ('topEntry', 'upEntry'),
        ('upEntry', 'upEntry'),
        ('bottomEntry', 'downEntry'),
        ('downEntry', 'downEntry'),
        ('gownInTheMotown', 'gownInTheMotown'),
        ('driversSeat', 'driversSeat'),
    )
    def testEntryPropertyAliases(self, testData):
        property, expected = testData
        actual = Parser.expandEntryPropertyAliases(Token(property))
        self.assertEqual(actual, expected)

    @data(
        ('t = "thunder"[3]', 'n'),
        ('t = "deep blue sea"[0]', 'd'),
        ('t = "deep blue sea".first()', 'd'),
        ('t = "deep blue sea".last()', 'a'),
        ('t = "devil".middle()', 'v'),
        ('t = "BETWEEN".lower()', 'between'),
        ('t = "BeTWeeN".lower()', 'between'),
        ('t = "Between".lower()', 'between'),
        ('t = "driving rain".capitalize()', 'Driving rain'),
        ('t = "between the devil and the deep blue sea".title()', 'Between The Devil And The Deep Blue Sea'),
        ('t = "devil and the deep blue sea".left(5)', 'devil'),
        ('t = "devil and the deep blue sea".right(13)', 'deep blue sea'),
        ('t = "devil and the deep blue sea".mid(19, 23)', 'blue'),
        ('t = "123".zfill(2)', '123'),
        ('t = "123".zfill(10)', '0000000123'),
    )
    def testStringManipulationFunctions(self, testData):
        input, expected = testData
        parser = getParserWithData('#warningsOff ' + input)
        actual = parser.varStorage.getGlobalVariable('t').value
        actual = Util.unquotedString(actual)
        self.assertEqual(actual, expected)

def extractExpectedData(testData, key):
    assert isinstance(testData, (list, tuple))
    assert isinstance(key, str)

    input, expectedData = testData
    return input, expectedData[key] if key in expectedData else ()
