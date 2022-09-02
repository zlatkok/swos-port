'''Merge separated sprite layer images into a single multi-layered TIFF/PSD more convenient for artists.
Do the reverse operation if started with command line parameter "x".
Requires ImageMagick to be present in the path.'''

import os
import sys
import subprocess

from typing import Final

kPlayerSpritesDir: Final = os.path.join('sprites', 'game', 'player')
kGoalkeeperSpritesDir: Final = os.path.join('sprites', 'game', 'goalkeeper')
kStadiumSpritesDir: Final = os.path.join('sprites', 'game', 'stadium')
kBenchSpritesDir: Final = os.path.join('sprites', 'game', 'bench')

kOutputDir: Final = 'sprite-layers'
kOutputPlayersDir: Final = os.path.join(kOutputDir, 'player')
kOutputGoalkeepersDir: Final = os.path.join(kOutputDir, 'goalkeeper')
kOutputStadiumSpritesDir: Final = os.path.join(kOutputDir, 'stadium')

kUnpackInputDir: Final = 'assets-packed'
kUnpackOutputDir: Final = 'sprite-layers-unpacked'
kUnpackOutputPlayersDir: Final = os.path.join(kUnpackOutputDir, 'player')
kUnpackOutputGoalkeepersDir: Final = os.path.join(kUnpackOutputDir, 'goalkeeper')
kUnpackOutputStadiumSpritesDir: Final = os.path.join(kUnpackOutputDir, 'stadium')
kUnpackOutputBenchSpritesDir: Final = os.path.join(kUnpackOutputDir, 'bench')

kLayers: Final = ('background', 'hair', 'skin', 'shirt', 'shorts', 'socks')

kStadiumSpriteLayers: Final = (
    ((5, 0, 1, 2, 3, 4), 'player-vertical-stripes'),
    ((5, 0, 1, 7, 3, 4), 'player-horizontal-stripes'),
    ((5, 0, 1, 6, 3, 4), 'player-colored-sleeves'),
    ((8, 0, 1, 3, 4),    'goalkeeper'),
)

def main():
    extract = len(sys.argv) <= 1 or sys.argv[1].lower() != 'x'
    if extract:
        commands = packSpriteLayers()
    else:
        commands = unpackSpriteLayers()

    result = runCommands(commands)

    if extract:
        for i in range(10):
            filename = f'{i}.png'
            if os.path.isfile(filename):
                print(f"Warning: orphaned file {filename}")

    return result

def packSpriteLayers():
    mkdir(kOutputDir)
    mkdir(kOutputPlayersDir)
    mkdir(kOutputGoalkeepersDir)
    mkdir(kOutputStadiumSpritesDir)

    commands = packPlayers()
    commands += packGoalkeepers()
    commands += packStadiumSprites()

    return commands

def unpackSpriteLayers():
    if not os.path.isdir(kUnpackInputDir):
        sys.exit(f'"{kUnpackInputDir}" directory missing.')

    mkdir(kUnpackOutputDir)
    mkdir(kUnpackOutputPlayersDir)
    mkdir(kUnpackOutputGoalkeepersDir)
    mkdir(kUnpackOutputStadiumSpritesDir)
    mkdir(kUnpackOutputBenchSpritesDir)

    commands = unpackPlayers()
    commands += unpackGoalkeepers()
    commands += unpackStadiumSprites()
    commands += unpackBenchSprites()

    return commands

def packPlayers():
    return packPlayersAndGoalkeepers(kLayers, 'player')

def packGoalkeepers():
    layers = list(filter(lambda layer: layer != 'shirt', kLayers))
    return packPlayersAndGoalkeepers(layers, 'goalkeeper')

def packStadiumSprites():
    filenames = tuple(f'spr000{i}.png' for i in range(9))

    commands = []
    for layers, outName in kStadiumSpriteLayers:
        fileList = []
        for i in layers:
            fileList.append(os.path.join(kStadiumSpritesDir, filenames[i]))
        commands.extend(formPackCommand(fileList, os.path.join(kOutputStadiumSpritesDir, outName)))

    return commands

def packPlayersAndGoalkeepers(layers, inputDir):
    commands = []
    inputPath = os.path.join('sprites', 'game', inputDir)
    outputPath = os.path.join(kOutputDir, inputDir)

    for entry in os.scandir(os.path.join(inputPath, 'background')):
        if entry.is_file():
            name, ext = os.path.splitext(entry.name)
            if ext.lower() == '.png':
                inputFiles = []
                for layer in layers:
                    inputFiles.append(os.path.join('sprites', 'game', inputDir, layer, name + ext))
                ord = extractIndex(name)
                outputBasePath = os.path.join(outputPath, inputDir + ord)
                commandLines = formPackCommand(inputFiles, outputBasePath)
                commands.extend(commandLines)

    return commands

def unpackPlayers():
    return unpackPlayersAndGoalkeepers(kLayers, 'player')

def unpackGoalkeepers():
    layers = list(filter(lambda layer: layer != 'shirt', kLayers))
    return unpackPlayersAndGoalkeepers(layers, 'goalkeeper')

def unpackPlayersAndGoalkeepers(layers, dir):
    commands = []
    inputPath = os.path.join(kUnpackInputDir, 'sprites', 'game', dir)
    outputPath = os.path.join(kUnpackOutputDir, dir)

    for layer in layers:
        mkdir(os.path.join(outputPath, layer))

    def handleGraphicFile(path, index):
        # skip layer 1 since it appears to contain the whole image (all layers combined)
        commands.append(formUnpackCommand(path))
        for i, layer in enumerate(layers):
            dst = os.path.join(outputPath, layer, f'spr{index}.png')
            commands.append(f'move {i}.png {dst}')

    scanDirForGraphicFiles(inputPath, handleGraphicFile)

    return commands

def unpackStadiumSprites():
    commands = []
    layers = []
    usedIndices = set()
    inputPath = os.path.join(kUnpackInputDir, kStadiumSpritesDir)

    for indices, baseFilename in kStadiumSpriteLayers:
        indicesToExtract = set(indices) - usedIndices
        indices = list(map(lambda i: i if i in indicesToExtract else None, indices))
        layers.append((indices, baseFilename))
        usedIndices.update(indicesToExtract)

    for indices, baseFilename in layers:
        for ext in ('psd', 'tif', 'tiff'):
            filePath = os.path.join(inputPath, f"{baseFilename}.{ext}")
            if os.path.isfile(filePath):
                break
        else:
            sys.exit('Missing file: {baseFilename}.tiff/psd')

        commands.append(formUnpackCommand(filePath))

        for srcIndex, dstIndex in enumerate(indices):
            src = f"{srcIndex}.png"
            if dstIndex is not None:
                dst = os.path.join(kUnpackOutputStadiumSpritesDir, f"spr{dstIndex:04}.png")
                commands.append(f'move {src} {dst}')
            else:
                commands.append(f'del {src}')

    return commands

def unpackBenchSprites():
    commands = []

    backgroundDir = os.path.join(kUnpackOutputBenchSpritesDir, 'background')
    shirtDir = os.path.join(kUnpackOutputBenchSpritesDir, 'shirt')
    mkdir(backgroundDir)
    mkdir(shirtDir)

    inputPath = os.path.join(kUnpackInputDir, kBenchSpritesDir)

    def handleGraphicFile(path, index):
        commands.append(formUnpackCommand(path))
        dstBackground = os.path.join(backgroundDir, f'spr{index}.png')
        dstShirt = os.path.join(shirtDir, f'spr{index}.png')
        commands.append(f'move 0.png {dstShirt}')
        commands.append(f'move 1.png {dstBackground}')

    scanDirForGraphicFiles(inputPath, handleGraphicFile)

    return commands

def scanDirForGraphicFiles(dirPath, handler):
    for entry in os.scandir(dirPath):
        if entry.is_file():
            name, ext = os.path.splitext(entry.name)
            if ext.lower() in ('.tif', '.tiff', '.psd'):
                index = extractIndex(name)
                handler(entry.path, index)

def extractIndex(filename):
    return ''.join(filter(str.isdigit, filename))

def formPackCommand(inputFiles, outputPath):
    commands = []

    # tiff
    cmdLine = 'magick convert -colorspace RGB -type TrueColorAlpha '
    cmdLine += ' '.join(inputFiles) + ' '
    cmdLine += outputPath + '.tiff'
    commands.append(cmdLine)

    # PSD
    tempLayer = 'flatenedlayer.png'
    cmdLine = 'magick convert '
    cmdLine += ' '.join(inputFiles)
    cmdLine += ' -background none -flatten ' + tempLayer
    commands.append(cmdLine)
    cmdLine = 'magick convert ' + tempLayer + ' '
    cmdLine += ' '.join(inputFiles) + ' '
    cmdLine += outputPath + '.psd'
    commands.append(cmdLine)
    commands.append(f'del {tempLayer}')

    return commands

def formUnpackCommand(filePath):
    return f'magick convert -delete 1,1 -dispose Background -layers coalesce {filePath} %d.png'

lastPercentage = -1
def showProgress(current, total):
    global lastPercentage

    percentage = 0
    if current:
        percentage = 100 * current // total

    if percentage != lastPercentage:
        print(f'\r{percentage}% done', end='', flush=True)

    lastPercentage = percentage

def runCommands(commands):
    for i, command in enumerate(commands):
        p = subprocess.run(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if p.returncode == 0:
            showProgress(i, len(commands) - 1)
        else:
            if p.stdout:
                print(p.stdout)
            return p.returncode

    return 0

def mkdir(path):
    try:
        os.mkdir(path)
    except FileExistsError:
        pass

if __name__ == '__main__':
    main()
