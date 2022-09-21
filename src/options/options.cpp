#include "options.h"
#include "file.h"
#include "controls.h"
#include "keyboard.h"
#include "joypads.h"
#include "audio.h"
#include "music.h"
#include "chants.h"
#include "windowManager.h"
#include "drawMenu.h"
#include "menuItemRenderer.h"
#include "amigaMode.h"
#include "pitch.h"
#include "spinningLogo.h"
#include "replays.h"
#include "render.h"
#include "overlay.h"
#include "OptionVariable.h"
#include "OptionAccessor.h"
#include "util.h"

enum SoundEnabledState { kUnspecified, kOn, kOff, } static m_soundState;

using OptionalControls = std::pair<bool, Controls>;
using OptionalJoypadGuid = std::pair<bool, std::string>;

static OptionalControls m_pl1Controls;
static OptionalControls m_pl2Controls;

static OptionalJoypadGuid m_pl1Joypad;
static OptionalJoypadGuid m_pl2Joypad;

static int16_t m_gameStyle;         // currently: 0 = PC, 1 = Amiga
static int16_t m_showPreMatchMenus = 1;

static constexpr char kIniFilename[] =
#ifdef DEBUG
    "swos-debug.ini";
#else
    "swos.ini";
#endif

// ini sections
static const char kStandardSection[] = "standardOptions";
static const char kVideoSection[] = "video";
static const char kAudioSection[] = "audio";
static const char kReplaySection[] = "replays";
static const char kGameplaySection[] = "gameplay";

// option keys
// video
static const char kFlashMenuCursorKey[] = "flashMenuCursor";
static const char kShowFpsKey[] = "showFps";
static const char kUseLinearFilteringKey[] = "useLinearFiltering";
static const char kClearScreenKey[] = "clearScreen";
static const char kSpinningLogoKey[] = "showSpinningLogo";
static const char kZoomKey[] = "zoom";
static const char kMenuItemGradientBackground[] = "menuItemGradientBackground";
// audio
static const char kSoundEnabledKey[] = "soundEnabled";
static const char kMusicEnabledKey[] = "musicEnabled";
static const char kCommentaryEnabledKey[] = "commentaryEnabled";
static const char kCrowdChantsEnabledKey[] = "crowdChantsEnabled";
static const char kMasterVolumeKey[] = "masterVolume";
static const char kMusicVolumeKey[] = "musicVolume";
// replays
static const char kAutoSaveReplaysKey[] = "autoSaveReplays";
static const char kShowReplayPercentageKey[] = "showReplayPercentage";
// gameplay
static const char kGameStyleKey[] = "gameStyle";
static const char kShowPreMatchMenusKey[] = "preMatchMenus";

static const std::array<OptionVariable<int16_t>, 5> kStandardOptions = {
    "gameLength",  &swos.g_gameLength, 0, 3, 0,
    "autoReplays",  &swos.g_autoReplays, 0, 1, 0,
    "autoSaveHighlights",  &swos.g_autoSaveHighlights, 0, 1, 1,
    "allPlayerTeamsEqual", &swos.g_allPlayerTeamsEqual, 0, 1, 0,
    "pitchType", &swos.g_pitchType, -2, 6, 4,
};

static const std::array<OptionAccessor<bool>, 13> kBoolOptions = {
    soundEnabled, initSoundEnabled, kAudioSection, kSoundEnabledKey, true,
    musicEnabled, initMusicEnabled, kAudioSection, kMusicEnabledKey, true,
    commentaryEnabled, setCommentaryEnabled, kAudioSection, kCommentaryEnabledKey, true,
    areCrowdChantsEnabled, initCrowdChantsEnabled, kAudioSection, kCrowdChantsEnabledKey, true,
    cursorFlashingEnabled, setFlashMenuCursor, kVideoSection, kFlashMenuCursorKey, true,
    getShowFps, setShowFps, kVideoSection, kShowFpsKey, false,
    getLinearFiltering, setLinearFiltering, kVideoSection, kUseLinearFilteringKey, true,
    getClearScreen, setClearScreen, kVideoSection, kClearScreenKey, true,
    spinningLogoEnabled, enableSpinningLogo, kVideoSection, kSpinningLogoKey, true,
    getAutoSaveReplays, setAutoSaveReplays, kReplaySection, kAutoSaveReplaysKey, true,
    getShowReplayPercentage, setShowReplayPercentage, kReplaySection, kShowReplayPercentageKey, false,
    menuGradientFillEnabled, enableMenuGradientFill, kVideoSection, kMenuItemGradientBackground, true,
    showPreMatchMenus, setPreMatchMenus, kStandardSection, kShowPreMatchMenusKey, true,
};

static const std::array<OptionAccessor<int>, 3> kIntOptions = {
    getMasterVolume, initMasterVolume, kAudioSection, kMasterVolumeKey, 60,
    getMusicVolume, initMusicVolume, kAudioSection, kMusicVolumeKey, 55,
    getGameStyle, setGameStyle, kGameplaySection, kGameStyleKey, kPcGameStyle,
};
static const std::array<OptionAccessor<float>, 1> kFloatOptions = {
    getZoomFactor, initZoomFactor, kVideoSection, kZoomKey, 0.925f,
};

#ifdef __ANDROID__
static SI_Error loadIniFile(CSimpleIni& ini, const std::string& path);
static SI_Error saveIniFile(const CSimpleIni& ini, const std::string& path);
#endif

static SI_Error safeSaveIniFile(CSimpleIni& ini, const std::string& path);
static void setDefaultOptions();
template<typename T, size_t N, typename F>
static void bulkLoadOptions(const std::array<OptionAccessor<T>, N>& options, F f);
template<typename IniType, typename T, size_t N, typename F>
static void bulkSaveOptions(const std::array<OptionAccessor<T>, N>& options, F f);

void loadOptions()
{
    auto path = pathInRootDir(kIniFilename);
    logInfo("Loading options from %s", path.c_str());

    CSimpleIniA ini(true);

#ifdef __ANDROID__
    auto errorCode = loadIniFile(ini, path);
#else
    auto errorCode = ini.LoadFile(path.c_str());
#endif

    if (errorCode < 0)
        logWarn("Error loading options, error code: %d", errorCode);

    setDefaultOptions();

    loadOptions(ini, kStandardOptions, kStandardSection);
    loadWindowOptions(ini);
    loadControlOptions(ini);

    using namespace std::placeholders;

    bulkLoadOptions(kBoolOptions, std::bind(&CSimpleIniA::GetBoolValue, &ini, _1, _2, _3, nullptr));
    bulkLoadOptions(kIntOptions, std::bind(&CSimpleIniA::GetLongValue, &ini, _1, _2, _3, nullptr));
    bulkLoadOptions(kFloatOptions, std::bind(&CSimpleIniA::GetDoubleValue, &ini, _1, _2, _3, nullptr));
}

void saveOptions()
{
    auto path = pathInRootDir(kIniFilename);
    logInfo("Saving options to %s...", path.c_str());

    CSimpleIniA ini(true);
    ini.SetValue(kStandardSection, nullptr, nullptr, "; Automatically generated by SWOS\n"
        "; Careful when editing the file -- the changes might get lost!\n" );
    ini.SetValue(kGameplaySection, nullptr, nullptr, "; gameStyle: 0 = PC, 1 = Amiga");

    saveOptions(ini, kStandardOptions, kStandardSection);
    saveWindowOptions(ini);
    saveControlOptions(ini);

    using namespace std::placeholders;

    bulkSaveOptions<bool>(kBoolOptions, std::bind(&CSimpleIniA::SetBoolValue, &ini, _1, _2, _3, nullptr, false));
    bulkSaveOptions<long>(kIntOptions, std::bind(&CSimpleIniA::SetLongValue, &ini, _1, _2, _3, nullptr, false, false));
    bulkSaveOptions<double>(kFloatOptions, std::bind(&CSimpleIniA::SetDoubleValue, &ini, _1, _2, _3, nullptr, false));

    auto errorCode = safeSaveIniFile(ini, path);

    if (errorCode < 0) {
        beep();
        logWarn("Failed to save options, error code: %d", errorCode);
    }
}

template<typename T, size_t N, typename F>
static void bulkLoadOptions(const std::array<OptionAccessor<T>, N>& options, F f)
{
    for (const auto& opt : options) {
        auto value = static_cast<T>(f(opt.section, opt.key, opt.defaultValue));
        opt.set(value);
    }
}

template<typename IniType, typename T, size_t N, typename F>
static void bulkSaveOptions(const std::array<OptionAccessor<T>, N>& options, F f)
{
    for (const auto& opt : options) {
        assert(opt.get);
        f(opt.section, opt.key, static_cast<IniType>(opt.get()));
    }
}

static void setDefaultOptions()
{
    swos.g_autoReplays = 1;
    swos.g_autoSaveHighlights = 1;
    swos.g_pitchType = -1;
    m_showPreMatchMenus = 1;
}

static SI_Error safeSaveIniFile(CSimpleIni& ini, const std::string& path)
{
    const auto& tmpFile = path + ".tmp";
#ifdef __ANDROID__
    auto errorCode = saveIniFile(ini, tmpFile.c_str());
#else
    auto errorCode = ini.SaveFile(tmpFile.c_str());
#endif
    if (errorCode != SI_OK)
        return errorCode;

    return renameFile(tmpFile.c_str(), path.c_str()) ? SI_OK : SI_FILE;
}

#ifdef __ANDROID__
static SI_Error loadIniFile(CSimpleIni& ini, const std::string& path)
{
    auto f = openFile(path.c_str(), "r");
    if (!f)
        return SI_FILE;

    auto size = SDL_RWsize(f);
    if (size < 0) {
        SDL_RWclose(f);
        return SI_FILE;
    }

    char iniBuf[6 * 1024];
    void *buf = iniBuf;
    size_t numRead = 0;

    if (size > sizeof(iniBuf)) {
        buf = SDL_malloc(size);
        if (!buf) {
            SDL_RWclose(f);
            return SI_NOMEM;
        }
    }
    numRead = SDL_RWread(f, buf, size, 1);

    if (!numRead) {
        SDL_RWclose(f);
        return SI_FILE;
    }

    auto result = ini.LoadData(iniBuf, size);

    if (buf != iniBuf)
        SDL_free(buf);

    return result;
}

class RWopsWriter : public CSimpleIni::OutputWriter {
public:
    RWopsWriter(const std::string& path) {
        m_context = openFile(path.c_str(), "w");
    }
    ~RWopsWriter() {
        if (m_context)
            SDL_RWclose(m_context);
    }
    void Write(const char *buf) override {
        if (m_context)
            SDL_RWwrite(m_context, buf, strlen(buf), 1);
    }
private:
    SDL_RWops *m_context = nullptr;
};

static SI_Error saveIniFile(const CSimpleIni& ini, const std::string& path)
{
    RWopsWriter writer(path);
    return ini.Save(writer, true);
}
#endif

template<typename LogFunction>
static void parseControls(LogFunction log, OptionalControls& controls, const char *str)
{
    char *endPtr;
    auto controlsIndex = strtol(str, &endPtr, 0);

    if (endPtr == str) {
        controls.first = true;

        if (strstr(str, "none") == str) {
            controls.second = kNone;
        } else if (strstr(str, "keyboard1") == str) {
            controls.second = kKeyboard1;
        } else if (strstr(str, "keyboard2") == str) {
            controls.second = kKeyboard2;
        } else if (strstr(str, "joystick") == str || strstr(str, "joypad")) {
            controls.second = kJoypad;
        } else {
            log("Unrecognized control name: "s + str);
            controls.first = false;
        }
    } else {
        if (controlsIndex >= 0 && controlsIndex < kNumControls) {
            controls.first = true;
            controls.second = static_cast<Controls>(controlsIndex);
        } else {
            auto max = std::to_string(static_cast<int>(kNumControls) - 1);
            log(std::to_string(controls.second) + " is not valid value for controls ([0.." + max + "] accepted)");
        }
    }
}

std::vector<LogItem> parseCommandLine(int argc, char **argv)
{
    std::vector<LogItem> commandLineWarnings;

    const char kSwosDir[] = "--swos-dir=";
    const char kSound[] = "--sound=";
    const char kNoPreMatchMenus[] = "--no-pre-match-menus";
    const char kPl1Controls[] = "--pl1controls=";
    const char kPl2Controls[] = "--pl2controls=";
    const char kPl1Joypad[] = "--pl1joypad=";
    const char kPl2Joypad[] = "--pl2joypad=";

    auto log = [&commandLineWarnings](const std::string& str, LogCategory category = kWarning) {
        commandLineWarnings.emplace_back(category, str);
    };

    for (int i = 1; i < argc; i++) {
        if (strstr(argv[i], kSwosDir) == argv[i]) {
            setRootDir(argv[i] + sizeof(kSwosDir) - 1);
        } else if (strstr(argv[i], kSound) == argv[i]) {
            auto setting = argv[i] + sizeof(kSound) - 1;
            m_soundState = (tolower(setting[0]) != 'o' || tolower(setting[1]) != 'n') && setting[0] != '1' ? kOff : kOn;
        } else if (!strcmp(argv[i], kNoPreMatchMenus)) {
            m_showPreMatchMenus = 1;
        } else if (strstr(argv[i], kPl1Controls) == argv[i] || strstr(argv[i], kPl2Controls)) {
            assert(argv[i][4] == '1' || argv[i][4] == '2');

            auto& controls = argv[i][4] == '1' ? m_pl1Controls : m_pl2Controls;
            auto controlsStr = argv[i] + sizeof(kPl1Controls) - 1;

            parseControls(log, controls, controlsStr);

            if (controls.first && controls.second == kNone && &controls == &m_pl1Controls) {
                controls.first = false;
                log("Player 1 can not have controls disabled");
            }
        } else if (strstr(argv[i], kPl1Joypad) == argv[i] || strstr(argv[i], kPl2Joypad) == argv[i]) {
            assert(argv[i][4] == '1' || argv[i][4] == '2');

            auto& joypad = argv[i][4] == '1' ? m_pl1Joypad : m_pl2Joypad;
            auto joypadStr = argv[i] + sizeof(kPl1Joypad) - 1;

            if (*joypadStr) {
                joypad.first = true;
                joypad.second = joypadStr;
            }
        } else {
            log("Unknown option ignored: "s + argv[i]);
        }
    }

    return commandLineWarnings;
}

void normalizeOptions()
{
    if (m_soundState != kUnspecified) {
        auto soundOn = m_soundState != kOff;
        initSoundEnabled(soundOn);
        initMusicEnabled(soundOn);
    }

    if (!keyboardPresent())
        unsetKeyboardControls();
}

bool showPreMatchMenus()
{
    return m_showPreMatchMenus != 0;
}

void setPreMatchMenus(bool showPreMatchMenus)
{
    m_showPreMatchMenus = showPreMatchMenus;
}

int getGameStyle()
{
    return m_gameStyle;
}

void setGameStyle(int gameStyle)
{
    m_gameStyle = gameStyle;
    setAmigaModeEnabled(m_gameStyle == kAmigaGameStyle);
}
