#include "mainMenu.h"
#include "drawMenu.h"
#include "replaysMenu.h"
#include "render.h"
#include "timer.h"
#include "music.h"
#include "main.mnu.h"

static void initMainMenu();
static void initMainMenuGlobals();

void showMainMenu()
{
    logInfo("Initializing main menu");
    initMainMenu();

    logInfo("Showing main menu");
    showMenu(mainMenu);
}

bool mainMenuActive()
{
    return getCurrentPackedMenu() == &mainMenu;
}

void activateExitGameButton()
{
    showQuitMenu();
}

static void initMainMenu()
{
    initMusic();
    initMainMenuGlobals();

    // speed up starting up in debug mode
#ifndef DEBUG
    enqueueMenuFadeIn();
#endif
}

void initMainMenuGlobals()
{
    ZeroOutStars();
    swos.twoPlayers = -1;
    swos.flipOnOff = 1;
    swos.inFriendlyMenu = 0;
    swos.isNationalTeam = 0;

    D0 = kGameTypeNoGame;
    InitCareerVariables();

    swos.g_exitMenu = 0;
    swos.fireResetFlag = 0;
    swos.dseg_10E848 = 0;
    swos.dseg_10E846 = 0;
    swos.dseg_11F41C = -1;
    swos.coachOrPlayer = 1;
    SetDefaultNameAndSurname();
    swos.plNationality = kEng;       // English is the default
    swos.competitionOver = 0;
    InitHighestScorersTable();
    swos.teamsLoaded = 0;
    swos.poolplyrLoaded = 0;
    swos.importTacticsFilename[0] = 0;
    swos.chooseTacticsTeamPtr = nullptr;
    InitUserTactics();
}

static void mainMenuOnInit()
{
    markFrameStartTime();
    swos.currentTeamNumber = -1;
    CommonMenuExit();
}

//
// quit menu
//

constexpr char kQuitToOS[] = "QUIT TO "
#ifdef _WIN32
"WINDOWS";
#elif defined(__ANDROID__)
"ANDROID";
#else
# error "Define OS name"
#endif

#include "quit.mnu.h"

static void showQuitMenu()
{
    menuFadeOut();
    enqueueMenuFadeIn();
    showMenu(quitMenu);
    CommonMenuExit();
}

static void quitGame()
{
    menuFadeOut();
    std::exit(EXIT_SUCCESS);
}

static void returnToGame()
{
    menuFadeOut();
    enqueueMenuFadeIn();
    SetExitMenuFlag();
}
