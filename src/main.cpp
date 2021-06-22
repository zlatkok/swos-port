#include "log.h"
#include "init.h"
#include "options.h"
#include "render.h"
#include "audio.h"
#include "file.h"
#include "crash.h"
#include "joypads.h"

static void turnOnDebugHeap()
{
#ifndef NDEBUG
# ifdef _WIN32
    auto flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    _CrtSetDbgFlag(flags | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
# endif
#endif
}

static void setSdlHints()
{
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
#endif
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
}

int main(int argc, char **argv)
{
    turnOnDebugHeap();

    auto commandLineWarnings = parseCommandLine(argc, argv);

    atexit(finishLog);          // dispose log last
    initLog();

    logInfo("SWOS port compiled at %s, %s", __DATE__, __TIME__);

    for (const auto& logItem : commandLineWarnings)
        log(logItem.category, "%s", logItem.text.c_str());

    installCrashHandler();
    loadOptions();
    setSdlHints();

    auto flags = IMG_INIT_JPG | IMG_INIT_PNG;
    if (IMG_Init(flags) != flags)
        logWarn("Failed to properly initialize SDL Image");

    initRendering();
    normalizeOptions();
    initJoypads();

    atexit(finishRendering);
    atexit(saveOptions);        // must be set after finishRendering
    atexit(finishAudio);
    atexit(IMG_Quit);

    SDL_StopTextInput();

    startMainMenuLoop();

    return EXIT_SUCCESS;
}
