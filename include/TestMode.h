#pragma once
// Test hooks for the UI screenshot harness (tools/ui_script.cpp). Shipped code paths never set these:
// only the harness does, before the first frame. When `active` is false every hook is a no-op and the
// game behaves exactly as before.
struct TestMode {
  bool active = false;         // master switch: Input:: reads the scripted queue instead of raylib
  bool fixedStep = false;      // every frame advances exactly fixedDt, whatever the wall clock says
  float fixedDt = 1.0f / 60.0f;
  bool reduceMotion = false;   // initial value of Settings::reduceMotion (read when App is constructed)
  bool audioDisabled = false;  // AudioManager::init() does not open a device
  unsigned seed = 0;           // seeds raylib's and the C RNG (the game currently has no other random source)
  double clock = 0.0;          // simulated seconds since start (advanced by App::frame in fixed-step mode)

  static TestMode& get() { static TestMode t; return t; }
  // Seed the RNGs. Call once after setting the fields.
  static void apply();
};
