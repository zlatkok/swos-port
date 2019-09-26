"""Holds all the data necessary to render a menu."""

import Constants
from Entry import Entry
from Entry import ResetTemplateEntry

class Menu:
    def __init__(self):
        self.properties = {
            'defaultWidth': 0, 'defaultHeight': 0, 'defaultX': 0, 'defaultY': 0, 'defaultColor': 0,
            'defaultTextFlags': 0, 'x': 0, 'y': 0
        }
        self.propertyTokens = {}
        self.exportedVariables = {}
        self.entries = {}
        self.variables = {}
        self.templateEntry = Entry()
        self.templateActive = False
        self.name = ''

        self.lastEntry = None
        self.initialX = 0
        self.initialY = 0

        self.onInit = 0
        self.onReturn = 0
        self.onDraw = 0

        for field in Constants.kPreviousEntryFields:
            setattr(self, 'previous' + field.capitalize(), 0)

    def addEntry(self, name, isTemplate):
        entry, internalName = self.createNewEntry(name, isTemplate)

        self.entries[internalName] = entry

        if isTemplate:
            self.templateEntry = entry
            self.templateActive = True
        else:
            self.lastEntry = entry

        return entry

    def createNewEntry(self, name, isTemplate):
        if isTemplate:
            return self.createNewTemplateEntry()
        else:
            return self.createNewStandardEntry(name)

    def createNewTemplateEntry(self, isReset=False):
        entry = ResetTemplateEntry() if isReset else Entry()
        entry.ordinal = Constants.kTemplateEntryOrdinalStart + len(self.entries)
        internalName = f'{len(self.entries):02}'    # use a number since it can't be assigned to entry name by the user

        return entry, internalName

    def createNewStandardEntry(self, name):
        assert isinstance(name, str)

        entry = Entry()
        for property in ('x', 'y', 'width', 'height'):
            value = getattr(self.templateEntry, property)
            setattr(entry, property, value)

        internalName = name
        if not name:
            internalName = str(len(self.entries))

        entry.name = name
        entry.ordinal = self.getNextEntryOrdinal()

        return entry, internalName

    def numEntries(self):
        numEntries = 0
        for currentEntry in self.entries.values():
            if isinstance(currentEntry, Entry) and not currentEntry.isTemplate():
                numEntries += 1

        return numEntries

    def getLastEntryOrdinal(self):
        for currentEntry in reversed(list(self.entries.values())):
            if isinstance(currentEntry, Entry) and not currentEntry.isTemplate():
                return currentEntry.ordinal

        return -1

    def getNextEntryOrdinal(self):
        return self.getLastEntryOrdinal() + 1
