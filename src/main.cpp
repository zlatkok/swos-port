#include "log.h"
#include "options.h"
#include "render.h"
#include "audio.h"
#include "file.h"
#include "crash.h"
#include "joypads.h"

int main(int argc, char **argv)
{
#ifndef NDEBUG
    auto flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    _CrtSetDbgFlag(flags | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

    auto commandLineWarnings = parseCommandLine(argc, argv);

    atexit(finishLog);
    initLog();

    logInfo("SWOS port compiled at %s, %s", __DATE__, __TIME__);

    for (const auto& logItem : commandLineWarnings)
        log(logItem.category, "%s", logItem.text.c_str());

    installCrashHandler();

    atexit(saveOptions);
    loadOptions();

    atexit(finishRendering);
    initRendering();

    normalizeOptions();

    atexit(finishAudio);

    initJoypads();

    SWOS::SWOS();

    return EXIT_SUCCESS;
}
