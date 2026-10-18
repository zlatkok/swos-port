#include "ForeignMarketTest.h"
#include "unitTest.h"
#include "swossym.h"

static ForeignMarketTest t;

auto ForeignMarketTest::getCases() -> CaseList
{
    return {
        { "test foreign market player lookup", "foreign-market-lookup", {},
            bind(&ForeignMarketTest::testForeignMarketPlayerLookup), 1, false },
    };
}

void ForeignMarketTest::testForeignMarketPlayerLookup()
{
    memset(swos.poolplyrDatBuffer, 0, sizeof(swos.poolplyrDatBuffer));

    auto poolPlayers = reinterpret_cast<PoolPlayer *>(swos.poolplyrDatBuffer);
    auto& expected = poolPlayers[kNumPoolPlayers - 1];
    expected.teamId = 0x1234;
    expected.playerOrdinal = 15;

    D0 = expected.teamId;
    D1 = expected.playerOrdinal;
    SWOS::cseg_5A666();

    assertTrue(A0.as<PoolPlayer *>() == &expected);
    swos.poolplyrLoaded = false;
}
