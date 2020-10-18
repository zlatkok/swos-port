#pragma once

#undef assert

#ifdef _MSC_VER
# define debugBreak() __debugbreak()
#elif defined(__clang__)
# ifdef __ARM_ARCH
#  define debugBreak() raise(SIGINT)
# else
#  define debugBreak() asm("int $3")
# endif
#else
# error Define debug break directive for your compiler
#endif

#ifdef SWOS_TEST
# define assert(e) SWOS_UnitTest::assertImp(e, #e, __FILE__, __LINE__)
namespace SWOS_UnitTest {
    void assertImp(bool expr, const char *exprStr, const char *file, int line);
}
#elif defined(NDEBUG)
# define assert(e) ((void)0)
#else
template<typename T>
void assert(T t)
{
    if (!t)
        debugBreak();
}
#endif
