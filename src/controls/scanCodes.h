#pragma once

struct ScanCodes {
    byte up;
    byte down;
    byte left;
    byte right;
    byte fire;
    byte bench;

    bool conflicts(const ScanCodes& other) const;
    bool has(byte scanCode) const;
    bool hasNullScancode() const;
    void fromArray(const byte *scanCodes);
    void load(const CSimpleIni& ini, const char *section, const char **keys, const ScanCodes& default);
    void save(CSimpleIni& ini, const char *section, const char **keys);
    void log(const char *leading = "");
};

const char *pcScanCodeToString(uint8_t scanCode);
uint8_t sdlScanCodeToPc(SDL_Scancode sdlScanCode);
