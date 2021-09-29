#pragma once

template <typename T> struct Option {
    const char *key;
    T *value;
    typename std::make_signed<T>::type min;
    typename std::make_signed<T>::type max;
    T defaultValue;

    static_assert(std::is_integral<T>::value, "");

    T clamp(long val) const {
        if (val < min || val > max)
            logInfo("Option value for %s out of range (%d), clamping to [%d, %d]", key, val, min, max);

        val = std::min<long>(max, val);
        val = std::max<long>(min, val);

        return static_cast<T>(val);
    }
};

template<typename T, size_t N>
static void loadOptions(const CSimpleIniA& ini, const std::array<Option<T>, N>& options, const char *section)
{
    for (const auto& opt : options) {
        auto val = ini.GetLongValue(section, opt.key, opt.defaultValue);
        *opt.value = opt.clamp(val);
    }
}

template<typename T, size_t N>
inline void saveOptions(CSimpleIni& ini, const std::array<Option<T>, N>& options, const char *section)
{
    for (const auto& opt : options)
        ini.SetLongValue(section, opt.key, *opt.value);
}

void loadOptions();
void saveOptions();
std::vector<LogItem> parseCommandLine(int argc, char **argv);
void normalizeOptions();

bool showPreMatchMenus();
bool showSpinningLetterS();
void toggleSpinningLetterS();
