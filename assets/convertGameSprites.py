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

# actual number of images saved
g_numImages = 0

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
    image = image.resize((12 * image.width, 12 * image.height), Image.LANCZOS, None, 3.0)

    name = f'spr{index:04}.png'
    path = kDestDir / dir1 / dir2 / name
    image.save(path)

    if metadata:
        dstPath = str(path).replace('png', 'txt')
        with open(dstPath, 'w') as f:
            f.writelines(metadata)

    global g_numImages
    g_numImages += 1

def handlePlayersAndGoalkeepers(index, image, pal, metadata):
    isGoalkeeper = kGoalkeepersStart <= index <= kGoalkeepersEnd

    dir1 = 'goalkeeper' if isGoalkeeper else 'player'
    index -= kGoalkeepersStart if isGoalkeeper else kPlayersStart   # start from 0

    layerImages = [newEmptyImage(image) for i in range(len(Layer) + 1)]

    def pixelHandler(x, y, pal, pixel):
        backLayerIndex = len(Layer)
        if pixel == kZero:
            layerImage[backLayerIndex].putpixel((x, y), (0, 0, 36, 255))
        else:
            layer = kPixelToLayer.get(pixel, backLayerIndex)
            putPixel(layerImages[layer], x, y, pal, pixel)

    processImage(image, pal, pixelHandler)

    for layer in Layer:
        if layer != Layer.kShirt or not isGoalkeeper:
            saveImage(layerImages[layer.value], index, grayscale=layer != Layer.kShirt, dir1=dir1, dir2=layerDirName(layer))

    saveImage(layerImages[-1], index, dir1=dir1, dir2='background', metadata=metadata)

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
    kNumImages: Final = 1280
    if g_numImages != kNumImages:
        sys.exit(f'Invalid number of images processed: {g_numImages}, required: {kNumImages}')

def mkdir(path):
    (Path(kDestDir) / path).mkdir(parents=True, exist_ok=True)

def ensureDirectories():
    for root in ('goalkeeper', 'player'):
        for dir in ('background', 'socks', 'hair', 'skin', 'shorts'):
            mkdir(Path(root) / dir)

    for root, dir in (('player', 'shirt'), ('bench', 'background'), ('bench', 'shirt')):
        mkdir(Path(root) / dir)

def main():
    ensureDirectories()

    kSpriteHandlers = (
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
