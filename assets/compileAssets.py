import re
import os
import sys
import math
import pathlib
import operator
import traceback
import multiprocessing

from typing import Final
from tabulate import tabulate

from PIL import Image
from PyTexturePacker import Packer, Utils

kOutDir: Final = os.path.join('..', 'tmp', 'assets')
k4kDir: Final = os.path.join(kOutDir, '4k')
kHdDir: Final = os.path.join(kOutDir, 'hd')
kLowResDir: Final = os.path.join(kOutDir, 'low-res')

kTextureDimension: Final = 2048
kPadding: Final = 2

kPitchesDir: Final = 'pitches'
kPitchWidth: Final = 42
kPitchHeight: Final = 53
kDefaultTrainingPitch: Final = 6

kResMultipliers: Final = (12, 6, 3)
kNumResolutions: Final = 3

kFixedSpritePrefixes: Final = ('charset', 'gameSprites')
kVariableSpritePrefixes: Final = ('goalkeeper', 'player', 'bench')

kWrapLimit: Final = 120

kWarning: Final = r'''// auto-generated, do not edit!
#pragma once'''

kAssert: Final = 'static_assert(static_cast<int>(AssetResolution::kNumResolutions) == 3, "Floating through the 80s in a digital 8-bit form");'

kHeader: Final = kWarning + '''\n
#include "PackedSprite.h"
#include "assetManager.h"
''' + '\n' + kAssert

def mkdir(path):
    try:
        os.mkdir(path)
    except FileExistsError:
        pass

def createDirs():
    mkdir(kOutDir)
    mkdir(k4kDir)
    mkdir(kHdDir)
    mkdir(kLowResDir)

def pack(kwargs):
    input = kwargs.pop('input')
    outputPath = kwargs.pop('outputPath')
    atlasNamePattern = kwargs.pop('atlasNamePattern')
    postProcessRoutine = kwargs.pop('post_process_routine', None)

    if 'trim_mode' not in kwargs:
        kwargs['trim_mode'] = 100
    if 'detect_duplicates' not in kwargs:
        kwargs['detect_duplicates'] = True
    if 'inner_padding' not in kwargs:
        kwargs['inner_padding'] = 1

    try:
        if atlasNamePattern.startswith('pitch'):
            return packPitch(input, atlasNamePattern, outputPath, **kwargs)
        else:
            packer = Packer.create(max_width=kTextureDimension, max_height=kTextureDimension,
                ignore_blank=True, border_padding=kPadding, shape_padding=kPadding, crop_atlas=True,
                atlas_format=Utils.ATLAS_FORMAT_JSON, **kwargs)

            return packer.pack(input, atlasNamePattern, output_path=outputPath, save_atlas_data=False,
                post_process_routine=postProcessRoutine)
    except Exception as e:
        print('Packing failed!')
        traceback.print_exc()
        print()
        raise e

def getResolutionName(res):
    return ('4k', 'HD', 'low-res')[res]

def extractIndex(name, prefix='spr'):
    start = len(prefix)
    end = len(prefix) + 4

    if name.startswith(prefix) and name[start:end].isdigit():
        return int(name[start:end])
    else:
        return -1

def getDirAtlasSprites(atlasData, metadata, res, atlasNamePrefixes, textureIndex):
    assert 0 <= res <= 2

    sprites = []
    maxSprite = -1

    for i, atlas in enumerate(atlasData):
        if not atlas['meta']['image'].startswith(atlasNamePrefixes):
            continue

        for filename, sprite in atlas['frames'].items():
            index = extractIndex(filename)
            if index < 0:
                continue

            maxSprite = max(maxSprite, index)

            spriteData = []

            # sync with PackedSprite.h
            for prop in ('x', 'y', 'w', 'h'):
                spriteData.append(sprite['frame'][prop])
            for floatValue in (False, True):
                for prop in ('x', 'y'):
                    value = sprite['spriteSourceSize'][prop]
                    spriteData.append(value / kResMultipliers[res] if floatValue else value)
            for dim in ('w', 'h'):
                if sprite['sourceSize'][dim] % kResMultipliers[res] != 0:
                    print(f"{filename}: invalid sprite dimension {sprite['sourceSize'][dim]} for resolution {res}")
                spriteData.append(sprite['sourceSize'][dim] // kResMultipliers[res])
            for dim in ('w', 'h'):
                spriteData.append(sprite['frame'][dim] / kResMultipliers[res])
            for floatValue in (False, True):
                for coord in metadata.get(index, (0, 0)):
                    value = coord / kResMultipliers[0] if floatValue else coord
                    spriteData.append(value)
            spriteData += (textureIndex + i, str(sprite['rotated']).lower())

            if sprite['rotated']:
                spriteData[2], spriteData[3] = spriteData[3], spriteData[2]
                spriteData[10], spriteData[11] = spriteData[11], spriteData[10]

            sprites.append((index, spriteData))

    return sprites, maxSprite

def getMetadata(paths):
    metadata = {}

    for path in paths:
        for file in pathlib.Path('sprites').joinpath(path).glob('spr*.txt'):
            index = extractIndex(file.name)
            if index >= 0:
                with open(file, 'r') as f:
                    lines = f.readlines()
                    assert len(lines) == 2
                    assert index not in metadata
                    metadata[index] = tuple(map(int, lines))

    return metadata

def getSprites(atlasData, atlasNamePrefixes, metadataPaths, files):
    assert len(atlasData) % kNumResolutions == 0

    sprites = [[], [], []]
    numSprites = -1
    textureIndices = [0, 0, 0]

    metadata = getMetadata(metadataPaths)

    for i, dirAtlasData in enumerate(atlasData):
        res = i % kNumResolutions
        dirSprites, maxSprite = getDirAtlasSprites(dirAtlasData, metadata, res, atlasNamePrefixes, textureIndices[res])
        numSprites = max(numSprites, maxSprite)
        sprites[res].extend(dirSprites)
        if dirAtlasData[0]['meta']['image'] in files:
            textureIndices[res] += len(dirAtlasData)

    numSprites += 1
    spriteArray = [[None] * numSprites, [None] * numSprites, [None] * numSprites]

    for res, resSprites in enumerate(sprites):
        for index, sprite in resSprites:
            assert spriteArray[res][index] is None
            spriteArray[res][index] = sprite

    return spriteArray, numSprites

def getFilenameMapping(atlasData, atlasNamePrefixes):
    files = set()

    for resAtlasData in atlasData:
        for atlas in resAtlasData:
            if atlas['meta']['image'].startswith(atlasNamePrefixes):
                files.add(atlas['meta']['image'])

    return {file:index for (index, file) in enumerate(sorted(files))}

def generateFileMap(atlasData, atlasNamePrefixes):
    assert len(atlasData) % kNumResolutions == 0

    files = getFilenameMapping(atlasData, atlasNamePrefixes)

    fileMap = [[] for i in range(kNumResolutions)]

    for i, resAtlasData in enumerate(atlasData):
        res = i % kNumResolutions
        for atlas in resAtlasData:
            if atlas['meta']['image'].startswith(atlasNamePrefixes):
                fileMap[res].append(files[atlas['meta']['image']])

    maxFiles = len(files)
    fileMap = map(lambda l: l + ([-1] * (maxFiles - len(l))), fileMap)

    return maxFiles, files, fileMap

def convertSpriteFieldForOutput(field):
    import struct

    if isinstance(field, float):
        field = struct.unpack('f', struct.pack('f', field))[0]
    elif isinstance(field, int):
        field = hex(field) + 'u' if field > 0x7fff else field
    return str(field)

def outputSprite(sprite, index, out):
    out('        { ', end='')
    if sprite:
        line = ', '.join(map(convertSpriteFieldForOutput, sprite))
    else:
        line = '0'
    out((line + ' },').ljust(48), f'// {index}')

def outputSprites(sprites, out):
    for i, resSprites in enumerate(sprites):
        out(f'    // {getResolutionName(i)}\n    {{{{')
        for j, sprite in enumerate(resSprites):
            outputSprite(sprite, j, out)
        out('    }},')
    out('}};')

def outputSpritesArray(atlasData, out, files):
    sprites, numSprites = getSprites(atlasData, kFixedSpritePrefixes, ('game', 'menu'), files)
    out(f'static std::array<std::array<PackedSprite, {numSprites}>, {kNumResolutions}> m_packedSprites =\n{{{{')
    outputSprites(sprites, out)

def getVariableSpritePaths():
    paths = []

    for root in ('goalkeeper', 'player'):
        for subdir in ('background', 'socks', 'hair', 'skin', 'shorts'):
            paths.append((root, subdir))

    for root, subdir in (('player', 'shirt'), ('bench', 'background'), ('bench', 'shirt')):
        paths.append((root, subdir))

    return paths

def outputVariableSprites(atlasData, out, files):
    for root, subdir in getVariableSpritePaths():
        sprites, numSprites = getSprites(atlasData, f'{root}-{subdir}', (os.path.join('game', root, subdir),), files)
        if subdir == 'background':
            if root == 'player':
                out(f'constexpr int kNumPlayerSprites = {numSprites};')
            elif root == 'goalkeeper':
                out(f'constexpr int kNumGoalkeeperSprites = {numSprites};')
        out(f'\nstatic const std::array<std::array<PackedSprite, {numSprites}>, {kNumResolutions}> k{root.title()}{subdir.title()} =\n{{{{')
        outputSprites(sprites, out)

def getNumTexturesNeeded(files):
    numBasicTextures = sum('-' not in file for file in files)
    # 2 * 3 players sets (one set per team per face), 2 * 2 goalkeeper sets, 2 bench player sets
    return numBasicTextures, numBasicTextures + 2 * 3 + 2 * 2 + 2

def outputTextureFilenames(maxFiles, files, fileMap, out, generateTextureToFile=True):
    import textwrap

    out(f'static const std::array<const char *, {maxFiles}> kTextureFilenames = {{')

    fileList = ', '.join(map(lambda filename: f'"{filename}"', files.keys()))
    fileListOutput = textwrap.fill(fileList, width=kWrapLimit, expand_tabs=False, replace_whitespace=False,
        break_long_words=False, break_on_hyphens=False, initial_indent='    ', subsequent_indent='    ')
    out(fileListOutput)
    out('};\n')

    if generateTextureToFile:
        out(f'static const std::array<std::array<int, {maxFiles}>, {kNumResolutions}> kTextureToFile =', '{{')
        for fileIndices in fileMap:
            out('    {', ', '.join(map(str, fileIndices)), ' },')
        out('}};\n')

def getOutputFunc(file):
    return lambda *args, **kwargs: print(*args, **kwargs, file=file)

def generateSpriteDatabase(atlasData):
    assert len(atlasData) % kNumResolutions == 0

    with open(os.path.join(kOutDir, 'spriteDatabase.h'), 'w') as f:
        out = getOutputFunc(f)

        out(kHeader)

        maxFiles, files, fileMap = generateFileMap(atlasData, kFixedSpritePrefixes)
        outputTextureFilenames(maxFiles, files, fileMap, out)

        numBasicTextures, numTextures = getNumTexturesNeeded(files)
        out(f'constexpr int kNumBasicTextures = {numBasicTextures};')
        out(f'static std::array<std::array<SDL_Texture *, {numTextures}>, {kNumResolutions}> m_textures;\n')

        outputSpritesArray(atlasData, out, files)

def generateVariableSprites(atlasData):
    with open(os.path.join(kOutDir, 'variableSprites.h'), 'w') as f:
        out = getOutputFunc(f)

        out(kHeader)

        maxFiles, files, fileMap = generateFileMap(atlasData, kVariableSpritePrefixes)
        outputTextureFilenames(maxFiles, files, fileMap, out)

        outputVariableSprites(atlasData, out, files)

def outputPitchIndices(pitches, out):
    out(f'static const uint16_t kPitchIndices[{len(pitches)}][{kPitchHeight}][{kPitchWidth}] = {{')
    for pitch in pitches:
        indexFile = os.path.join(pitch, os.path.basename(pitch) + '.txt')
        with open(indexFile, 'r') as f:
            pitchMatrix = f.read().split()
            assert len(pitchMatrix) == 55 * kPitchWidth

            out('    {')
            for i in range(1, 54):
                out('        ', end='')
                for j in range(kPitchWidth):
                    out(f"{pitchMatrix[kPitchWidth * i + j]},{' ' if j < 41 else chr(10)}", end='')
            out('    },')
    out('};')

def getPatterns(atlasData, pitches):
    patterns = [[] for i in range(kNumResolutions)]
    memoryPerPitch = [[0] * kNumResolutions for i in range(len(pitches))]
    pitchStartPatterns = [0] * len(pitches)

    texture = 0
    maxAtlases = 0

    # relies on the fact that the pitches are in the original order
    for i, resAtlasData in enumerate(atlasData):
        res = i % kNumResolutions

        pitchName = resAtlasData[0]['meta']['image']
        if pitchName.startswith('pitch'):
            index = int(pitchName[5:pitchName.index('-')]) - 1
            if res == 0:
                pitchStartPatterns[index] = len(patterns[res])

            # careful, files within an atlas are arbitrarily ordered
            atlasPatterns = []
            for j, atlas in enumerate(resAtlasData):
                memoryPerPitch[index][res] += atlas['meta']['size']['w'] * atlas['meta']['size']['h'] * 4
                for filename, pattern in atlas['frames'].items():
                    fileIndex = int(filename[filename.index('-') + 1 : filename.index('.')])
                    atlasPatterns.append((fileIndex, (pattern['frame']['x'], pattern['frame']['y'], texture + j)))

            for pattern in (patterns[1] for patterns in sorted(atlasPatterns, key=operator.itemgetter(0))):
                patterns[res].append(pattern)

            if res == kNumResolutions - 1:
                maxAtlases = max(map(len, atlasData[i - 3:i]))
                texture += maxAtlases

    return patterns, pitchStartPatterns, memoryPerPitch

def outputPitchPatterns(resPatterns, out):
    out(f'static const std::array<std::array<PackedPitchPattern, {len(resPatterns[0])}>, {kNumResolutions}> kPatterns = {{{{')
    for patterns in resPatterns:
        out('    {{')
        for i, pattern in enumerate(patterns):
            out(f'        {{ {", ".join(map(str, pattern))} }}, '.ljust(32), f'// {i}')
        out('    }},')
    out('}};')

def outputPitchPatternStartIndices(pitchStartPatterns, out):
    out(f'static const std::array<int, {len(pitchStartPatterns)}> kPitchPatternStartIndices = {{{{')
    out('   ', ', '.join(map(str, pitchStartPatterns)))
    out('}};')

def printPitchMemoryUsage(memoryPerPitch):
    assert kNumResolutions == 3

    def humanize(val):
        p = int(math.floor(math.log(val, 2) / 10))
        val = val / math.pow(1024, p)
        units = ['b','KiB','MiB','GiB','TiB','PiB','EiB','ZiB','YiB'][p]
        return f'{val:.2f} {units}'

    for i, row in enumerate(memoryPerPitch):
        row = list(map(humanize, row))
        memoryPerPitch[i] = [i + 1] + row

    print('Approximate video memory requirements per pitch:')
    print(tabulate(memoryPerPitch, headers=['#', '4k', 'HD', 'low-res'], tablefmt='fancy_grid'))

def outputPitchPatternData(atlasData, pitches, trainingPitch, out):
    resPatterns, pitchStartPatterns, memoryPerPitch = getPatterns(atlasData, pitches)
    printPitchMemoryUsage(memoryPerPitch)
    print(f'\nTraining pitch: {trainingPitch}')
    outputPitchPatterns(resPatterns, out)
    out('')
    outputPitchPatternStartIndices(pitchStartPatterns, out)

def generatePitchDatabase(atlasData, pitches, trainingPitch):
    with (open(os.path.join(kOutDir, 'pitchDatabase.h'), 'w')) as f:
        out = getOutputFunc(f)

        out(kWarning, '#include "assetManager.h"', kAssert, sep='\n\n', end='\n\n')
        out(f'static constexpr int kNumPitches = {len(pitches)};')
        out(f'static constexpr int kPitchPatternWidth = {kPitchWidth};')
        out(f'static constexpr int kPitchPatternHeight = {kPitchHeight};\n')

        out(f'static constexpr int kTrainingPitchIndex = {trainingPitch - 1};\n')

        maxFiles, files, fileMap = generateFileMap(atlasData, 'pitch')
        outputTextureFilenames(maxFiles, files, fileMap, out, generateTextureToFile=False)

        out(f'static std::array<std::array<SDL_Texture *, {maxFiles}>, {kNumResolutions}> m_pitchTextures;\n')

        patternSizes = map(lambda mul: str(mul * 16), kResMultipliers)
        out('constexpr int kPatternSizes[static_cast<int>(AssetResolution::kNumResolutions)] = {', end='')
        out(f' {", ".join(patternSizes)} }};')
        out('''
struct PackedPitchPattern {
    int16_t x;
    int16_t y;
    int8_t texture;
};
''')

        outputPitchIndices(pitches, out)
        out('')
        outputPitchPatternData(atlasData, pitches, trainingPitch, out)

def generateStadiumSprites(atlasData):
    with open(os.path.join(kOutDir, 'stadiumSprites.h'), 'w') as f:
        out = getOutputFunc(f)

        out(kHeader)

        maxFiles, files, fileMap = generateFileMap(atlasData, 'stadium')
        sprites, numSprites = getSprites(atlasData, 'stadium', os.path.join('game', 'stadium'), files)

        if maxFiles > 1:
            sys.exit("Can't handle multiple stadium sprite atlas files")

        out(f'\nconstexpr char kStadiumSpritesAtlas[] = "{next(iter(files))}";')

        assert numSprites == 9

        out('''
enum StadiumSprites {
    kPlayerSkin = 0,
    kPlayerHair = 1,
    kPlayerShirtVerticalStripes = 2,
    kPlayerShorts = 3,
    kPlayerSocks = 4,
    kPlayerBackground = 5,
    kPlayerShirtColoredSleeves = 6,
    kPlayerShirtHorizontalStripes = 7,
    kGoalkeeperBackground = 8,
};\n''')

        out(f'static const std::array<byte, {len(kResMultipliers)}> kResMultipliers = {{\n   ', end='')
        for multiplier in kResMultipliers:
            out(f' {multiplier},', end='')
        out('\n};\n')

        out(f'static const std::array<std::array<PackedSprite, {numSprites}>, {kNumResolutions}> m_stadiumSprites = {{{{')
        outputSprites(sprites, out)

def formDir(dir1, dir2):
    return os.path.join('sprites', 'game', dir1, dir2)

def getPitches():
    seenPitches = set()
    pitches = []

    for entry in os.scandir(kPitchesDir):
        if entry.is_dir():
            pitchIndex = extractIndex(entry.name, 'pitch')
            if pitchIndex >= 0:
                if pitchIndex in seenPitches:
                    sys.exit(f'Duplicate pitch {pitchIndex} encountered')
                seenPitches.add(pitchIndex)
                pitchDir = os.path.join(kPitchesDir, entry.name)
                pitches.append(pitchDir)

    seenPitches = sorted(seenPitches)
    if (seenPitches[0] != 1):
        sys.exit('Pitches must start with 1')

    if len(seenPitches) != seenPitches[-1]:
        sys.exit('Pitch numbers must be consecutive')

    return pitches

def getOutputPatternSize(images, scale):
    if not all(images[0].size == image.size for image in images):
        sys.exit('All pitch patterns must have same dimensions')

    if images[0].width != images[0].height:
        sys.exit('Pitch patterns must be squares')

    patternSize = images[0].width
    outputPatternSize = patternSize * scale

    if outputPatternSize != int(outputPatternSize):
        sys.exit(f'Invalid pattern size {patternSize} for scale {scale}')

    return int(outputPatternSize)

def getPitchAtlasSize(numPatterns, maxPatternsPerSide):
    import math

    patternsX = patternsY = int(math.sqrt(numPatterns))
    if patternsX * patternsY < numPatterns:
        patternsY += 1
    if patternsX * patternsY < numPatterns:
        patternsX += 1

    assert patternsX * patternsY >= numPatterns

    patternsX = min(patternsX, maxPatternsPerSide)
    patternsY = min(patternsY, maxPatternsPerSide)

    return patternsX, patternsY

def resizeImages(images, outputPatternSize, scale):
    if scale == 1:
        return images

    scaledImages = []

    for i, image in enumerate(images):
        scaledImage = image.resize((outputPatternSize, outputPatternSize), Image.LANCZOS)
        scaledImage.filename = image.filename
        scaledImages.append(scaledImage)

    return scaledImages

def growPatternEdges(atlas, img, x, y):
    borders = (
        ((x, y - 1), (0, 0, img.width, 1)),                                 # top
        ((x - 1, y), (0, 0, 1, img.height)),                                # left
        ((x + img.width, y), (img.width - 1, 0, img.width, img.height)),    # right
        ((x, y + img.height), (0, img.height - 1, img.width, img.height)),  # bottom
    )

    for (destX, destY), border in borders:
        region = img.crop(border)
        atlas.paste(region, (destX, destY))

    pa = atlas.load()
    pa[x - 1, y - 1] = pa[x, y]
    pa[x + img.width, y - 1] = pa[x + img.width - 1, y]
    pa[x - 1, y + img.height] = pa[x, y + img.height - 1]
    pa[x + img.width, y + img.height] = pa[x + img.width - 1, y + img.height - 1]

def packPitch(input, atlasNamePattern, outputPath, **kwargs):
    images = filter(lambda fname: fname.lower().endswith('.png'), os.listdir(input))
    images = map(lambda fname: os.path.join(input, fname), images)
    images = list(map(Image.open, images))

    scale = kwargs.get('scale', 1)
    outputPatternSize = getOutputPatternSize(images, scale)
    images = resizeImages(images, outputPatternSize, scale)

    maxPatternsPerSide = (kTextureDimension - kPadding) // (outputPatternSize + kPadding)
    maxPatternsPerAtlas = maxPatternsPerSide ** 2
    numAtlases = (len(images) + (maxPatternsPerAtlas - 1)) // maxPatternsPerAtlas

    result = []
    currentPattern = 0

    for i in range(numAtlases):
        atlasName = atlasNamePattern.replace('%d', str(i)) + '.png'
        atlasData = {'frames': {}, 'meta': {'image': atlasName, 'format': 'RGB888', 'scale': 1 }}
        patternsLeft = lambda: len(images) - currentPattern
        patternsX, patternsY = getPitchAtlasSize(patternsLeft(), maxPatternsPerSide)

        w = patternsX * (outputPatternSize + kPadding) + kPadding
        h = patternsY * (outputPatternSize + kPadding) + kPadding

        atlas = Image.new(mode="RGB", size=(w, h))

        y = kPadding
        for _ in range(patternsY):
            x = kPadding

            for _ in range(min(patternsX, patternsLeft())):
                img = images[currentPattern]
                assert img.width == outputPatternSize and img.height == outputPatternSize

                atlas.paste(img, (x, y))
                growPatternEdges(atlas, img, x, y)

                atlasData['frames'][os.path.basename(img.filename)] = {
                    'frame': { 'x': x, 'y': y, 'w': outputPatternSize, 'h': outputPatternSize },
                    'spriteSourceSize': { 'x': x, 'y': y, 'w': outputPatternSize, 'h': outputPatternSize },
                    'sourceSize': { 'w': outputPatternSize, 'h': outputPatternSize },
                    'rotated': False, 'trimmed': False,
                }

                x += outputPatternSize + kPadding
                currentPattern += 1

            y += outputPatternSize + kPadding

        atlasPath = os.path.join(outputPath, atlasName)
        atlas.save(atlasPath)

        atlasData['meta']['size'] = {'w': atlas.width, 'h': atlas.height}

        result.append(atlasData)

    return result

def getOptions(pitches):
    gameSprites = tuple(map(str, pathlib.Path('sprites').joinpath('game').glob('spr*.png')))

    options = [
        dict(input=gameSprites, atlasNamePattern='gameSprites%d'),
        dict(input=os.path.join('sprites', 'menu'), atlasNamePattern='charset%d'),
        dict(input=os.path.join('sprites', 'game', 'stadium'), enable_rotated=False, atlasNamePattern='stadium%d'),
    ]

    for root, subdir in getVariableSpritePaths():
        # disable rotation since SDL surfaces don't support it
        options.append(dict(enable_rotated=False, input=formDir(root, subdir), atlasNamePattern=f'{root}-{subdir}%d'))

    for i, pitch in enumerate(pitches, 1):
        assert pitch.endswith(f'pitch{i}')
        options.append(dict(input=pitch, enable_rotated=False, inner_padding=0, atlasNamePattern=f'pitch{i}-%d'))

    result = []
    for option in options:
        # turn off trimming and duplicate removal for background so it can be used as a destination
        if 'background' in option['input']:
            option['trim_mode'] = 0
            option['detect_duplicates'] = 0

        result.append(dict(option, outputPath=k4kDir))
        result.append(dict(option, outputPath=kHdDir, scale=0.5, scale_filter='LANCZOS'))
        result.append(dict(option, outputPath=kLowResDir, scale=0.25, scale_filter='LANCZOS'))

    return result

def parseCommandLine(options):
    kTrainingPitchRegex: Final = re.compile(r'\s*--training-?pitch\s*=\s*(?P<trainingPitch>.*)', re.IGNORECASE)

    blocky = False
    trainingPitch = kDefaultTrainingPitch

    for arg in sys.argv[1:]:
        if arg.lower().replace('-', '') == 'blocky':
            if not blocky:
                for option in options:
                    if isinstance(option['input'], str):
                        option['input'] = (option['input'],)
                    result = []
                    for path in option['input']:
                        path = os.path.normpath(path)
                        if path.startswith('sprites/game'):
                            path = path.replace('sprites/game', 'sprites/game/blocky')
                        result.append(path)
                    if len(result) == 1:
                        option['input'] = result[0]
                    else:
                        option['input'] = result
                blocky = True
        elif match := re.match(kTrainingPitchRegex, arg):
            try:
                trainingPitch = int(match.group('trainingPitch'))
            except ValueError:
                sys.exit('Training pitch must be specified with an integer')

    return options, trainingPitch

def main():
    createDirs()

    pitches = getPitches()
    options = getOptions(pitches)
    options, trainingPitch = parseCommandLine(options)

    if trainingPitch > len(pitches):
        sys.exit(f'Invalid training pitch {trainingPitch}')

    pool = multiprocessing.Pool()
    atlasData = pool.map(pack, options)

    pool.close()
    pool.join()

    generateSpriteDatabase(atlasData)
    generateVariableSprites(atlasData)
    generatePitchDatabase(atlasData, pitches, trainingPitch)
    generateStadiumSprites(atlasData)

    print('\nSuccess.', end='');

if __name__ == '__main__':
    main()
