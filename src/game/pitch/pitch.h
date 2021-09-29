#pragma once

void initPitches();
void loadPitch();
std::pair<float, float> drawPitch(float cameraX, float cameraY);
std::pair<float, float> drawPitchAtCurrentCamera();
bool zoomIn(float step = 0);
bool zoomOut(float step = 0);
float getZoomFactor();
bool setZoomFactor(float zoom, float step = 0);
