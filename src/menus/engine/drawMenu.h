#pragma once

void drawMenu(bool updateScreen = true);
void drawMenuItem(MenuEntry *entry);
void flipInMenu();
void menuFadeIn();
void menuFadeOut();
void enqueueMenuFadeIn();
void registerMenuLocalSprite(int width, int height, SDL_Texture *texture, bool cleanUp = true);
void clearMenuLocalSprites();
bool cursorFlashingEnabled();
void setFlashMenuCursor(bool flashMenuCursor);
