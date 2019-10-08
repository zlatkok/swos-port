#pragma once

#include "exceptions.h"

namespace SWOS_UnitTest
{
    using MenuCallback = std::function<bool ()>;

    void setMenuCallback(MenuCallback callback);
    bool exitMenuProc();

    namespace Detail {
        template<typename T1, typename T2>
        bool different(const T1& t1, const T2& t2) {
            return t1 != t2;
        }
        static inline bool different(const char *t1, const char *t2) {
            return strcmp(t1, t2) != 0;
        }
        static inline bool different(const char *t1, char *t2) {
            return strcmp(t1, t2) != 0;
        }
        static inline bool different(char *t1, const char *t2) {
            return strcmp(t1, t2) != 0;
        }
        static inline bool different(char *t1, char *t2) {
            return strcmp(t1, t2) != 0;
        }
    };
    template<typename T1, typename T2>
    void assertEqualImp(bool mustBeEqual, const T1& t1, const T2& t2, const char *t1Str, const char *t2Str, const char *file, int line)
    {
        if (Detail::different(t1, t2) == mustBeEqual)
            throw FailedEqualAssertException(mustBeEqual, t1, t2, t1Str, t2Str, file, line);
    }

    void assertTrueImp(bool expression, const char *exprStr, const char *file, int line);
    void assertStringEqualImp(const char *s1, const char *s2, const char *s1Str, const char *s2Str, const char *file, int line);
    void assertNumItemsImp(int num, const char *numStr, const char *file, int line);
    void assertItemIsNumberImp(int index, const char *indexStr, int value, const char *file, int line);
    void assertItemIsVisibleImp(int index, const char *indexStr, bool visible, const char *file, int line);
    void assertItemIsVisibleImp(const MenuEntry *entry, const char *, bool visible, const char *file, int line);
    void assertItemEnabledImp(int index, const char *indexStr, bool enabled, const char *file, int line);
    void assertItemIsStringImp(int index, const char *indexStr, const char *value, const char *file, int line);
    void assertItemIsStringImp(const MenuEntry *entry, const char *, const char *value, const char *file, int line);
    void assertItemIsStringImp(const MenuEntry *entry, const char *, const std::string& value, const char *file, int line);
    void assertItemIsStringTableImp(int index, const char *indexStr, const char *value, const char *file, int line);
    void assertItemIsSpriteImp(int index, const char *indexStr, int spriteIndex, const char *spriteIndexStr, const char *file, int line);
    void assertItemHasColorImp(int index, const char *indexStr, int color, const char *colorStr, const char *file, int line);
    void assertItemHasTextColorImp(int index, const char *indexStr, int color, const char *colorStr, const char *file, int line);
    void selectItemImp(int index, const char *indexStr, const char *file, int line);
    void selectItemImp(MenuEntry *entry, const char *entryStr, const char *file, int line);
    void clickItemImp(int index, const char *indexStr, const char *file, int line);
    void setNumericItemValueImp(int index, const char *indexStr, int value, const char *file, int line);
    void sendMouseWheelEventImp(int index, const char *indexStr, int direction, const char *file, int line);

    int numItems();
    bool isItemVisible(int index);
    bool queueKeys(const std::vector<SDL_Scancode>& keys);
}

#define assertTrue(e) SWOS_UnitTest::assertTrueImp(e, #e, __FILE__, __LINE__)
#define assertFalse(e) SWOS_UnitTest::assertTrueImp(!(e), #e, __FILE__, __LINE__)
#define assertEqual(v1, v2) SWOS_UnitTest::assertEqualImp(true, v1, v2, #v1, #v2, __FILE__, __LINE__)
#define assertNotEqual(v1, v2) SWOS_UnitTest::assertEqualImp(false, v1, v2, #v1, #v2, __FILE__, __LINE__)
#define assertStringEqualCaseInsensitive(s1, s2) SWOS_UnitTest::assertStringEqualImp(s1, s2, #s1, #s2, __FILE__, __LINE__)
#define assertItemIsNumber(i, v) SWOS_UnitTest::assertItemIsNumberImp(i, #i, v, __FILE__, __LINE__)
#define assertItemIsVisible(i) SWOS_UnitTest::assertItemIsVisibleImp(i, #i, true, __FILE__, __LINE__)
#define assertItemIsInvisible(i) SWOS_UnitTest::assertItemIsVisibleImp(i, #i, false, __FILE__, __LINE__)
#define assertItemVisibility(i, v) SWOS_UnitTest::assertItemIsVisibleImp(i, #i, v, __FILE__, __LINE__)
#define assertItemIsString(i, s) SWOS_UnitTest::assertItemIsStringImp(i, #i, s, __FILE__, __LINE__)
#define assertItemIsStringTable(i, s) SWOS_UnitTest::assertItemIsStringTableImp(i, #i, s, __FILE__, __LINE__)
#define assertItemIsSprite(i, s) SWOS_UnitTest::assertItemIsSpriteImp(i, #i, s, #s, __FILE__, __LINE__)
#define assertItemHasColor(i, c) SWOS_UnitTest::assertItemHasColorImp(i, #i, c, #c, __FILE__, __LINE__)
#define assertItemHasTextColor(i, c) SWOS_UnitTest::assertItemHasTextColorImp(i, #i, c, #c, __FILE__, __LINE__)
#define selectItem(i) SWOS_UnitTest::selectItemImp(i, #i, __FILE__, __LINE__)
#define clickItem(i) SWOS_UnitTest::clickItemImp(i, #i, __FILE__, __LINE__)
#define setNumericItemValue(i, v) SWOS_UnitTest::setNumericItemValueImp(i, #i, v, __FILE__, __LINE__)
#define assertItemEnabled(i, e) SWOS_UnitTest::assertItemEnabledImp(i, #i, e, __FILE__, __LINE__)
#define sendMouseWheelEvent(i, d) SWOS_UnitTest::sendMouseWheelEventImp(i, #i, d, __FILE__, __LINE__)
#define assertNumItems(n) SWOS_UnitTest::assertNumItemsImp(n, #n, __FILE__, __LINE__)
