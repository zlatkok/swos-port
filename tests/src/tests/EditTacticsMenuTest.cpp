#include "EditTacticsMenuTest.h"
#include "unitTest.h"
#include "menu.h"

enum MenuEntries {
    kHeader, kUserA, kUserB, kUserC, kUserD, kUserE, kUserF, kExit, kNumItems,
};

const std::vector<std::pair<Tactics, const char *>> kTacticsData = {
    { kTactic_541, "541" }, { kTactic_451, "451" }, { kTactic_532, "532" },
    { kTactic_352, "352" }, { kTactic_433, "433" }, { kTactic_424, "424" },
    { kTactic_343, "343" }, { kTactic_523, "523" },
    { kTacticSweep, "SWEEP" }, { kTacticAttack, "ATTACK" }, { kTacticDefend, "DEFEND" },
    { kTacticUserA, "USER_A" }, { kTacticUserB, "USER_B" }, { kTacticUserC, "USER_C" },
    { kTacticUserD, "USER_D" }, { kTacticUserE, "USER_E" }, { kTacticUserF, "USER_F" },
};

static EditTacticsMenuTest t;

const char *EditTacticsMenuTest::name() const
{
    return "edit-tactics-menu";
}

const char *EditTacticsMenuTest::displayName() const
{
    return "edit tactics menu";
}

auto EditTacticsMenuTest::getCases() -> CaseList
{
    return {
        { "test loading custom tactics", "load-custom-tactics", bind(&EditTacticsMenuTest::setupShowTacticsMenuTest),
            bind(&EditTacticsMenuTest::showTacticsMenu), kTacticsData.size() },
    };
}

void EditTacticsMenuTest::setupShowTacticsMenuTest()
{
    // simple test, but here to cover crash at menu start
    chosenTactics = kTacticsData[m_currentDataIndex].first;
    SAFE_INVOKE(EditTacticsMenu);
}

void EditTacticsMenuTest::showTacticsMenu()
{
    auto tacticName = kTacticsData[m_currentDataIndex].second;

    char expectedMenuName[32];
    snprintf(expectedMenuName, sizeof(expectedMenuName), "EDIT TACTICS (%s)", tacticName);

    auto actualMenuName = getMenuEntry(1)->u2.string;
    assertEqual(actualMenuName, expectedMenuName);
}
