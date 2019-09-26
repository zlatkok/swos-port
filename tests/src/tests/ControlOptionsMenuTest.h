#pragma once

#include "BaseTest.h"

class ControlOptionsMenuTest : public BaseTest
{
    void init() override {};
    void finish() override {};
    void defaultCaseInit() override {};
    const char *name() const override;
    const char *displayName() const override;
    CaseList getCases() override;

private:
    void setupEntryColorsTest();
    void testEntryColors();
};
