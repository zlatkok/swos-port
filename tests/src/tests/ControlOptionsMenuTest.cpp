#include "ControlOptionsMenuTest.h"
#include "controlOptionsMenu.h"
#include "unitTest.h"
#include "sdlProcs.h"

static ControlOptionsMenuTest t;

void ControlOptionsMenuTest::init()
{
    takeOverInput();
}

const char *ControlOptionsMenuTest::name() const
{
    return "control-options-menu";
}

const char *ControlOptionsMenuTest::displayName() const
{
    return "control options menu";
}

auto ControlOptionsMenuTest::getCases() -> CaseList
{
    return {
        { "test entry colors", "controls-entry-colors", bind(&ControlOptionsMenuTest::setupEntryColorsTest),
            bind(&ControlOptionsMenuTest::testEntryColors), },
    };
}

void ControlOptionsMenuTest::setupEntryColorsTest()
{
    showControlOptionsMenu();
}

void ControlOptionsMenuTest::testEntryColors()
{
    ;
}
