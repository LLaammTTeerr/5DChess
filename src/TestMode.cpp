#include "TestMode.h"
#include "Render/Motion.h"
#include <raylib.h>
#include <cstdlib>

void TestMode::apply() {
  TestMode& t = get();
  SetRandomSeed(t.seed);
  std::srand(t.seed);
  if (t.reduceMotion) UI::Motion::setReduced(true);
}
