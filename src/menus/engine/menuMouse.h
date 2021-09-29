#pragma once

struct MouseWheelEntryGroup {
    MouseWheelEntryGroup(int ordinal, int last, int scrollUpEntry, int scrollDownEntry)
        : first(ordinal), last(last), scrollUpEntry(scrollUpEntry), scrollDownEntry(scrollDownEntry) {}
    int first;
    int last;
    int scrollUpEntry;
    int scrollDownEntry;
};

using MouseWheelEntryGroupList = std::vector<MouseWheelEntryGroup>;

void initMenuMouse();
void resetMenuMouseData();
void menuMouseOnAboutToShowNewMenu();
void menuMouseOnOldMenuRestored();

void setMouseWheelEntries(const MouseWheelEntryGroupList& mouseWheelEntries);
void setGlobalWheelEntries(int upEntry = -1, int downEntry = -1);

void updateMouse();
std::tuple<bool, int, int> getClickCoordinates();
#ifdef __ANDROID__
void updateTouch(float x, float y, SDL_FingerID fingerId);
void fingerUp(SDL_FingerID fingerId);
#endif

void determineReachableEntries(bool force = false);
bool isEntryReachable(int index);
