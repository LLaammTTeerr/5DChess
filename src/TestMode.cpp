#include "TestMode.h"
#include <raylib.h>
#include <cstdlib>

void TestMode::apply() {
  TestMode& t = get();
  SetRandomSeed(t.seed);
  std::srand(t.seed);
}
