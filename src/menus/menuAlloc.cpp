//
// Menu allocation routines
//
// These routines allocate memory from the unused space in the menu buffer.
// This memory is only valid for the duration of the current menu.
//

#include "menuAlloc.h"

constexpr int kMenuMemorySize = 1024;
static char *m_menuMemory;

inline static void verifyMenuMemoryPtr()
{
    assert(m_menuMemory >= swos.g_currentMenu + sizeof(swos.g_currentMenu) - kMenuMemorySize &&
        m_menuMemory < swos.g_currentMenu + sizeof(swos.g_currentMenu));
}

void resetMenuAllocator()
{
    m_menuMemory = swos.g_currentMenu + sizeof(swos.g_currentMenu) - kMenuMemorySize;
}

char *menuAlloc(size_t size)
{
    auto buf = m_menuMemory;
    m_menuMemory += size;

    verifyMenuMemoryPtr();
    return buf;
}

void menuFree(size_t size)
{
    m_menuMemory -= size;
    verifyMenuMemoryPtr();
}

dword menuAllocString(const char *str)
{
    auto buf = m_menuMemory;

    int len = 0;
    while (*str)
        buf[len++] = *str++;

    buf[len] = '\0';
    m_menuMemory += len + 1;

    verifyMenuMemoryPtr();
    return SwosVM::ptrToOffset(buf);
}

char *menuAllocStringTable(size_t size)
{
    auto buf = m_menuMemory;
    auto alignment = (~reinterpret_cast<uintptr_t>(buf) + 1) & 3;
    m_menuMemory += alignment + size;

    verifyMenuMemoryPtr();
    return buf + alignment;
}

char *menuAllocMultilineText(size_t numLines)
{
    auto buf = m_menuMemory;
    auto alignment = ~reinterpret_cast<uintptr_t>(buf) & 3;

    m_menuMemory += alignment + numLines * 4 + 1;
    verifyMenuMemoryPtr();
    return buf + alignment;
}
