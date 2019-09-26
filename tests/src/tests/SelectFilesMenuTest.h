#pragma once

#include "BaseTest.h"

class SelectFilesMenuTest : public BaseTest
{
    void init() override;
    void finish() override;
    void defaultCaseInit() override;
    const char *name() const override;
    const char *displayName() const override;
    CaseList getCases() override;

private:
    void setupLoadingCompetitionTest();
    void testLoadingCompetition();
    void testStringElision();
};
