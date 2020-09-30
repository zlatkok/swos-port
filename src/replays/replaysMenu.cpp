#include "replaysMenu.h"
#include "replays.h"
#include "ReplayData.h"
#include "menuMouse.h"
#include "selectFilesMenu.h"
#include "replays.mnu.h"

using EntryList = std::array<int, 2>;
static bool m_initialized;

void showReplaysMenu()
{
    showMenu(replaysMenu);
    CommonMenuExit();
}

static void updateButtonState(bool enabled, const EntryList& entries)
{
    int color = enabled ? kGreen : kGray;

    for (auto entryIndex : entries) {
        auto entry = getMenuEntry(entryIndex);
        entry->setEnabled(enabled);
        entry->setBackgroundColor(color);
    }
}

static void updateReplaysAndHighlightsState()
{
    using namespace ReplaysMenu;

    bool highlightsLoaded = highlightsValid() && swos.hilNumGoals > 0;
    updateButtonState(highlightsLoaded, { viewHighlights, saveHighlights });
    updateButtonState(gotReplay(), { viewReplays, saveReplays });
}

static void selectHighlightToLoad()
{
    auto files = findFiles(".HIL");
    auto menuTitle = "LOAD HIGHLIGHTS";
    auto selectedFilename = showSelectFilesMenu(menuTitle, files);

    if (!selectedFilename.empty()) {
        if (loadHighlightsFile(selectedFilename.c_str()))
            highlightEntry(ReplaysMenu::viewHighlights);
        else
            showError(swos.aNotAHighlights);
    }

    updateReplaysAndHighlightsState();
}

static bool saveHighlightsFile()
{
    if (swos.hilFilename[0]) {
        auto fileSize = swos.hilNumGoals * kSingleHighlightBufferSize + kHilHeaderSize;
        return saveFile(swos.hilFilename, swos.hilFileBuffer, fileSize);
    }

    return false;
}

static void selectHighlightToSave()
{
    auto files = findFiles(".HIL");
    auto menuTitle = "SAVE HIGHLIGHTS";

    showSelectFilesMenu(menuTitle, files, ".HIL", swos.hilFilename);
    if (swos.hilFilename[0] && !saveHighlightsFile())
        showError("ERROR SAVING HIGHLIGHTS FILE");
}

static void selectReplayToSave()
{
    auto files = findFiles(".rpl", ReplayData::kReplaysDir);
    auto menuTitle = "SAVE REPLAYS";

    showSelectFilesMenu(menuTitle, files, ".RPL", swos.hilFilename);
    if (swos.hilFilename[0] && !saveReplayFile(swos.hilFilename))
        showError("ERROR SAVING REPLAY FILE");
}

static void selectReplayToLoad()
{
    auto files = findFiles(".rpl", ReplayData::kReplaysDir);
    auto menuTitle = "LOAD REPLAY";
    auto selectedFilename = showSelectFilesMenu(menuTitle, files);

    if (!selectedFilename.empty()) {
        if (loadReplayFile(selectedFilename.c_str()))
            highlightEntry(ReplaysMenu::viewReplays);
        else
            showError("LOADING REPLAY FILE FAILED");
    }

    updateReplaysAndHighlightsState();
}

static void playReplay()
{
    startReplay();
    updateMenuButtonsState();
}

static void initReplaysMenu()
{
    updateMenuButtonsState();
    m_initialized = true;
}

static void updateMenuButtonsState()
{
    using namespace ReplaysMenu;

    updateReplaysAndHighlightsState();
    determineReachableEntries();

    if (!m_initialized && getMenuEntry(viewHighlights)->disabled)
        highlightEntry(getMenuEntry(viewReplays)->disabled ? loadHighlights : viewReplays);
}
