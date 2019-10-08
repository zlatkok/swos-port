#pragma once

struct MouseWheelEntry {
    MouseWheelEntry(int ordinal, int scrollUpEntry, int scrollDownEntry)
        : ordinal(ordinal), scrollUpEntry(scrollUpEntry), scrollDownEntry(scrollDownEntry) {}
    int ordinal;
    int scrollUpEntry;
    int scrollDownEntry;
};

using MouseWheelEntryList = std::vector<MouseWheelEntry>;

void menuMouseOnAboutToShowNewMenu();
void menuMouseOnOldMenuRestored();

void setMouseWheelEntries(const MouseWheelEntryList& mouseWheelEntries);
void setGlobalWheelEntries(int upEntry = -1, int downEntry = -1);

void updateMouse();
void determineReachableEntries(const MenuBase *menu);
