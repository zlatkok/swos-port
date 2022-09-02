#pragma once

bool mapCoordinatesToGameArea(int& x, int& y);
void updateLogicalScreenSize();
void updateGameScaleFactor(int width, int height);
float getGameScale();
float getGameScreenOffsetX();
float getGameScreenOffsetY();
float getFieldWidth();
float getFieldHeight();
SDL_FRect mapRect(int x, int y, int width, int height);
