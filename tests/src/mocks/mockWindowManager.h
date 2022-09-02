#pragma once

constexpr int kDefaultWindowWidth = 640;
constexpr int kDefaultWindowHeight = 400;

SDL_Window *createWindow();
void setWindowResizable(bool resizable);
void setWindowDisplayIndex(int windowIndex);
void setIsInFullScreenMode(bool isInFullScreen);
void failNextDisplayModeSwitch();
