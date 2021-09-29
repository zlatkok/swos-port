#pragma once

constexpr int kVgaWidth = 320;
constexpr int kVgaHeight = 200;
constexpr int kVgaScreenSize = kVgaWidth * kVgaHeight;

struct Color;

void initRendering();
void finishRendering();
SDL_Renderer *getRenderer();
SDL_Rect getViewport();
void updateScreen(bool delay = false);

void fadeIn(std::function<void()> render, double factor = 1.0);
void fadeOut(std::function<void()> render, double factor = 1.0);

void drawRectangle(int x, int y, int width, int height, const Color& color);

bool getLinearFiltering();
void setLinearFiltering(bool useLinearFiltering);

void makeScreenshot();
