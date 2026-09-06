#!/usr/bin/env python3

# Parse animationTables.in, validate its sprite-frame references and direction
# mappings, and generate equivalent C++ frame arrays and animation-table data.

import argparse
import dataclasses
import pathlib
import re
import sys

import originalSwosAnimationTables

kDirections = ['top', 'top-right', 'right', 'bottom-right', 'bottom', 'bottom-left', 'left', 'top-left']
kSections = {'team 1 player': 0, 'team1 player': 0, 'team1player': 0,
    'team 2 player': 1, 'team2 player': 1, 'team2player': 1,
    'goalkeeper 1': 2, 'goalkeeper1': 2, 'goalkeeper 2': 3, 'goalkeeper2': 3}
kReservedKeywords = {'delay', 'export', 'loopback', 'wait'}
kOriginalExportedFrameNames = {
    'ballMovingFrameTable': 'ballMovingFrameIndices',
    'ballStaticFrameTable': 'ballStaticFrameIndices',
}
kMinLoopbackFrames = 2
kMaxLoopbackFrames = 32668
kMaxOutputLineLength = 108


class ParseError(Exception):
    pass


@dataclasses.dataclass
class Token:
    value: str
    line: int
    column: int


@dataclasses.dataclass
class FrameTable:
    name: str
    values: list
    line: int
    resolvedValues: list
    exported: bool = False

    def __post_init__(self):
        if not self.resolvedValues:
            raise ParseError(f'frame table {self.name!r} must contain at least one value')


@dataclasses.dataclass
class AnimationTable:
    name: str
    frameDelay: int
    sections: list
    referee: bool
    line: int


@dataclasses.dataclass
class ParsedFile:
    frameTables: list
    animationTables: list
    warnings: list


def fail(token, message):
    raise ParseError(f'{token.line}:{token.column}: {message}')


def tokenize(text):
    tokens = []
    for lineNumber, rawLine in enumerate(text.splitlines(), 1):
        line = re.split(r'//|;', rawLine, maxsplit=1)[0]
        position = 0
        while position < len(line):
            match = re.match(r'\s+|[()\[\]{},:]|-?\d+|[A-Za-z_][A-Za-z0-9_-]*', line[position:])
            if not match:
                raise ParseError(f'{lineNumber}:{position + 1}: unexpected character {line[position]!r}')
            value = match.group(0)
            if not value.isspace():
                tokens.append(Token(value, lineNumber, position + 1))
            position += len(value)
    return tokens


def readConstants(path):
    text = pathlib.Path(path).read_text(encoding='utf-8')
    return {name: int(value) for name, value in
        re.findall(r'\b(k[A-Za-z0-9_]+)\s*=\s*(-?\d+)', text)}


class Parser:
    def __init__(self, text, constants=None):
        self.tokens = tokenize(text)
        self.position = 0
        self.constants = constants or {}
        self.frames = []
        self.animations = []
        self.warnings = []

    def current(self):
        return self.tokens[self.position] if self.position < len(self.tokens) else None

    def take(self, value=None):
        token = self.current()
        if token is None:
            raise ParseError('unexpected end of file')
        if value is not None and token.value != value:
            fail(token, f'expected {value!r}, got {token.value!r}')
        self.position += 1
        return token

    def parse(self):
        names = set()
        while self.current():
            exported = self.current().value.lower() == 'export'
            if exported:
                self.take()
            name = self.take()
            if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', name.value):
                fail(name, 'expected an entity identifier')
            if name.value.lower() in kReservedKeywords:
                fail(name, f'{name.value!r} is reserved and cannot name a frame or animation table')
            if name.value in names:
                fail(name, f'duplicate entity {name.value!r}')
            names.add(name.value)
            if self.current() and self.current().value == '{':
                if exported:
                    fail(name, 'only frame tables can be exported')
                self.animations.append(self.parseAnimation(name))
            else:
                self.frames.append(self.parseFrames(name, exported))
        frameNames = {table.name for table in self.frames}
        undefined = set()
        for animation in self.animations:
            for section in animation.sections:
                for reference in section:
                    if reference and reference not in frameNames:
                        undefined.add(reference)
        if undefined:
            names = ', '.join(sorted(undefined))
            raise ParseError(f'undefined frame tables: {names}')
        return ParsedFile(self.frames, self.animations, self.warnings)

    def parseFrames(self, name, exported=False):
        return self.parseFrameTable(name, exported=exported)

    def parseFrameTable(self, name, closing=None, exported=False):
        values = []
        while self.current():
            token = self.current()
            if token.value == closing:
                break
            elif token.value in ('[', ']'):
                fail(token, f'unexpected {token.value!r} in frame table')
            elif token.value == ',':
                self.take()
            elif re.fullmatch(r'-?\d+', token.value):
                values.append(int(self.take().value))
            elif token.value.lower() in ('wait', 'delay'):
                values.append(self.parseFrameDelay())
            elif token.value.lower() == 'loopback':
                values.append(self.parseFrameLoopback())
            elif token.value in self.constants:
                values.append(self.take().value)
            else:
                break
        if not values:
            fail(name, 'frame table must contain at least one value')
        numeric = [self.constants.get(value, value) for value in values]
        maximum = self.constants.get('kNumSprites', 1334)
        if any(value > maximum or value < -32768 for value in numeric):
            fail(name, 'frame value is outside the supported range')
        if numeric[-1] > -100:
            fail(name, 'frame table must end in a loop, hold, or loopback marker')
        self.validateFrameControlFlow(name, numeric)
        return FrameTable(name.value, values, name.line, numeric, exported)

    def parseFrameDelay(self):
        return -self.parsePositiveFrameCount(1, 99)

    def parseFrameLoopback(self):
        return -100 - self.parsePositiveFrameCount(kMinLoopbackFrames, kMaxLoopbackFrames)

    def parsePositiveFrameCount(self, minimum, maximum):
        keyword = self.take()
        parenthesized = self.current() and self.current().value == '('
        if parenthesized:
            self.take('(')
        if not self.current():
            fail(keyword, f'{keyword.value} must be followed by a frame count')
        count = self.take()
        if not re.fullmatch(r'\d+', count.value):
            fail(count, f'{keyword.value} frame count must be a positive integer')
        countValue = int(count.value)
        if countValue < minimum or countValue > maximum:
            fail(count, f'{keyword.value} frame count must be in the range {minimum}..{maximum}')
        if parenthesized:
            self.take(')')
        return countValue

    def validateFrameControlFlow(self, name, values):
        if -100 in values:
            fail(name, 'frame value -100 causes an infinite control loop')

        if not any(value >= 0 for value in values):
            self.warnings.append(f'{name.line}:{name.column}: frame table contains no sprite indices')

        for index, value in enumerate(values[:-1]):
            if value in (-999, -101):
                self.warnings.append(
                    f'{name.line}:{name.column}: elements after frame control value {value} are unreachable')
                break

        for start in range(len(values)):
            index = start
            visited = set()
            while True:
                if index < 0 or index >= len(values):
                    fail(name, f'frame control flow from element {start} leaves the table')
                value = values[index]
                if value >= 0 or value == -101:
                    break
                if index in visited:
                    fail(name, f'frame control flow from element {start} never reaches a sprite or hold marker')
                visited.add(index)
                if value == -999:
                    index = 0
                elif value <= -100:
                    index += value + 100
                else:
                    index += 1

    def parseAnimation(self, name):
        self.take('{')
        delay = self.take()
        if not re.fullmatch(r'\d+', delay.value):
            fail(delay, 'animation frame delay must be a non-negative integer')
        if int(delay.value) > 255:
            fail(delay, 'animation frame delay does not fit in uint8_t')
        if self.current() and self.current().value == ',':
            self.take()
        sections = [[None] * 8 for _ in range(4)]
        referee = 'ref' in name.value.lower()
        if referee:
            sections[0] = self.parseDirectionBody(False)
        else:
            used = set()
            while self.current() and self.current().value != '}':
                words = []
                sectionToken = self.current()
                while self.current() and self.current().value != '{':
                    words.append(self.take().value)
                if not self.current():
                    fail(sectionToken, 'unterminated animation table')
                sectionName = ' '.join(words).lower()
                compactName = ''.join(words).lower()
                index = kSections.get(sectionName, kSections.get(compactName))
                if index is None:
                    fail(sectionToken, f'unknown animation section {sectionName!r}')
                if index in used:
                    fail(sectionToken, f'duplicate animation section {sectionName!r}')
                used.add(index)
                self.take('{')
                sections[index] = self.parseDirectionBody()
        self.take('}')
        return AnimationTable(name.value, int(delay.value), sections, referee, name.line)

    def parseDirectionBody(self, consumeClosing=True):
        entries = []
        while self.current() and self.current().value != '}':
            if self.current().value == ']':
                fail(self.current(), f'unexpected {self.current().value!r} in animation table')
            if self.current().value == '[':
                token = self.current()
                entries.append((None, self.parseFrameReference(), token))
                continue
            first = self.take()
            if self.current() and self.current().value == ':':
                self.take(':')
                entries.append(([first.value.lower()], self.parseFrameReference(), first))
            elif self.current() and self.current().value == 'to':
                self.take('to')
                last = self.take()
                self.take(':')
                entries.append(([first.value.lower(), last.value.lower()],
                    self.parseFrameReference(), first))
            else:
                entries.append((None, first.value, first))
        if consumeClosing:
            self.take('}')
        return self.placeEntries(entries)

    def parseFrameReference(self):
        if self.current() and self.current().value == '[':
            opening = self.take('[')
            name = Token(f'@inlineFrameTable{len(self.frames)}', opening.line, opening.column)
            table = self.parseFrameTable(name, ']')
            self.take(']')
            self.frames.append(table)
            return table.name
        return self.take().value

    def placeEntries(self, entries):
        slots = [None] * 8
        explicit = [entry for entry in entries if entry[0] is not None]
        implicit = [entry for entry in entries if entry[0] is None]
        allEntries = [entry for entry in explicit if entry[0] == ['all']]
        if allEntries:
            if len(entries) != 1:
                fail(allEntries[0][2], 'all: cannot be combined with other frame tables')
            return [allEntries[0][1]] * 8
        for specifier, reference, token in explicit:
            for index in self.directionIndices(specifier, token):
                if slots[index] is not None:
                    fail(token, f'direction {kDirections[index]!r} is assigned more than once')
                slots[index] = reference
        for _, reference, token in implicit:
            try:
                index = slots.index(None)
            except ValueError:
                fail(token, 'animation section contains more than eight frame tables')
            slots[index] = reference
        return slots

    def directionIndices(self, specifier, token):
        try:
            first = kDirections.index(specifier[0])
            if len(specifier) == 1:
                return [first]
            last = kDirections.index(specifier[1])
        except ValueError:
            fail(token, 'unknown direction or range endpoint')
        if first == last:
            self.warnings.append(f'{token.line}:{token.column}: range starts and ends at {specifier[0]!r}')
            return [first]
        result = [first]
        while result[-1] != last:
            result.append((result[-1] + 1) % 8)
        return result


def parseText(text, constants=None):
    return Parser(text, constants).parse()


def packFrameTables(frameTables, foldTables=True):
    frameData = []
    offsets = {}
    if not foldTables:
        for table in frameTables:
            offsets[table.name] = len(frameData)
            frameData.extend(table.resolvedValues)
        if len(frameData) > 32768 or any(offset > 32767 for offset in offsets.values()):
            raise ParseError('packed frame table data does not fit in signed 16-bit offsets')
        return frameData, offsets

    packedTables = []
    orderedTables = sorted(frameTables, key=lambda table: -len(table.values))
    for table in orderedTables:
        values = tuple(table.resolvedValues)
        containingTable = next((packed for packed in packedTables
            if len(values) <= len(packed[0]) and packed[0][-len(values):] == values), None)
        if containingTable:
            packedValues, packedOffset = containingTable
            offsets[table.name] = packedOffset + len(packedValues) - len(values)
        else:
            offsets[table.name] = len(frameData)
            frameData.extend(table.resolvedValues)
            packedTables.append((values, offsets[table.name]))

    if len(frameData) > 32768 or any(offset > 32767 for offset in offsets.values()):
        raise ParseError('packed frame table data does not fit in signed 16-bit offsets')
    return frameData, offsets


def packAnimationData(tables, frameOffsets, baseOffset, foldTables=True):
    animationData = []
    animationOffsets = {}
    animationValues = []
    for table in tables:
        flags, sections = getAnimationSections(table)
        if not sections:
            raise ParseError(f'animation table {table.name!r} does not contain any frame tables')
        offsets = tuple(frameOffsets[reference] if reference else -1
            for section in sections for reference in section)
        values = (table.frameDelay | flags << 8,) + offsets
        animationValues.append((table, values))

    packedTables = []
    orderedTables = animationValues if not foldTables else sorted(
        animationValues, key=lambda item: -len(item[1]))
    for table, values in orderedTables:
        containingTable = None if not foldTables else next((packed for packed in packedTables
            if len(values) <= len(packed[0]) and packed[0][-len(values):] == values), None)
        if containingTable:
            packedValues, packedOffset = containingTable
            animationOffsets[table.name] = baseOffset + packedOffset + len(packedValues) - len(values)
        else:
            animationOffsets[table.name] = baseOffset + len(animationData)
            animationData.extend(values)
            packedTables.append((values, animationOffsets[table.name] - baseOffset))

    if baseOffset + len(animationData) > 32768 or any(
        offset > 32767 for offset in animationOffsets.values()):
        raise ParseError('combined table data does not fit in signed 16-bit offsets')
    return animationData, animationOffsets


def getAnimationSections(table):
    if table.referee:
        return 1 << 4, [table.sections[0]]

    flags = 0
    sections = []
    for sectionIndex, section in enumerate(table.sections):
        if any(section):
            flags |= 1 << sectionIndex
            sections.append(section)
    return flags, sections


def getterName(tableName):
    return 'get' + tableName[0].upper() + tableName[1:]


def frameOffsetGetterName(tableName):
    return getterName(tableName) + 'Offset'


def formatArray(prefix, values, suffix):
    if not values:
        return [prefix + suffix]

    lines = []
    line = prefix
    for index, value in enumerate(values):
        last = index == len(values) - 1
        piece = str(value) + ('' if last else ',')
        separator = '' if line.endswith(' ') else ' '
        requiredLength = len(line) + len(separator) + len(piece) + (len(suffix) if last else 0)
        if requiredLength > kMaxOutputLineLength:
            lines.append(line.rstrip())
            line = '    ' + piece
        else:
            line += separator + piece
    lines.append(line + suffix)
    return lines


def originalAnimationFlags(name, frameTables):
    if name.lower().startswith('ref'):
        return 1 << 4

    flags = 0
    for section in range(4):
        if len(frameTables) > section * 8:
            flags |= 1 << section
    return flags


def generateOriginalTables():
    tableData = []
    frameOffsets = {}
    frameLengths = {}
    rawFrameOffsets = {}
    for rawOffset, name, values in originalSwosAnimationTables.kFrameTables:
        if values:
            frameOffsets[name] = len(tableData)
            rawFrameOffsets[rawOffset & 0xffffffff] = len(tableData)
            frameLengths[len(tableData)] = len(values)
            tableData.extend(values)
        elif rawOffset != -1:
            raise ParseError(f'original SWOS frame table {name!r} contains no data')
    if len(tableData) > 32768:
        raise ParseError('original SWOS frame table data does not fit in signed 16-bit offsets')

    rawAnimationTables = []
    for rawOffset, name, frameDelay, references in originalSwosAnimationTables.kAnimationTables:
        missing = [reference for reference in references
            if reference is not None and reference not in frameOffsets]
        if missing:
            raise ParseError(f'original SWOS animation table {name!r} references unknown frame table '
                f'{missing[0]!r}')
        offsets = [frameOffsets[reference] if reference is not None else -1
            for reference in references]
        flags = originalAnimationFlags(name, references)
        animationOffset = len(tableData)
        tableData.extend([frameDelay | flags << 8, *offsets])
        rawAnimationTables.append((rawOffset, name, animationOffset))
    if len(tableData) > 32768:
        raise ParseError('original SWOS table data does not fit in signed 16-bit offsets')
    source = formatArray('static const int16_t kTableData[] = { ', tableData, ' };')
    source.extend(['', '#if !defined(NDEBUG) || defined(SWOS_TEST)'])
    sortedFrameOffsets = sorted(set(rawFrameOffsets.values()))
    source.extend(formatArray('static const int16_t kValidFrameTableOffsets[] = { ',
        sortedFrameOffsets, ' };'))
    source.extend(formatArray('static const int16_t kFrameTableLengths[] = { ',
        [frameLengths[offset] for offset in sortedFrameOffsets], ' };'))
    source.extend(formatArray('static const int16_t kValidAnimationTableOffsets[] = { ',
        [offset for _, _, offset in rawAnimationTables], ' };'))
    source.append('#endif')
    originalFrameOffsets = {name: frameOffsets[name] for _, name, values in
        originalSwosAnimationTables.kFrameTables if values}
    return source, rawFrameOffsets, rawAnimationTables, originalFrameOffsets


def generate(parsed, sourceName, foldTables=True):
    header = [f'// Generated from {sourceName}; do not edit.', '#pragma once', '']
    source = [f'// Generated from {sourceName}; do not edit.', '#include "animationTables.h"',
        '']
    originalSource, rawFrameOffsets, rawAnimationTables, originalFrameOffsets = generateOriginalTables()
    source[1] = '// Include this file from a parent C++ implementation file after animation.h.'
    source.extend(['static constexpr int16_t kInvalidTableData = '
        'static_cast<int16_t>("zk"[0] | ("zk"[1] << 8));', ''])
    originalAnimationNames = {name for _, name, _ in rawAnimationTables}
    missingOriginalAnimations = [table.name for table in parsed.animationTables
        if table.name not in originalAnimationNames]
    includeOriginalTables = not missingOriginalAnimations
    if includeOriginalTables:
        source.append('#ifdef SWOS_TEST')
        source.extend(originalSource)
        source.extend(['', '#else', ''])
    frameData, frameOffsets = packFrameTables(parsed.frameTables, foldTables)
    frameOffsets = {name: offset + 1 for name, offset in frameOffsets.items()}
    frameLengths = {}
    for table in parsed.frameTables:
        offset = frameOffsets[table.name]
        frameLengths[offset] = max(frameLengths.get(offset, 0), len(table.values))
    sortedFrameOffsets = sorted(set(frameOffsets.values()))
    animationData, animationOffsets = packAnimationData(
        parsed.animationTables, frameOffsets, len(frameData) + 1, foldTables)
    source.extend(formatArray('static const int16_t kTableData[] = { ',
        ['kInvalidTableData'] + frameData + animationData, ' };'))
    source.extend(['', '#if !defined(NDEBUG) || defined(SWOS_TEST)'])
    source.extend(formatArray('static const int16_t kValidFrameTableOffsets[] = { ',
        sortedFrameOffsets or [-1], ' };'))
    source.extend(formatArray('static const int16_t kFrameTableLengths[] = { ',
        [frameLengths[offset] for offset in sortedFrameOffsets] or [0], ' };'))
    source.extend(formatArray('static const int16_t kValidAnimationTableOffsets[] = { ',
        sorted(set(animationOffsets.values())) or [-1], ' };'))
    source.extend(['#endif', ''])
    for table in parsed.animationTables:
        getter = getterName(table.name)
        header.append(f'int16_t {getter}();')
    for table in parsed.frameTables:
        if table.exported:
            header.append(f'int16_t {frameOffsetGetterName(table.name)}();')
    if includeOriginalTables:
        source.append('#endif')
    source.extend(['#if !defined(NDEBUG) || defined(SWOS_TEST)',
        'bool isValidFrameTableOffset(int16_t offset)', '{',
        '    return std::find(std::begin(kValidFrameTableOffsets),',
        '        std::end(kValidFrameTableOffsets), offset) != std::end(kValidFrameTableOffsets);',
        '}', '#endif', '',
        'const int16_t *getFrameTable(int16_t offset)', '{',
        '    if (offset < 0)', '        return nullptr;',
        '    assert(kTableData[0] != kInvalidTableData || offset != 0);',
        '    assert(isValidFrameTableOffset(offset));',
        '    return &kTableData[offset];', '}', '',
        'int16_t getFrameTableElement(int16_t offset, int index)', '{',
        '#if !defined(NDEBUG) || defined(SWOS_TEST)',
        '    const auto offsetIt = std::find(std::begin(kValidFrameTableOffsets),',
        '        std::end(kValidFrameTableOffsets), offset);',
        '    assert(offsetIt != std::end(kValidFrameTableOffsets));',
        '    assert(index >= 0 && index < kFrameTableLengths[',
        '        offsetIt - std::begin(kValidFrameTableOffsets)]);',
        '#endif',
        '    return getFrameTable(offset)[index];', '}', '',
        'const AnimationTable *getAnimationTable(int16_t offset)', '{',
        '    if (offset < 0)', '        return nullptr;',
        '    assert(kTableData[0] != kInvalidTableData || offset != 0);',
        '    assert(std::find(std::begin(kValidAnimationTableOffsets),',
        '        std::end(kValidAnimationTableOffsets), offset) !=',
        '        std::end(kValidAnimationTableOffsets));',
        '    return reinterpret_cast<const AnimationTable *>(&kTableData[offset]);', '}', ''])
    originalAnimationOffsets = {name: tableOffset for _, name, tableOffset in rawAnimationTables}
    if includeOriginalTables:
        source.append('#ifdef SWOS_TEST')
        for table in parsed.frameTables:
            if table.exported:
                getter = frameOffsetGetterName(table.name)
                originalName = kOriginalExportedFrameNames.get(table.name, table.name)
                if originalName not in originalFrameOffsets:
                    raise ParseError(f'exported frame table {table.name!r} has no original SWOS table')
                source.extend([f'int16_t {getter}()', '{',
                    f'    return {originalFrameOffsets[originalName]};', '}', ''])
        for table in parsed.animationTables:
            getter = getterName(table.name)
            source.extend([f'int16_t {getter}()', '{',
                f'    return {originalAnimationOffsets[table.name]};', '}', ''])
        source.extend(['#else', ''])
    for table in parsed.frameTables:
        if table.exported:
            getter = frameOffsetGetterName(table.name)
            source.extend([f'int16_t {getter}()', '{',
                f'    return {frameOffsets[table.name]};', '}', ''])
    for table in parsed.animationTables:
        getter = getterName(table.name)
        source.extend([f'int16_t {getter}()', '{',
            f'    return {animationOffsets[table.name]};', '}', ''])
    if includeOriginalTables:
        source.extend(['#endif', ''])
    if includeOriginalTables:
        source.extend(['#ifdef SWOS_TEST',
            'int16_t getOriginalSwosFrameTable(int32_t offset)', '{',
            '    switch (offset)', '    {', '    case -1:',
            '        return kInvalidFrameTableOffset;'])
        for rawOffset, packedOffset in rawFrameOffsets.items():
            rawOffsetText = (str(rawOffset) if rawOffset < 0x80000000 else
                (str(rawOffset - 0x100000000) if rawOffset >= 0xfffffffd else
                    ('static_cast<int32_t>(0x80000000)' if rawOffset == 0x80000000 else
                        f'static_cast<int32_t>(0x80000000 | {rawOffset & 0x7fffffff})')))
            source.extend([f'    case {rawOffsetText}:',
                f'        return {packedOffset};'])
        source.extend(['    default:', '        return kInvalidFrameTableOffset;', '    }', '}', '',
            'int16_t getOriginalSwosAnimationTable(int32_t offset)', '{',
            '    if (offset == -1)',
            '        return kInvalidAnimationTableOffset;',
            '    switch (offset)', '    {'])
        for rawOffset, name, tableOffset in rawAnimationTables:
            source.extend([f'    case {rawOffset}:',
                f'        return {tableOffset};'])
        source.extend(['    default:', '        return kInvalidAnimationTableOffset;',
            '    }', '}', '#endif', ''])
    return '\n'.join(header) + '\n', '\n'.join(source)


def printWarnings(path, warnings):
    for warning in warnings:
        print(f'{path}:{warning}: warning', file=sys.stderr)


def fileErrorMessage(error):
    path = error.filename or '<unknown file>'
    reason = error.strerror or str(error)
    return f'{path}: cannot open file: {reason}'


def main():
    arguments = argparse.ArgumentParser(
        description='Validate an animation-table source file and generate C++ declarations and data.',
        epilog='Example: python generateAnimationTables.py animationTables.in animationTables.h '
            'animationTables.cpp --sprites ../../sprites/sprites.h --swos ../../swos/swos.h')
    arguments.add_argument('input', help='animation-table source file to parse')
    arguments.add_argument('header', help='generated C++ header path')
    arguments.add_argument('source',
        help='generated C++ implementation fragment path (include from a parent .cpp)')
    arguments.add_argument('--sprites', required=True,
        help='sprites.h path used to resolve SpriteIndices constants')
    arguments.add_argument('--swos', required=True,
        help='swos.h path used to resolve animation marker constants')
    arguments.add_argument('--ignore-warnings', action='store_true',
        help='do not print parser warnings')
    arguments.add_argument('--no-table-folding', '--no-frame-table-folding',
        dest='fold_tables', action='store_false',
        help='store duplicate and suffix frame and animation tables separately')
    args = arguments.parse_args()
    try:
        constants = readConstants(args.sprites)
        constants.update(readConstants(args.swos))
        inputText = pathlib.Path(args.input).read_text(encoding='utf-8')
    except OSError as error:
        sys.exit(fileErrorMessage(error))
    parser = None
    try:
        parser = Parser(inputText, constants)
        parsed = parser.parse()
    except ParseError as error:
        if parser and not args.ignore_warnings:
            printWarnings(args.input, parser.warnings)
        sys.exit(f'{args.input}:{error}')
    if not args.ignore_warnings:
        printWarnings(args.input, parsed.warnings)
    header, source = generate(parsed, pathlib.Path(args.input).name, args.fold_tables)
    try:
        pathlib.Path(args.header).write_text(header, encoding='utf-8', newline='\n')
        pathlib.Path(args.source).write_text(source, encoding='utf-8', newline='\n')
    except OSError as error:
        sys.exit(fileErrorMessage(error))


if __name__ == '__main__':
    main()
