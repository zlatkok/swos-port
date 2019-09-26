import re
import unittest

from unittest import mock
from ddt import ddt, data

import Util
import TestHelper
from TestHelper import getParserWithData

@ddt
class TestPreprocessor(unittest.TestCase):
    @mock.patch('builtins.print')
    @data(
        ('i=2,3,4,i+5', 7),
        ('(10?i=5:(j=20)),j', 0),
        ('(10?i=5:(j=20)),i', 5),
        ('(0?i=5:(j=20)),j', 20),
        ('(0?i=5:(j=20)),i', 0),
        ('(i=5)||(j=12),j', 0),
        ('(i=5)||(j=12),i', 5),
        ('i=13,j=17,(i,j)=29,j', 29),   # comma expression as lvalue
        ('(5,10,15,i)=125,i', 125),     # comma expression as lvalue (rvalues first)
        ('i=0,j=23,k=i&&j||i+1,k', 1),
        ('i=15,j=0,k=i&&j||j,k', 0),
        ('i=15,j=0,k=i&&j||3*j,k', 0),
        ('3|5', 7),
        ('3|8-2',7),
        ('0^0', 0),
        ('1^0', 1),
        ('1^1&&1', 0),
        ('0x777&0x111', 0x111),
        ('0x777&0x111|0x222', 0x333),
        ('1==2==1', 0),
        ('1==2==0', 1),
        ('1==2==3', 0),
        ('1==2?0:1', 1),
        ('5!=10&&6==6', 1),
        ('i=5>3,i', 1),
        ('u=15,u>=6&&u<=20', 1),
        ('u=45,u>=6&&u<=20', 0),
        ('1<<5', 32),
        ('32>>2', 8),
        ('1|>>1', -0x80000000),
        ('0x80000000<<|1', 1),
        ('99 + 1', 100),
        ('-10 - 1', -11),
        ('2*5 + 3*6', 28),
        ('10%3', 1),
        ('10/3', 3),
        ('i=42,i++', 42),
        ('i=41,++i', 42),
        ('i=-1,z=5+i--,z+i', 2),
        ('~5', -6),
        ('!45', 0),
        ('!0', 1),
        ('(6+4)*5', 50),
        ('(i=5)*2>8?33:44', 33),
        ('j=2*((i=2)>0) + 5,j', 7),
        ('i+=12,i*=3', 36),
        ('i=1,i|>>=1', -0x80000000),
        ('i=0x80000000,i<<=|1', 1),
        ('i=0,i^=1,i|=3,-i', -3),
        ('@kScreenWidth-@kScreenHeight', 120),
    )
    def testExpressions(self, testData, mockPrint):
        rawInput, expectedOutput = testData
        input = f'#print #{{{rawInput}}}'
        getParserWithData(input)

        mockPrint.assert_called_once()
        actualOutput = mockPrint.call_args[0][1]
        self.assertEqual(actualOutput[1:-1], str(expectedOutput))

    @mock.patch('builtins.print')
    @data(
        ('#print "heya"`#join(Menu)`SanMateo`{}', ((1, 'heya'),), 'Menu:SanMateo'),
        ('Menu SantaClara {`#print "heh"`#join(Entry) Algorithm {}`}', ((2, 'heh'),), 'Menu:SantaClara.Algorithm'),
        ('Menu PaloAlto {`Entry SkyHook { #print "hipie"`#join(x):56 }`}', ((2, 'hipie'),), 'Menu:PaloAlto.SkyHook.x=56'),
        ('Menu MountainView`{`Entry #join(Mont, Blanc) {}`}', None, 'Menu:MountainView.MontBlanc'),
        ('a = #{3 * 221}', None, 'a=663'),
        ('a = "abc".left(#{10 / 5})', None, 'a="ab"'),
        ('#repeat 2`#print #join(hip, #{i}, hop)`#endRepeat', ((2, "hip0hop"), (2, "hip1hop")), ''),
    )
    def testPreprocessorExpressionContexts(self, testData, mockPrint):
        input, printOutput, properties = testData
        input = input.replace('`', '\n')
        parser = getParserWithData('#warningsOff ' + input)
        calls = mockPrint.call_args_list

        if properties:
            if properties.startswith('Menu:'):
                menu, entry, property = (properties.split(':')[1].split('.') + 3 * [None])[:3]

                self.assertEqual(len(parser.menus), 1)
                self.assertIn(menu, parser.menus)
                menu = parser.menus[menu]

                if entry:
                    self.assertEqual(len(menu.entries), 1)
                    self.assertIn(entry, menu.entries)
                    entry = menu.entries[entry]

                    if property:
                        property, value = property.split('=')
                        self.assertTrue(hasattr(entry, property))
                        self.assertEqual(getattr(entry, property), value)

        if printOutput:
            for call, (line, text) in zip(calls, printOutput):
                expectedOutput = Util.formatMessage(f"`{text}'", TestHelper.kTestFilename, line)
                args, _ = call
                actualOutput = ' '.join(args)
                self.assertEqual(actualOutput, expectedOutput)
        else:
            mockPrint.assert_not_called()

    @mock.patch('builtins.print')
    @data(
        ('''
            #repeat 3
                #print #i
                #repeat 4
                    #print #i, #j
                    #repeat 5
                        #print #i, #j, #k
                    #endRepeat
                #endRepeat
            #endRepeat
        ''', (
            ((0,), 3),
            ((0, 0), 5),
            ((0, 0, 0), 7),
            ((0, 0, 1), 7),
            ((0, 0, 2), 7),
            ((0, 0, 3), 7),
            ((0, 0, 4), 7),
            ((0, 1), 5),
            ((0, 1, 0), 7),
            ((0, 1, 1), 7),
            ((0, 1, 2), 7),
            ((0, 1, 3), 7),
            ((0, 1, 4), 7),
            ((0, 2), 5),
            ((0, 2, 0), 7),
            ((0, 2, 1), 7),
            ((0, 2, 2), 7),
            ((0, 2, 3), 7),
            ((0, 2, 4), 7),
            ((0, 3), 5),
            ((0, 3, 0), 7),
            ((0, 3, 1), 7),
            ((0, 3, 2), 7),
            ((0, 3, 3), 7),
            ((0, 3, 4), 7),
            ((1,), 3),
            ((1, 0), 5),
            ((1, 0, 0), 7),
            ((1, 0, 1), 7),
            ((1, 0, 2), 7),
            ((1, 0, 3), 7),
            ((1, 0, 4), 7),
            ((1, 1), 5),
            ((1, 1, 0), 7),
            ((1, 1, 1), 7),
            ((1, 1, 2), 7),
            ((1, 1, 3), 7),
            ((1, 1, 4), 7),
            ((1, 2), 5),
            ((1, 2, 0), 7),
            ((1, 2, 1), 7),
            ((1, 2, 2), 7),
            ((1, 2, 3), 7),
            ((1, 2, 4), 7),
            ((1, 3), 5),
            ((1, 3, 0), 7),
            ((1, 3, 1), 7),
            ((1, 3, 2), 7),
            ((1, 3, 3), 7),
            ((1, 3, 4), 7),
            ((2,), 3),
            ((2, 0), 5),
            ((2, 0, 0), 7),
            ((2, 0, 1), 7),
            ((2, 0, 2), 7),
            ((2, 0, 3), 7),
            ((2, 0, 4), 7),
            ((2, 1), 5),
            ((2, 1, 0), 7),
            ((2, 1, 1), 7),
            ((2, 1, 2), 7),
            ((2, 1, 3), 7),
            ((2, 1, 4), 7),
            ((2, 2), 5),
            ((2, 2, 0), 7),
            ((2, 2, 1), 7),
            ((2, 2, 2), 7),
            ((2, 2, 3), 7),
            ((2, 2, 4), 7),
            ((2, 3), 5),
            ((2, 3, 0), 7),
            ((2, 3, 1), 7),
            ((2, 3, 2), 7),
            ((2, 3, 3), 7),
            ((2, 3, 4), 7),
        )),
        ('''Menu Bombay
            {
                #{c = 33}
                #{rep1 = 3, rep2 = 2}
                #repeat rep1 => yin
                    Entry {
                        #repeat rep2 => yang
                            #print 'Searching for a paradise: ', #{2 * yin + yang}, #{c}, #{c + yin}
                        #endRepeat
                    }
                #endRepeat
            }
        ''', (
            (('Searching for a paradise: ', 0, 33, 33), 8),
            (('Searching for a paradise: ', 1, 33, 33), 8),
            (('Searching for a paradise: ', 2, 33, 34), 8),
            (('Searching for a paradise: ', 3, 33, 34), 8),
            (('Searching for a paradise: ', 4, 33, 35), 8),
            (('Searching for a paradise: ', 5, 33, 35), 8),
        )),
        ('''Menu GeorgyPorgy
            {
                export kMaxColumns = 3
                export kMaxEntriesPerColumn = 2

                #repeat kMaxColumns
                    #repeat kMaxEntriesPerColumn
                        #print #i, #j, #{i * kMaxEntriesPerColumn + j}:02
                    #endRepeat
                #endRepeat
            }
        ''', (
            ((0, 0, '00'), 8),
            ((0, 1, '01'), 8),
            ((1, 0, '02'), 8),
            ((1, 1, '03'), 8),
            ((2, 0, '04'), 8),
            ((2, 1, '05'), 8),
        )),
    )
    def testRepeat(self, testData, mockPrint):
        input, expectedOutput = testData
        getParserWithData(input)

        calls = mockPrint.call_args_list
        self.assertIsNotNone(calls)
        self.assertEqual(len(calls), len(expectedOutput))

        extractParams = lambda string: re.findall("`([^']+)'", string)

        for call, (expectedParams, expectedLine) in zip(calls, expectedOutput):
            actualOutput = call[0][1]
            actualLine = re.search(r'(\d+)', call[0][0]).group()
            actualParams = extractParams(actualOutput)
            expectedParams = list(map(str, expectedParams))
            self.assertEqual(actualParams, expectedParams)
            self.assertEqual(actualLine, str(expectedLine))

    @mock.patch('builtins.print')
    @data(  # expected values read directly from SWOS :D
        ('HA-HA-HA', 'true', 58, 8),
        ('HA-HA-HA', 'false', 44, 6),
        ('mIxEdCaSe', 1, 69, 8),
        ('mIxEdCaSe', 0, 50, 6),
        ('@#$*BRB|];~!\t', 'big', 100, 8),
        ('@#$*BRB|];~!\t', 'small', 75, 6),
        ('LAST REMNANT GONE AND WASHED AWAY IT WAS TIME TO MOVE. FAST.', 512, 409, 8),
        ('NIGHT WAS COLD AND RAINY, AND THEY WERE CLOSING IN ON ME.', 0x0, 285, 6),
    )
    def testTextMeasurement(self, testData, mockPrint):
        string, bigFont, *expectedDimensions = testData

        input = ''
        kDimensions = ('Width', 'Height')

        for dimension in kDimensions:
            input += f'{dimension} = #text{dimension}("{string}", {bigFont})\n'

        parser = getParserWithData(input)

        for dimension, expectedDimension in zip(kDimensions, expectedDimensions):
            self.assertIn(dimension, parser.varStorage.globals)
            self.assertEqual(parser.varStorage.globals[dimension].value, str(expectedDimension))
