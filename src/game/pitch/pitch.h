#pragma once

constexpr int kPitchWidth = 672;
constexpr int kPitchHeight = 848;

void initPitches();
void setPitchTypeAndNumber();
void loadPitch();
std::pair<float, float> drawPitch(float cameraX, float cameraY);
std::pair<float, float> drawPitchAtCurrentCamera();
bool zoomIn(float step = 0);
bool zoomOut(float step = 0);
bool resetZoom();
float getZoomFactor();
void initZoomFactor(float zoom);
bool setZoomFactor(float zoom, float step = 0);
Uint32 getLastZoomChangeTime();
