import importlib.util
import contextlib
import io
import pathlib
import tempfile
import unittest


kScript = pathlib.Path(__file__).with_name('generateAnimationTables.py')
kSpec = importlib.util.spec_from_file_location('animationTables', kScript)
animationTables = importlib.util.module_from_spec(kSpec)
kSpec.loader.exec_module(animationTables)


class ParserTest(unittest.TestCase):
    def parse(self, text):
        constants = {
            'kSprite': 12,
            'kNumSprites': 1334,
            'kLastFrameLoopMarker': -999,
            'kLastFrameHoldMarker': -101,
            'kFrameLoopbackMarker': -100,
        }
        return animationTables.parseText(text, constants)

    def testFrameConstantsCommentsAndForwardReference(self):
        parsed = self.parse('''
            frames kSprite, -5, 13, -101 ; assembly comment
            animation {
                5 // C++ comment
                team1player { all: later }
            }
            later 10, -999
        ''')
        self.assertEqual(parsed.frameTables[0].values, ['kSprite', -5, 13, -101])
        self.assertEqual(parsed.animationTables[0].sections[0], ['later'] * 8)

    def testExportedFrameTableGeneratesOffsetGetter(self):
        parsed = self.parse('''
            complete 1, 2, -999
            export suffix 2, -999
            sampleAnim { 5 team 1 player { all: suffix } }
        ''')
        self.assertTrue(parsed.frameTables[1].exported)
        header, source = animationTables.generate(parsed, 'test.in')
        self.assertIn('int16_t getSuffixOffset();', header)
        self.assertIn('int16_t getSuffixOffset()\n{\n    return 2;\n}', source)

    def testRejectsExportedAnimationTable(self):
        self.assertParseError('export animation { 5 }', 'only frame tables can be exported')

    def testSymbolicSpriteAndTerminalConstants(self):
        parsed = self.parse('''
            loopingFrames kSprite, kLastFrameLoopMarker
            holdingFrames kSprite, kLastFrameHoldMarker
        ''')
        self.assertEqual(parsed.frameTables[0].values, ['kSprite', 'kLastFrameLoopMarker'])
        self.assertEqual(parsed.frameTables[1].values, ['kSprite', 'kLastFrameHoldMarker'])
        _, source = animationTables.generate(parsed, 'test.in')
        self.assertIn('12, -999, 12, -101', source)

    def testTerminalRelativeLoopbackValue(self):
        parsed = self.parse('frames 10, 11, 12, 13, -104')
        self.assertEqual(parsed.frameTables[0].values[-1], -104)

    def testNamedFrameDelays(self):
        parsed = self.parse('frames 10, wait(1), 11, delay (99), kLastFrameHoldMarker')
        self.assertEqual(parsed.frameTables[0].values, [10, -1, 11, -99,
            'kLastFrameHoldMarker'])

    def testNamedLoopbackWithAndWithoutParentheses(self):
        parsed = self.parse('''
            parenthesized 10, 11, 12, 13, loopback(4)
            spaced 20, 21, loopback 2
        ''')
        self.assertEqual(parsed.frameTables[0].values[-1], -104)
        self.assertEqual(parsed.frameTables[1].values[-1], -102)

    def testRejectsNamedLoopbackOutsideRange(self):
        for count in (1, 32669):
            with self.subTest(count=count):
                self.assertParseError(
                    f'frames 1, loopback({count})', 'range 2..32668')

    def testRejectsNamedLoopbackBeforeTableStart(self):
        self.assertParseError('frames 1, loopback 2', 'leaves the table')

    def testRejectsMissingControlExpressionClosingParenthesis(self):
        self.assertParseError('frames 1, delay(5, -999', 'expected')

    def testRejectsNamedFrameDelayOutsideRange(self):
        for delay in (0, 100):
            with self.subTest(delay=delay):
                self.assertParseError(
                    f'frames 1, wait {delay}, kLastFrameHoldMarker', 'range 1..99')

    def testRejectsInvalidNamedFrameDelay(self):
        self.assertParseError(
            'frames 1, delay nope, kLastFrameHoldMarker', 'positive integer')

    def testRejectsCommaInsideNamedFrameDelay(self):
        self.assertParseError(
            'frames 1, delay, 5, kLastFrameHoldMarker', 'positive integer')

    def testRejectsReservedFrameTableName(self):
        self.assertParseError('wait 1, -999', 'reserved')

    def testRejectsReservedAnimationTableName(self):
        self.assertParseError('DELAY { 5 }', 'reserved')

    def testRejectsLoopbackAsTableName(self):
        self.assertParseError('Loopback 1, -999', 'reserved')

    def testRejectsZeroOffsetLoopback(self):
        self.assertParseError('frames 1, kFrameLoopbackMarker', 'infinite control loop')

    def testRejectsControlFlowBeforeTableStart(self):
        self.assertParseError('frames 1, -103', 'leaves the table')

    def testRejectsNegativeOnlyControlCycle(self):
        self.assertParseError(
            'frames -1, kLastFrameLoopMarker', 'never reaches a sprite or hold marker')

    def testWarnsAboutUnreachableTrailingElements(self):
        parsed = self.parse('frames 1, kLastFrameLoopMarker, 2, kLastFrameHoldMarker')
        self.assertTrue(any('unreachable' in warning for warning in parsed.warnings))

    def testWarnsWhenFrameTableHasNoSpriteIndices(self):
        parsed = self.parse('frames -5, kLastFrameHoldMarker')
        self.assertTrue(any('no sprite indices' in warning for warning in parsed.warnings))

    def testExplicitEntriesArePlacedBeforeImplicitEntries(self):
        parsed = self.parse('''
            first 1, -999
            rightFrames 2, -999
            last 3, -999
            animation {
                5
                team 1 player { first right: rightFrames last }
            }
        ''')
        self.assertEqual(parsed.animationTables[0].sections[0],
            ['first', 'last', 'rightFrames', None, None, None, None, None])

    def testWrappingRangeAndSameEndpointWarning(self):
        parsed = self.parse('''
            a 1, -999
            b 2, -999
            refAnimation { 5 top-left to right: a left to left: b }
        ''')
        section = parsed.animationTables[0].sections[0]
        self.assertEqual(section, ['a', 'a', 'a', None, None, None, 'b', 'a'])
        self.assertEqual(len(parsed.warnings), 1)

    def testSectionNamesWithAndWithoutSpaces(self):
        parsed = self.parse('''
            a 1, -999
            animation {
                5
                team 1 player { all: a }
                team2player { all: a }
                goalkeeper 1 { all: a }
                goalkeeper2 { all: a }
            }
        ''')
        self.assertTrue(all(section == ['a'] * 8 for section in parsed.animationTables[0].sections))

    def testAnimationSectionsPackPlayersBeforeGoalkeepers(self):
        parsed = self.parse('''
            team1Frames 11, -999
            team2Frames 22, -999
            goalkeeper1Frames 33, -999
            goalkeeper2Frames 44, -999
            animation {
                5
                goalkeeper 2 { all: goalkeeper2Frames }
                team 2 player { all: team2Frames }
                goalkeeper 1 { all: goalkeeper1Frames }
                team 1 player { all: team1Frames }
            }
        ''')
        _, source = animationTables.generate(parsed, 'test.in', foldTables=False)
        compactSource = ' '.join(source.split())
        self.assertIn('3845, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, '
            '5, 5, 5, 5, 5, 5, 5, 5, 7, 7, 7, 7, 7, 7, 7, 7', compactSource)

    def testOptionalCommaAfterAnimationDelay(self):
        withoutComma = self.parse('frames 1,-999 refAnim { 5 all: frames }')
        withComma = self.parse('frames 1,-999 refAnim { 5, all: frames }')
        self.assertEqual(withComma.animationTables, withoutComma.animationTables)

    def testInlineFrameTable(self):
        parsed = self.parse('refAnim { 5, all: [ kSprite, wait 3, 13, -999 ] }')
        inline = parsed.frameTables[0]
        self.assertEqual(inline.values, ['kSprite', -3, 13, -999])
        self.assertEqual(parsed.animationTables[0].sections[0], [inline.name] * 8)
        header, source = animationTables.generate(parsed, 'test.in')
        self.assertIn('"zk"[0] | ("zk"[1] << 8)', source)
        self.assertIn('const int16_t kTableData[] = '
            '{ kInvalidTableData, 12, -3, 13, -999, 4101, 1, 1, 1, 1, 1, 1, 1, 1 };', source)
        self.assertIn('int16_t getRefAnim()\n{\n    return 5;', source)

    def testInlineFrameTableUsesNormalValidation(self):
        self.assertParseError('refAnim { 5 all: [ 1, 2 ] }', 'must end')

    def testRejectsMissingInlineFrameTableClosingBracket(self):
        self.assertParseError('refAnim { 5 all: [ 1, -999 }', 'expected')

    def testRejectsStrayInlineFrameTableClosingBracket(self):
        self.assertParseError('frames 1,-999 refAnim { 5 all: frames ] }', 'unexpected')

    def testRejectsNestedInlineFrameTableOpeningBracket(self):
        self.assertParseError('refAnim { 5 all: [ 1, [ 2, -999 ] ] }', 'unexpected')

    def assertParseError(self, text, message):
        with self.assertRaisesRegex(animationTables.ParseError, message):
            self.parse(text)

    def testRejectsAllCombinedWithAnotherEntry(self):
        self.assertParseError('a 1,-999 b 2,-999 refAnim { 5 all: a b }', 'cannot be combined')

    def testRejectsDuplicateDirection(self):
        self.assertParseError('a 1,-999 b 2,-999 refAnim { 5 right: a right: b }', 'assigned more than once')

    def testRejectsMoreThanEightEntries(self):
        frames = ' '.join(f'f{i} {i}, -999' for i in range(9))
        references = ' '.join(f'f{i}' for i in range(9))
        self.assertParseError(f'{frames} refAnim {{ 5 {references} }}', 'more than eight')

    def testRejectsUndefinedForwardReference(self):
        self.assertParseError('refAnim { 5 all: missing }', 'undefined frame table')

    def testRejectsNonterminatedFrameTable(self):
        self.assertParseError('frames 1, 2', 'must end')

    def testFrameTableConstructionRejectsEmptyData(self):
        with self.assertRaisesRegex(animationTables.ParseError, 'at least one value'):
            animationTables.FrameTable('empty', [], 1, [])

    def testRejectsAnimationDelayOutsideUint8(self):
        self.assertParseError(
            'frames 1,-999 refAnim { 256 all: frames }', 'does not fit in uint8_t')

    def testGeneratePacksDuplicateAndSuffixFrameTables(self):
        parsed = self.parse('''
            complete 1, 2, -999
            duplicate 1, 2, -999
            suffix 2, -999
            sampleAnim {
                7
                goalkeeper 2 { all: duplicate }
                team 1 player { all: suffix }
            }
        ''')
        header, source = animationTables.generate(parsed, 'test.in')
        compactSource = ' '.join(source.split())
        self.assertTrue(header.startswith('// Generated from test.in; do not edit.\n'))
        self.assertNotIn('#include', header)
        self.assertTrue(source.startswith('// Generated from test.in; do not edit.\n'
            '// Include this file from a parent C++ implementation file after animation.h.\n'))
        self.assertNotIn('#include', source)
        self.assertNotIn('\nnamespace\n', source)
        self.assertIn('static constexpr int16_t kInvalidTableData', source)
        self.assertIn('static const int16_t kTableData[]', source)
        self.assertNotIn('struct AnimationTable', header)
        self.assertIn('const int16_t kTableData[] = '
            '{ kInvalidTableData, 1, 2, -999, 2311, '
            '2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1 };',
            compactSource)
        self.assertIn('static const int16_t kValidFrameTableOffsets[] = { 1, 2 };', source)
        self.assertIn('static const int16_t kFrameTableLengths[] = { 3, 2 };', source)
        self.assertIn('static const int16_t kValidAnimationTableOffsets[] = { 4 };', source)
        self.assertIn('int16_t getSampleAnim()\n{\n    return 4;', source)
        self.assertNotIn('const AnimationTable& getSampleAnim', source)
        self.assertNotIn('getSampleAnimOffset', header + source)
        self.assertNotIn('extern const int16_t kTableData', header)
        self.assertNotIn('extern const AnimationTable sampleAnim', header)

    def testGenerateCanDisableDuplicateAndSuffixFrameTableFolding(self):
        parsed = self.parse('''
            complete 1, 2, -999
            duplicate 1, 2, -999
            suffix 2, -999
            sampleAnim {
                7
                goalkeeper 2 { all: duplicate }
                team 1 player { all: suffix }
            }
        ''')
        header, source = animationTables.generate(parsed, 'test.in', foldTables=False)
        compactSource = ' '.join(source.split())
        self.assertIn('const int16_t kTableData[] = '
            '{ kInvalidTableData, 1, 2, -999, 1, 2, -999, 2, -999, 2311, '
            '7, 7, 7, 7, 7, 7, 7, 7, 4, 4, 4, 4, 4, 4, 4, 4 };', compactSource)
        self.assertIn('int16_t getSampleAnim()\n{\n    return 9;', source)
        self.assertIn('int16_t getSampleAnim();', header)

    def testGenerateIncludesOriginalSwosTablesForProductionInput(self):
        root = kScript.parents[3]
        constants = animationTables.readConstants(root / 'src/sprites/sprites.h')
        constants.update(animationTables.readConstants(root / 'src/swos/swos.h'))
        inputPath = kScript.with_name('animationTables.in')
        parsed = animationTables.parseText(inputPath.read_text(encoding='utf-8'), constants)
        header, source = animationTables.generate(parsed, inputPath.name)
        self.assertIn('int16_t getStaticHeaderAttemptAnimTable()\n'
            '{\n    return 1809;', source)
        self.assertIn('int16_t getStaticHeaderAttemptAnimTable()\n'
            '{\n    return 1616;', source)
        self.assertNotIn('#ifdef SWOS_TEST', header)
        self.assertIn('#ifdef SWOS_TEST', source)
        self.assertIn('#else', source)
        self.assertIn('\n#else\n', source)
        self.assertIn('\n#endif\n#if !defined(NDEBUG) || defined(SWOS_TEST)\n'
            'bool isValidFrameTableOffset', source)
        self.assertIn('const int16_t kTableData[]', source)
        self.assertNotIn('kFrameTableData', source)
        self.assertNotIn('kAnimationTableData', source)
        self.assertIn('int16_t getOriginalSwosFrameTable(int32_t offset)', source)
        self.assertIn('case -1:\n        return kInvalidFrameTableOffset;', source)
        self.assertIn('case static_cast<int32_t>(0x80000000):', source)
        self.assertIn('case static_cast<int32_t>(0x80000000 | 24):', source)
        self.assertIn('case -2:', source)
        self.assertIn('case -3:', source)
        self.assertNotIn('0x80000000 | 2147483646', source)
        self.assertNotIn('0x80000000 | 2147483645', source)
        self.assertIn('int16_t getOriginalSwosAnimationTable(int32_t offset)', source)
        self.assertIn('if (offset == -1)\n        return kInvalidAnimationTableOffset;', source)
        self.assertNotIn('kInvalidFrameTable', header)
        self.assertNotIn('kInvalidAnimationTable', header)
        self.assertNotIn('const int16_t *const kInvalidFrameTable =', source)
        self.assertNotIn('const AnimationTable *const kInvalidAnimationTable =', source)
        self.assertIn('return kInvalidFrameTableOffset;', source)
        self.assertIn('return kInvalidAnimationTableOffset;', source)
        self.assertIn('case 1382:', source)
        self.assertIn('int16_t getBallMovingFrameTableOffset();', header)
        self.assertIn('int16_t getBallStaticFrameTableOffset();', header)
        self.assertIn('int16_t getBallMovingFrameTableOffset()\n'
            '{\n    return 0;', source)
        self.assertIn('int16_t getBallStaticFrameTableOffset()\n{\n    return 5;', source)

    def testOriginalSwosDataPreservesMalformedAndUnlabelledTables(self):
        original = animationTables.originalSwosAnimationTables
        frameTables = {name: (offset, values) for offset, name, values in original.kFrameTables}
        animationTablesByName = {
            name: (offset, delay, references)
            for offset, name, delay, references in original.kAnimationTables
        }
        self.assertEqual(frameTables['nullptr'], (-1, ()))
        self.assertEqual(frameTables['jumpHeaderHitTeam1TopFrames'][0], 0x80000000)
        self.assertEqual(len(animationTablesByName['playerTackledAnimTable'][2]), 20)
        self.assertIn('unnamedAnimTable_130', animationTablesByName)

    def testGeneratePacksRefereeTableAndMissingDirections(self):
        parsed = self.parse('''
            topFrames 1, -999
            refSample { 9 top: topFrames }
        ''')
        header, source = animationTables.generate(parsed, 'test.in')
        compactSource = ' '.join(source.split())
        self.assertIn('const int16_t kTableData[] = '
            '{ kInvalidTableData, 1, -999, 4105, 1, -1, -1, -1, -1, -1, -1, -1 };',
            compactSource)
        self.assertIn('int16_t getRefSample()\n{\n    return 3;', source)
        self.assertNotIn('getFrameTableOffsets', header)
        self.assertNotIn('getFrameTable(int16_t offset)', header)
        self.assertIn('static const int16_t kValidFrameTableOffsets[] = { 1 };', source)
        self.assertIn('static const int16_t kFrameTableLengths[] = { 2 };', source)
        self.assertIn('static const int16_t kValidAnimationTableOffsets[] = { 3 };', source)
        self.assertIn('#if !defined(NDEBUG) || defined(SWOS_TEST)\n'
            'static const int16_t kValidFrameTableOffsets[]', source)
        self.assertIn('assert(kTableData[0] != kInvalidTableData || offset != 0);', source)
        self.assertIn('std::find(std::begin(kValidFrameTableOffsets)', source)
        self.assertIn('bool isValidFrameTableOffset(int16_t offset)', source)
        self.assertIn('assert(isValidFrameTableOffset(offset));', source)
        self.assertIn('int16_t getFrameTableElement(int16_t offset, int index)', source)
        self.assertIn('int16_t getFrameTableElement(int16_t offset, int index)\n{\n'
            '#if !defined(NDEBUG) || defined(SWOS_TEST)', source)
        self.assertIn('index < kFrameTableLengths[', source)
        self.assertIn('const AnimationTable *getAnimationTable(int16_t offset)', source)
        self.assertIn('std::find(std::begin(kValidAnimationTableOffsets)', source)
        self.assertIn('return reinterpret_cast<const AnimationTable *>(&kTableData[offset]);', source)
        self.assertIn('int16_t getRefSample();', header)

    def testGeneratedArrayValuesWrapAt108Columns(self):
        frames = ', '.join(str(index % 20) for index in range(80))
        parsed = self.parse(f'frames {frames}, -999 refAnimation {{ 5 all: frames }}')
        _, source = animationTables.generate(parsed, 'test.in')
        self.assertTrue(any(line.startswith('    ') for line in source.splitlines()))
        for line in source.splitlines():
            self.assertLessEqual(len(line), 108, line)

    def testAnimationTableDataFoldingCanBeDisabled(self):
        parsed = self.parse('''
            frames 1, -999
            refFirstAnim { 5 all: frames }
            refDuplicateAnim { 5 all: frames }
        ''')
        _, folded = animationTables.generate(parsed, 'test.in')
        foldedCompact = ' '.join(folded.split())
        self.assertIn('const int16_t kTableData[] = '
            '{ kInvalidTableData, 1, -999, 4101, 1, 1, 1, 1, 1, 1, 1, 1 };',
            foldedCompact)
        self.assertEqual(folded.count('    return 3;'), 2)

        _, unfolded = animationTables.generate(parsed, 'test.in', foldTables=False)
        unfoldedCompact = ' '.join(unfolded.split())
        self.assertIn('const int16_t kTableData[] = '
            '{ kInvalidTableData, 1, -999, 4101, 1, 1, 1, 1, 1, 1, 1, 1, '
            '4101, 1, 1, 1, 1, 1, 1, 1, 1 };', unfoldedCompact)
        self.assertIn('    return 3;', unfolded)
        self.assertIn('    return 12;', unfolded)

    def testMainPrintsWarningsBeforeFatalError(self):
        text = ('frames 1, -999\n'
            'refWarningAnim { 5 left to left: frames }\n'
            'refInvalidAnim { 5 all: missing }\n')
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            inputPath = root / 'tables.in'
            constantsPath = root / 'constants.h'
            inputPath.write_text(text)
            constantsPath.write_text('constexpr int kNumSprites = 1334;')
            arguments = [str(inputPath), str(root / 'out.h'), str(root / 'out.cpp'),
                '--sprites', str(constantsPath), '--swos', str(constantsPath)]
            stderr = io.StringIO()
            originalArguments = animationTables.sys.argv
            animationTables.sys.argv = ['generateAnimationTables.py'] + arguments
            try:
                with contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit):
                    animationTables.main()
            finally:
                animationTables.sys.argv = originalArguments
            self.assertIn('range starts and ends', stderr.getvalue())

    def testMainCanIgnoreWarningsBeforeFatalError(self):
        text = ('frames 1, -999\n'
            'refWarningAnim { 5 left to left: frames }\n'
            'refInvalidAnim { 5 all: missing }\n')
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            inputPath = root / 'tables.in'
            constantsPath = root / 'constants.h'
            inputPath.write_text(text)
            constantsPath.write_text('constexpr int kNumSprites = 1334;')
            arguments = [str(inputPath), str(root / 'out.h'), str(root / 'out.cpp'),
                '--sprites', str(constantsPath), '--swos', str(constantsPath), '--ignore-warnings']
            stderr = io.StringIO()
            originalArguments = animationTables.sys.argv
            animationTables.sys.argv = ['generateAnimationTables.py'] + arguments
            try:
                with contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit):
                    animationTables.main()
            finally:
                animationTables.sys.argv = originalArguments
            self.assertNotIn('range starts and ends', stderr.getvalue())

    def testMainReportsMissingInputFileWithoutTraceback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            constantsPath = root / 'constants.h'
            constantsPath.write_text('constexpr int kNumSprites = 1334;')
            missingPath = root / 'missing.in'
            arguments = [str(missingPath), str(root / 'out.h'), str(root / 'out.cpp'),
                '--sprites', str(constantsPath), '--swos', str(constantsPath)]
            originalArguments = animationTables.sys.argv
            animationTables.sys.argv = ['generateAnimationTables.py'] + arguments
            try:
                with self.assertRaises(SystemExit) as context:
                    animationTables.main()
            finally:
                animationTables.sys.argv = originalArguments
            message = str(context.exception)
            self.assertIn(str(missingPath), message)
            self.assertIn('cannot open file', message)

    def testMainReportsOutputOpenErrorWithoutTraceback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            inputPath = root / 'tables.in'
            constantsPath = root / 'constants.h'
            inputPath.write_text('frames 1, -999')
            constantsPath.write_text('constexpr int kNumSprites = 1334;')
            arguments = [str(inputPath), str(root), str(root / 'out.cpp'),
                '--sprites', str(constantsPath), '--swos', str(constantsPath)]
            originalArguments = animationTables.sys.argv
            animationTables.sys.argv = ['generateAnimationTables.py'] + arguments
            try:
                with self.assertRaises(SystemExit) as context:
                    animationTables.main()
            finally:
                animationTables.sys.argv = originalArguments
            message = str(context.exception)
            self.assertIn(str(root), message)
            self.assertIn('cannot open file', message)


if __name__ == '__main__':
    unittest.main()
