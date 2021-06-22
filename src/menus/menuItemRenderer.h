#pragma once

void cacheMenuItemBackgrounds();
void drawMenuItemSolidBackground(const MenuEntry *entry);
void drawMenuItemInnerFrame(int x, int y, int width, int height, word color);
void drawMenuItemOuterFrame(MenuEntry *entry);
