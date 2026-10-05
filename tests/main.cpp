#ifdef NDEBUG
#error "tests must be built with assertions enabled (NDEBUG undefined)"
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
