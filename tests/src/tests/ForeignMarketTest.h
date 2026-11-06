#pragma once

#include "BaseTest.h"

class ForeignMarketTest : public BaseTest
{
public:
    void init() override {}
    void finish() override {}
    void defaultCaseInit() override {}
    const char *name() const override { return "foreign-market"; }
    const char *displayName() const override { return "foreign market player lookup test"; }
    CaseList getCases() override;

private:
    void testForeignMarketPlayerLookup();
};
