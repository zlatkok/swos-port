#include "ForeignMarketTest.h"
#include "unitTest.h"
#include "swossym.h"

static ForeignMarketTest t;

extern void cseg_5A666();
extern void LoadPoolplyrDat();

auto ForeignMarketTest::getCases() -> CaseList
{
    return {
        { "test foreign market player lookup", "foreign-market-lookup", {}, bind(&ForeignMarketTest::testForeignMarketPlayerLookup), 1, false },
    };
}

void ForeignMarketTest::testForeignMarketPlayerLookup()
{
    LoadPoolplyrDat();
    // Test player from crash report: team_id = 0x0732 (Green Cross, country 50, team 7), player_ord = 14
    D0 = 0x0732;
    D1 = 14;
    cseg_5A666();

    auto ptr = A0.asPtr();
    auto teamId = *reinterpret_cast<const uint16_t *>(ptr);
    auto playerOrd = *reinterpret_cast<const uint8_t *>(ptr + 2);
    const char *teamName = ptr + 3;

    assertEqual(teamId, static_cast<uint16_t>(0x0732));
    assertEqual(playerOrd, static_cast<uint8_t>(14));
    assertEqual(std::string(teamName), std::string("GREEN CROSS"));
}
