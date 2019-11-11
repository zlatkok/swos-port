#pragma once

#include "BaseTest.h"

class VideoOptionsMenuTest : public BaseTest
{
    void init() override;
    void finish() override;
    void defaultCaseInit() override;
    const char *name() const override;
    const char *displayName() const override;
    CaseList getCases() override;

private:
    using DisplayModeList = std::vector<SDL_DisplayMode>;

    void setupWindowSizeTest();
    void testWindowSize();
    void setupVariousResolutionLists();
    void testVariousResolutionLists();
    void finalizeVariousResolutionsList();
    void setupModeSwitchingTest();
    void testModeSwitching();

    void setupResolutionListScrollingTest();
    void testResolutionListScrolling();
    enum ScrollMethod { kArrowClick, kArrowMouseWheel, kResolutionListMouseWheel, kNumScrollMethods };
    void scrollAndVerifyTwoStepsForwardOneStepBack(int linesToScroll, ScrollMethod scrollMethod,
        int scrollOffset, int oldScrollOffset, const DisplayModeList& displayModes);
    void scrollResolutionListOneLine(int direction, ScrollMethod method, const DisplayModeList& displayModes);
    void verifyResolutionListStrings(const DisplayModeList& displayModes, int scrollOffset = 0);

    void setupCustomWindowSizeTest();
    void testCustomWindowSize();
    void testExitButton();
    void setupResolutionSwitchFailureTest();
    void testResolutionSwitchFailure();
    void testTryingToSetCurrentResolution();
    void testResolutionListRebuildWhenChangingScreen();

    void setFakeDisplayModesForced(const DisplayModeList& displayModes);
    void verifyYellowPleaseWaitTextPresence();
    void setUpMode(size_t mode);
    void setUpWindowedMode();
    void setUpBorderlessMaximizedMode();
    void setUpFullScreenMode();
    void verifyMode(size_t mode);
    void verifyWindowedMode();
    void assertWindowedModeItemsSelected(bool enabled);
    void verifyBorderlessMaximizedMode();
    void verifyFullScreenMode();
    void verifyResolutionListColors(const char *current = nullptr);
    void switchToMode(size_t mode);
    const char *getResolutionString(int width, int height);

    DisplayModeList m_displayModeList;

    std::vector<std::pair<DisplayModeList, ScrollMethod>> m_testScrollingData;
    static constexpr int kNumDisplayListsForScrollTesting = 3;

    bool m_verifyYellowText = false;
};
