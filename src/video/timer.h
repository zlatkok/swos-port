#pragma once

constexpr double kTargetFps = 70;

void initTimer();
void initFrameTicks();
void timerProc(int factor = 1);
void markFrameStartTime();
void frameDelay(double factor = 1.0);
void measureRendering(std::function<void()> render);
