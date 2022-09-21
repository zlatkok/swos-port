#pragma once

constexpr double kTargetFpsPC = 70;
constexpr double kTargetFpsAmiga = 50;

void initTimer();
void initFrameTicks();
double targetFps();
void setTargetFps(double fps);
void timerProc(int factor = 1);
void markFrameStartTime();
void menuFrameDelay();
void gameFrameDelay();
void measureRendering(std::function<void()> render);
