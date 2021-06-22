#pragma once

enum InputTextAllowedChars {
    kLimitedCharset,
    kFileCharset,
    kFullCharset
};

bool inputText(MenuEntry& entry, size_t maxLength, InputTextAllowedChars allowExtraChars = kLimitedCharset);
bool inputNumber(MenuEntry& entry, int maxDigits, int minNum, int maxNum, bool allowNegative = false);
