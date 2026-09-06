#pragma once

#include "BaseTest.h"

class SwosDataLayoutTest : public BaseTest
{
    void init() override {}
    void finish() override {}
    void defaultCaseInit() override {}
    const char *name() const override;
    const char *displayName() const override;
    CaseList getCases() override;

    void verifyChairmanScenesStringPool();
    void verifyCompetitionFileBuffer();
    void verifyCareerFileBuffer();
    void verifyFixedFileBuffers();
    void verifyFrameTableArea();
};
