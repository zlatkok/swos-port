#include "options.h"
#include "file.h"
#include "controls.h"
#include "render.h"
#include "audio.h"
#include "music.h"
#include "options.mnu.h"
#include <adlmidi.h>

enum SoundEnabledState { kUnspecified, kOn, kOff, } static m_soundState;
static bool m_noIntro;
static bool m_noReels;
static int m_bankNo;
enum Controls {
    kNoControls, kKeyboardOnly, kJoypadOnly, kJoypadAndKeyboard, kKeyboardAndJoypad, kTwoJoypads, kMaxControl,
};
static Controls m_controlInput = kNoControls;

static int16_t m_gameStyle;         // 0 = PC, 1 = Amiga

static constexpr char kIniFilename[] = "swos.ini";
static constexpr char kStandardSection[] = "standard-options";
static constexpr char kControlsSection[] = "controls";
static constexpr char kVideoSection[] = "video";
static constexpr char kAudioSection[] = "audio";

template <typename T> struct Option {
    const char *key;
    T *value;
    typename std::make_signed<T>::type min;
    typename std::make_signed<T>::type max;

    T clamp(long val) const {
        if (val < min || val > max)
            logInfo("Option value for %s out of range (%d), clamping to [%d, %d]", key, val, min, max);

        val = std::min<long>(max, val);
        val = std::max<long>(min, val);

        return static_cast<T>(val);
    }
};

static const std::array<Option<int16_t>, 9> kStandardOptions = {
    "gameLength",  &g_gameLength, 0, 3,
    "autoReplays",  &g_autoReplays, 0, 1,
    "menuMusic",  &g_menuMusic, 0, 1,
    "autoSaveHighlights",  &g_autoSaveHighlights, 0, 1,
    "allPlayerTeamsEqual", &g_allPlayerTeamsEqual, 0, 1,
    "pitchType", &g_pitchType, -2, 6,
    "chairmanScenes", &g_chairmanScenes, 0, 1,
    "showBigLetterS", &g_spinBigS, 0, 1,
    "gameStyle", &m_gameStyle, 0, 1,
};

static const std::array<Option<byte>, 5> kControlOptions = {
    "upScanCode", &g_upScanCode, -128, 127,
    "downScanCode", &g_downScanCode, -128, 127,
    "leftScanCode", &g_leftScanCode, -128, 127,
    "rightScanCode", &g_rightScanCode, -128, 127,
    "fireScanCode", &g_fireScanCode, -128, 127,
};

static const std::array<Option<int16_t>, 3> kAudioOptions = {
    "soundOff", &g_soundOff, 0, 1,
    "musicOff", &g_musicOff, 0, 1,
    "commentary", &g_commentary, 0, 1,
};

const char kMasterVolume[] = "masterVolume";
const char kMusicVolume[] = "musicVolume";

// video options
const char kWindowMode[] = "windowMode";
const char kWindowWidth[] = "windowWidth";
const char kWindowHeight[] = "windowHeight";
const char kWindowResizable[] = "windowResizable";
const char kFullScreenWidth[] = "fullScreenWidth";
const char kFullScreenHeight[] = "fullScreenHeight";

static void loadAudioOptions(const CSimpleIniA& ini);
static void saveAudioOptions(CSimpleIniA& ini);
static void loadVideoOptions(const CSimpleIniA& ini);
static void saveVideoOptions(CSimpleIniA& ini);

// controls
const char kInputControlsKey[] = "inputControls";
const char kDeadZoneJoypad1Section[] = "joypad1DeadZone";
const char kDeadZoneJoypad2Section[] = "joypad2DeadZone";
const char *kDeadZoneJoypadSections[] = { kDeadZoneJoypad1Section, kDeadZoneJoypad2Section };

const char kDeadZonePositiveX[] = "joypadDeadZoneXPositive";
const char kDeadZoneNegativeX[] = "joypadDeadZoneXNegative";
const char kDeadZoneNegativeY[] = "joypadDeadZoneYPositive";
const char kDeadZonePositiveY[] = "joypadDeadZoneYNegative";
const int kDefaultDeadZoneValue = 8000;

template<typename T, size_t N>
static void loadOptions(const CSimpleIniA& ini, const std::array<Option<T>, N>& options, const char *section)
{
    for (const auto& opt : options) {
        auto val = ini.GetLongValue(section, opt.key);
        *opt.value = opt.clamp(val);
    }
}

template<typename T, size_t N>
static void saveOptions(CSimpleIni& ini, const std::array<Option<T>, N>& options, const char *section)
{
    for (const auto& opt : options)
        ini.SetLongValue(section, opt.key, *opt.value);
}

void loadOptions()
{
    logInfo("Loading options");

    CSimpleIniA ini;
    auto path = pathInRootDir(kIniFilename);

    if (ini.LoadFile(path.c_str()) >= 0) {
        loadOptions(ini, kStandardOptions, kStandardSection);

        if (m_controlInput != kNoControls)
            g_inputControls = m_controlInput;
        else
            g_inputControls = static_cast<int16_t>(ini.GetLongValue(kControlsSection, kInputControlsKey));

        loadOptions(ini, kControlOptions, kControlsSection);

        typedef decltype(setJoypad1DeadZone) SetFunction;
        SetFunction *setFunctions[] = { setJoypad1DeadZone, setJoypad2DeadZone };

        for (int i = 0; i < 2; i++) {
            const auto section = kDeadZoneJoypadSections[i];
            int xPos = ini.GetLongValue(section, kDeadZonePositiveX, kDefaultDeadZoneValue);
            int xNeg = ini.GetLongValue(section, kDeadZoneNegativeX, -kDefaultDeadZoneValue);
            int yPos = ini.GetLongValue(section, kDeadZonePositiveY, kDefaultDeadZoneValue);
            int yNeg = ini.GetLongValue(section, kDeadZoneNegativeY, -kDefaultDeadZoneValue);
            setFunctions[i](xPos, xNeg, yPos, yNeg);
        }

        loadAudioOptions(ini);
        loadVideoOptions(ini);
    }
}

static void loadAudioOptions(const CSimpleIniA& ini)
{
    loadOptions(ini, kAudioOptions, kAudioSection);

    g_menuMusic = !g_musicOff;

    auto volume = ini.GetLongValue(kAudioSection, kMasterVolume);
    setMasterVolume(volume, false);

    auto musicVolume = ini.GetLongValue(kAudioSection, kMusicVolume);
    setMusicVolume(musicVolume, false);
}

static void loadVideoOptions(const CSimpleIniA& ini)
{
    auto windowMode = static_cast<WindowMode>(ini.GetLongValue(kVideoSection, kWindowMode, -1));

    if (windowMode < 0 || windowMode >= kMaxWindowMode)
        windowMode = kModeWindow;

    setWindowMode(windowMode, false);

    auto windowWidth = ini.GetLongValue(kVideoSection, kWindowWidth);
    auto windowHeight = ini.GetLongValue(kVideoSection, kWindowHeight);

    setWindowSize(windowWidth, windowHeight, false);

    auto displayWidth = ini.GetLongValue(kVideoSection, kFullScreenWidth);
    auto displayHeight = ini.GetLongValue(kVideoSection, kFullScreenHeight);

    setFullScreenResolution(displayWidth, displayHeight, false);

    auto windowResizable = ini.GetBoolValue(kVideoSection, kWindowResizable, true);
    setWindowResizable(windowResizable, false);
}

void saveOptions()
{
    logInfo("Saving options");

    CSimpleIniA ini;
    ini.SetValue(kStandardSection, nullptr, nullptr, "; Automatically generated by SWOS\n"
        "; Do not edit! All your changes will be lost\n" );

    saveOptions(ini, kStandardOptions, kStandardSection);

    const char kInputControlsComment[] = "; 1 = keyboard only, 2 = joypad only, 3 = keyboard & joypad, "
        "4 = joypad & keyboard, 5 = two joypads";
    ini.SetLongValue(kControlsSection, kInputControlsKey, g_inputControls, kInputControlsComment);

    saveOptions(ini, kControlOptions, kControlsSection);

    typedef decltype(joypad1DeadZone) GetFunction;
    GetFunction *getFunctions[] = { joypad1DeadZone, joypad2DeadZone };

    for (int i = 0; i < 2; i++) {
        int xPos, xNeg, yPos, yNeg;
        std::tie(xPos, xNeg, yPos, yNeg) = getFunctions[i]();

        const auto section = kDeadZoneJoypadSections[i];
        ini.SetLongValue(section, kDeadZonePositiveX, xPos);
        ini.SetLongValue(section, kDeadZoneNegativeX, xNeg);
        ini.SetLongValue(section, kDeadZonePositiveY, yPos);
        ini.SetLongValue(section, kDeadZoneNegativeY, yNeg);
    }

    saveAudioOptions(ini);
    saveVideoOptions(ini);

    auto path = pathInRootDir(kIniFilename);
    ini.SaveFile(path.c_str());
}

void saveAudioOptions(CSimpleIni& ini)
{
    g_musicOff = !g_menuMusic;

    saveOptions(ini, kAudioOptions, kAudioSection);

    auto masterVolume = getMasterVolume();
    ini.SetLongValue(kAudioSection, kMasterVolume, masterVolume);

    auto musicVolume = getMusicVolume();
    ini.SetLongValue(kAudioSection, kMusicVolume, musicVolume);
}

void saveVideoOptions(CSimpleIniA& ini)
{
    auto windowMode = getWindowMode();
    ini.SetLongValue(kVideoSection, kWindowMode, static_cast<long>(windowMode));

    int windowWidth, windowHeight;
    std::tie(windowWidth, windowHeight) = getWindowSize();

    ini.SetLongValue(kVideoSection, kWindowWidth, windowWidth);
    ini.SetLongValue(kVideoSection, kWindowHeight, windowHeight);

    int displayWidth, displayHeight;
    std::tie(displayWidth, displayHeight) = getFullScreenResolution();

    ini.SetLongValue(kVideoSection, kFullScreenWidth, displayWidth);
    ini.SetLongValue(kVideoSection, kFullScreenHeight, displayHeight);

    bool resizable = isWindowResizable();
    ini.SetBoolValue(kVideoSection, kWindowResizable, resizable);
}

std::vector<LogItem> parseCommandLine(int argc, char **argv)
{
    std::vector<LogItem> commandLineWarnings;

    const char kSwosDir[] = "--swos-dir=";
    const char kSound[] = "--sound=";
    const char kNoIntro[] = "--no-intro";
    const char kNoReels[] = "--no-image-reels";
    const char kMaxBank[] = "--max-bank";
    const char kBankNum[] = "--bank-number=";
    const char kControls[] = "--controls=";

    auto log = [&commandLineWarnings](const std::string& str, LogCategory category = kWarning) {
        commandLineWarnings.emplace_back(category, str);
    };

    for (int i = 1; i < argc; i++) {
        if (strstr(argv[i], kSwosDir) == argv[i]) {
            setRootDir(argv[i] + sizeof(kSwosDir) - 1);
        } else if (strstr(argv[i], kSound) == argv[i]) {
            auto setting = argv[i] + sizeof(kSound) - 1;
            m_soundState = (tolower(setting[0]) != 'o' || tolower(setting[1]) != 'n') && setting[0] != '1' ? kOff : kOn;
        } else if (!strcmp(argv[i], kNoIntro)) {
            m_noIntro = true;
        } else if (!strcmp(argv[i], kNoReels)) {
            m_noReels = true;
        } else if (!strcmp(argv[i], kMaxBank)) {
            log(std::string("Maximum bank number is ") + std::to_string(adl_getBanksCount() - 1), kInfo);
        } else if (strstr(argv[i], kBankNum) == argv[i]) {
            auto bankNumStr = argv[i] + sizeof(kBankNum) - 1;
            auto bankNo = atoi(bankNumStr);
            auto maxBankNo = adl_getBanksCount() - 1;
            if (bankNo >= 0 && bankNo <= maxBankNo) {
                m_bankNo = bankNo;
            } else {
                auto warningText = std::string("Invalid bank number ") + std::to_string(bankNo) +
                    ", range is [0.." + std::to_string(maxBankNo) + "]";
                log(warningText);
            }
        } else if (strstr(argv[i], kControls) == argv[i]) {
            auto controlsStr = argv[i] + sizeof(kControls) - 1;
            auto controls = atoi(controlsStr);
            if (controls > kNoControls && controls < kMaxControl) {
                m_controlInput = static_cast<Controls>(controls);
            } else {
                auto min = std::to_string(kNoControls + 1);
                auto max = std::to_string(kMaxControl - 1);
                log(std::to_string(controls) + " is not valid value for controls ([" + min + ".." + max + "] accepted)");
            }
        } else {
            commandLineWarnings.emplace_back(kWarning, std::string("Unknown option ignored: ") + argv[i]);
        }
    }

    return commandLineWarnings;
}

void normalizeOptions()
{
    if (m_soundState != kUnspecified) {
        auto soundOff = m_soundState == kOff;
        g_soundOff = soundOff;
        g_musicOff = soundOff;
        g_menuMusic = !g_musicOff;
    }

    normalizeInput();
}

bool disableIntro()
{
    return m_noIntro;
}

bool disableImageReels()
{
    return m_noReels;
}

int midiBankNumber()
{
    return m_bankNo;
}

//
// options menu
//

void SWOS::OptionsMenuSelected()
{
    logInfo("Showing options menu...");

    showMenu(optionsMenu);

    logInfo("Leaving options menu");
    CommonMenuExit();
}

void exitOptions()
{
    if (controlsStatus == 0x20) {
        SetExitMenuFlag();
        saveOptions();
    } else if (controlsStatus == 0x30 && ++showPrereleaseCounter == 15) {
        auto entry = getMenuEntryAddress(OptionsMenu::Entries::secret);
        entry->invisible = false;
    }
}

void doShowVideoOptionsMenu()
{
    showVideoOptionsMenu();
}

void doShowAudioOptionsMenu()
{
    showAudioOptionsMenu();
}

void showGameplayOptions()
{
    showMenu(gameplayOptionsMenu);
}

void changeGameStyle()
{
    m_gameStyle = !m_gameStyle;
}
