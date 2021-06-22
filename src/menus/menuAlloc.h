#pragma once

void resetMenuAllocator();

char *menuAlloc(size_t size);
void menuFree(size_t size);

dword menuAllocString(const char *str);
char *menuAllocStringTable(size_t size);
char *menuAllocMultilineText(size_t numLines);
