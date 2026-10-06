#include <doctest/doctest.h>

#include "ui/ConfirmArm.h"

using ui::ConfirmArm;

TEST_CASE("ConfirmArm: the first click arms, a later click on the same item confirms") {
  ConfirmArm arm;
  CHECK(arm.armed() == -1);
  arm.update(0.016f, 1);
  CHECK_FALSE(arm.click(1));
  CHECK(arm.armed() == 1);
  arm.update(ConfirmArm::kMinDelay + 0.01f, 1);
  CHECK(arm.click(1));
  CHECK(arm.armed() == -1); // confirmed: disarmed
}

TEST_CASE("ConfirmArm: a double-click does not confirm") {
  ConfirmArm arm;
  CHECK_FALSE(arm.click(0));
  arm.update(0.1f, 0); // ~100 ms later: the press of a double-click
  CHECK_FALSE(arm.click(0));
  CHECK(arm.armed() == 0); // still armed, the real second click can follow
  arm.update(0.4f, 0);
  CHECK(arm.click(0));
}

TEST_CASE("ConfirmArm: expires, and disarms when the pointer leaves or another item is clicked") {
  ConfirmArm arm;
  arm.click(2);
  arm.update(ConfirmArm::kExpire + 0.1f, 2);
  CHECK(arm.armed() == -1);
  CHECK_FALSE(arm.click(2)); // armed afresh, not confirmed

  arm.update(1.0f, -1); // the pointer left
  CHECK(arm.armed() == -1);

  arm.click(0);
  arm.update(1.0f, 0);
  CHECK_FALSE(arm.click(1)); // another item: arms that one instead
  CHECK(arm.armed() == 1);
  arm.update(1.0f, 1);
  CHECK(arm.click(1));
}
