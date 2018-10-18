#include "CrashRpt.h"
#include "log.h"
#include "file.h"

int CALLBACK crashCallback(CR_CRASH_CALLBACK_INFO *info)
{
    logError("We have crashed!");
    finishLog();

    char buf[64];
    buf[0] = '0';
    buf[1] = 'x';

    char regName[3];
    regName[2] = '\0';
    auto registers = reinterpret_cast<dword *>(&D0);

    for (uint8_t i = 0; i < &A6 - &D0 + 1; i++) {
        if (i < 8) {
            regName[0] = 'D';
            regName[1] = '0' + i;
        } else {
            regName[0] = 'A';
            regName[1] = '0' + i - 8;
        }

        auto value = registers[i];
        int ofs = 2;

        for (auto mask = 0xf0000000; mask > 0xf; mask >>= 4) {
            if (value & mask)
                break;
            buf[ofs++] = '0';
        }

        _itoa(registers[i], buf + ofs, 16);
        crAddProperty(regName, buf);
    }

    return CR_CB_DODEFAULT;
}

void installCrashHandler()
{
    CR_INSTALL_INFO info{};

    info.cb = sizeof(CR_INSTALL_INFO);
    info.pszAppName = "SWOS port";
    info.pszAppVersion = "1.0.0";
    info.pszEmailSubject = "[SWOS port v1.0.0] Crash Report";

    static char email[] = "lztaokk_rakasag^amlic_mo";
    auto len = sizeof(email) - 1;
    for (size_t i = 0; i < len; i++)
        if (email[i] == '_')
            email[i] = '.';
        else if (email[i] == '^')
            email[i] = '@';

    for (size_t i = 0; i < len - 1; i += 2)
        std::swap(email[i], email[i + 1]);

    info.pszEmailTo = email;

    auto crashDir = rootDir() + "CrashReports";
    info.pszErrorReportSaveDir = crashDir.c_str();
    info.dwFlags = CR_INST_ALL_POSSIBLE_HANDLERS | CR_INST_ALLOW_ATTACH_MORE_FILES |
        CR_INST_DONT_SEND_REPORT | CR_INST_STORE_ZIP_ARCHIVES;

    if (crInstall(&info)) {
        char buf[512];
        crGetLastErrorMsg(buf, sizeof(buf));
        logWarn("Crash reported failed to install, error message: %s", buf);
        return;
    }

    logInfo("Crash reporter installed OK");
    crSetCrashCallback(crashCallback, nullptr);
    crAddFile2(logPath().c_str(), nullptr, "Log file", CR_AF_MAKE_FILE_COPY | CR_AF_MISSING_FILE_OK);
    crAddFile2((rootDir() + "setup.dat").c_str(), nullptr, "Settings file", CR_AF_MAKE_FILE_COPY | CR_AF_MISSING_FILE_OK);
    crAddScreenshot2(CR_AS_PROCESS_WINDOWS | CR_AS_VIRTUAL_SCREEN, 0);
    crAddProperty("BuiltAt", __DATE__ ", " __TIME__);
}
