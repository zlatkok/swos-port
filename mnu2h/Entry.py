"""Represents menu entries or items"""

import Constants

class Entry:
    def __init__(self, ordinal=-1, name=''):
        assert isinstance(ordinal, int)
        assert isinstance(name, str)

        self.name = name
        self.ordinal = ordinal

        for property in Constants.kNextEntryProperties:
            setattr(self, property + 'Token', None)

        self.x = 0
        self.y = 0
        self.width = 0
        self.height = 0

        self.menuX = 0
        self.menuY = 0

        self.text = None
        self.number = None
        self.sprite = None
        self.stringTable = None
        self.multilineText = None
        self.boolOption = None
        self.customDrawBackground = None
        self.customDrawForeground = None
        self.menuSpecificSprite = None

        self.textFlags = 0

        self.color = 0
        self.invisible = 0
        self.disabled = 0
        self.leftEntry = -1
        self.rightEntry = -1
        self.upEntry = -1
        self.downEntry = -1
        self.directionLeft = -1
        self.directionRight = -1
        self.directionUp = -1
        self.directionDown = -1
        self.skipLeft = -1
        self.skipRight = -1
        self.skipUp = -1
        self.skipDown = -1
        self.controlMask = 0
        self.onSelect = 0
        self.beforeDraw = 0
        self.onReturn = 0

    def isTemplate(self):
        return self.ordinal >= Constants.kMaxEntries

class ResetTemplateEntry(Entry):
    pass
