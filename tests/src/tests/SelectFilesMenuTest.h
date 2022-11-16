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
    void setupSaveCompetitionTest();
    void testSaveCompetition();
    void testSaveCompetitionByClick();

    void testArrowBackground(const MenuEntry *arrowEntry);
    void checkArrowsOverlap();

    static constexpr char kCanadaDiy[] = "canada.diy";
    std::unique_ptr<const char[]> m_canadaDiyData;
    size_t m_canadaDiySize;
};
