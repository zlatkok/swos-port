#pragma once

using UpdateHook = std::function<void ()>;

void setWindowResizable(bool resizable);
void setWindowDisplayIndex(int windowIndex);
void setIsInFullScreenMode(bool isInFullScreen);
void failNextDisplayModeSwitch();
void setUpdateHook(UpdateHook updateHook);
