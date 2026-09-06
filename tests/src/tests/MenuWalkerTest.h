#pragma once

#include "BaseTest.h"

class MenuWalkerTest : public BaseTest
{
    void init() override;
    void finish() override;
    void defaultCaseInit() override;
    const char *name() const override;
    const char *displayName() const override;
    CaseList getCases() override;

    void openMainMenu();
    void inventoryCurrentMenu();
    void testMainMenuRoundTrip();
    void openEditCustomTeamsMenu();
    void inventoryEditCustomTeamsMenu();
    void walkEditCustomTeamsEntry();
    void walkEditCustomTeamsImport();
    void walkEditCustomTeamsOk();
};
