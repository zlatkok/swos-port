#pragma once

#include "BaseTest.h"

class SelectFilesMenuTest : public BaseTest
{
public:
    void init() override;
    void finish() override;
    void defaultCaseInit() override {};
    const char *name() const override;
    const char *displayName() const override;
    CaseList getCases() override;

private:
    void setupLoadCompetitionTest();
    void testLoadCompetition();
    void testStringElision();
    void testLayout();
    void setupEntryTransitionsTest();
    void testEntryTransitions();
    void testAbortButton();
    void testSelectingFiles();
    void testScrolling();
    void testAbortSave();
    void testSaveButtonShowHide();
    void testLongExtensions();
    void testSaveCompetition();

    void testArrowBackground(const MenuEntry *arrowEntry);
    void checkArrowsOverlap();

    static constexpr int kArrowWidth = 6;
    static constexpr int kArrowHeight = 12;
    char upArrowBuffer[kArrowWidth * kArrowHeight];
    char downArrowBuffer[kArrowWidth * kArrowHeight];
};
