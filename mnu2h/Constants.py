"""Place for all the shared constants"""

kInitialEntry = 'initialEntry'
kOnInit = 'onInit'
kOnReturn = 'onReturn'
kOnDraw = 'onDraw'
kOnDestroy = 'onDestroy'

kMaxEntries = 30_000    # just a random big number to avoid clashes with non-templates
kPreprocReservedWords = ('include', 'textWidth', 'textHeight', 'repeat', 'endRepeat', 'join', 'print')
kPreprocValueDirectives = ( 'eval', '{', 'textWidth', 'textHeight' )
kMenuFunctions = (kOnInit, kOnReturn, kOnDraw, kOnDestroy)

kMenuDefaults = (   # expansion of these is delayed, so that they can have different values for different entries
    'x', 'y', 'width', 'height', 'color', 'textFlags', 'leftEntry', 'rightEntry', 'upEntry', 'downEntry',
)
kDefaultPropertyAliases = {
    'topEntry': 'upEntry', 'bottomEntry': 'downEntry'
}

kMenuProperties = kMenuFunctions + (kInitialEntry, 'x', 'y')

kNextEntryProperties = ('leftEntry', 'rightEntry', 'upEntry', 'downEntry', 'skipLeft', 'skipRight', 'skipUp', 'skipDown')
kEntryTypeProperties = ('text', 'stringTable', 'multilineText', 'boolOption', 'number',
    'sprite', 'menuSpecificSprite', 'customDrawForeground')
kEntryFunctions = ('onSelect', 'beforeDraw', 'onReturn', 'customDrawBackground', 'customDrawForeground')
kImmutableEntryProperties = ('name', 'ordinal')
kPreviousEntryFields = ('ordinal', 'x', 'y', 'width', 'height', 'color', 'textFlags')

kConstants = {
    # backgrounds
    'kNoBackground': 0, 'kNoFrame': 0, 'kGray': 7, 'kDarkBlue': 3, 'kLightBrownWithOrangeFrame': 4,
    'kOrangeWithOrangeFrame': 5, 'kLightBrownWithYellowFrame': 6, 'kRed': 10, 'kPurple': 11,
    'kLightBrownWithRedFrame': 12, 'kLightBlue': 13, 'kGreen': 14,

    # inner frames
    'kGrayFrame': 0x10, 'kWhiteFrame': 0x20, 'kBlackFrame': 0x30, 'kBrownFrame': 0x40,
    'kLightBrownFrame': 0x50, 'kOrangeFrame': 0x60, 'kDarkGrayFrame': 0x70, 'kNearBlackFrame': 0x80,
    'kVeryDarkGreenFrame': 0x90, 'kRedFrame': 0xa0, 'kBlueFrame': 0xb0, 'kPurpleFrame': 0xc0,
    'kSoftBlueFrame': 0xd0, 'kGreenFrame': 0xe0, 'kYellowFrame': 0xf0,

    # text colors, practically the same as inner frames (low nibble, and 0 is different)
    'kDarkGrayText': 1, 'kWhiteText': 2, 'kBlackText': 3, 'kBrownText': 4,
    'kLightBrownText': 5, 'kOrangeText': 6, 'kGrayText': 7, 'kNearBlackText': 8,
    'kVeryDarkGreenText': 9, 'kRedText': 10, 'kBlueText': 11, 'kPurpleText': 12,
    'kSoftBlueText': 13, 'kGreenText': 14, 'kYellowText': 15,

    # text flags
    'kCenterAligned': 0,
    'kLeftAligned': 1 << 15, 'kRightAligned': 1 << 14, 'kShowText': 1 << 9, 'kBlinkText': 1 << 13,
    'kBigText': 1 << 4, 'kBigFont': 1 << 4,

    # controls mask values
    'kControlFire': 0x01, 'kControlShortFire': 0x20,
    'kControlLeft': 0x02, 'kControlRight': 0x04, 'kControlUp': 0x08, 'kControlDown': 0x10,

    # entry directions
    'kNoEntry': -1, 'kLeft': 0, 'kRight': 1, 'kUp': 2, 'kDown': 3,

    # nullptr for string, must be filled later (at runtime)
    'kNullText': 0, 'kNull': 0,
}

# font character width tables used for calculating string widths
kSmallFontWidths = (
    0, 2, 0, 0, 0, 6, 0, 2, 3, 3, 6, 6, 2, 4, 2, 6, 6, 2, 6, 6, 6, 6, 6, 6, 6, 6, 2, 3,
    0, 0, 0, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6, 2, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 0, 6, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,
)

kBigFontWidths = (
    0, 3, 0, 0, 0, 8, 0, 2, 4, 4, 8, 8, 3, 5, 3, 8, 8, 5, 8, 8, 8, 8, 8, 8, 8, 8, 3, 4,
    0, 0, 0, 8, 0, 8, 8, 8, 8, 8, 8, 8, 8, 5, 8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 7, 8, 8, 8,
    7, 7, 8, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8,
)

kSmallFontHeight = 6
kBigFontHeight = 8
