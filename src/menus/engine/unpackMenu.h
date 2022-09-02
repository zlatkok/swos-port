#pragma once

#include "menuCodes.h"

void unpackMenu(const void *src, char *dst = swos.g_currentMenu);
void restoreMenu(const void *menu, int selectedEntry);
void activateMenu(const void *menu);
const void *getCurrentPackedMenu();
std::pair<SwosMenu::EntryBoolOption::GetFn, SwosMenu::EntryBoolOption::SetFn> getBoolOptionAccessors(const MenuEntry& entry);
