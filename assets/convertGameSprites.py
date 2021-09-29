'''Split player and goalkeeper sprites into separate layers which can be customized and recombined.
Convert colors 10 and 11 to zero for result digits.
As for other sprites, simply convert them to PNG (with transparent background).'''

import os
import sys
from typing import Final
from PIL import Image, ImageEnhance
from pathlib import Path
from enum import Enum, unique

kSourceDir: Final = Path(__file__).parent / 'sprites' / 'src'
kDestDir: Final = Path(__file__).parent / 'sprites' / 'game'

kNumImages: Final = 1289

# color indices to swap
kSkin1: Final = 4
kSkin2: Final = 5
kSkin3: Final = 6
kZero: Final = 7
kHair1: Final = 12
kHair2: Final = 9
kHair3: Final = 13
kShirt: Final = 10
kStripes: Final = 11
kShorts: Final = 14
kSocks: Final  = 15

kStadiumSpritesStart: Final = 210
kStadiumSpritesEnd: Final = 213
kScoreDigitsStart: Final = 287
kScoreDigitsEnd: Final = 306
kPlayersStart: Final = 341
kPlayersEnd: Final = 441
kSingleSleevePlayersStart: Final = 442
kSingleSleevePlayersEnd: Final = 542
kHorizontalStripePlayersStart: Final = 644
kHorizontalStripePlayersEnd: Final = 744
kGoalkeepersStart: Final = 947
kGoalkeepersEnd: Final = 1004
kBenchStart: Final = 1310
kBenchEnd: Final = 1321
kOtherSpritesStart: Final = 1179
kOtherSpritesEnd: Final = 1308
kResultDashSprite: Final = 309
kTimeDigitsStart: Final = 328
kTimeDigitsEnd: Final = 340

@unique
class Layer(Enum):
    kSkin = 0
    kHair = 1
    kShirt = 2
    kShorts = 3
    kSocks = 4

def layerDirName(layer):
    assert layer.name.startswith('k')
    return layer.name[1:].lower()

kPixelToLayer: Final = {
    kSkin1: Layer.kSkin.value, kSkin2: Layer.kSkin.value, kSkin3: Layer.kSkin.value,
    kHair1: Layer.kHair.value, kHair2: Layer.kHair.value, kHair3: Layer.kHair.value,
    kShirt: Layer.kShirt.value, kStripes: Layer.kShirt.value,
    kShorts: Layer.kShorts.value, kSocks: Layer.kSocks.value,
}

g_numImages = 0     # actual number of images saved
g_blocky = False

def newEmptyImage(image):
    return Image.new('RGBA', image.size, (0, 0, 0, 0))

def processImage(image, pal, pixelHandler):
    for x in range(image.width):
        for y in range(image.height):
            pixel = image.getpixel((x, y))
            if pixel < 16:
                pixelHandler(x, y, pal, pixel)

def getRgb(pixel, pal):
    return tuple(pal[3 * pixel : 3 * pixel + 3] + [255])

def putPixel(image, x, y, pal, pixel):
    rgb = getRgb(pixel, pal)
    image.putpixel((x, y), rgb)

def saveImage(image, index, grayscale=False, dir1='', dir2='', metadata=None):
    if grayscale:
        image = ImageEnhance.Color(image).enhance(0)

    global g_blocky
    filter =  Image.NEAREST if g_blocky else Image.LANCZOS
    reducingGap = None if g_blocky else 3.0

    image = image.resize((12 * image.width, 12 * image.height), filter, None, reducingGap)

    name = f'spr{index:04}.png'
    path = getDestDir() / dir1 / dir2 / name
    image.save(path)

    if metadata:
        dstPath = str(path).replace('png', 'txt')
        with open(dstPath, 'w') as f:
            f.writelines(metadata)

    global g_numImages
    g_numImages += 1

def handleStadiumSprites(index, image, pal, metadata):
    isGoalkeeper = index == kStadiumSpritesEnd
    isFirst = index == kStadiumSpritesStart
    index = 0 if isGoalkeeper else index - kStadiumSpritesStart
    layerImages = handleLayeredSprite('stadium', isGoalkeeper, index, image, pal, metadata, doSave=False)
    if isFirst:
        saveLayers(layerImages, 0, isGoalkeeper=False, dir1='stadium', metadata=metadata, separateIntoDirs=False)
    elif isGoalkeeper:
        saveImage(layerImages[-1], len(Layer) + 3, dir1='stadium', metadata=metadata)
    else:
        saveImage(layerImages[Layer.kShirt.value], len(Layer) + index, dir1='stadium', metadata=metadata)

def handlePlayersAndGoalkeepers(index, image, pal, metadata):
    isGoalkeeper = kGoalkeepersStart <= index <= kGoalkeepersEnd

    index -= kGoalkeepersStart if isGoalkeeper else kPlayersStart   # start from 0
    handleLayeredSprite('', isGoalkeeper, index, image, pal, metadata, True)

def saveLayers(layerImages, index, isGoalkeeper, dir1, metadata, separateIntoDirs):
    for layer in Layer:
        if layer != Layer.kShirt or not isGoalkeeper:
            dir2 = layerDirName(layer) if separateIntoDirs else ''
            saveImage(layerImages[layer.value], index, grayscale=layer != Layer.kShirt, dir1=dir1, dir2=dir2)
            if not separateIntoDirs:
                index += 1

    dir2 = 'background' if separateIntoDirs else ''
    saveImage(layerImages[-1], index, dir1=dir1, dir2=dir2, metadata=metadata)

def handleLayeredSprite(dirBase, isGoalkeeper, index, image, pal, metadata, doSave):
    dir1 = 'goalkeeper' if isGoalkeeper else 'player'
    dir1 = os.path.join(dirBase, dir1)

    layerImages = [newEmptyImage(image) for i in range(len(Layer) + 1)]

    def pixelHandler(x, y, pal, pixel):
        backLayerIndex = len(Layer)
        if pixel == kZero:
            layerImages[backLayerIndex].putpixel((x, y), (0, 0, 36, 255))
        else:
            layer = kPixelToLayer.get(pixel, backLayerIndex)
            putPixel(layerImages[layer], x, y, pal, pixel)

    processImage(image, pal, pixelHandler)

    if doSave:
        saveLayers(layerImages, index, isGoalkeeper, dir1, metadata, separateIntoDirs=True)
    else:
        return layerImages

def handleHorizontalStripeAndColoredSleevePlayers(index, image, pal, metadata):
    outImage = newEmptyImage(image)

    def pixelHandler(x, y, pal, pixel):
        if pixel == kZero:
            outImage.putpixel((x, y), (0, 0, 36, 255))
        elif pixel in (kShirt, kStripes):
            putPixel(outImage, x, y, pal, pixel)

    processImage(image, pal, pixelHandler)

    numPlayerSprites = kPlayersEnd - kPlayersStart + 1
    if index >= kHorizontalStripePlayersStart:
        index -= kHorizontalStripePlayersStart - numPlayerSprites
    else:
        assert index >= kSingleSleevePlayersStart
        index -= kSingleSleevePlayersStart - 2 * numPlayerSprites

    saveImage(outImage, index, dir1='player', dir2='shirt')

def handleBenchPlayers(index, image, pal, metadata):
    index -= kBenchStart
    backImage = newEmptyImage(image)
    shirtImage = newEmptyImage(image)

    def pixelHandler(x, y, pal, pixel):
        image = shirtImage if pixel in (kShirt, kStripes) else backImage
        putPixel(image, x, y, pal, pixel)

    processImage(image, pal, pixelHandler)
    saveImage(backImage, index, dir1='bench', dir2='background', metadata=metadata)
    saveImage(shirtImage, index, dir1='bench', dir2='shirt')

def handleOtherSprites(index, image, pal, metadata):
    outImage = newEmptyImage(image)

    def pixelHandler(x, y, pal, pixel):
        putPixel(outImage, x, y, pal, pixel)

    processImage(image, pal, pixelHandler)
    saveImage(outImage, index, metadata=metadata)

def handleScoreDigits(index, image, pal, metadata):
    outImage = newEmptyImage(image)

    def pixelHandler(x, y, pal, pixel):
        if 10 <= pixel <= 11:
            pixel = 2
        putPixel(outImage, x, y, pal, pixel)

    processImage(image, pal, pixelHandler)
    saveImage(outImage, index, metadata=metadata)

def verifyNumImagesProcessed():
    if g_numImages != kNumImages:
        sys.exit(f'Invalid number of images processed: {g_numImages}, required: {kNumImages}')

def getDestDir():
    global g_blocky
    base = 'blocky' if g_blocky else ''
    return Path(kDestDir) / base

def mkdir(path):
    (getDestDir() / path).mkdir(parents=True, exist_ok=True)

def ensureDirectories():
    for root in ('goalkeeper', 'player'):
        for dir in ('background', 'socks', 'hair', 'skin', 'shorts'):
            mkdir(Path(root) / dir)

    for root, dir in (('player', 'shirt'), ('bench', 'background'), ('bench', 'shirt')):
        mkdir(Path(root) / dir)

    mkdir(Path('stadium'))

def parseCommandLine():
    for arg in sys.argv[1:]:
        if arg.lower().replace('-', '') == 'blocky':
            global g_blocky
            g_blocky = True

def main():
    parseCommandLine()
    ensureDirectories()

    kSpriteHandlers = (
        ((kStadiumSpritesStart, kStadiumSpritesEnd), handleStadiumSprites),
        ((kPlayersStart, kPlayersEnd), handlePlayersAndGoalkeepers),
        ((kScoreDigitsStart, kScoreDigitsEnd), handleScoreDigits),
        ((kResultDashSprite, kResultDashSprite), handleScoreDigits),
        ((kGoalkeepersStart, kGoalkeepersEnd), handlePlayersAndGoalkeepers),
        ((kBenchStart, kBenchEnd), handleBenchPlayers),
        ((kOtherSpritesStart, kOtherSpritesEnd), handleOtherSprites),
        ((kTimeDigitsStart, kTimeDigitsEnd), handleOtherSprites),
        ((kHorizontalStripePlayersStart, kHorizontalStripePlayersEnd), handleHorizontalStripeAndColoredSleevePlayers),
        ((kSingleSleevePlayersStart, kSingleSleevePlayersEnd), handleHorizontalStripeAndColoredSleevePlayers),
    )

    for p in Path(kSourceDir).glob('*.bmp'):
        index = ''.join(filter(str.isdigit, p.name))
        if not index:
            continue

        index = int(index)

        for handler in kSpriteHandlers:
            lo, hi = handler[0]
            if lo <= index <= hi:
                with Image.open(str(p)) as image:
                    try:
                        with open(Path(kSourceDir) / f'spr{index:04}.txt', 'r') as f:
                            metadata = f.readlines()
                        metadata = map(lambda l: f'{int(l) * 12}\n', metadata)
                    except FileNotFoundError:
                        metadata = []

                    pal = image.getpalette()
                    handler[1](index, image, pal, metadata)
                break

    verifyNumImagesProcessed()

if __name__ == '__main__':
    main()
