# Script to fix SwosVM offsets in procedures pasted into C++ files.
# Looks for "vm.cpp" in current directory, then iterates over command line, processes each file
# and outputs fixed version in current directory (no overwrite).

import os
import re
import sys

def readVariables():
    vars = {}
    with open('vm.cpp') as vmFile:
        for line in vmFile:
            if match := re.match(r'[^\/]+\/\/ (\w+):(?:.*\[\+(\d+) alignment byte\(s\)\])?.*\|(\d+)\|$', line):
                address = match.group(3)
                if match.group(2):
                    address = str(int(address) + int(match.group(2)))
                vars[match.group(1)] = address
    return vars


def findNumber(line, i=0, end=None):
    num = None
    start = None
    if end is None:
        end = len(line)

    while i < end:
        if line[i].isdigit() and (i == 0 or not line[i - 1].isalpha()):
            num = 0
            start = i
            while line[i].isdigit() and i < end:
                num = num * 10 + int(line[i])
                i += 1
            if (i >= end or not line[i + 1].isalpha()) and num in range (256, 600_000):
                return num, i, start
        else:
            i += 1

    return None, i, start


def isPotentialVmSymbol(token):
    ignored = {
        'mov', 'movzx', 'movsx', 'lea', 'cmp', 'test', 'add', 'sub', 'and', 'or', 'xor',
        'shl', 'shr', 'sar', 'inc', 'dec', 'neg', 'not', 'imul', 'mul', 'idiv', 'div',
        'push', 'pop', 'call', 'byte', 'word', 'dword', 'ptr', 'small',
        'al', 'ah', 'ax', 'eax', 'bl', 'bh', 'bx', 'ebx', 'cl', 'ch', 'cx', 'ecx',
        'dl', 'dh', 'dx', 'edx', 'esi', 'edi', 'esp', 'ebp',
        'A0', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6',
        'D0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6', 'D7',
    }
    return token not in ignored and not re.fullmatch(r'[A-Z][A-Z0-9_]*', token)


numArgs = len(sys.argv)

if numArgs <= 1:
    sys.exit('No input files specified')

vars = readVariables()
missingSymbols = []

kOverflowLine8 = 'flags.overflow = dstSigned < 0 ? srcSigned > dstSigned - INT8_MIN : srcSigned < dstSigned - INT8_MAX'
kOverflowLine16 = 'flags.overflow = dstSigned < 0 ? srcSigned > dstSigned - INT16_MIN : srcSigned < dstSigned - INT16_MAX'
kOverflowLine32 = 'flags.overflow = dstSigned < 0 ? srcSigned > dstSigned - INT32_MIN : srcSigned < dstSigned - INT32_MAX'
kOverflowSubReplacement8 = 'flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int8_t>(res))) < 0'
kOverflowSubReplacement16 = 'flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int16_t>(res))) < 0'
kOverflowSubReplacement32 = 'flags.overflow = ((dstSigned ^ srcSigned) & (dstSigned ^ static_cast<int32_t>(res))) < 0'
kOverflowSubReplacements = ((kOverflowLine8, kOverflowSubReplacement8),
    (kOverflowLine16, kOverflowSubReplacement16), (kOverflowLine32, kOverflowSubReplacement32))
kOverflowAddReplacement8 = 'flags.overflow = ((dstSigned ^ static_cast<int8_t>(res)) & (srcSigned ^ static_cast<int8_t>(res))) < 0'
kOverflowAddReplacement16 = 'flags.overflow = ((dstSigned ^ static_cast<int16_t>(res)) & (srcSigned ^ static_cast<int16_t>(res))) < 0'
kOverflowAddReplacement32 = 'flags.overflow = ((dstSigned ^ static_cast<int32_t>(res)) & (srcSigned ^ static_cast<int32_t>(res))) < 0'
kOverflowAddReplacements = ((kOverflowSubReplacement8, kOverflowAddReplacement8),
    (kOverflowSubReplacement16, kOverflowAddReplacement16),
    (kOverflowSubReplacement32, kOverflowAddReplacement32))
kCarrySubLine8 = 'flags.carry = (byte)(dstSigned - srcSigned) > (byte)dstSigned'
kCarrySubLine16 = 'flags.carry = (word)(dstSigned - srcSigned) > (word)dstSigned'
kCarrySubLine32 = 'flags.carry = (dword)(dstSigned - srcSigned) > (dword)dstSigned'
kCarryAddLine8 = 'flags.carry = (byte)(dstSigned + srcSigned) < (byte)dstSigned'
kCarryAddLine16 = 'flags.carry = (word)(dstSigned + srcSigned) < (word)dstSigned'
kCarryAddLine32 = 'flags.carry = (dword)(dstSigned + srcSigned) < (dword)dstSigned'
kCarrySubReplacement8 = 'flags.carry = static_cast<uint8_t>(dstSigned) < static_cast<uint8_t>(srcSigned)'
kCarrySubReplacement16 = 'flags.carry = static_cast<uint16_t>(dstSigned) < static_cast<uint16_t>(srcSigned)'
kCarrySubReplacement32 = 'flags.carry = static_cast<uint32_t>(dstSigned) < static_cast<uint32_t>(srcSigned)'
kCarryAddReplacement8 = 'flags.carry = res < static_cast<uint8_t>(dstSigned)'
kCarryAddReplacement16 = 'flags.carry = res < static_cast<uint16_t>(dstSigned)'
kCarryAddReplacement32 = 'flags.carry = res < static_cast<uint32_t>(dstSigned)'
kCarryReplacements = ((kCarryAddLine8, kCarryAddReplacement8), (kCarryAddLine16, kCarryAddReplacement16), \
    (kCarryAddLine32, kCarryAddReplacement32), (kCarrySubLine8, kCarrySubReplacement8), \
    (kCarrySubLine16, kCarrySubReplacement16), (kCarrySubLine32, kCarrySubReplacement32))
kZeroFlagReplacement = (('flags.zero = res == 0 != 0', 'flags.zero = res == 0'),)


for i in range(1, numArgs):
    inputFilename = sys.argv[i]
    with open(inputFilename) as inputFile:
        outputFilename = os.path.basename(inputFilename);
        storedLines = []
        addressToReplace = [None, None]
        with open(outputFilename, 'w') as outputFile:
            lineNo = 0
            for line in inputFile:
                lineNo += 1
                replaced = False
                newLine = ''
                for replacements in (kOverflowSubReplacements, kCarryReplacements, kZeroFlagReplacement):
                    for find, replace in replacements:
                        newLine = line.replace(find, replace, 1)
                        if line != newLine:
                            replaced = True
                            break
                    if replaced:
                        if storedLines:
                            storedLines.append(newLine)
                        else:
                            outputFile.write(newLine)
                        break
                if replaced:
                    continue
                if line and line[0].isspace() and line.strip() == '{':
                    assert not storedLines
                    storedLines = [line]
                    continue
                elif line.strip() == '}':
                    if storedLines:
                        outputFile.write(''.join(storedLines))
                    storedLines = []
                    addressToReplace = [None, None]
                    outputFile.write(line)
                    continue
                continueOuter = False
                match = re.match(r'(?:.*g_memByte\[(\d+)\])?(?:.*(\/\/ .*$))?', line, re.I)
                if match.group(2):
                    i = 0
                    firstReplaced = False
                    parenthesizedOffset = False
                    explicitOffset = False
                    missingMemorySymbol = None
                    for token in match.group(2).replace(',', ' ').split()[1:]:
                        if token.startswith(';'):
                            break
                        if token.startswith('[') and token.endswith(']'):
                            token = token[1:-1]
                        try:
                            if token.index('+') < token.index('.'):
                                continue
                        except:
                            pass
                        if token == '(offset':
                            parenthesizedOffset = True
                            explicitOffset = True
                            continue
                        elif token.endswith(')') and parenthesizedOffset:
                            token = token[:-1]
                            parenthesizedOffset = False
                        elif token == 'offset':
                            explicitOffset = True
                            continue
                        if token != 'offset' and re.match(r'^[_a-z][\w.+]+$', token, re.I):
                            displacement = 0
                            if token.count('.') == 1:
                                token, field = token.split('.')
                                if field and token != 'Tactics':
                                    field, offset = (field.split('+') + [0])[:2]
                                    members = {
                                        'speed': 44, 'destX': 58, 'direction': 42, 'fullDirection': 82,
                                        'x': 30, 'y': 34, 'z': 38, 'deltaY': 50, 'deltaZ': 54,
                                        'playerNumber': 4, 'playerCoachNumber': 6, 'imageIndex': 70,
                                        'playerHasBall': 40, 'frameIndicesTable': 18, 'frameIndex': 22,
                                        'cycleFramesTimer': 26, 'firePressed': 50, 'resetControls': 142,
                                        'spritesTable': 20, 'destReachedState': 100
                                    }
                                    if not (fieldOffset := members.get(field)):
                                        print(f'Unknown struct field {field} at {inputFilename}:{lineNo}')
                                        continue
                                    displacement += members[field] + int(offset)
                            elif token.count('+') == 1:
                                token, offset = token.split('+')
                                try:
                                    displacement += int(offset)
                                except:
                                    continue
                            if not (address := vars.get(token)):
                                if explicitOffset:
                                    missingSymbols.append((inputFilename, lineNo, token))
                                elif match.group(1) and isPotentialVmSymbol(token):
                                    # A numeric g_memByte access is itself an explicit VM address.
                                    # Remember the last symbol-like operand so a removed VM item
                                    # cannot silently leave its former numeric address behind.
                                    missingMemorySymbol = token
                                explicitOffset = False
                                continue
                            missingMemorySymbol = None
                            explicitOffset = False
                            if displacement:
                                address = str(int(address) + displacement)
                            num, i, start = findNumber(line, i, match.start(2))
                            if num is not None:
                                if not match.group(1):
                                    if not address:
                                        print(f'{inputFilename}:{lineNo}')
                                    line = line[:start] + address + line[i:]
                                    if storedLines:
                                        print(f'{inputFilename}:{lineNo}')
                                        print(line, num)
                                        print(storedLines)
                                    assert not storedLines
                                    break
                                else:
                                    line = line[:start] + address + line[i:]
                                    if storedLines:
                                        outputFile.write(''.join(storedLines))
                                        outputFile.write(line)
                                        storedLines = []
                                        addressToReplace = [None, None]
                                        continue
                            if num is None and storedLines:
                                if addressToReplace[0] and not firstReplaced:
                                    assert [s for s in storedLines if addressToReplace[0] in s]
                                    storedLines = list(map(lambda s: s.replace(addressToReplace[0], address), storedLines))
                                    firstReplaced = True
                                    if addressToReplace[1]:
                                        continue
                                    else:
                                        break
                                elif addressToReplace[1]:
                                    assert [s for s in storedLines if addressToReplace[1] in s]
                                    storedLines = list(map(lambda s: s.replace(addressToReplace[1], address), storedLines))
                                break
                            elif not match.group(1):
                                outputFile.write(line)
                                continueOuter = True
                                break
                    if missingMemorySymbol:
                        missingSymbols.append((inputFilename, lineNo, missingMemorySymbol))
                    if continueOuter:
                        pass
                    elif storedLines:
                        if '// add' in line:
                            replaced = False
                            for j, storedLine in enumerate(storedLines):
                                for find, replace in kOverflowAddReplacements:
                                    newLine = storedLine.replace(find, replace, 1)
                                    if storedLine != newLine:
                                        storedLines[j] = newLine
                                        replaced = True
                                        break
                                if replaced:
                                    break
                        outputFile.write(''.join(storedLines))
                        outputFile.write(line)
                        storedLines = []
                        addressToReplace = [None, None]
                    elif not addressToReplace[0]:
                        outputFile.write(line)
                else:
                    if storedLines:
                        storedLines.append(line)
                        j = 0
                        for i in range(len(addressToReplace)):
                            if not addressToReplace[i]:
                                addressToReplace[i], j = findNumber(line, j)[:2]
                                if addressToReplace[i] is None:
                                    break
                                else:
                                    addressToReplace[i] = str(addressToReplace[i])
                    else:
                        outputFile.write(line)

if missingSymbols:
    for filename, lineNo, symbol in missingSymbols:
        print(f"Unknown VM symbol '{symbol}' in offset comment at {filename}:{lineNo}", file=sys.stderr)
    sys.exit(1)
